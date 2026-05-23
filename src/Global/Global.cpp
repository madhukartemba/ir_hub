#include "Global.h"

// Instantiate the global objects
Button button;
Haptics haptics;
Router router;
Speaker speaker;
NeoRing ledRing;
Display display;
IdGen idGen;
IRManager irManager;
DeviceManager deviceManager(idGen);
AlexaConnector alexaConnector(deviceManager, irManager);
MQTTConnector mqttConnector(deviceManager, irManager);
WiFiManagerLib wifiManager(display, ledRing, speaker);
OtaUpdater otaUpdater;