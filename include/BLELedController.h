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
#define CHAR_FOLD_POINT_UUID     "a5b0f9a7-3c65-8e9f-cf0c-6789abcdef01"  // WRITE + READ
#define CHAR_TIME_SYNC_UUID      "d6e1a0b8-4a76-9f0c-dc1a-789abcdef012"  // WRITE
#define CHAR_DEVICE_CONTROL_UUID "c7f8e0d9-5b87-1a2b-be9d-7890abcdef23"  // WRITE
#define CHAR_EFFECTS_LIST_UUID   "d8f9e1ea-6c98-2b3c-cf0e-890abcdef234"  // READ

// NOTA: Il servizio LED richiede ~16 handle (10 char). Il default è 15.
// In BLELedController.cpp usare: pServer->createService(LED_SERVICE_UUID, 50);

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
    bool bladeEnabled = false;  // Stato accensione lama (false = spenta, true = accesa)
    bool statusLedEnabled = true;  // Stato del LED integrato sul pin 4
    uint8_t foldPoint = 72;  // Punto di piegatura LED (default = metà)

    // Boot behavior
    bool autoIgnitionOnBoot = false;   // Se true, accende la lama automaticamente dopo un delay
    uint32_t autoIgnitionDelayMs = 5000; // Delay accensione automatica (ms)
    bool motionOnBoot = false;        // Se true, abilita motion e avvia camera all'avvio

    // Time sync data (ChronoSaber)
    uint32_t epochBase = 0;      // Unix timestamp di riferimento
    uint32_t millisAtSync = 0;   // millis() al momento del sync

    // Chrono themes (modulare: marker ore + cursori secondi/minuti)
    uint8_t chronoHourTheme = 0;   // 0=Classic, 1=Neon, 2=Plasma, 3=Digital
    uint8_t chronoSecondTheme = 0; // 0=Classic, 1=TimeSpiral, 2=FireClock, 3=Lightning, 4=Particle, 5=Quantum
};

// Forward declaration
class LedEffectEngine;

class BLELedController {
private:
    BLEServer* pServer;
    BLECharacteristic* pCharState;
    BLECharacteristic* pCharColor;
    BLECharacteristic* pCharEffect;
    BLECharacteristic* pCharBrightness;
    BLECharacteristic* pCharStatusLed;
    BLECharacteristic* pCharFoldPoint;
    BLECharacteristic* pCharTimeSync;
    BLECharacteristic* pCharDeviceControl;
    BLECharacteristic* pCharEffectsList;
    bool deviceConnected;
    LedState* ledState;
    bool configDirty;
    LedEffectEngine* effectEngine;

public:
    explicit BLELedController(LedState* state);
    void begin(BLEServer* server);
    void notifyState();
    bool isConnected();
    void setConnected(bool connected);
    void setConfigDirty(bool dirty);
    bool isConfigDirty();
    void setEffectEngine(LedEffectEngine* engine);

    // Callback classes (friend)
    friend class ColorCallbacks;
    friend class EffectCallbacks;
    friend class BrightnessCallbacks;
    friend class StatusLedCallbacks;
    friend class FoldPointCallbacks;
    friend class TimeSyncCallbacks;
    friend class DeviceControlCallbacks;
    friend class EffectsListCallbacks;
};

#endif
