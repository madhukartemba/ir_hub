#!/usr/bin/env python3
"""
IR Hub - OTA Upload Script
Uploads firmware to multiple boards over WiFi using their IP addresses
"""

import subprocess
import sys
import argparse
import os
import concurrent.futures
import threading
import time
import socket
import re
from pathlib import Path

try:
    from zeroconf import ServiceBrowser, ServiceListener, Zeroconf
except ImportError:
    Zeroconf = None
    ServiceBrowser = None
    ServiceListener = object


def build_firmware_once(env_name):
    """Build the firmware once and return the path to the binary"""
    print(f"🔨 Building firmware for environment {env_name}...")

    try:
        # Build the firmware
        cmd = ["pio", "run", "-e", env_name]
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)

        # Find the firmware binary path
        # PlatformIO typically stores binaries in .pio/build/{env_name}/firmware.bin
        firmware_path = f".pio/build/{env_name}/firmware.bin"

        if os.path.exists(firmware_path):
            print(f"✅ Firmware built successfully: {firmware_path}")
            return firmware_path
        else:
            print(f"❌ Firmware binary not found at expected path: {firmware_path}")
            return None

    except subprocess.CalledProcessError as e:
        print(f"❌ Build failed: {e.stderr}")
        return None
    except Exception as e:
        print(f"❌ Unexpected error during build: {e}")
        return None


def find_espota_script():
    """Locate espota.py from common PlatformIO package paths."""
    candidates = [
        Path.home() / ".platformio/packages/framework-arduinoespressif8266/tools/espota.py",
        Path.home() / ".platformio/packages/tool-esptoolpy/espota.py",
    ]
    for path in candidates:
        if path.exists():
            return str(path)
    return None


def read_ota_password(secrets_file):
    """Read OTA_PASSWORD from include/secrets.h-style header."""
    path = Path(secrets_file)
    if not path.exists():
        print(f"❌ secrets file not found: {path}")
        return None
    text = path.read_text()
    match = re.search(r'^\s*#define\s+OTA_PASSWORD\s+"([^"]*)"\s*$', text, re.M)
    if not match:
        print(f"❌ OTA_PASSWORD not found in {path}")
        return None
    return match.group(1)


def upload_firmware_to_ip(ip_address, firmware_path, espota_path, ota_port, ota_password, lock):
    """Upload pre-built firmware to a specific IP address"""
    with lock:
        print(f"📡 Uploading firmware to {ip_address}...")

    try:
        # Upload directly through espota.py so auth can be passed reliably.
        cmd = [
            sys.executable,
            espota_path,
            "-i",
            ip_address,
            "-p",
            str(ota_port),
            "-f",
            firmware_path,
        ]
        if ota_password:
            cmd.extend(["-a", ota_password])
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)

        with lock:
            print(f"✅ Successfully uploaded to {ip_address}")
        return True

    except subprocess.CalledProcessError as e:
        with lock:
            print(f"❌ Failed to upload to {ip_address}")
            print(f"   Error: {e.stderr}")
        return False
    except Exception as e:
        with lock:
            print(f"❌ Unexpected error uploading to {ip_address}: {e}")
        return False


class OtaMdnsListener(ServiceListener):
    """Collect ArduinoOTA mDNS services and resolve IP addresses."""

    def __init__(self, name_prefix):
        self.name_prefix = (name_prefix or "").lower()
        self._ips = set()
        self._lock = threading.Lock()

    @property
    def ips(self):
        with self._lock:
            return sorted(self._ips)

    def remove_service(self, zeroconf, type_, name):
        return

    def update_service(self, zeroconf, type_, name):
        self.add_service(zeroconf, type_, name)

    def add_service(self, zeroconf, type_, name):
        info = zeroconf.get_service_info(type_, name, timeout=1000)
        if not info:
            return

        server = (info.server or "").lower().rstrip(".")
        service_name = (info.name or "").lower()
        if self.name_prefix and self.name_prefix not in server and self.name_prefix not in service_name:
            return

        addresses = []
        if hasattr(info, "parsed_addresses"):
            addresses = info.parsed_addresses()
        if not addresses:
            addresses = [socket.inet_ntoa(addr) for addr in (info.addresses or [])]

        with self._lock:
            for ip in addresses:
                if ip and "." in ip and ":" not in ip:
                    self._ips.add(ip)


def discover_ota_devices(timeout_sec, name_prefix):
    """Discover ArduinoOTA endpoints via mDNS (_arduino._tcp.local.)."""
    if Zeroconf is None:
        print("❌ mDNS discovery requires zeroconf package.")
        print("   Install with: python3 -m pip install zeroconf")
        return []

    listener = OtaMdnsListener(name_prefix)
    zc = Zeroconf()
    try:
        ServiceBrowser(zc, "_arduino._tcp.local.", listener)
        print(f"🔎 Discovering OTA devices via mDNS for {timeout_sec:.1f}s...")
        time.sleep(timeout_sec)
        return listener.ips
    finally:
        zc.close()


def main():
    parser = argparse.ArgumentParser(
        description="Upload IR Hub firmware to multiple boards via OTA",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Auto-discover IR Hubs by mDNS hostname prefix (recommended with unique hostnames)
  python upload_ota.py -e ir_hub_version_3 --discover
  python upload_ota.py -e ir_hub_version_3 --discover --discover-timeout 8

  # Read OTA password from include/secrets.h and upload
  python upload_ota.py -e ir_hub_version_3 --discover --secrets-file include/secrets.h

  # Upload to multiple devices (builds once, uploads in parallel)
  python upload_ota.py -e ir_hub_version_0 -i 192.168.1.100 192.168.1.101
  python upload_ota.py -e ir_hub_version_1 -i 192.168.0.183 192.168.0.110 192.168.0.25 192.168.0.161
  python upload_ota.py -e ir_hub_version_0 -i 192.168.1.100,192.168.1.101,192.168.1.102
  
  # Control parallel upload workers
  python upload_ota.py -e ir_hub_version_0 -i 192.168.1.100,192.168.1.101,192.168.1.102 --max-workers 6
  
  # Dry run to see what would happen
  python upload_ota.py -e ir_hub_version_0 -i 192.168.1.100 192.168.1.101 --dry-run
        """,
    )

    parser.add_argument(
        "-e",
        "--env",
        required=True,
        help="PlatformIO environment name (e.g., ir_hub_version_0)",
    )
    parser.add_argument(
        "-i",
        "--ips",
        required=False,
        nargs="+",
        help="IP addresses to upload to (space-separated or comma-separated)",
    )
    parser.add_argument(
        "--discover",
        action="store_true",
        help="Discover ArduinoOTA devices over mDNS and upload to all matching hosts.",
    )
    parser.add_argument(
        "--discover-timeout",
        type=float,
        default=5.0,
        help="Seconds to wait for mDNS discovery (default: 5.0).",
    )
    parser.add_argument(
        "--discover-prefix",
        default="ir-hub-",
        help="mDNS hostname/service prefix filter for discovery (default: ir-hub-).",
    )
    parser.add_argument(
        "--secrets-file",
        default="include/secrets.h",
        help="Path to secrets header containing OTA_PASSWORD (default: include/secrets.h).",
    )
    parser.add_argument(
        "--ota-password",
        default=None,
        help="Override OTA password (otherwise read from --secrets-file).",
    )
    parser.add_argument(
        "--ota-port",
        type=int,
        default=8266,
        help="ArduinoOTA TCP port (default: 8266).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without actually uploading",
    )
    parser.add_argument(
        "--max-workers",
        type=int,
        default=8,
        help="Maximum number of parallel upload workers (default: 8)",
    )

    args = parser.parse_args()

    if not args.ips and not args.discover:
        parser.error("Provide --ips and/or --discover.")

    # Parse IP addresses (handle both space and comma separated)
    ip_list = []
    for ip_group in args.ips or []:
        if "," in ip_group:
            ip_list.extend([ip.strip() for ip in ip_group.split(",") if ip.strip()])
        else:
            ip = ip_group.strip()
            if ip:
                ip_list.append(ip)

    if args.discover:
        discovered = discover_ota_devices(args.discover_timeout, args.discover_prefix)
        if discovered:
            print(f"✅ Discovered {len(discovered)} OTA device(s): {', '.join(discovered)}")
            ip_list.extend(discovered)
        else:
            print("⚠️  No OTA devices discovered over mDNS")

    # Remove duplicates while preserving order
    ip_list = list(dict.fromkeys(ip_list))

    if not ip_list:
        print("❌ No target devices found (via --ips or --discover).")
        sys.exit(1)

    print("🚀 IR Hub OTA Upload Script")
    print("=" * 30)
    print(f"Environment: {args.env}")
    print(f"IP Addresses: {', '.join(ip_list)}")
    print(f"Total boards: {len(ip_list)}")
    print(f"Max workers: {args.max_workers}")

    espota_path = find_espota_script()
    if not espota_path:
        print("❌ Could not locate espota.py in PlatformIO packages.")
        print("   Try running: pio run -e <env> once, then retry.")
        sys.exit(1)

    ota_password = args.ota_password
    if ota_password is None:
        ota_password = read_ota_password(args.secrets_file)
        if ota_password is None:
            sys.exit(1)
    auth_status = "set" if ota_password else "empty (unauthenticated)"
    print(f"OTA auth: {auth_status}")

    if args.dry_run:
        print("\n🔍 Dry run mode - no actual uploads will be performed")
        for i, ip in enumerate(ip_list, 1):
            print(f"  {i}. Would upload to {ip}")
        return

    start_time = time.time()

    # Step 1: Build firmware once
    print("\n🔨 Building firmware...")
    print("-" * 20)
    firmware_path = build_firmware_once(args.env)

    if not firmware_path:
        print("❌ Build failed. Cannot proceed with uploads.")
        sys.exit(1)

    # Step 2: Upload to all devices in parallel
    print(f"\n📡 Uploading to {len(ip_list)} devices in parallel...")
    print("-" * 20)

    successful = 0
    failed = 0

    if len(ip_list) == 1:
        # Single device - no need for threading
        ip = ip_list[0]
        print(f"📡 Uploading to {ip}...")
        if upload_firmware_to_ip(
            ip, firmware_path, espota_path, args.ota_port, ota_password, threading.Lock()
        ):
            successful += 1
        else:
            failed += 1
    else:
        # Multiple devices - use parallel uploads
        print_lock = threading.Lock()

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=args.max_workers
        ) as executor:
            # Submit all upload tasks
            futures = []
            for ip in ip_list:
                future = executor.submit(
                    upload_firmware_to_ip,
                    ip,
                    firmware_path,
                    espota_path,
                    args.ota_port,
                    ota_password,
                    print_lock,
                )
                futures.append((future, ip))

            # Wait for all uploads to complete
            for future, ip in futures:
                try:
                    if future.result():
                        successful += 1
                    else:
                        failed += 1
                except Exception as e:
                    with print_lock:
                        print(f"❌ Exception during upload to {ip}: {e}")
                    failed += 1

    # Calculate total time
    total_time = time.time() - start_time

    # Print summary
    print("\n📊 Upload Summary:")
    print("=" * 20)
    print(f"✅ Successful: {successful}")
    print(f"❌ Failed: {failed}")
    print(f"📊 Total: {len(ip_list)}")
    print(f"⏱️  Total time: {total_time:.1f} seconds")
    if len(ip_list) > 1:
        avg_time_per_board = total_time / len(ip_list)
        print(f"📈 Average time per board: {avg_time_per_board:.1f} seconds")

    if failed > 0:
        print(f"\n⚠️  {failed} upload(s) failed. Check the error messages above.")
        sys.exit(1)
    else:
        print("\n🎉 All uploads completed successfully!")


if __name__ == "__main__":
    main()
