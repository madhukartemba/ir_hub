#ifndef EspalexaDevice_h
#define EspalexaDevice_h

#include "Arduino.h"
#include <functional>

class EspalexaDevice;

typedef std::function<void(uint8_t b)> BrightnessCallbackFunction;
typedef std::function<void(EspalexaDevice* d)> DeviceCallbackFunction;
typedef std::function<void(uint8_t br, uint32_t col)> ColorCallbackFunction;

enum class EspalexaColorMode : uint8_t { none = 0, ct = 1, hs = 2, xy = 3 };
enum class EspalexaDeviceType : uint8_t { onoff = 0, dimmable = 1, whitespectrum = 2, color = 3, extendedcolor = 4 };
enum class EspalexaDeviceProperty : uint8_t { none = 0, on = 1, off = 2, bri = 3, hs = 4, ct = 5, xy = 6 };

class EspalexaDevice {
private:
  String _deviceName;
  BrightnessCallbackFunction _callback = nullptr;
  DeviceCallbackFunction _callbackDev = nullptr;
  ColorCallbackFunction _callbackCol = nullptr;
  uint8_t _val, _val_last, _sat = 0;
  uint16_t _hue = 0, _ct = 0;
  float _x = 0.5, _y = 0.5;
  uint32_t _rgb = 0;
  uint8_t _id = 0;
  // IRHUB: caller-supplied stable Hue uniqueid. 0xFFFF means "unset" — in
  // that case Espalexa falls back to the legacy registration-index scheme.
  // 16-bit so a long-lived IR Hub with thousands of historical IdGen IDs
  // can still pass its raw device.id through. Values >127 share the
  // bottom 7 bits with `value % 128` (see Espalexa::encodeLightKey) — only
  // a problem if you somehow exceed 128 *live* devices, which is far above
  // ESPALEXA_MAXDEVICES (10).
  uint16_t _stableId = 0xFFFF;
  EspalexaDeviceType _type;
  EspalexaDeviceProperty _changed = EspalexaDeviceProperty::none;
  EspalexaColorMode _mode = EspalexaColorMode::xy;
  
public:
  EspalexaDevice();
  ~EspalexaDevice();
  EspalexaDevice(String deviceName, BrightnessCallbackFunction bcb, uint8_t initialValue =0);
  EspalexaDevice(String deviceName, DeviceCallbackFunction dcb, EspalexaDeviceType t =EspalexaDeviceType::dimmable, uint8_t initialValue =0);
  EspalexaDevice(String deviceName, ColorCallbackFunction ccb, uint8_t initialValue =0);
  
  String getName();
  uint8_t getId();
  EspalexaDeviceProperty getLastChangedProperty();
  uint8_t getValue();
  uint8_t getLastValue(); //last value that was not off (1-255)
  bool    getState();
  uint8_t getPercent();
  uint8_t getDegrees();
  uint16_t getHue();
  uint8_t getSat();
  uint16_t getCt();
  uint32_t getKelvin();
  float getX();
  float getY();
  uint32_t getRGB();
  uint8_t getR();
  uint8_t getG();
  uint8_t getB();
  uint8_t getW();
  EspalexaColorMode getColorMode();
  EspalexaDeviceType getType();
  
  void setId(uint8_t id);
  // IRHUB: pin this device to a caller-controlled Hue uniqueid that
  // survives reorder/add/remove of other devices. Pass any 16-bit value
  // except 0xFFFF.
  void setStableId(uint16_t stableId);
  uint16_t getStableId() const;
  bool hasStableId() const;
  void setPropertyChanged(EspalexaDeviceProperty p);
  void setValue(uint8_t bri);
  void setState(bool onoff);
  void setPercent(uint8_t perc);
  void setName(String name);
  void setColor(uint16_t ct);
  void setColor(uint16_t hue, uint8_t sat);
  void setColorXY(float x, float y);
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  
  void doCallback();
};

#endif