# IR Hub

A self-contained, open-source smart IR universal remote built around the ESP8266. IR Hub learns the codes from your existing TV/AC/audio remotes, stores them locally, and lets you fire them from a physical button, an on-device menu, Home Assistant (MQTT), or Alexa — all over your home Wi-Fi.

It ships as a complete hardware + firmware project: custom KiCad PCB, 3D-printed enclosure, and PlatformIO firmware with OTA updates.

- **Help (web preview):** https://ir-hub.pages.dev/help
- **Help (markdown source):** [`docs/HELP.md`](docs/HELP.md)
- **Release / OTA notes:** [`docs/RELEASE.md`](docs/RELEASE.md)

---

## What It Does

- **Learns IR remotes.** Point any IR remote at the receiver, press a button, and the Hub stores the raw signal.
- **Replays IR commands.** 8x parallel high-current IR LEDs give very long range so the Hub can sit across the room from the appliance.
- **Smart home bridge.** Each learned device shows up as an MQTT entity (works great with Home Assistant) and as an Alexa-controllable device (via FauxmoESP / Espalexa).
- **Standalone UI.** 128x64 OLED + single button + 30-LED NeoPixel ring + haptic motor + buzzer. No phone required for day-to-day use.
- **Captive-portal Wi-Fi setup.** First boot exposes an `IRHub V3 Setup` AP; users join it and configure Wi-Fi + MQTT from their phone.
- **OTA updates.** Cloud OTA (via Cloudflare Pages manifest, polled hourly) and LAN OTA (ArduinoOTA + mDNS auto-discovery).
- **Safe boot.** Boot-loop counter + LittleFS format-on-failure recovery so a bad save never bricks the device.

---

## Hardware Variants

**Only `v3` (the custom PCB build) is actively supported.** The older `v0` (NodeMCU perfboard) and `v1` (D1 Mini perfboard) variants still exist in the codebase as historical prototypes, but they are **deprecated** — no longer maintained, no longer released, and not recommended for new builds.

The rest of this README only covers v3.

---

## Bill of Materials (v3)

Full BOM with LCSC-style designators lives in [`PCB/IRHub_PCB_V3/production/bom.csv`](PCB/IRHub_PCB_V3/production/bom.csv). The major active parts:

| Block | Part | Role |
|---|---|---|
| MCU | **ESP-12F** (ESP8266) | Brains + Wi-Fi |
| USB | **USB-C receptacle + CH340C** | USB-UART for programming and 5 V power |
| Power | **AMS1117-3.3** LDO | 5 V -> 3.3 V regulation |
| Display | **OLED-128O064D** (SH1106, I²C) | 128x64 status/menu screen |
| LEDs | **30x WS2812B-2020** | Circular addressable RGB ring |
| IR TX | **8x IR204A** + UMH3N driver + AO3400A MOSFET | High-current IR blaster array |
| IR RX | **TSOP382xx** | 38 kHz IR demodulator for learning |
| Haptics | **DRV2605L** + external LRA (JST connector) | Tactile click feedback |
| Audio | **CMT-7525-80-SMT-TR** magnetic buzzer | UI beeps / chimes |
| Input | **Single tactile push button** | All UI navigation |

Plus passives, test points, and the JST-SH connector for the LRA.

---

## How To Build One (v3)

### 1. Order the PCB

KiCad project: [`PCB/IRHub_PCB_V3/`](PCB/IRHub_PCB_V3/)

- Source schematic + board: `IRHub_V3.kicad_sch`, `IRHub_V3.kicad_pcb`
- Manufacturing bundle: [`PCB/IRHub_PCB_V3/production/IR_Hub_3.zip`](PCB/IRHub_PCB_V3/production/IR_Hub_3.zip) (gerbers + drills, ready to upload to JLCPCB / PCBWay / OSHPark)
- For JLCPCB-style PCBA also use: `production/bom.csv` + `production/positions.csv`
- 3D preview / STEP: `IRHub_V3.step` and `IRHub_V3_Model.step`

Default board specs are 2-layer, 1.6 mm, HASL — anything in that ballpark from any low-cost fab works fine.

### 2. Print the Enclosure

3D-printable enclosure parts for the v3 PCB live in [`3D Print/Version 3/`](3D%20Print/Version%203/):

- `Base.stl` — main housing the PCB drops into
- `Top Cover.stl` — top shell with the OLED + button cutouts
- `Led Ring.stl` — diffuser ring that sits over the WS2812 ring

Plus the full Fusion 360 source as `IR Hub V3 v74.f3z`, and ready-to-print GCode under `3D Print/Version 3/GCode/`.

Recommended print settings: PLA, 0.2 mm layer height, 15–20% infill, supports off for `Top Cover` (geometry is overhang-friendly).

### 3. Assemble

1. Solder the PCB (or order PCBA). If hand-soldering, start with the QFP / SMD parts (CH340C, AMS1117, DRV2605L), then passives, then through-hole IR LEDs and the button, then the WS2812 ring last so it doesn't get cooked.
2. Verify 3.3 V on test point `TP9` and 5 V on `5V1` *before* installing the ESP-12F.
3. Solder/clip in the OLED, plug the LRA into the JST-SH connector.
4. Drop the populated PCB into the printed `Base`, screw on `Top Cover`, slip `Led Ring` over the LEDs.

### 4. Flash the Firmware

You only need to do this once — after that, OTA updates take over.

1. Install **PlatformIO** (VS Code extension or `pip install platformio`).
2. Clone this repo and open it in PlatformIO.
   ```bash
   git clone https://github.com/madhukartemba/ir_hub.git
   cd ir_hub
   ```
3. Create your `include/secrets.h` from the template:
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```
   Edit it to set (all optional, but recommended):
   - `MQTT_HOST` / `MQTT_USER` / `MQTT_PASSWORD` — Home Assistant broker.
   - `OTA_PASSWORD` — required for LAN OTA pushes.
   - `OTA_MANIFEST_URL` — your Cloudflare Pages manifest URL (only needed if you fork and self-host releases).
4. Plug the assembled board into USB-C and run:
   ```bash
   pio run -e ir_hub_version_3 -t upload
   ```
5. Open the serial monitor to see boot logs:
   ```bash
   pio device monitor -b 115200
   ```

That's it — the rest of the configuration (Wi-Fi, MQTT credentials, learned remotes) happens on-device.

### 5. Power On & Configure

On first boot the Hub shows `IR Hub — Ready! (Offline)` and starts a captive-portal AP called **`IRHub V3 Setup`**. Connect a phone to it and the captive portal walks you through Wi-Fi (and optional MQTT) setup. From there, see the [User Help Guide](docs/HELP.md) for adding remotes, using Alexa, etc.

---

## Pinout (v3)

For reference, defined in [`src/config.h`](src/config.h):

| Function | ESP-12F Pin |
|---|---|
| Button | D0 |
| OLED SCL | D1 |
| OLED SDA | D2 |
| Buzzer | D3 |
| IR Receiver | D5 |
| IR Transmitter | D6 |
| NeoPixel ring | D7 |

---

## Project Layout

```
ir_hub/
  src/                Firmware entry + UI screens
  lib/                Internal libraries (IR, MQTT, Display, OtaUpdater, etc.)
  include/            Project headers + secrets template
  PCB/                KiCad sources + gerbers + production BOM (v3; v1 archived)
  3D Print/           Fusion sources + STL + GCode for enclosures
  docs/               User help, release/OTA documentation
  scripts/            Release automation + local OTA upload helper
  ota/                Public OTA manifest served via Cloudflare Pages
  binaries/           Signed firmware images served via Cloudflare Pages
```

---

## For Developers

All commands target v3 — the older `v0` / `v1` PlatformIO envs are deprecated and not part of the release pipeline.

- **Build:** `pio run -e ir_hub_version_3`
- **Cut a release:** `python3 scripts/release.py 1.2.3` — bumps `custom_firmware_version`, builds `ir_hub_version_3`, stages the binary under `binaries/`, updates `ota/manifest.json`, commits + pushes. See [`docs/RELEASE.md`](docs/RELEASE.md) for the full flow.
- **LAN OTA push:** `python3 scripts/local_upload_ota.py -e ir_hub_version_3 --discover` — mDNS-discovers `ir-hub-*` devices and pushes the current build over the network. Password is read from `include/secrets.h`.

---

## License

MIT — see `LICENSE` (or use the source freely; attribution appreciated).

---

<div align="center">
  Made by <a href="mailto:madhusmiles.madhukar@gmail.com">Madhukar Temba</a>
</div>
