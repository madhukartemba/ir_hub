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
 // IRHUB: 20 is the ESP8266 sweet-spot. Each extra device costs ~500-700 B
 // of runtime heap (EspalexaDevice + JSON scratch); above ~30 the WiFi/MQTT
 // stacks start hitting fragmentation. Hard ceiling is 128, enforced by the
 // static_assert in encodeLightKey().
 #define ESPALEXA_MAXDEVICES 20
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

  // IRHUB: caller-overridable bridge identity. Defaults match upstream
  // ("Espalexa (IP:80)" / WiFi MAC) so unmodified callers see no change.
  // The IR Hub overrides these to (a) distinguish multiple hubs on one LAN
  // in the Alexa app, and (b) rotate the bridge MAC on factory reset so
  // Amazon's cloud cache treats the wiped hub as a brand-new bridge with
  // no stale-entity carry-over.
  String customFriendlyName_ = "";
  String bridgeIdHex_ = "";  // 16-char uppercase, "<MAC1-3>FFFE<MAC4-6>" format
  uint8_t bridgeMac_[6] = {0, 0, 0, 0, 0, 0};
  bool bridgeMacSet_ = false;
  
  //private member functions
  const char* modeString(EspalexaColorMode m)
  {
    if (m == EspalexaColorMode::xy) return "xy";
    if (m == EspalexaColorMode::hs) return "hs";
    return "ct";
  }
  
  // IRHUB: real Hue type strings. Upstream returned "" for `onoff` which
  // caused Alexa to silently filter those entries out of /api/lights.
  const char* typeString(EspalexaDeviceType t)
  {
    switch (t)
    {
      case EspalexaDeviceType::onoff:         return PSTR("On/Off plug-in unit");
      case EspalexaDeviceType::dimmable:      return PSTR("Dimmable light");
      case EspalexaDeviceType::whitespectrum: return PSTR("Color temperature light");
      case EspalexaDeviceType::color:         return PSTR("Color light");
      case EspalexaDeviceType::extendedcolor: return PSTR("Extended color light");
      default: return "";
    }
  }

  // IRHUB: default Hue modelids (real Philips part numbers). The
  // per-device pool above overrides these in /lights to defeat Alexa's
  // UI dedupe; this fallback is used only when no device is supplied.
  const char* modelidString(EspalexaDeviceType t)
  {
    switch (t)
    {
      case EspalexaDeviceType::onoff:         return "LOM001";
      case EspalexaDeviceType::dimmable:      return "LWB010";
      case EspalexaDeviceType::whitespectrum: return "LWT010";
      case EspalexaDeviceType::color:         return "LST001";
      case EspalexaDeviceType::extendedcolor: return "LCT015";
      default: return "";
    }
  }

  // IRHUB: per-device identity variation.
  // ──────────────────────────────────────────────────────────────────────
  //   Why this exists
  // ──────────────────────────────────────────────────────────────────────
  // Live HTTP-capture proved Alexa's smart-home app collapses two cards
  // that share *any* of: bridge id, modelid, manufacturername, productname,
  // swversion, friendly-name-first-word, or uniqueid prefix. With every
  // Espalexa light defaulting to the same values for all of those, only
  // the FIRST card rendered — the second light's entity existed in
  // Alexa's cloud and routed commands correctly, but its card was hidden.
  //
  //   How we defeat it
  // ──────────────────────────────────────────────────────────────────────
  // The per-device EUI-48 uniqueid prefix (encodeLightId) is the real
  // silver bullet — see also EspalexaDevice::_uidMac. The four per-device
  // string pools below are belt-and-suspenders against any future Alexa
  // heuristic that might also key on modelid/manufacturer/product/sw.
  //
  // Pool size 16 keeps every field distinct across the full 20-device cap.
  // Indices are sub-byte-deterministic over stableId so a given device
  // always picks the same combination — Alexa's cloud never sees field
  // flip-flop on re-poll.
  // ──────────────────────────────────────────────────────────────────────
  const char* perDeviceModelId(EspalexaDevice* dev, EspalexaDeviceType t)
  {
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
  
  // IRHUB: 1-based per-device sub-key used in both uniqueid and dict key.
  // Returns stableId+1 if set, else legacy arrayIdx+1. The +1 avoids
  // emitting "...-00" / dict-key 0, which some Alexa generations reject.
  inline uint16_t lightSubkey(uint8_t arrayIdx)
  {
    if (arrayIdx < currentDeviceCount && devices[arrayIdx] != nullptr &&
        devices[arrayIdx]->hasStableId()) {
      return (uint16_t)(devices[arrayIdx]->getStableId() + 1);
    }
    return (uint16_t)(arrayIdx + 1);
  }

  // IRHUB: emits the canonical Hue uniqueid "XX:XX:XX:FF:FE:XX:XX:XX-XX"
  // — 6-byte MAC with FF:FE injected (Zigbee EUI-64 convention) + 2-hex
  // endpoint. Prefers the per-device EUI-48 (set via setUniqueIdMac)
  // when present so each light looks like a distinct Zigbee node;
  // falls back to the bridge MAC otherwise. The 2-hex endpoint caps
  // stableId at 255, which is comfortably above ESPALEXA_MAXDEVICES.
  void encodeLightId(uint8_t arrayIdx, char* out)
  {
    const uint8_t* mac = bridgeMac_;
    if (arrayIdx < currentDeviceCount && devices[arrayIdx] != nullptr &&
        devices[arrayIdx]->hasUniqueIdMac()) {
      mac = devices[arrayIdx]->getUniqueIdMac();
    }
    uint16_t sub = lightSubkey(arrayIdx);
    sprintf_P(out, PSTR("%02x:%02x:%02x:ff:fe:%02x:%02x:%02x-%02x"),
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
              (unsigned)(sub & 0xFFU));
  }

  // 'Globally unique' JSON dict key (signed int). Derived from the
  // per-device stable sub-key. Bottom 7 bits are the only ones decodeLightKey
  // reads, so MAXDEVICES > 128 would alias — guarded by static_assert.
  inline int encodeLightKey(uint8_t arrayIdx)
  {
    static_assert(ESPALEXA_MAXDEVICES <= 128, "");
    uint16_t sub = lightSubkey(arrayIdx);
    return (mac24 << 7) | (sub & 0x7F);
  }

  // Reverse of encodeLightKey. Walks the device list because stableIds
  // are caller-supplied and may not match the array index. Returns 255
  // when no live device matches.
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

  // IRHUB: default Hue product names — fallback for perDeviceProductName's
  // default branch (used only for color types, which the per-device pool
  // doesn't override).
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

  // See perDeviceModelId() banner above for the rationale behind these pools.
  const char* perDeviceManufacturer(EspalexaDevice* dev)
  {
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

  // IRHUB: builds the `/lights/<id>` JSON body. Two shapes:
  //   - onoff (Hue Smart Plug LOM001):  no `bri`, alert "select", config block
  //   - dimmable/colour (LWB/LCT lamp): standard Hue light shape
  // Both mirror a real Hue v2 bridge response byte-for-byte; upstream's
  // "E0" productname + "espalexa-..." swversion got entries silently
  // rejected by Alexa, restarting the discovery loop endlessly.
  // All identity fields (modelid / mfgr / productname / swversion / uniqueid)
  // are per-device — see perDeviceModelId() banner for why.
  void deviceJsonString(EspalexaDevice* dev, char* buf)
  {
    char buf_lightid[29];  // "XX:XX:XX:XX:XX:XX:XX:XX-XX\0"
    encodeLightId(dev->getId(), buf_lightid);

    const EspalexaDeviceType t = dev->getType();
    const char* devModel = perDeviceModelId(dev, t);
    const char* devProd  = perDeviceProductName(dev, t);
    const char* devMfg   = perDeviceManufacturer(dev);
    const char* devSwVer = perDeviceSwVersion(dev);

    if (t == EspalexaDeviceType::onoff) {
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

    // Dimmable / colour: upstream-Espalexa-exact JSON shape, with productname
    // and swversion replaced by realistic Hue values (upstream's "E%u" /
    // "espalexa-..." caused Alexa to reject entries).
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
    
    // IRHUB: caller-overridable friendlyName so multiple hubs on one LAN
    // are distinguishable (defaults to upstream's "Espalexa (IP:80)").
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

  // IRHUB: build `/api/<user>/config` body. Modern Echo gens validate this
  // BEFORE enumerating lights and silently drop the bridge if it returns
  // `{}` (upstream's behaviour). Mirrors a real Hue v2 bridge response.
  void buildConfigJson(char* out, size_t outSize)
  {
    IPAddress localIP = WiFi.localIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
             localIP[0], localIP[1], localIP[2], localIP[3]);

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

  // IRHUB: stream every registered light over a chunked response. Building
  // the full lights JSON in heap (~500 B × MAXDEVICES) is the difference
  // between "works" and "WiFi stack OOM" on the '8266's 16-19 KB free heap.
  // Caller must have set CONTENT_LENGTH_UNKNOWN and started the response.
  void streamLightsDict()
  {
    server->sendContent_P(PSTR("{"));
    for (int i = 0; i < currentDeviceCount; i++) {
      char keyBuf[24];  // ',"<int>":' ≤ 15 B
      snprintf(keyBuf, sizeof(keyBuf), "%s\"%d\":",
               (i == 0) ? "" : ",", encodeLightKey(i));
      server->sendContent(keyBuf);
      char buf[768];  // sized for full Hue plug payload (~560 B)
      deviceJsonString(devices[i], buf);
      server->sendContent(buf);
    }
    server->sendContent_P(PSTR("}"));
  }

  // IRHUB: GET /api/<user>/config. 1024-byte buffer leaves margin over the
  // ~640 B template + substitutions; a prior 640-byte buffer truncated and
  // left the JSON unterminated.
  void serveConfig()
  {
    char buf[1024];
    buildConfigJson(buf, sizeof(buf));
    server->send(200, "application/json", buf);
  }

  // IRHUB: GET /api/<user> — Hue's "full datastore" endpoint. Some Echo
  // gens walk this instead of /lights + /config separately. Streamed
  // chunked so we never hold the full ~3 KB response in heap at once.
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
    server->sendContent("");  // terminates chunked response
  }

  //respond to UDP SSDP M-SEARCH
  void respondToSearch()
  {
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    char buf[1024];

    // IRHUB: canonical 16-char Hue bridge id (e.g. 308398FFFE80B5FE).
    // Echo gens cross-check this against /api/<user>/config; upstream's
    // lowercase 12-char MAC was silently rejected.
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

    // IRHUB: snapshot the effective bridge MAC (caller-supplied or WiFi
    // fallback) into the derived identity fields. Everything below — SSDP,
    // description.xml, /config, uniqueid prefix — reads these.
    if (!bridgeMacSet_) {
      WiFi.macAddress(bridgeMac_);
    }

    char macHex[13];
    snprintf(macHex, sizeof(macHex), "%02x%02x%02x%02x%02x%02x",
             bridgeMac_[0], bridgeMac_[1], bridgeMac_[2],
             bridgeMac_[3], bridgeMac_[4], bridgeMac_[5]);
    escapedMac = String(macHex);
    mac24 = strtol(escapedMac.substring(6, 12).c_str(), 0, 16);

    // Canonical Hue bridgeid: <MAC1-3>FFFE<MAC4-6>, uppercase.
    String upperMac = escapedMac;
    upperMac.toUpperCase();
    bridgeIdHex_ = upperMac.substring(0, 6) + "FFFE" + upperMac.substring(6, 12);

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
          char buf[768];  // sized for full Hue plug payload
          deviceJsonString(devices[idx], buf);
          server->send(200, "application/json", buf);
        } else {
          server->send(200, "application/json", "{}");
        }
      }
      
      return true;
    }

    // IRHUB: /api/<user>/config — Echo gens validate the bridge here
    // before hitting /lights. Upstream returned {} which silently dropped us.
    if (req.indexOf("/config") > 0) {
      EA_DEBUGLN("cfg");
      serveConfig();
      return true;
    }

    // IRHUB: bare /api/<user> — Hue's "full datastore" endpoint. Some Echo
    // gens prefer this over walking /lights + /config separately.
    int apiPos = req.indexOf("/api/");
    if (apiPos >= 0) {
      int userStart = apiPos + 5;
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

  // IRHUB: override the bridge name shown under Devices in the Alexa app.
  // Empty string reverts to the upstream default. Ideally called before
  // begin(); changes after take effect on Alexa's next discovery cycle.
  void setFriendlyName(const String& name)
  {
    customFriendlyName_ = name;
  }

  // IRHUB: install a caller-managed 6-byte EUI-48 as the bridge identity.
  // MUST be called BEFORE begin() — begin() snapshots this into escapedMac,
  // mac24, and bridgeIdHex. Pass a random locally-administered MAC that
  // you re-roll on factory reset; that's how we give Alexa a fresh bridge
  // identity post-wipe and prevent stale-entity carry-over from its cache.
  void setBridgeMac(const uint8_t mac[6])
  {
    for (uint8_t i = 0; i < 6; i++) bridgeMac_[i] = mac[i];
    bridgeMacSet_ = true;
  }

  // IRHUB: canonical 16-char Hue bridge id (e.g. "308398FFFE80B5FE"). For
  // callers that need to advertise the same identity Espalexa is using.
  const String& getBridgeIdHex() const { return bridgeIdHex_; }

  // IRHUB: multicast `ssdp:byebye` NOTIFY frames so Alexa expires the
  // bridge entry promptly. Without this Alexa caches the bridge for the
  // full max-age (~100 s) and re-attaches stale uniqueids after reboot.
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

  // IRHUB: shut down UDP + HTTP cleanly (used before ESP.restart() so we
  // don't leave stale sockets in lwIP). After stop() the instance must
  // be re-begin()'d before use.
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
