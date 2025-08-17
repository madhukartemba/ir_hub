#include "Global.h"

// Instantiate the global objects
Button button;
Router router;
Speaker speaker;
LedRing ring;
Display display;
IdGen idGen;
IRManager irManager;
DeviceManager deviceManager(idGen);
AlexaConnector alexaConnector(deviceManager, irManager);
WiFiManagerLib wifiManager(display, speaker);