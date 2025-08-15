# AlexaConnector Library

The AlexaConnector library provides seamless integration between the IR Hub device manager and Amazon Alexa voice assistant.

## Features

- **Automatic Device Discovery**: New devices added to DeviceManager are automatically registered with Alexa
- **Device Removal**: Removed devices are automatically unregistered from Alexa
- **Startup Registration**: All existing devices are registered with Alexa on system startup
- **IR Command Execution**: Alexa voice commands trigger actual IR signals to control devices
- **Error Handling**: Robust error handling and logging for debugging

## Architecture

### Observer Pattern Integration

The AlexaConnector uses an observer pattern to automatically detect device changes:

```
DeviceManager ←→ AlexaConnector ←→ Amazon Alexa
```

### Device Lifecycle

1. **Device Addition**:

   - User adds device via UI → DeviceManager saves device → Notifies AlexaConnector → Device registered with Alexa

2. **Device Removal**:

   - User removes device via UI → DeviceManager deletes device → Notifies AlexaConnector → Device unregistered from Alexa

3. **System Startup**:
   - System boots → DeviceManager loads all devices → AlexaConnector registers all devices with Alexa

## Usage

### Initialization

The AlexaConnector is automatically initialized in `main.cpp`:

```cpp
// Set up DeviceManager callbacks for AlexaConnector
deviceManager.setDeviceAddedCallback([&](const Device& device) {
    alexaConnector.onDeviceAdded(device);
});
deviceManager.setDeviceRemovedCallback([&](int deviceId) {
    alexaConnector.onDeviceRemoved(deviceId);
});

// Initialize AlexaConnector after WiFi connection
alexaConnector.setup();
```

### Device Registration

Devices are automatically registered with Alexa using the naming convention:

- Format: `ir_device_<device_id>`
- Example: `ir_device_123` for device with ID 123

### Alexa Voice Commands

Once devices are registered, users can control them via Alexa:

- "Alexa, turn on ir_device_123"
- "Alexa, turn off ir_device_123"

## Dependencies

- **fauxmoESP**: Provides Alexa emulation functionality
- **DeviceManager**: Manages device storage and lifecycle
- **IRManager**: Sends IR commands to devices
- **WiFiManager**: Handles WiFi connectivity

## Error Handling

The library includes comprehensive error handling:

- Invalid device registration attempts are logged and ignored
- IR command failures are logged with detailed error messages
- Network connectivity issues are handled gracefully
- Device not found errors are logged when Alexa commands reference non-existent devices

## Logging

The library provides detailed logging for debugging:

- Device registration/unregistration events
- Alexa command reception and processing
- IR command execution results
- Error conditions and exceptions

## Configuration

The AlexaConnector uses the following default configuration:

- **Port**: 80 (HTTP)
- **Device Name Prefix**: "ir*device*"
- **Auto-registration**: Enabled for all device types
- **Error Recovery**: Automatic retry for failed operations

## Troubleshooting

### Common Issues

1. **Devices not appearing in Alexa**: Check WiFi connectivity and ensure fauxmoESP is properly initialized
2. **Commands not working**: Verify IRManager is properly configured and IR transmitter is connected
3. **Duplicate devices**: Check if devices are being registered multiple times

### Debug Commands

Enable debug logging to troubleshoot issues:

```cpp
// In your main.cpp or configuration
#define LOG_LEVEL_DEBUG
```

## Future Enhancements

- Support for device groups and scenes
- Custom device names for better Alexa integration
- Power state tracking and reporting
- Multi-room device support
