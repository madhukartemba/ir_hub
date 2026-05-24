# IR Hub Help Guide

Welcome to IR Hub. This guide explains what each feature does and gives practical, step-by-step tutorials.

## Quick Navigation

- [What IR Hub Does](#what-ir-hub-does)
- [Main Menu Overview](#main-menu-overview)
- [Settings Overview](#settings-overview)
- [Tutorial: First-Time Setup](#tutorial-first-time-setup)
- [Tutorial: Add a Device](#tutorial-add-a-device)
- [Tutorial: Check for Updates](#tutorial-check-for-updates)
- [Tutorial: Local OTA (Wi-Fi Upload)](#tutorial-local-ota-wi-fi-upload)
- [Tutorial: Factory Reset](#tutorial-factory-reset)
- [Troubleshooting](#troubleshooting)
- [Contact & Help QR](#contact--help-qr)

---

## What IR Hub Does

IR Hub lets you control IR-based appliances (TV, AC, etc.) and bridge them with:

- physical button interactions
- on-device menu
- MQTT / Home Assistant
- Alexa integration
- OTA firmware updates

---

## Main Menu Overview

From the Home screen, long-press to open Main Menu.

- **Devices**: View/manage learned devices.
- **Add Device**: Learn new IR remote codes.
- **Settings**: Device behavior, OTA checks, reset actions.
- **Help**: Opens a QR code to this help document.
- **Contact**: Opens a QR code for contact email.

---

## Settings Overview

Settings includes:

- **Sound / Haptics** toggles
- **Check for Updates** (opens a live status screen)
- **Firmware / Last Check / Status** info rows
- **Restart**
- **Reset Wi-Fi** (with confirmation)
- **Erase Saved Data** (with confirmation)
- **Factory Reset** (with confirmation; clears Wi-Fi + saved data + pending OTA slot)
- **By Madhukar Temba :)** easter egg

---

## Tutorial: First-Time Setup

1. Power on IR Hub.
2. If Wi-Fi is not configured, follow captive portal flow.
3. Open Main Menu -> **Settings** and verify:
   - Sound/Haptics preferences
   - Firmware version shown correctly
4. Return to Home screen and confirm status display + LED ring behavior.

---

## Tutorial: Add a Device

1. Open Main Menu -> **Add Device**.
2. Follow on-screen recording flow:
   - record first IR code
   - record second IR code
3. Keep remote pointed at IR receiver while recording.
4. On success, device is saved and available in **Devices**.

Tips:

- Avoid very long button holds while recording.
- Keep ambient IR noise low (bright direct sunlight can interfere).

---

## Tutorial: Check for Updates

1. Open **Settings** -> **Check for Updates**.
2. A dedicated screen opens and starts check after entry.
3. Watch status:
   - `Checking...`
   - `Up to date`
   - `No Wi-Fi`
   - `Check failed`
   - `Update found`

If update is found, IR Hub transitions into OTA install flow automatically.

---

## Tutorial: Local OTA (Wi-Fi Upload)

Use local OTA to push firmware directly from your network.

### Requirements

- Device and computer on same network
- `OTA_PASSWORD` set in `include/secrets.h`
- PlatformIO installed

### Upload by IP list

```bash
python3 upload_ota.py -e ir_hub_version_3 -i 192.168.0.173 192.168.0.151 192.168.0.74 192.168.0.94
```

### Upload using auto-discovery

```bash
python3 upload_ota.py -e ir_hub_version_3 --discover
```

The script reads `OTA_PASSWORD` from `include/secrets.h` by default.

---

## Tutorial: Factory Reset

Use this only when you want a clean start.

1. Open **Settings** -> **Factory Reset**.
2. Read warning and long-press to confirm.
3. Device will:
   - clear Wi-Fi credentials
   - erase saved data (LittleFS format)
   - clear pending OTA marker
   - restart

After restart, configure Wi-Fi again.

---

## Troubleshooting

### Update check says `No Wi-Fi`

- Confirm network connection first.
- Reconnect Wi-Fi if needed.

### Update check says `Check failed`

- Verify internet access.
- Verify manifest URL is reachable.
- Retry from **Check for Updates**.

### Local OTA auth failures

- Ensure upload password matches `OTA_PASSWORD`.
- Reflash once if password changed recently.

### USB upload timeouts

- Try a better USB cable/port.
- Lower upload speed if needed.

---

## Contact & Help QR

- Main Menu -> **Help**: QR to this guide
- Main Menu -> **Contact**: QR to contact email

---

If you are maintaining IR Hub and want release workflow details, see [`docs/RELEASE.md`](RELEASE.md).
