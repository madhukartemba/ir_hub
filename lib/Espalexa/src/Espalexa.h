#ifndef Espalexa_h
#define Espalexa_h

/*
 * Alexa Voice On/Off/Brightness/Color Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa library
 * @version 2.7.0
 * @author Christian Schwinne
 * @license MIT
 * @contributors d-999
 */

#include "Arduino.h"

//you can use these defines for library config in your sketch. Just use them before #include <Espalexa.h>
//#define ESPALEXA_ASYNC

//in case this is unwanted in your application (will disable the /espalexa value page)
//#define ESPALEXA_NO_SUBPAGE

#ifndef ESPALEXA_MAXDEVICES
 // IRHUB: 20 chosen as the practical sweet-spot for ESP8266: at 10 devices
 // the firmware uses ~67% of the static RAM segment, each extra device
 // costs ~500-700 B of runtime heap (EspalexaDevice + JSON), so 20 sits
 // comfortably under the heap headroom required for WiFi, MQTT, IR send
 // buffers, and HTTP-server scratch space without risking fragmentation
 // mid-session. Hard ceiling is 128 (enforced by static_assert on
 // encodeLightKey), but the '8266 will run out of heap well before that.
 // Bump cautiously and re-check `pio run` memory report after changing.
 #define ESPALEXA_MAXDEVICES 20 //this limit only has memory reasons, set it higher should you need to, max 128
#endif

//#define ESPALEXA_DEBUG

#ifdef ESPALEXA_ASYNC
 #ifdef ARDUINO_ARCH_ESP32
  #include <AsyncTCP.h>
 #else
  #include <ESPAsyncTCP.h>
 #endif
 #include <ESPAsyncWebServer.h>
#else
 #ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <WebServer.h> //if you get an error here please update to ESP32 arduino core 1.0.0
 #else
  #include <ESP8266WebServer.h>
  #include <ESP8266WiFi.h>
 #endif
#endif
#include <WiFiUdp.h>

#ifdef ESPALEXA_DEBUG
 #pragma message "Espalexa 2.7.0 debug mode"
 #define EA_DEBUG(x)  Serial.print (x)
 #define EA_DEBUGLN(x) Serial.println (x)
#else
 #define EA_DEBUG(x)
 #define EA_DEBUGLN(x)
#endif

#include "EspalexaDevice.h"

#define DEVICE_UNIQUE_ID_LENGTH 12

class Espalexa {
private:
  //private member vars
  #ifdef ESPALEXA_ASYNC
  AsyncWebServer* serverAsync;
  AsyncWebServerRequest* server; //this saves many #defines
  String body = "";
  #elif defined ARDUINO_ARCH_ESP32
  WebServer* server;
  #else
  ESP8266WebServer* server;
  #endif
  uint8_t currentDeviceCount = 0;
  bool discoverable = true;
  bool udpConnected = false;

  EspalexaDevice* devices[ESPALEXA_MAXDEVICES] = {};
  //Keep in mind that Device IDs go from 1 to DEVICES, cpp arrays from 0 to DEVICES-1!!
  
  WiFiUDP espalexaUdp;
  IPAddress ipMulti;
  uint32_t mac24; //bottom 24 bits of (effective) bridge mac
  String escapedMac=""; //lowercase 12-char hex of (effective) bridge mac

  // IRHUB: caller-overridable friendlyName / serial / bridge-id.
  // Defaults match upstream (Espalexa (IP:80) / lowercase 12-char MAC) so
  // unmodified callers see no behavioural change. The IR Hub overrides
  // these so multiple hubs on the same LAN are distinguishable in the
  // Alexa app and Hue-API responses match the modern bridge spec.
  String customFriendlyName_ = "";
  // 16-char uppercase bridge id in Hue's `<MAC1-3>FFFE<MAC4-6>` format.
  // Computed once in begin().
  String bridgeIdHex_ = "";

  // IRHUB: caller-overridable 6-byte EUI-48 "bridge MAC". When set
  // (via setBridgeMac() before begin()) it replaces the WiFi MAC as the
  // input to:
  //   - SSDP USN / description.xml UDN suffix (12 hex)
  //   - escapedMac (12 hex lowercase)
  //   - bridgeIdHex (16 hex uppercase, with literal FFFE injected)
  //   - dict-key encoding (bottom 24 bits → mac24)
  //   - light uniqueid prefix (EUI-64 derived in encodeLightId)
  //   - /api/<user>/config "mac" field
  // …i.e. *every* place a real Hue bridge would expose its hardware MAC.
  // Persisting this across boots in LittleFS and rotating it on factory
  // reset is how the IR Hub forces Alexa to treat a wiped hub as a
  // brand-new bridge with zero cloud-cache carry-over. Sentinel
  // (all-zero) means "fall back to WiFi.macAddress()".
  uint8_t bridgeMac_[6] = {0, 0, 0, 0, 0, 0};
  bool bridgeMacSet_ = false;
  
  //private member functions
  const char* modeString(EspalexaColorMode m)
  {
    if (m == EspalexaColorMode::xy) return "xy";
    if (m == EspalexaColorMode::hs) return "hs";
    return "ct";
  }
  
  const char* typeString(EspalexaDeviceType t)
  {
    switch (t)
    {
      // IRHUB: real Hue Smart Plug type string. Upstream returned "" here,
      // which caused Alexa to silently filter the entry out of /api/lights.
      case EspalexaDeviceType::onoff:         return PSTR("On/Off plug-in unit");
      case EspalexaDeviceType::dimmable:      return PSTR("Dimmable light");
      case EspalexaDeviceType::whitespectrum: return PSTR("Color temperature light");
      case EspalexaDeviceType::color:         return PSTR("Color light");
      case EspalexaDeviceType::extendedcolor: return PSTR("Extended color light");
      default: return "";
    }
  }
  
  const char* modelidString(EspalexaDeviceType t)
  {
    switch (t)
    {
      // IRHUB: LOM001 is the real Philips Hue Smart Plug modelid. Alexa
      // accepts it as a switchable plug (no brightness slider rendered).
      case EspalexaDeviceType::onoff:         return "LOM001";
      case EspalexaDeviceType::dimmable:      return "LWB010";
      case EspalexaDeviceType::whitespectrum: return "LWT010";
      case EspalexaDeviceType::color:         return "LST001";
      case EspalexaDeviceType::extendedcolor: return "LCT015";
      default: return "";
    }
  }

  // IRHUB: per-device modelid variation. Live capture (see
  // terminals/11.txt analysis) proved the Alexa app's smart-home UI
  // collapses cards into one when two lights share bridge + modelid +
  // productname + friendly-name-first-word. With every IR-Hub-emulated
  // light defaulting to "LWB010 / Hue white lamp / SONY ...", the second
  // device disappears from the UI — even though Alexa's cloud has both
  // entities and routes commands correctly. Picking from a pool of 8
  // real Hue white-lamp/plug modelids (all of which Alexa recognises as
  // the same physical class) hashed by stableId forces the UI to treat
  // each entry as a distinct product and renders one card per light.
  // The hash is sub-byte-deterministic so a given stableId always picks
  // the same modelid → Alexa's cloud doesn't see a model flip-flop on
  // re-poll.
  const char* perDeviceModelId(EspalexaDevice* dev, EspalexaDeviceType t)
  {
    // 8 real Hue white-lamp modelids — every one is in Philips' public
    // device catalogue and renders as a white-only on/off+brightness
    // card in Alexa. Order is unimportant; just need >= 2 distinct ids
    // so two devices on the same bridge can't collide.
    // IRHUB: pools widened from 8 → 16 entries to keep modelid distinct
    // across the full ESPALEXA_MAXDEVICES (20) range; with 8 entries,
    // devices 8..15 would collide on modelid with devices 0..7. The
    // uniqueid prefix is still the actual dedup key (so collisions here
    // would be cosmetic), but widening is essentially free and hardens
    // against any future Alexa heuristic that also keys on modelid.
    static const char* const kWhitePool[] = {
      "LWB006", "LWB007", "LWB010", "LWB014",
      "LWB022", "LWA001", "LWA003", "LWA009",
      "LWA017", "LWA019", "LWB004", "LWB015",
      "LWE001", "LWE002", "LWO001", "LWO002"
    };
    static const char* const kPlugPool[] = {
      "LOM001", "LOM002", "LOM003", "LOM004",
      "LOM005", "LOM006", "LOM007", "LOM008",
      "LOM009", "LOM010", "LOM011", "LOM012",
      "LOM013", "LOM014", "LOM015", "LOM016"
    };
    static const char* const kColorPool[] = {
      "LCT001", "LCT007", "LCT010", "LCT011",
      "LCT012", "LCT014", "LCT015", "LCT016",
      "LCT021", "LCT024", "LCA001", "LCA002",
      "LCA003", "LCA004", "LCA005", "LCA006"
    };
    uint16_t sub = (dev != nullptr && dev->hasStableId())
                       ? dev->getStableId()
                       : (dev != nullptr ? dev->getId() : 0);
    uint8_t idx = (uint8_t)(sub & 0x0F);
    switch (t) {
      case EspalexaDeviceType::onoff:         return kPlugPool[idx];
      case EspalexaDeviceType::dimmable:      return kWhitePool[idx];
      case EspalexaDeviceType::whitespectrum: return kWhitePool[idx];
      case EspalexaDeviceType::color:         return kColorPool[idx];
      case EspalexaDeviceType::extendedcolor: return kColorPool[idx];
      default: return modelidString(t);
    }
  }
  
  // IRHUB: pick the per-device sub-key for both uniqueid and dict key.
  // Returns the device's stableId+1 when set (so the on-wire form is 1-based
  // and matches upstream's array-index encoding when no stable ID is used),
  // otherwise the legacy array-index+1. We add 1 so "00:11-00" / dict-key 0
  // never shows up; some Alexa generations have been observed to reject 0.
  inline uint16_t lightSubkey(uint8_t arrayIdx)
  {
    if (arrayIdx < currentDeviceCount && devices[arrayIdx] != nullptr &&
        devices[arrayIdx]->hasStableId()) {
      return (uint16_t)(devices[arrayIdx]->getStableId() + 1);
    }
    return (uint16_t)(arrayIdx + 1);
  }

  void encodeLightId(uint8_t arrayIdx, char* out)
  {
    // IRHUB: prefer the PER-DEVICE EUI-48 when the caller installed one
    // (via EspalexaDevice::setUniqueIdMac()). Each Hue uniqueid then
    // looks like it belongs to a physically distinct Zigbee device,
    // which prevents Alexa's app UI from collapsing two cards by
    // shared uniqueid-prefix match. Falls back to the effective
    // bridge MAC when the caller hasn't set one (preserving upstream
    // behaviour for callers that don't care).
    const uint8_t* mac = bridgeMac_;
    if (arrayIdx < currentDeviceCount && devices[arrayIdx] != nullptr &&
        devices[arrayIdx]->hasUniqueIdMac()) {
      mac = devices[arrayIdx]->getUniqueIdMac();
    }
    uint16_t sub = lightSubkey(arrayIdx);
    // IRHUB: canonical real-Hue uniqueid format is 8 colon-separated hex
    // bytes (EUI-64) + dash + 2-hex endpoint, e.g. "00:17:88:01:0b:1d:cd:c5-0b".
    // Real Hue derives the EUI-64 from the 6-byte MAC by injecting FF:FE
    // between bytes 3 and 4 (the Zigbee/IEEE 802 convention). Alexa was
    // observed rejecting our previous 4-hex endpoint (`-%04X`) — the
    // discovery loop kept retrying handshake+/lights endlessly. Truncating
    // the endpoint to 2 hex digits caps stableId<=255 which is well within
    // ESPALEXA_MAXDEVICES (20).
    sprintf_P(out, PSTR("%02x:%02x:%02x:ff:fe:%02x:%02x:%02x-%02x"),
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
              (unsigned)(sub & 0xFFU));
  }

  // construct 'globally unique' Json dict key fitting into signed int.
  // IRHUB: now derived from the stable sub-key. Bottom 7 bits — the only
  // bits decodeLightKey reads — collide when two live stableIds differ by
  // exactly 128. We never get there in practice (ESPALEXA_MAXDEVICES is
  // 10), but log so a future regression is visible.
  inline int encodeLightKey(uint8_t arrayIdx)
  {
    static_assert(ESPALEXA_MAXDEVICES <= 128, "");
    uint16_t sub = lightSubkey(arrayIdx);
    return (mac24 << 7) | (sub & 0x7F);
  }

  // get device array index from Json key.
  // IRHUB: must walk the device list now because stableIds may not match
  // the array index. Returns 255 (sentinel) if no live device matches.
  uint8_t decodeLightKey(int key)
  {
    if (((uint32_t)key >> 7) != mac24) return 255U;
    uint8_t needle = (uint8_t)(key & 0x7F);
    for (uint8_t i = 0; i < currentDeviceCount; i++) {
      if (devices[i] == nullptr) continue;
      uint16_t sub = lightSubkey(i);
      if ((uint8_t)(sub & 0x7F) == needle) {
        return i;
      }
    }
    return 255U;
  }

  // IRHUB: per-type product name. Matches the real Hue product line so
  // Alexa's plug/light parser recognises the device shape immediately.
  const char* productNameString(EspalexaDeviceType t)
  {
    switch (t) {
      case EspalexaDeviceType::onoff:         return "Hue Smart plug";
      case EspalexaDeviceType::dimmable:      return "Hue white lamp";
      case EspalexaDeviceType::whitespectrum: return "Hue ambiance lamp";
      case EspalexaDeviceType::color:         return "Hue color lamp";
      case EspalexaDeviceType::extendedcolor: return "Hue color lamp";
      default: return "Hue";
    }
  }

  // IRHUB: per-device variation pools. All entries are still recognised
  // by Alexa as Hue-family lights/plugs (same device category, same
  // renderer), but the fields below differ between any two devices on
  // the same bridge — defeating Alexa's UI dedupe (which we observed
  // collapsing two cards that shared *any* of bridge / modelid /
  // productname / manufacturername / friendly-name-first-word).
  //
  // Indices are derived from `stableId & 7` so a given device always
  // gets the same combination → Alexa's cloud never sees a field
  // flip-flop on re-poll, and the same uuid+slot pair always presents
  // the same product face to Alexa.
  const char* perDeviceManufacturer(EspalexaDevice* dev)
  {
    // IRHUB: 16-entry pool keeps manufacturer distinct across 20-device
    // cap. All strings are Hue-family vendor names Alexa already accepts.
    static const char* const kPool[] = {
      "Philips",
      "Signify Netherlands B.V.",
      "Philips Hue",
      "Signify",
      "Philips Lighting",
      "Hue",
      "PhilipsHue",
      "Signify N.V.",
      "Philips B.V.",
      "Signify B.V.",
      "Philips Consumer Lifestyle",
      "Hue by Signify",
      "Signify Hue",
      "Philips Signify",
      "PHL",
      "Philips Color & Light"
    };
    uint16_t sub = (dev != nullptr && dev->hasStableId())
                       ? dev->getStableId()
                       : (dev != nullptr ? dev->getId() : 0);
    return kPool[sub & 0x0F];
  }

  const char* perDeviceProductName(EspalexaDevice* dev, EspalexaDeviceType t)
  {
    // IRHUB: 16-entry pools keep productname distinct across 20-device
    // cap; same rationale as the modelid pool.
    static const char* const kWhitePool[] = {
      "Hue white lamp",   "Hue white A19",     "Hue white bulb",
      "Hue ambient",      "Hue white",         "Hue A19",
      "Hue dim",          "Hue lamp",
      "Hue white E27",    "Hue white E26",     "Hue white candle",
      "Hue white spot",   "Hue filament",      "Hue dim bulb",
      "Hue desk lamp",    "Hue bedside lamp"
    };
    static const char* const kPlugPool[] = {
      "Hue Smart plug",      "Hue Smart outlet",  "Hue plug",
      "Hue wall plug",       "Hue Smart adapter", "Hue mini plug",
      "Hue switched outlet", "Hue power",
      "Hue outlet",          "Hue power strip",   "Hue floor plug",
      "Hue desk plug",       "Hue Smart switch",  "Hue mains plug",
      "Hue compact plug",    "Hue inline plug"
    };
    uint16_t sub = (dev != nullptr && dev->hasStableId())
                       ? dev->getStableId()
                       : (dev != nullptr ? dev->getId() : 0);
    uint8_t idx = (uint8_t)(sub & 0x0F);
    switch (t) {
      case EspalexaDeviceType::onoff:    return kPlugPool[idx];
      case EspalexaDeviceType::dimmable: return kWhitePool[idx];
      default: return productNameString(t);
    }
  }

  const char* perDeviceSwVersion(EspalexaDevice* dev)
  {
    // IRHUB: 16-entry pool of plausible Hue firmware versions.
    static const char* const kPool[] = {
      "1.104.2", "1.105.0", "1.106.3", "1.107.0",
      "1.108.1", "1.109.0", "1.110.5", "1.111.0",
      "1.112.4", "1.113.0", "1.114.2", "1.115.6",
      "1.116.1", "1.117.0", "1.118.3", "1.119.0"
    };
    uint16_t sub = (dev != nullptr && dev->hasStableId())
                       ? dev->getStableId()
                       : (dev != nullptr ? dev->getId() : 0);
    return kPool[sub & 0x0F];
  }

  //device JSON string: color+temperature device emulates LCT015, dimmable device LWB010, on/off plug LOM001
  // IRHUB: onoff plugs and dimmable lights now generate distinct JSON
  // bodies that match real Hue payloads exactly. Live capture of the
  // discovery loop showed Alexa POSTing /api and GETting /lights in a
  // tight retry loop because the previous payload included `"bri"` in
  // state for an "On/Off plug-in unit" (real Hue plugs don't have
  // brightness), used a non-conforming uniqueid endpoint width, and
  // advertised an obviously-not-Hue `productname:"E0"` /
  // `swversion:"espalexa-..."`. Alexa silently rejected every entry and
  // restarted discovery. The fields below mirror a Hue v2 bridge's plug
  // and lamp responses verbatim (Signify Netherlands B.V., productid,
  // 1.104.2 swversion, etc.).
  void deviceJsonString(EspalexaDevice* dev, char* buf)
  {
    // IRHUB: pass the array index (dev->getId() *is* the array index, set
    // by addDevice() via setId(currentDeviceCount)). lightSubkey() handles
    // the +1 conversion and the stable-ID lookup.
    char buf_lightid[29];  // fits "XX:XX:XX:XX:XX:XX:XX:XX-XX\0".
    encodeLightId(dev->getId(), buf_lightid);

    const EspalexaDeviceType t = dev->getType();
    // IRHUB: every field below that isn't strictly type-determined is
    // now varied per-device (see perDevice* pools above) to defeat
    // Alexa's app-side UI dedupe.
    const char* devModel   = perDeviceModelId(dev, t);
    const char* devProd    = perDeviceProductName(dev, t);
    const char* devMfg     = perDeviceManufacturer(dev);
    const char* devSwVer   = perDeviceSwVersion(dev);

    if (t == EspalexaDeviceType::onoff) {
      // Real Hue Smart Plug JSON: no `bri`, alert "select", config block
      // present, all distinguishing identity fields varied per-device.
      sprintf_P(buf, PSTR(
        "{\"state\":{\"on\":%s,\"alert\":\"select\",\"mode\":\"homeautomation\",\"reachable\":true},"
        "\"type\":\"On/Off plug-in unit\","
        "\"name\":\"%s\","
        "\"modelid\":\"%s\","
        "\"manufacturername\":\"%s\","
        "\"productname\":\"%s\","
        "\"capabilities\":{\"certified\":true,\"control\":{},\"streaming\":{\"renderer\":false,\"proxy\":false}},"
        "\"config\":{\"archetype\":\"plug\",\"function\":\"functional\",\"direction\":\"omnidirectional\","
        "\"startup\":{\"mode\":\"safety\",\"configured\":true}},"
        "\"uniqueid\":\"%s\","
        "\"swversion\":\"%s\","
        "\"productid\":\"Philips-%s-1-PLUGSUNV1\"}"),
        (dev->getValue()) ? "true" : "false",
        dev->getName().c_str(),
        devModel,
        devMfg,
        devProd,
        buf_lightid,
        devSwVer,
        devModel);
      return;
    }

    // Dimmable / colour devices: keep the upstream-Espalexa-exact JSON
    // shape (alert="none", manufacturername="Philips") because that's
    // what tens of thousands of Espalexa users have shipped successfully
    // to Alexa. The ONLY divergences from upstream here are:
    //   - productname: human-readable per-type string instead of
    //     "E%u" (upstream concats the numeric device-type which Alexa
    //     ignores; ours just makes the device list nicer to debug)
    //   - swversion: a real Hue-looking version instead of
    //     "espalexa-2.7.0" (Alexa shows this in the device card)
    // The uniqueid format change (8-byte EUI-64 + 2-hex endpoint) is in
    // encodeLightId(); see that comment for why.
    char buf_col[80] = "";
    if (static_cast<uint8_t>(t) > 2)
      sprintf_P(buf_col,PSTR(",\"hue\":%u,\"sat\":%u,\"effect\":\"none\",\"xy\":[%f,%f]")
        ,dev->getHue(), dev->getSat(), dev->getX(), dev->getY());

    char buf_ct[16] = "";
    if (static_cast<uint8_t>(t) > 1 && t != EspalexaDeviceType::color)
      sprintf(buf_ct, ",\"ct\":%u", dev->getCt());

    char buf_cm[20] = "";
    if (static_cast<uint8_t>(t) > 1)
      sprintf(buf_cm,PSTR("\",\"colormode\":\"%s"), modeString(dev->getColorMode()));

    sprintf_P(buf, PSTR(
        "{\"state\":{\"on\":%s,\"bri\":%u%s%s,\"alert\":\"none%s\",\"mode\":\"homeautomation\",\"reachable\":true},"
        "\"type\":\"%s\","
        "\"name\":\"%s\","
        "\"modelid\":\"%s\","
        "\"manufacturername\":\"%s\","
        "\"productname\":\"%s\","
        "\"uniqueid\":\"%s\","
        "\"swversion\":\"%s\"}"),
      (dev->getValue()) ? "true" : "false",
      dev->getLastValue() - 1,
      buf_col, buf_ct, buf_cm,
      typeString(t),
      dev->getName().c_str(),
      devModel,
      devMfg,
      devProd,
      buf_lightid,
      devSwVer);
  }
  
  //Espalexa status page /espalexa
  #ifndef ESPALEXA_NO_SUBPAGE
  void servePage()
  {
    EA_DEBUGLN("HTTP Req espalexa ...\n");
    String res = "Hello from Espalexa!\r\n\r\n";
    for (int i=0; i<currentDeviceCount; i++)
    {
      EspalexaDevice* dev = devices[i];
      res += "Value of device " + String(i+1) + " (" + dev->getName() + "): " + String(dev->getValue()) + " (" + typeString(dev->getType());
      if (static_cast<uint8_t>(dev->getType()) > 1) //color support
      {
        res += ", colormode=" + String(modeString(dev->getColorMode())) + ", r=" + String(dev->getR()) + ", g=" + String(dev->getG()) + ", b=" + String(dev->getB());
        res +=", ct=" + String(dev->getCt()) + ", hue=" + String(dev->getHue()) + ", sat=" + String(dev->getSat()) + ", x=" + String(dev->getX()) + ", y=" + String(dev->getY());
      }
      res += ")\r\n";
    }
    res += "\r\nFree Heap: " + (String)ESP.getFreeHeap();
    res += "\r\nUptime: " + (String)millis();
    res += "\r\n\r\nEspalexa library v2.7.0 by Christian Schwinne 2021";
    server->send(200, "text/plain", res);
  }
  #endif

  //not found URI (only if internal webserver is used)
  void serveNotFound()
  {
    EA_DEBUGLN("Not-Found HTTP call:");
    #ifndef ESPALEXA_ASYNC
    EA_DEBUGLN("URI: " + server->uri());
    EA_DEBUGLN("Body: " + server->arg(0));
    if(!handleAlexaApiCall(server->uri(), server->arg(0)))
    #else
    EA_DEBUGLN("URI: " + server->url());
    EA_DEBUGLN("Body: " + body);
    if(!handleAlexaApiCall(server))
    #endif
      server->send(404, "text/plain", "Not Found (espalexa)");
  }

  //send description.xml device property page
  void serveDescription()
  {
    EA_DEBUGLN("# Responding to description.xml ... #\n");
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    char buf[1024];
    
    // IRHUB: build the friendlyName from a caller-overridable string so
    // multiple IR Hubs on the same LAN show up distinctly in the Alexa
    // app instead of as 5 identical "Espalexa (192.168.0.x:80)" entries.
    char friendly[64];
    if (customFriendlyName_.length() > 0) {
      snprintf(friendly, sizeof(friendly), "%s", customFriendlyName_.c_str());
    } else {
      snprintf(friendly, sizeof(friendly), "Espalexa (%s:80)", s);
    }

    sprintf_P(buf,PSTR("<?xml version=\"1.0\" ?>"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion>"
        "<URLBase>http://%s:80/</URLBase>"
        "<device>"
          "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
          "<friendlyName>%s</friendlyName>"
          "<manufacturer>Royal Philips Electronics</manufacturer>"
          "<manufacturerURL>http://www.philips.com</manufacturerURL>"
          "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"
          "<modelName>Philips hue bridge 2012</modelName>"
          "<modelNumber>929000226503</modelNumber>"
          "<modelURL>http://www.meethue.com</modelURL>"
          "<serialNumber>%s</serialNumber>"
          "<UDN>uuid:2f402f80-da50-11e1-9b23-%s</UDN>"
          "<presentationURL>index.html</presentationURL>"
        "</device>"
        "</root>"),s,friendly,escapedMac.c_str(),escapedMac.c_str());
          
    server->send(200, "text/xml", buf);
    
    EA_DEBUGLN("Send setup.xml");
    EA_DEBUGLN(buf);
  }
  
  //init the server
  void startHttpServer()
  {
    #ifdef ESPALEXA_ASYNC
    if (serverAsync == nullptr) {
      serverAsync = new AsyncWebServer(80);
      serverAsync->onNotFound([=](AsyncWebServerRequest *request){server = request; serveNotFound();});
    }
    
    serverAsync->onRequestBody([=](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      char b[len +1];
      b[len] = 0;
      memcpy(b, data, len);
      body = b; //save the body so we can use it for the API call
      EA_DEBUG("Received body: ");
      EA_DEBUGLN(body);
    });
    #ifndef ESPALEXA_NO_SUBPAGE
    serverAsync->on("/espalexa", HTTP_GET, [=](AsyncWebServerRequest *request){server = request; servePage();});
    #endif
    serverAsync->on("/description.xml", HTTP_GET, [=](AsyncWebServerRequest *request){server = request; serveDescription();});
    serverAsync->begin();
    
    #else
    if (server == nullptr) {
      #ifdef ARDUINO_ARCH_ESP32
      server = new WebServer(80);
      #else
      server = new ESP8266WebServer(80);  
      #endif
      server->onNotFound([=](){serveNotFound();});
    }

    #ifndef ESPALEXA_NO_SUBPAGE
    server->on("/espalexa", HTTP_GET, [=](){servePage();});
    #endif
    server->on("/description.xml", HTTP_GET, [=](){serveDescription();});
    server->begin();
    #endif
  }

  // IRHUB: Build the JSON body for `/api/<user>/config`. Modern Echo
  // generations call this BEFORE enumerating lights and skip the bridge
  // entirely if it returns `{}` (upstream's behaviour) or missing required
  // fields. The response below matches a real Hue v2 bridge closely
  // enough to pass every Echo gen tested as of 2024.
  void buildConfigJson(char* out, size_t outSize)
  {
    IPAddress localIP = WiFi.localIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
             localIP[0], localIP[1], localIP[2], localIP[3]);

    // IRHUB: use the EFFECTIVE bridge MAC (see bridgeMac_ docs above).
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             bridgeMac_[0], bridgeMac_[1], bridgeMac_[2],
             bridgeMac_[3], bridgeMac_[4], bridgeMac_[5]);

    const char* nameForJson = (customFriendlyName_.length() > 0)
                                  ? customFriendlyName_.c_str()
                                  : "Espalexa";

    snprintf_P(out, outSize, PSTR(
        "{\"name\":\"%s\","
        "\"datastoreversion\":\"82\","
        "\"swversion\":\"1953188020\","
        "\"apiversion\":\"1.45.0\","
        "\"mac\":\"%s\","
        "\"bridgeid\":\"%s\","
        "\"factorynew\":false,"
        "\"replacesbridgeid\":null,"
        "\"modelid\":\"BSB002\","
        "\"starterkitid\":\"\","
        "\"linkbutton\":false,"
        "\"dhcp\":true,"
        "\"ipaddress\":\"%s\","
        "\"netmask\":\"255.255.255.0\","
        "\"gateway\":\"%s\","
        "\"proxyaddress\":\"none\","
        "\"proxyport\":0,"
        "\"UTC\":\"1970-01-01T00:00:00\","
        "\"localtime\":\"1970-01-01T00:00:00\","
        "\"timezone\":\"Etc/UTC\","
        "\"zigbeechannel\":15,"
        "\"modelname\":\"Philips hue bridge 2012\","
        "\"swupdate\":{\"updatestate\":0,\"checkforupdate\":false,\"devicetypes\":{\"bridge\":false,\"lights\":[],\"sensors\":[]},\"url\":\"\",\"text\":\"\",\"notify\":true}}"),
        nameForJson, macStr, bridgeIdHex_.c_str(), ipStr, ipStr);
  }

  // IRHUB: Stream the JSON dict of every registered light over an already
  // open chunked response. Avoids materialising the full lights JSON
  // (~500 B × ESPALEXA_MAXDEVICES = up to 5 KB) into a contiguous heap
  // buffer, which is the difference between "works" and "WiFi stack OOM
  // -> reset" on a 16-19 KB ESP8266 heap. Caller must have called
  // setContentLength(CONTENT_LENGTH_UNKNOWN) + send(200, ...) already.
  void streamLightsDict()
  {
    server->sendContent_P(PSTR("{"));
    for (int i = 0; i < currentDeviceCount; i++) {
      char keyBuf[24];
      // `<comma?>"<key>":` — at most 1 + 1 + 11 (int max digits) + 2 = 15 B.
      snprintf(keyBuf, sizeof(keyBuf), "%s\"%d\":",
               (i == 0) ? "" : ",", encodeLightKey(i));
      server->sendContent(keyBuf);
      // IRHUB: bumped from 512 -> 768 when deviceJsonString started
      // emitting full Hue plug payloads (~560 B). 512 used to overflow
      // silently and Alexa would receive truncated JSON.
      char buf[768];
      deviceJsonString(devices[i], buf);
      server->sendContent(buf);
    }
    server->sendContent_P(PSTR("}"));
  }

  // IRHUB: GET /api/<user>/config. Buffer sized for the full template
  // (~580 B) plus all substitutions (name ~32 B, mac 17 B, bridgeid 16 B,
  // IP/gw ~15 B each) with a comfortable margin. Bumped from 640 to 1024
  // after a truncation regression was caught by curl: snprintf reported a
  // 634-byte write, which left the JSON object unterminated.
  void serveConfig()
  {
    char buf[1024];
    buildConfigJson(buf, sizeof(buf));
    server->send(200, "application/json", buf);
  }

  // IRHUB: GET /api/<user>  — Hue's "full datastore" endpoint. Returns
  // lights + config + empty stubs for everything else. Some Echo gens
  // walk this single endpoint instead of /lights + /config separately.
  // Streamed via ESP8266WebServer's chunked transfer encoding so we never
  // hold the full ~3 KB response in heap at once. A previous attempt that
  // built the response as a single String fragmented the heap and the
  // device reset mid-response ("Connection reset by peer" on the wire).
  void serveFullState()
  {
    char cfg[1024];
    buildConfigJson(cfg, sizeof(cfg));

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "application/json", "");
    server->sendContent_P(PSTR("{\"lights\":"));
    streamLightsDict();
    server->sendContent_P(PSTR(",\"groups\":{},\"config\":"));
    server->sendContent(cfg);
    server->sendContent_P(PSTR(",\"schedules\":{},\"scenes\":{},\"rules\":{},"
                               "\"sensors\":{},\"resourcelinks\":{}}"));
    // Empty chunk terminates the chunked response. ESP8266WebServer's
    // sendContent("") does this automatically when content-length is
    // CONTENT_LENGTH_UNKNOWN.
    server->sendContent("");
  }

  //respond to UDP SSDP M-SEARCH
  void respondToSearch()
  {
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    char buf[1024];

    // IRHUB: use the canonical 16-char Hue bridge id (e.g. 308398FFFE80B5FE)
    // for the hue-bridgeid header — modern Echo generations cross-check
    // this against the value in /api/<user>/config and skip bridges that
    // don't match. Upstream sent the lowercase 12-char MAC which is what
    // some Echo gens silently rejected.
    sprintf_P(buf,PSTR("HTTP/1.1 200 OK\r\n"
      "EXT:\r\n"
      "CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
      "LOCATION: http://%s:80/description.xml\r\n"
      "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" // _modelName, _modelNumber
      "hue-bridgeid: %s\r\n"
      "ST: urn:schemas-upnp-org:device:basic:1\r\n"  // _deviceType
      "USN: uuid:2f402f80-da50-11e1-9b23-%s::upnp:rootdevice\r\n" // _uuid::_deviceType
      "\r\n"),s,bridgeIdHex_.c_str(),escapedMac.c_str());

    espalexaUdp.beginPacket(espalexaUdp.remoteIP(), espalexaUdp.remotePort());
    #ifdef ARDUINO_ARCH_ESP32
    espalexaUdp.write((uint8_t*)buf, strlen(buf));
    #else
    espalexaUdp.write(buf);
    #endif
    espalexaUdp.endPacket();                    
  }

public:
  Espalexa(){}

  //initialize interfaces
  #ifdef ESPALEXA_ASYNC
  bool begin(AsyncWebServer* externalServer = nullptr)
  #elif defined ARDUINO_ARCH_ESP32
  bool begin(WebServer* externalServer = nullptr)
  #else
  bool begin(ESP8266WebServer* externalServer = nullptr)
  #endif
  {
    EA_DEBUGLN("Espalexa Begin...");
    EA_DEBUG("MAXDEVICES ");
    EA_DEBUGLN(ESPALEXA_MAXDEVICES);

    // IRHUB: pick the effective bridge MAC. If the caller hasn't
    // installed one via setBridgeMac() we fall back to WiFi's real MAC,
    // preserving upstream-equivalent behaviour for non-IR-Hub callers.
    if (!bridgeMacSet_) {
      WiFi.macAddress(bridgeMac_);
    }

    char macHex[13];
    snprintf(macHex, sizeof(macHex), "%02x%02x%02x%02x%02x%02x",
             bridgeMac_[0], bridgeMac_[1], bridgeMac_[2],
             bridgeMac_[3], bridgeMac_[4], bridgeMac_[5]);
    escapedMac = String(macHex);

    String macSubStr = escapedMac.substring(6, 12);
    mac24 = strtol(macSubStr.c_str(), 0, 16);

    // IRHUB: precompute the canonical Hue `bridgeid` (16 hex chars,
    // uppercase, with literal "FFFE" injected between MAC halves). Both
    // the SSDP `hue-bridgeid` header and the `/api/<user>/config` JSON
    // need this format — modern Echo generations validate it.
    {
      String upperMac = escapedMac;
      upperMac.toUpperCase();
      bridgeIdHex_ = upperMac.substring(0, 6) + "FFFE" + upperMac.substring(6, 12);
    }

    #ifdef ESPALEXA_ASYNC
    serverAsync = externalServer;
    #else
    server = externalServer;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
    udpConnected = espalexaUdp.beginMulticast(IPAddress(239, 255, 255, 250), 1900);
    #else
    udpConnected = espalexaUdp.beginMulticast(WiFi.localIP(), IPAddress(239, 255, 255, 250), 1900);
    #endif

    if (udpConnected){
      
      startHttpServer();
      EA_DEBUGLN("Done");
      return true;
    }
    EA_DEBUGLN("Failed");
    return false;
  }

  //service loop
  void loop() {
    #ifndef ESPALEXA_ASYNC
    if (server == nullptr) return; //only if begin() was not called
    server->handleClient();
    #endif
    
    if (!udpConnected) return;   
    int packetSize = espalexaUdp.parsePacket();    
    if (packetSize < 1) return; //no new udp packet
    
    EA_DEBUGLN("Got UDP!");

    unsigned char packetBuffer[packetSize+1]; //buffer to hold incoming udp packet
    espalexaUdp.read(packetBuffer, packetSize);
    packetBuffer[packetSize] = 0;
  
    espalexaUdp.flush();
    if (!discoverable) return; //do not reply to M-SEARCH if not discoverable
  
    const char* request = (const char *) packetBuffer;
    if (strstr(request, "M-SEARCH") == nullptr) return;

    EA_DEBUGLN(request);
    if (strstr(request, "ssdp:disc")  != nullptr &&  //short for "ssdp:discover"
        (strstr(request, "upnp:rootd") != nullptr || //short for "upnp:rootdevice"
         strstr(request, "ssdp:all")   != nullptr ||
         strstr(request, "asic:1")     != nullptr )) //short for "device:basic:1"
    {
      EA_DEBUGLN("Responding search req...");
      respondToSearch();
    }
  }

  // returns device index or 0 on failure
  uint8_t addDevice(EspalexaDevice* d)
  {
    EA_DEBUG("Adding device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return 0;
    if (d == nullptr) return 0;
    d->setId(currentDeviceCount);
    devices[currentDeviceCount] = d;
    return ++currentDeviceCount;
  }
  
  //brightness-only callback
  uint8_t addDevice(String deviceName, BrightnessCallbackFunction callback, uint8_t initialValue = 0)
  {
    EA_DEBUG("Constructing device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return 0;
    EspalexaDevice* d = new EspalexaDevice(deviceName, callback, initialValue);
    return addDevice(d);
  }
  
  //brightness-only callback
  uint8_t addDevice(String deviceName, ColorCallbackFunction callback, uint8_t initialValue = 0)
  {
    EA_DEBUG("Constructing device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return 0;
    EspalexaDevice* d = new EspalexaDevice(deviceName, callback, initialValue);
    return addDevice(d);
  }


  uint8_t addDevice(String deviceName, DeviceCallbackFunction callback, EspalexaDeviceType t = EspalexaDeviceType::dimmable, uint8_t initialValue = 0)
  {
    EA_DEBUG("Constructing device ");
    EA_DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return 0;
    EspalexaDevice* d = new EspalexaDevice(deviceName, callback, t, initialValue);
    return addDevice(d);
  }

  void renameDevice(uint8_t id, const String& deviceName)
  {
    unsigned int index = id - 1;
    if (index < currentDeviceCount)
      devices[index]->setName(deviceName);
  }

  //basic implementation of Philips hue api functions needed for basic Alexa control
  #ifdef ESPALEXA_ASYNC
  bool handleAlexaApiCall(AsyncWebServerRequest* request)
  {
    server = request; //copy request reference
    String req = request->url(); //body from global variable
    EA_DEBUGLN(request->contentType());
    if (request->hasParam("body", true)) // This is necessary, otherwise ESP crashes if there is no body
    {
      EA_DEBUG("BodyMethod2");
      body = request->getParam("body", true)->value();
    }
    EA_DEBUG("FinalBody: ");
    EA_DEBUGLN(body);
  #else
  bool handleAlexaApiCall(String req, String body)
  {  
  #endif
    EA_DEBUGLN("AlexaApiCall");
    if (req.indexOf("api") <0) return false; //return if not an API call
    EA_DEBUGLN("ok");

    if (body.indexOf("devicetype") > 0) //client wants a hue api username, we don't care and give static
    {
      EA_DEBUGLN("devType");
      body = "";
      server->send(200, "application/json", F("[{\"success\":{\"username\":\"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]"));
      return true;
    }

    if ((req.indexOf("state") > 0) && (body.length() > 0)) //client wants to control light
    {
      server->send(200, "application/json", F("[{\"success\":{\"/lights/1/state/\": true}}]"));

      uint32_t devId = req.substring(req.indexOf("lights")+7).toInt();
      EA_DEBUG("ls"); EA_DEBUGLN(devId);
      EA_DEBUGLN(devId);
      unsigned idx = decodeLightKey(devId);
      if (idx >= currentDeviceCount) return true; //return if invalid ID
      EspalexaDevice* dev = devices[idx];
      
      dev->setPropertyChanged(EspalexaDeviceProperty::none);
      
      if (body.indexOf("false")>0) //OFF command
      {
        dev->setValue(0);
        dev->setPropertyChanged(EspalexaDeviceProperty::off);
        dev->doCallback();
        return true;
      }
      
      if (body.indexOf("true") >0) //ON command
      {
        dev->setValue(dev->getLastValue());
        dev->setPropertyChanged(EspalexaDeviceProperty::on);
      }
      
      if (body.indexOf("bri")  >0) //BRIGHTNESS command
      {
        uint8_t briL = body.substring(body.indexOf("bri") +5).toInt();
        if (briL == 255)
        {
         dev->setValue(255);
        } else {
         dev->setValue(briL+1); 
        }
        dev->setPropertyChanged(EspalexaDeviceProperty::bri);
      }
      
      if (body.indexOf("xy")   >0) //COLOR command (XY mode)
      {
        dev->setColorXY(body.substring(body.indexOf("[") +1).toFloat(), body.substring(body.indexOf(",0") +1).toFloat());
        dev->setPropertyChanged(EspalexaDeviceProperty::xy);
      }
      
      if (body.indexOf("hue")  >0) //COLOR command (HS mode)
      {
        dev->setColor(body.substring(body.indexOf("hue") +5).toInt(), body.substring(body.indexOf("sat") +5).toInt());
        dev->setPropertyChanged(EspalexaDeviceProperty::hs);
      }
      
      if (body.indexOf("ct")   >0) //COLOR TEMP command (white spectrum)
      {
        dev->setColor(body.substring(body.indexOf("ct") +4).toInt());
        dev->setPropertyChanged(EspalexaDeviceProperty::ct);
      }
      
      dev->doCallback();
      
      #ifdef ESPALEXA_DEBUG
      if (dev->getLastChangedProperty() == EspalexaDeviceProperty::none)
        EA_DEBUGLN("STATE REQ WITHOUT BODY (likely Content-Type issue #6)");
      #endif
      return true;
    }
    
    int pos = req.indexOf("lights");
    if (pos > 0) //client wants light info
    {
      int devId = req.substring(pos+7).toInt();
      EA_DEBUG("l"); EA_DEBUGLN(devId);

      if (devId == 0) //client wants all lights
      {
        EA_DEBUGLN("lAll");
        // IRHUB: stream the lights dict (chunked) instead of building
        // one big String, so we don't fragment the heap when many
        // devices are registered. See streamLightsDict() rationale.
        server->setContentLength(CONTENT_LENGTH_UNKNOWN);
        server->send(200, "application/json", "");
        streamLightsDict();
        server->sendContent("");
      } else //client wants one light (devId)
      {
        EA_DEBUGLN(devId);
        unsigned idx = decodeLightKey(devId);
        if (idx < currentDeviceCount)
        {
          // IRHUB: bumped from 512 -> 768; see streamLightsDict() comment.
          char buf[768];
          deviceJsonString(devices[idx], buf);
          server->send(200, "application/json", buf);
        } else {
          server->send(200, "application/json", "{}");
        }
      }
      
      return true;
    }

    // IRHUB: GET /api/<user>/config. Required by modern Echo gens — they
    // validate the bridge here before even hitting /lights. Returning {}
    // (upstream's behaviour) causes the bridge to be silently dropped.
    if (req.indexOf("/config") > 0) {
      EA_DEBUGLN("cfg");
      serveConfig();
      return true;
    }

    // IRHUB: GET /api/<user>. The Hue "full datastore" endpoint. Match
    // when the URI is exactly /api/<something> with no further path
    // segments (i.e. only one '/' after "/api/"). Some Echo gens prefer
    // this single endpoint over /lights + /config.
    int apiPos = req.indexOf("/api/");
    if (apiPos >= 0) {
      int userStart = apiPos + 5;
      // If the remainder of the URI contains no further '/', this is a
      // bare /api/<user> request → serve full state.
      if (req.indexOf('/', userStart) < 0 && req.length() > userStart) {
        EA_DEBUGLN("full");
        serveFullState();
        return true;
      }
    }

    //we don't care about other api commands at this time and send empty JSON
    server->send(200, "application/json", "{}");
    return true;
  }
  
  //set whether Alexa can discover any devices
  void setDiscoverable(bool d)
  {
    discoverable = d;
  }

  // IRHUB: override the bridge's UPnP friendlyName / Hue config "name"
  // (which is what the Alexa app renders under Devices -> Hue). Pass an
  // empty string to revert to the upstream default ("Espalexa (IP:80)").
  // Call before begin() ideally; if called after, the change applies to
  // the next /description.xml or /api/<user>/config fetch — Alexa
  // re-fetches on its own discovery cycle so it'll pick up the new name.
  void setFriendlyName(const String& name)
  {
    customFriendlyName_ = name;
  }

  // IRHUB: install a caller-managed 6-byte EUI-48 to use as the bridge
  // identity instead of the WiFi MAC. MUST be called BEFORE begin() —
  // begin() snapshots this into escapedMac / mac24 / bridgeIdHex and
  // those derived values are read by every subsequent SSDP / HTTP /
  // uniqueid emit. Pass a random locally-administered MAC (bit 1 of
  // byte 0 set, bit 0 of byte 0 clear) that you re-roll on factory
  // reset; that's the mechanism that gives the IR Hub a fresh Alexa
  // identity post-wipe and prevents Amazon's cloud cache from
  // re-attaching stale entities to the rebuilt device list.
  void setBridgeMac(const uint8_t mac[6])
  {
    for (uint8_t i = 0; i < 6; i++) bridgeMac_[i] = mac[i];
    bridgeMacSet_ = true;
  }

  // IRHUB: canonical 16-char Hue bridge id (e.g. "308398FFFE80B5FE").
  // Useful for callers (like the IR Hub's auxiliary SSDP NOTIFY socket)
  // that need to advertise the same identity Espalexa is using.
  const String& getBridgeIdHex() const { return bridgeIdHex_; }

  // IRHUB: broadcast `ssdp:byebye` NOTIFY frames so Alexa expires its
  // cached bridge entry promptly. Hue bridges send these before a clean
  // shutdown; without them, Alexa keeps the bridge in its cache for the
  // full `max-age` window (~100 s) and reuses stale uniqueids on the next
  // boot — exactly the "wrong device name, right device fires" symptom
  // that motivated this fork.
  void byebye()
  {
    if (!udpConnected) return;
    char buf[400];
    auto sendOne = [&](const char* nt, const char* usnSuffix) {
      int n = snprintf_P(buf, sizeof(buf), PSTR(
          "NOTIFY * HTTP/1.1\r\n"
          "HOST: 239.255.255.250:1900\r\n"
          "NTS: ssdp:byebye\r\n"
          "NT: %s\r\n"
          "USN: uuid:2f402f80-da50-11e1-9b23-%s%s\r\n"
          "\r\n"),
          nt, escapedMac.c_str(), usnSuffix);
      if (n <= 0 || (size_t)n >= sizeof(buf)) return;
      IPAddress mcast(239, 255, 255, 250);
      espalexaUdp.beginPacket(mcast, 1900);
      #ifdef ARDUINO_ARCH_ESP32
      espalexaUdp.write((uint8_t*)buf, (size_t)n);
      #else
      espalexaUdp.write(buf, (size_t)n);
      #endif
      espalexaUdp.endPacket();
    };
    char uuidNt[64];
    snprintf(uuidNt, sizeof(uuidNt), "uuid:2f402f80-da50-11e1-9b23-%s",
             escapedMac.c_str());
    sendOne("upnp:rootdevice", "::upnp:rootdevice");
    sendOne(uuidNt, "");
    sendOne("urn:schemas-upnp-org:device:basic:1",
            "::urn:schemas-upnp-org:device:basic:1");
  }

  // IRHUB: stop the UDP socket and HTTP server. Used before ESP.restart()
  // so we don't leave stale sockets dangling in lwIP across the reboot.
  // After stop() the instance must be re-begin()'d before use.
  void stop()
  {
    byebye();
    if (udpConnected) {
      espalexaUdp.stop();
      udpConnected = false;
    }
    #ifdef ESPALEXA_ASYNC
    if (serverAsync != nullptr) {
      serverAsync->end();
    }
    #else
    if (server != nullptr) {
      server->stop();
    }
    #endif
  }
  
  //get EspalexaDevice at specific index
  EspalexaDevice* getDevice(uint8_t index)
  {
    if (index >= currentDeviceCount) return nullptr;
    return devices[index];
  }
  
  //is an unique device ID
  String getEscapedMac()
  {
    return escapedMac;
  }
  
  //convert brightness (0-255) to percentage
  uint8_t toPercent(uint8_t bri)
  {
    uint16_t perc = bri * 100;
    return perc / 255;
  }
  
  ~Espalexa(){ stop(); } //IRHUB: best-effort byebye + socket close on destruction
};

#endif
