#ifndef BLUETOOTH_LOGGER_H
#define BLUETOOTH_LOGGER_H

#include <Arduino.h>
#include <BluetoothSerial.h>

class BluetoothLogger {
private:
    BluetoothSerial* bt;
    bool enabled;

public:
    explicit BluetoothLogger(BluetoothSerial* bluetooth);
    void begin(const char* deviceName);
    void log(const String& message);
    void logf(const char* format, ...);
    bool isConnected();
};

#endif
