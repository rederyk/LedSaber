#include "BluetoothLogger.h"
#include <cstdarg>

BluetoothLogger::BluetoothLogger(BluetoothSerial* bluetooth) {
    bt = bluetooth;
    enabled = false;
}

void BluetoothLogger::begin(const char* deviceName) {
    if (!bt->begin(deviceName)) {
        Serial.println("[BT ERROR] Failed to start Bluetooth Classic!");
        enabled = false;
    } else {
        Serial.printf("[BT OK] Bluetooth Classic started: %s\n", deviceName);
        enabled = true;
    }
}

void BluetoothLogger::log(const String& message) {
    unsigned long ms = millis();
    String timestamped = "[" + String(ms) + "ms] " + message;

    // Serial USB (sempre attivo)
    Serial.println(timestamped);

    // Bluetooth Classic (se connesso)
    if (enabled && bt->hasClient()) {
        bt->println(timestamped);
    }
}

void BluetoothLogger::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(String(buffer));
}

bool BluetoothLogger::isConnected() {
    return enabled && bt->hasClient();
}
