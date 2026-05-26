# Espalexa (IR Hub fork)

Vendored from upstream **Aircoookie/Espalexa @ v2.7.0** so the IR Hub can ship
fixes that upstream has not adopted. PlatformIO picks this copy over any
registry entry because `lib/` takes priority over `lib_deps`.

## Why this fork exists

The stock library has several bugs that show up as "Alexa keeps forgetting /
mixing up my devices" or "Alexa won't discover the bridge at all":

1. **Empty `type`/`modelid` for `EspalexaDeviceType::onoff`** — Alexa silently
   drops Hue entries that don't carry a recognised modelid, so on/off devices
   never appear unless the user discovers them as `dimmable` (which then
   shows a useless brightness slider in the Alexa app).
2. **`uniqueid` derived from registration index** — `encodeLightId()` and
   `encodeLightKey()` hash the *array slot* (1, 2, 3…) instead of any caller-
   supplied identifier. The instant the IR Hub's device list reorders (add /
   remove / rename), every Alexa-cached uniqueid points at the wrong device.
   The Alexa cloud cache is keyed on uniqueid, so it ends up firing the right
   IR command for the wrong on-screen device label.
3. **`/api/<user>/config` and `/api/<user>` return `{}`** — modern Echo gens
   call `/config` BEFORE enumerating lights to validate the bridge identity.
   A bridge that answers with an empty object is silently dropped from the
   discovery results. Some Echo gens also walk `/api/<user>` (the "full
   datastore" endpoint) instead of `/lights` + `/config` individually.
4. **`hue-bridgeid` header was the bare 12-char MAC** — real Hue bridges use
   the 16-char `<MAC1-3>FFFE<MAC4-6>` format (e.g. `308398FFFE80B5FE`), and
   Alexa cross-checks the SSDP header against `bridgeid` in `/config`.
5. **All Espalexa bridges shared the friendlyName template** `"Espalexa (IP:80)"`,
   making it impossible to tell multiple IR Hubs apart in the Alexa app — and
   some Echo gens dedupe by friendlyName, causing 4 of 5 IR Hubs to vanish.
6. **Bridge identity was MAC-derived and therefore immortal across factory
   resets.** Wiping LittleFS gave us a fresh device list but the SSDP
   `USN`/`UDN`, `bridgeid`, and Hue `uniqueid` prefix all stayed the same
   because they were derived from the burned-in WiFi MAC. Amazon's smart-
   home cloud cache keys off those values as the bridge identity and was
   silently re-attaching stale entities (old "SONY 1" → new "SONY 2", etc.)
   to the rebuilt bridge. End-user symptom: "I removed all the devices and
   re-added them but Alexa is still firing the wrong IR command from the
   wrong-named card."

## Patches applied

- `addDevice(... uint16_t stableId)` — new overload that pins the Hue
  uniqueid/light-key to a caller-supplied 16-bit stable ID (the IR Hub passes
  `device.alexaSlot - 1` from DeviceManager, where `alexaSlot` is a 1..255
  byte allocated at creation time and persisted in the device JSON; it is
  stable for the lifetime of the device).
- `EspalexaDevice::setStableId()` / `getStableId()` — storage for the above.
- `encodeLightId()` / `encodeLightKey()` / `decodeLightKey()` — switched to
  use the stable ID when present, falling back to the legacy index-based
  scheme for callers that don't supply one.
- `EspalexaDeviceType::onoff` now advertises `modelid="LOM001"` + 
  `type="On/Off plug-in unit"` (the real Hue Smart Plug). Alexa renders it
  as an on/off plug card, no brightness slider, no fake dimming.
- New `Espalexa::byebye()` / called from `~Espalexa()` and a public `stop()`
  that multicasts SSDP `ssdp:byebye` NOTIFY frames so Alexa expires the cached
  bridge entry promptly when the IR Hub reboots.
- New `serveConfig()` / `serveFullState()` that answer `/api/<user>/config`
  and `/api/<user>` with a realistic Hue bridge config + datastore JSON
  (correct `bridgeid`, `mac`, `apiversion`, `swversion`, `modelid="BSB002"`,
  IP, etc.). Both stream via `setContentLength(CONTENT_LENGTH_UNKNOWN)` +
  `sendContent` so the ~3 KB response never has to live as a contiguous
  String on the ESP8266 heap. The existing `/api/<user>/lights` handler was
  also switched to chunked streaming so 20 registered devices (~10 KB) no
  longer risk a WiFi stack OOM mid-response.
- `setFriendlyName(String)` — caller-overridable bridge name used in both
  `description.xml` and the `config.name` field. `AlexaConnector` sets it to
  `IR Hub <last6mac>` so 5 IR Hubs on the same LAN are distinguishable.
- SSDP `hue-bridgeid` header and `/api/<user>/config.bridgeid` now use the
  canonical 16-char `<MAC1-3>FFFE<MAC4-6>` format Alexa expects.
- `setBridgeMac(uint8_t[6])` — install a caller-managed 6-byte EUI-48 that
  replaces the WiFi MAC as the input to every bridge-identity field
  (`escapedMac`, `mac24`, `bridgeIdHex`, SSDP `USN`, description.xml
  `<UDN>`, `/config.mac`, light `uniqueid` prefix). `AlexaConnector`
  persists a random locally-administered MAC in `/alexa_bridge_id.bin`
  and rotates it on factory reset (LittleFS.format() wipes the file →
  fresh ID on next boot) so Alexa sees a completely new bridge with no
  cloud-cache carry-over.
- `perDeviceModelId()` / `perDeviceManufacturer()` / `perDeviceProductName()` /
  `perDeviceSwVersion()` — hash the per-device stableId into 16-entry
  pools of plausible Hue variations (real modelids `LWB006..LWB022` /
  `LWA001..LWA019` for white lamps, multiple Signify/Philips manufacturer
  string variants, multiple human-product-name variants, multiple
  swversion variants) and emit a unique-per-device combination in
  `/lights`. Live capture with on-hub HTTP instrumentation proved the
  Alexa app collapses cards that share bridge + modelid + productname
  + manufacturer + uniqueid-prefix + friendly-name-first-word: with
  every Espalexa device defaulting to the same values for ALL of those,
  only one card rendered (the entities still existed in Alexa's cloud
  and routed commands correctly, but the user couldn't tap the hidden
  one). Each device now varies across every dedup axis. Pool size 16
  is sized to keep every field distinct across the full
  `ESPALEXA_MAXDEVICES` (20) range. Pool indices are sub-byte
  deterministic so Alexa's cloud never sees a field flip-flop on
  re-poll.
- `EspalexaDevice::setUniqueIdMac(uint8_t[6])` + `encodeLightId()` per-
  device override — install a 6-byte EUI-48 PER LIGHT (derived in
  AlexaConnector from the first 6 hex bytes of the IR-Hub device UUID)
  to use as the Hue `uniqueid` prefix instead of the bridge MAC. This
  closes the LAST shared-identity axis (`uniqueid` previously shared
  the first 8 bytes across every light on the bridge); after the
  patch each light's `uniqueid` looks like a physically distinct
  Zigbee endpoint, defeating any UI-side prefix-match dedupe. Falls
  back to the bridge MAC when no per-device MAC is installed, so
  upstream-equivalent callers see no behavioural change.

## Upgrading from upstream

Re-clone the desired tag into `lib/Espalexa/`, then reapply this diff (search
for `// IRHUB:` markers in `src/Espalexa.h` and `src/EspalexaDevice.{h,cpp}`).
