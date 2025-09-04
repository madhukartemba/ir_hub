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


def upload_firmware_to_ip(env_name, ip_address, firmware_path, lock):
    """Upload pre-built firmware to a specific IP address"""
    with lock:
        print(f"📡 Uploading firmware to {ip_address}...")

    try:
        # Use PlatformIO's upload command with direct IP specification
        cmd = [
            "pio",
            "run",
            "-e",
            env_name,
            "--target",
            "upload",
            "--upload-port",
            ip_address,
        ]
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


def main():
    parser = argparse.ArgumentParser(
        description="Upload IR Hub firmware to multiple boards via OTA",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Upload to multiple devices (builds once, uploads in parallel)
  python upload_ota.py -e ir_hub_version_0 -i 192.168.1.100 192.168.1.101
  python upload_ota.py -e ir_hub_version_1 -i 192.168.1.102
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
        required=True,
        nargs="+",
        help="IP addresses to upload to (space-separated or comma-separated)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be done without actually uploading",
    )
    parser.add_argument(
        "--max-workers",
        type=int,
        default=4,
        help="Maximum number of parallel upload workers (default: 4)",
    )

    args = parser.parse_args()

    # Parse IP addresses (handle both space and comma separated)
    ip_list = []
    for ip_group in args.ips:
        if "," in ip_group:
            ip_list.extend([ip.strip() for ip in ip_group.split(",")])
        else:
            ip_list.append(ip_group.strip())

    # Remove duplicates while preserving order
    ip_list = list(dict.fromkeys(ip_list))

    print("🚀 IR Hub OTA Upload Script")
    print("=" * 30)
    print(f"Environment: {args.env}")
    print(f"IP Addresses: {', '.join(ip_list)}")
    print(f"Total boards: {len(ip_list)}")
    print(f"Max workers: {args.max_workers}")

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
        if upload_firmware_to_ip(args.env, ip, firmware_path, threading.Lock()):
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
                    upload_firmware_to_ip, args.env, ip, firmware_path, print_lock
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
