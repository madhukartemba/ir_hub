# Remote OTA via GitHub Releases

The device polls a JSON manifest in this repo every 6 hours (and once shortly
after boot). When the manifest version is newer than what the device is
running, it downloads `firmware_<variant>.bin` from a GitHub Release and
self-flashes. No physical access required.

## One-time setup

### 1. Configure the device

Edit `include/secrets.h` before flashing:

```c
#define OTA_PASSWORD       "pick-something-long"
#define OTA_MANIFEST_URL   "https://cdn.jsdelivr.net/gh/<you>/<repo>@main/ota/manifest.json"
```

- `OTA_PASSWORD` protects LAN pushes via `pio run --target upload --upload-port <ip>`.
  An empty string disables the check (logged as a warning at boot).
- `OTA_MANIFEST_URL` is what the device polls. Empty disables HTTP-pull OTA
  entirely.

**Use jsDelivr, not `raw.githubusercontent.com`.** jsDelivr is a free CDN
that mirrors public GitHub repos and — crucially — supports the TLS MFLN
extension (RFC 6066). The ESP8266 has only ~18-20 KB free heap available
for a TLS handshake, which is too tight to receive full-size 16 KB TLS
records. MFLN lets the client negotiate smaller records so a 1 KB receive
buffer is sufficient. `raw.githubusercontent.com` is fronted by Fastly,
whose MFLN support is inconsistent — it works for some clients/networks
and crashes the SYS task (Exception 29) for others.

Both URLs serve the same file from the same commit; jsDelivr just plays
nicer with constrained TLS clients.

The actual **firmware binary** stays on GitHub Releases (linked from inside
the manifest). By the time the device downloads it, MQTT has been shut down
to free heap, so the larger TLS buffer needed for the GitHub Releases edge
(objects.githubusercontent.com) usually fits.

### 2. Commit the manifest skeleton

Create `ota/manifest.json` in this repo:

```json
{
  "variants": {
    "v0": { "version": "0.0.0", "url": "" },
    "v1": { "version": "0.0.0", "url": "" },
    "v3": { "version": "1.0.0", "url": "https://github.com/<you>/<repo>/releases/download/v1.0.0/firmware_v3.bin" }
  }
}
```

Only the variant that matches the device's `OTA_HW_VARIANT` (set per env in
`platformio.ini`) is consulted, so the others can stay at `0.0.0` until you
have a build for them.

### 3. Flash version 1.0.0 to the device

```bash
pio run -e ir_hub_version_3 --target upload
```

The Home screen header will now read `IR Hub v1.0.0`.

## Cutting a new release

1. **Bump the version** in `platformio.ini`:

   ```ini
   [env]
   custom_firmware_version = 1.0.1
   ```

2. **Build the binary**:

   ```bash
   pio run -e ir_hub_version_3
   cp .pio/build/ir_hub_version_3/firmware.bin firmware_v3.bin
   ```

3. **Create a GitHub Release** named `v1.0.1`:

   ```bash
   gh release create v1.0.1 firmware_v3.bin \
     --title "v1.0.1" \
     --notes "Fix XYZ, improve ABC"
   ```

   The asset will be reachable at the stable URL
   `https://github.com/<you>/<repo>/releases/download/v1.0.1/firmware_v3.bin`.

4. **Update `ota/manifest.json`** in the repo:

   ```json
   {
     "variants": {
       "v3": {
         "version": "1.0.1",
         "url": "https://github.com/<you>/<repo>/releases/download/v1.0.1/firmware_v3.bin"
       }
     }
   }
   ```

   Commit and push to `main`. The device will pick it up on its next poll.

5. **(Optional) Trigger an immediate update** instead of waiting 6 hours.
   If the device has MQTT configured, publish an empty message to its OTA
   topic:

   ```bash
   mosquitto_pub -h <broker> -u <user> -P <pass> \
     -t "ir_hub/<mac>/ota/check" -n
   ```

   The MAC appears in the device's MQTT discovery topics (and in the boot log).

## What the device does

On boot:
1. Bring up Wi-Fi, MQTT, Alexa, etc.
2. Wait 30 seconds (lets transient heap settle).
3. GET the manifest.
4. If a newer version exists, shut down MQTT to free heap, run
   `ESP8266HTTPUpdate.update(url)`, show progress on OLED + LED ring,
   restart on success. Failures get logged and shown briefly, then the
   device keeps running the old firmware.

On the loop, every 6 hours: same as steps 3-4 above.

Whenever a manifest fetch fails or the update aborts mid-stream, the existing
`BootGuard` + `LittleFS` recovery handles the worst case. If the new firmware
crashes 3 times in a row at boot, the device sticks on the error screen
("Boot loop detected — Power off & re-flash") instead of endlessly cycling.

## Security notes

The default config uses `WiFiClientSecure::setInsecure()` — bytes are
encrypted in transit but the server cert is not validated. That's good enough
to stop casual snooping but won't stop an attacker who can MITM the
connection to your repo (very rare in practice, especially against GitHub).

If you need stronger guarantees:

- **Pin a fingerprint.** Read GitHub's TLS cert SHA-256 once, hard-code it
  via `secure->setFingerprint(...)`. Breaks on cert rotation (~12 months).
- **Sign the binary.** Espressif supports SHA-256 signature verification in
  the bootloader (`USE_SIGNED_OTA`). Slightly more involved but the gold
  standard for fleet OTA.

For a friend-tester rollout, `setInsecure()` over HTTPS to a release URL
under your GitHub account is generally fine.

## Disabling OTA

If you ever want to ship a device without remote updates (e.g. air-gapped
network):

```c
#define OTA_MANIFEST_URL ""
```

The OTA module logs `[OTA-HTTP] Disabled` at boot and never touches the
network.
