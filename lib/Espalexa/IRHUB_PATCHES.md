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
  also switched to chunked streaming so 10 registered devices (~5 KB) no
  longer risk a WiFi stack OOM mid-response.
- `setFriendlyName(String)` — caller-overridable bridge name used in both
  `description.xml` and the `config.name` field. `AlexaConnector` sets it to
  `IR Hub <last6mac>` so 5 IR Hubs on the same LAN are distinguishable.
- SSDP `hue-bridgeid` header and `/api/<user>/config.bridgeid` now use the
  canonical 16-char `<MAC1-3>FFFE<MAC4-6>` format Alexa expects.

## Upgrading from upstream

Re-clone the desired tag into `lib/Espalexa/`, then reapply this diff (search
for `// IRHUB:` markers in `src/Espalexa.h` and `src/EspalexaDevice.{h,cpp}`).
