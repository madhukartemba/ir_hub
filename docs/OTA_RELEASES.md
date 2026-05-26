# Remote OTA via GitHub Releases

The device polls a JSON manifest in this repo every 1 hour (and once shortly
after boot). When the manifest version is newer than what the device is
running, it downloads `firmware_<variant>_v<version>.bin` via jsDelivr
(serving the file from this repo's `binaries/` folder) and self-flashes.
No physical access required.

The binary is *also* attached to a GitHub Release for human visibility, but
the device deliberately does not download from there — see the TLS note
below.

## One-time setup

### 1. Configure the device

Edit `include/secrets.h` before flashing:

```c
#define OTA_PASSWORD       "pick-something-long"
#define OTA_MANIFEST_URL   "https://<your-project>.pages.dev/ota/manifest.json"
```

- `OTA_PASSWORD` protects LAN pushes via `pio run --target upload --upload-port <ip>`.
  An empty string disables the check (logged as a warning at boot).
- `OTA_MANIFEST_URL` is what the device polls. Empty disables HTTP-pull OTA
  entirely.

**Use Cloudflare Pages for both manifest and binary, not jsDelivr, `raw.githubusercontent.com`
or GitHub Releases.** Cloudflare Pages is a free hosting service that you can link to your GitHub repo, 
and — crucially — supports the TLS MFLN extension (RFC 6066). The ESP8266
has only ~18-20 KB free heap available for a TLS handshake, which is too
tight to receive full-size 16 KB TLS records. MFLN lets the client negotiate
smaller records so a 4 KB receive buffer is sufficient.

Fastly (`raw.githubusercontent.com`), jsDelivr (which uses Fastly for some requests), and the GitHub Releases edge
(`objects.githubusercontent.com`) all negotiate MFLN inconsistently. They
work from a laptop but crash the ESP8266 mid-handshake or mid-download with
`Unhandled C++ exception: OOM` or TLS aborts.

That's why every release commits the firmware binary into this repo's
`binaries/` folder (e.g. `binaries/firmware_v3_v1.0.3.bin`) and the
manifest URL points at Cloudflare Pages:

```
https://<your-project>.pages.dev/binaries/firmware_v3_v1.0.3.bin
```

The GitHub Release still gets the binary as an attached asset for easy
manual download, but the device never touches that URL.

### 2. Commit the manifest skeleton

Create `ota/manifest.json` in this repo:

```json
{
  "variants": {
    "v0": { "version": "0.0.0", "url": "" },
    "v1": { "version": "0.0.0", "url": "" },
    "v3": { "version": "1.0.0", "url": "https://<your-project>.pages.dev/binaries/firmware_v3_v1.0.0.bin" }
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

Use `scripts/release.py` — it does all of the below in one command. See
`docs/RELEASE.md`. If you want to do it manually:

1. **Bump the version** in `platformio.ini`:

   ```ini
   [env]
   custom_firmware_version = 1.0.1
   ```

2. **Build the binary**:

   ```bash
   pio run -e ir_hub_version_3
   ```

3. **Commit the binary into `binaries/`** so Cloudflare Pages can serve it:

   ```bash
   mkdir -p binaries
   cp .pio/build/ir_hub_version_3/firmware.bin binaries/firmware_v3_v1.0.1.bin
   ```

4. **(Optional) Create a GitHub Release** named `v1.0.1` for human visibility:

   ```bash
   gh release create v1.0.1 binaries/firmware_v3_v1.0.1.bin \
     --title "v1.0.1" \
     --notes "Fix XYZ, improve ABC"
   ```

5. **Update `ota/manifest.json`** to point at the Cloudflare Pages URL:

   ```json
   {
     "variants": {
       "v3": {
         "version": "1.0.1",
         "url": "https://<your-project>.pages.dev/binaries/firmware_v3_v1.0.1.bin"
       }
     }
   }
   ```

6. **Commit and push** `platformio.ini`, `binaries/`, and `ota/manifest.json`
   to `main`. The device will pick it up on its next poll.

7. **(Optional) Trigger an immediate update** instead of waiting up to an hour.
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

On the loop, every 1 hour: same as steps 3-4 above.

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
