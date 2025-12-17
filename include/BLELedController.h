#ifndef BLE_LED_CONTROLLER_H
#define BLE_LED_CONTROLLER_H

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <ArduinoJson.h>

// UUIDs del servizio LED
#define LED_SERVICE_UUID         "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_LED_STATE_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // READ + NOTIFY
#define CHAR_LED_COLOR_UUID      "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab"  // WRITE
#define CHAR_LED_EFFECT_UUID     "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc"  // WRITE
#define CHAR_LED_BRIGHTNESS_UUID "f3e7c6e5-0d32-4c5a-ac6e-3456789012cd"  // WRITE
#define CHAR_STATUS_LED_UUID     "a4b8d7f9-1e43-6c7d-ad8f-456789abcdef"  // WRITE + READ

// Stato LED globale
struct LedState {
    uint8_t r = 255;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t brightness = 255;
    uint8_t statusLedBrightness = 32;
    String effect = "solid";
    uint8_t speed = 50;
    bool enabled = true;
    bool statusLedEnabled = true;  // Stato del LED integrato sul pin 4
};

class BLELedController {
private:
    BLEServer* pServer;
    BLECharacteristic* pCharState;
    BLECharacteristic* pCharColor;
    BLECharacteristic* pCharEffect;
    BLECharacteristic* pCharBrightness;
    BLECharacteristic* pCharStatusLed;
    bool deviceConnected;
    LedState* ledState;
    bool configDirty;

public:
    explicit BLELedController(LedState* state);
    void begin(BLEServer* server);
    void notifyState();
    bool isConnected();
    void setConnected(bool connected);
    void setConfigDirty(bool dirty);
    bool isConfigDirty();

    // Callback classes (friend)
    friend class ColorCallbacks;
    friend class EffectCallbacks;
    friend class BrightnessCallbacks;
    friend class StatusLedCallbacks;
};

#endif
