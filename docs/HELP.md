# IR Hub — User Guide

Welcome to your IR Hub! This guide walks you through everything you need — setting it up, using it daily, and getting the most out of its smart-home features.

> Looking for something specific? Jump straight to the section you need below.

## Quick Navigation

- [What is IR Hub?](#what-is-ir-hub)
- [Button Cheat Sheet](#button-cheat-sheet)
- [LED Ring Colors](#led-ring-colors)
- [First-Time Setup](#first-time-setup)
- [Adding a Remote (Add Device)](#adding-a-remote-add-device)
- [Using Your Saved Devices](#using-your-saved-devices)
- [Smart Home Integration](#smart-home-integration)
- [Settings Menu](#settings-menu)
- [Keeping It Up to Date](#keeping-it-up-to-date)
- [Resetting Your Hub](#resetting-your-hub)
- [Troubleshooting](#troubleshooting)
- [Contact & Support](#contact--support)

---

## What is IR Hub?

IR Hub is a smart infrared (IR) remote that lives in one place in your home and replaces a drawer full of remotes. It can:

- **Learn** the button presses from any IR remote you already own (TV, AC, set-top box, fan, soundbar — anything that uses an IR remote).
- **Replay** those signals on command, with enough range to reach across the room.
- **Connect to your smart home** so you can also trigger your appliances from Home Assistant or Alexa.

Once set up, you can keep using your old remotes *and* control everything from your phone, your voice, or the Hub itself.

---

## Button Cheat Sheet

Everything on IR Hub is done with one button. Here's the full list of gestures:

| Where you are | Single Click | Long Press |
|---|---|---|
| Home screen (display sleeping) | Wake the screen | Wake the screen |
| Home screen (display awake) | (nothing) | Open the **Main Menu** |
| Inside a menu | Move to the **next item** | **Select** the highlighted item |
| Inside a confirmation screen | Cancel / go back | **Confirm** the action |

*Tip: If the screen is dark, just click once — that always wakes it up.*

---

## LED Ring Colors

The ring of lights around the Hub gives you quick visual feedback:

| Color | What it means |
|---|---|
| Soft white / breathing | Idle, ready to use |
| Green pulse | Sent a successful "ON" command |
| Orange / red pulse | Sent an "OFF" command |
| Rainbow sweep | Successfully added a new device |
| Solid green | Everything is connected and ready |
| Solid red | Something went wrong (check the screen for details) |
| Blue breathing | Currently looking at info screens or Help / Contact |

The ring will fade out after a few seconds of inactivity to save power — that's normal.

---

## First-Time Setup

When you first power your Hub on, here's what to expect:

1. **Plug it in.** The screen will say `IR Hub — Initializing...`, then the ring will light up.
2. **Connect to the setup network.** Take your phone and look for a Wi-Fi network named **`IRHub V3 Setup`**. Join it.
3. **Captive portal opens.** Your phone should automatically pop open a setup page. (If it doesn't, just open any website in your browser and it'll redirect you.)
4. **Pick your Wi-Fi.** Choose your home Wi-Fi network from the list and enter the password.
5. *(Optional)* **Enter your MQTT details** if you want to connect to Home Assistant — see [Smart Home Integration](#smart-home-integration) below.
6. **Done!** The Hub will reboot, connect to your network, and show `Ready!` on the screen.

You can now start adding your remotes.

---

## Adding a Remote (Add Device)

Let's teach the Hub how to control one of your appliances.

1. From the Home screen, **long-press** the button to open the **Main Menu**.
2. Click to highlight **Add Device**, then long-press to select.
3. Grab the original remote for the appliance you want to control (e.g. your TV remote).
4. **Point the remote at the front of the Hub** (the small dark window is the IR receiver) from about 10–30 cm away.
5. When prompted, press the **ON** button on your remote.
6. When prompted again, press the **OFF** button (or repeat the same button if the appliance uses a single toggle).
7. The Hub will confirm with a rainbow sweep and a happy chime — your new device is saved!

**Recording tips:**
- Press firmly but briefly — a quick, deliberate press works better than holding it down.
- Make sure you have line-of-sight between your remote and the Hub.
- If recording fails, try again from a slightly closer distance.
- Avoid direct sunlight or bright halogen lights pointed at the Hub — they create IR noise.

---

## Using Your Saved Devices

Once you've added a device, you have three ways to control it:

**1. From the Hub itself**
- Open **Main Menu -> Devices**
- Pick the appliance and toggle it on/off

**2. From your phone / Home Assistant** (if MQTT is set up)
- Each device appears automatically as a switch in Home Assistant
- Trigger it from dashboards, automations, scripts, etc.

**3. With your voice** (Alexa, via local discovery)
- Just say *"Alexa, discover devices"* once after adding a new appliance
- Then: *"Alexa, turn on TV"* / *"Alexa, turn off AC"*, etc.

---

## Smart Home Integration

### Home Assistant (MQTT)

If you have a Home Assistant install with an MQTT broker (e.g. Mosquitto), the Hub can connect to it automatically:

- During first-time setup, enter your **MQTT broker address**, **username**, and **password** in the captive portal.
- Each device you add later will publish itself on MQTT automatically — no Home Assistant config edits required.
- You can check the connection status at any time under **Settings -> MQTT status**.

### Alexa

The Hub presents each saved device to Alexa as a smart switch (no Alexa account or skill required — it uses local discovery on your Wi-Fi):

1. Make sure your Echo and the Hub are on the **same Wi-Fi network**.
2. Add at least one device on the Hub.
3. Say *"Alexa, discover devices"* — your new device will be found.
4. Control it by name: *"Alexa, turn on the TV."*

If you rename a device on the Hub, ask Alexa to discover again.

---

## Settings Menu

From the Main Menu, open **Settings** to access:

| Setting | What it does |
|---|---|
| **Sound** | Turns the buzzer beeps on/off |
| **Haptics** | Turns the vibration feedback on/off |
| **Check for Updates** | Manually look for new firmware |
| **Firmware** | Shows the version you're currently running |
| **Last check** | When the Hub last looked for an update |
| **Update status** | The result of the last update check |
| **MQTT status** | Whether the Hub is connected to your MQTT broker |
| **Restart** | Safely reboot the Hub |
| **Reset Wi-Fi** | Forget your saved Wi-Fi (for moving to a new network) |
| **Erase Saved Data** | Delete all saved remotes/devices |
| **Factory Reset** | Wipe **everything** and start over |

All destructive actions (reset / erase / factory reset) ask for a long-press confirmation, so you won't accidentally wipe your setup.

---

## Keeping It Up to Date

Your Hub gets better over time as new features are released. Updates are delivered automatically over the internet — you don't need to do anything.

**Automatic updates**
- The Hub checks for a new version once per hour while it's online.
- If one is found, it downloads and installs it on its own. The screen will say `Update found` and `Restarting to install...`, then come back online a minute later running the new version.

**Manual updates**
- Open **Settings -> Check for Updates** any time.
- A dedicated screen will show you exactly what's happening:
  - `Checking...` — looking for a new version
  - `Up to date` — you're already on the latest
  - `Update found` — installing now
  - `No Wi-Fi` — Hub isn't connected, see [Troubleshooting](#troubleshooting)
  - `Low heap` — try again in a few seconds, the Hub was busy
  - `Check failed` — temporary network blip, just try again

Your saved remotes, settings, and Wi-Fi credentials are all preserved across updates.

---

## Resetting Your Hub

There are three levels of reset, depending on what you want to clear:

| Action | What it clears | What it keeps |
|---|---|---|
| **Restart** | Nothing — just a fresh boot | Everything |
| **Reset Wi-Fi** | Wi-Fi network + password | Saved remotes, MQTT, sound/haptic settings |
| **Erase Saved Data** | All learned remotes/devices | Wi-Fi, MQTT, sound/haptic settings |
| **Factory Reset** | **Everything** (Wi-Fi, devices, settings) | Nothing |

All of these are under **Settings**. Each one shows a confirmation screen — **long-press** to actually go through with it.

After a Factory Reset, the Hub starts fresh and reopens the `IRHub V3 Setup` Wi-Fi network, ready for [First-Time Setup](#first-time-setup) again.

---

## Troubleshooting

### The screen is blank
The Hub dims the screen after a few seconds of inactivity to save power. **Click the button once** to wake it up.

### Can't find the `IRHub V3 Setup` Wi-Fi
- Wait about 30 seconds after plugging it in — the network takes a moment to start.
- Make sure your phone's Wi-Fi is on and refresh the list.
- If you've already set it up before, it won't broadcast `IRHub V3 Setup` again. Use **Settings -> Reset Wi-Fi** to get back to setup mode.

### Captive portal doesn't open on my phone
- Open any non-secure website (e.g. `http://example.com`) in your browser — that usually forces the portal to appear.
- Some Android phones need you to tap the network and choose *"Sign in to Wi-Fi network"*.

### My appliance doesn't respond after I added it
- Make sure the Hub has line-of-sight to your appliance.
- Try re-adding it; sometimes the first recording captures a weak signal.
- IR has a typical range of about 5–7 meters indoors — closer is more reliable.

### Update check says `No Wi-Fi`
- The Hub is offline. Check your home Wi-Fi is up.
- If you've changed your router/password recently, go to **Settings -> Reset Wi-Fi** and reconnect.

### Update check says `Check failed` or `Low heap`
- Usually a temporary network blip or a busy moment. Wait a few seconds and try again.
- If it keeps failing, use **Settings -> Restart** and then try one more time.

### MQTT status shows `Disconnected`
- Verify your broker's address, port, username, and password are correct.
- Make sure your broker is reachable from the same Wi-Fi the Hub is on.
- Try **Settings -> Restart** to force a fresh connection attempt.

### Alexa can't find my devices
- Make sure your Echo and the Hub are on the **same Wi-Fi network** (and the same Wi-Fi band — some routers separate 2.4 GHz and 5 GHz).
- Say *"Alexa, discover devices"* after adding new ones on the Hub.

### Nothing is working — I just want to start over
Use **Settings -> Factory Reset**. This clears everything and brings you back to the first-time setup flow.

---

## Contact & Support

Need a hand? We're happy to help.

- **On the device:** Open **Main Menu -> Contact** and scan the QR code with your phone — it opens an email to support directly.
- **Help docs anywhere:** Scan the **Help** QR (Main Menu -> Help) to open this guide on your phone.
- **Email:** [madhusmiles.madhukar@gmail.com](mailto:madhusmiles.madhukar@gmail.com)

---

<div align="center">
  Made with care by Madhukar Temba — enjoy your IR Hub!
</div>
