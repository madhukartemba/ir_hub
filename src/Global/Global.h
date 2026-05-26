#pragma once

#include <NeoRing.h>
#include "AlexaConnector.h"
#include "MQTTConnector.h"
#include "Button.h"
#include "Haptics.h"
#include "DeviceManager.h"
#include "Display.h"
#include "IRCode.h"
#include "IRManager.h"
#include "Log.h"
#include "OtaUpdater.h"
#include "Router.h"
#include "Speaker.h"
#include "WiFiManagerLib.h"
#include "preferences.h"

extern Button button;
extern Router router;
extern Speaker speaker;
extern Haptics haptics;
extern NeoRing ledRing;
extern Display display;
extern IRManager irManager;
extern DeviceManager deviceManager;
extern AlexaConnector alexaConnector;
extern MQTTConnector mqttConnector;
extern WiFiManagerLib wifiManager;
extern OtaUpdater otaUpdater;