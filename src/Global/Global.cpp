#include "Global.h"

// Instantiate the global objects
Button button;
Haptics haptics;
Router router;
Speaker speaker;
NeoRing ledRing;
Display display;
IRManager irManager;
DeviceManager deviceManager;
AlexaConnector alexaConnector(deviceManager, irManager);
MQTTConnector mqttConnector(deviceManager, irManager);
WiFiManagerLib wifiManager(display, ledRing, speaker);
OtaUpdater otaUpdater;