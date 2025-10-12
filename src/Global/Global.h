#pragma once

#include "AlexaConnector.h"
#include "Button.h"
#include "DeviceManager.h"
#include "Display.h"
#include "IRCode.h"
#include "IRManager.h"
#include "IdGen.h"
#include "Log.h"
#include "NeoRing.h"
#include "Router.h"
#include "Speaker.h"
#include "WiFiManagerLib.h"
#include "preferences.h"

extern Button button;
extern Router router;
extern Speaker speaker;
extern NeoRing ledRing;
extern Display display;
extern IdGen idGen;
extern IRManager irManager;
extern DeviceManager deviceManager;
extern AlexaConnector alexaConnector;
extern WiFiManagerLib wifiManager;