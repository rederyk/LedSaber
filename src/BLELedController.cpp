#include "BLELedController.h"
#include "LedEffectEngine.h"
#include <esp_system.h>

// Callback scrittura colore
class ColorCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit ColorCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            controller->ledState->r = doc["r"] | controller->ledState->r;
            controller->ledState->g = doc["g"] | controller->ledState->g;
            controller->ledState->b = doc["b"] | controller->ledState->b;

            Serial.printf("[BLE] Color set to RGB(%d,%d,%d)\n",
                controller->ledState->r,
                controller->ledState->g,
                controller->ledState->b);

            controller->setConfigDirty(true);
        } else {
            Serial.println("[BLE ERROR] Invalid JSON for color");
        }
    }
};

// Callback scrittura effetto
class EffectCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit EffectCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            controller->ledState->effect = doc["mode"] | "solid";
            controller->ledState->speed = doc["speed"] | 50;

            // Chrono themes (opzionale, solo se presenti nel JSON)
            if (!doc["chronoHourTheme"].isNull()) {
                controller->ledState->chronoHourTheme = doc["chronoHourTheme"];
            }
            if (!doc["chronoSecondTheme"].isNull()) {
                controller->ledState->chronoSecondTheme = doc["chronoSecondTheme"];
            }

            Serial.printf("[BLE] Effect set to %s (speed: %d, chrono_h: %d, chrono_s: %d)\n",
                controller->ledState->effect.c_str(),
                controller->ledState->speed,
                controller->ledState->chronoHourTheme,
                controller->ledState->chronoSecondTheme);

            controller->setConfigDirty(true);
        }
    }
};

// Callback scrittura luminosità
class BrightnessCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit BrightnessCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            uint8_t requestedBrightness = doc["brightness"] | 255;
            // IMPORTANTE: FastLED gestisce la potenza (5V 4500mA), permettiamo range completo 0-255
            static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 255;
            controller->ledState->brightness = min(requestedBrightness, MAX_SAFE_BRIGHTNESS);
            controller->ledState->enabled = doc["enabled"] | true;

            if (requestedBrightness > MAX_SAFE_BRIGHTNESS) {
                Serial.printf("[BLE] Brightness clamped: %d -> %d (max safe limit)\n",
                    requestedBrightness, MAX_SAFE_BRIGHTNESS);
            }

            Serial.printf("[BLE] Brightness set: %d (enabled: %d)\n",
                controller->ledState->brightness,
                controller->ledState->enabled);

            controller->setConfigDirty(true);
        }
    }
};

// Callback controllo LED integrato (pin 4)
class StatusLedCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit StatusLedCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            bool updated = false;

            if (!doc["enabled"].isNull()) {
                controller->ledState->statusLedEnabled = doc["enabled"] | controller->ledState->statusLedEnabled;
                updated = true;
            }

            if (!doc["brightness"].isNull()) {
                uint8_t requestedBrightness = doc["brightness"] | controller->ledState->statusLedBrightness;
                requestedBrightness = constrain(requestedBrightness, static_cast<uint8_t>(0), static_cast<uint8_t>(255));
                controller->ledState->statusLedBrightness = requestedBrightness;
                updated = true;
            }

            if (updated) {
                Serial.printf("[BLE] Status LED (pin 4): enabled=%d brightness=%d\n",
                    controller->ledState->statusLedEnabled,
                    controller->ledState->statusLedBrightness);
                controller->setConfigDirty(true);
            }
        }
    }

    void onRead(BLECharacteristic *pChar) override {
        JsonDocument doc;
        doc["enabled"] = controller->ledState->statusLedEnabled;
        doc["brightness"] = controller->ledState->statusLedBrightness;

        String jsonString;
        serializeJson(doc, jsonString);
        pChar->setValue(jsonString.c_str());
    }
};

// Callback configurazione punto di piegatura
class FoldPointCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit FoldPointCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            uint8_t requestedFoldPoint = doc["foldPoint"] | controller->ledState->foldPoint;

            // Validazione: deve essere tra 1 e 143 (NUM_LEDS-1)
            if (requestedFoldPoint >= 1 && requestedFoldPoint < 144) {
                controller->ledState->foldPoint = requestedFoldPoint;
                Serial.printf("[BLE] Fold point set to %d\n", controller->ledState->foldPoint);
                controller->setConfigDirty(true);
            } else {
                Serial.printf("[BLE ERROR] Invalid fold point: %d (must be 1-143)\n",
                    requestedFoldPoint);
            }
        }
    }

    void onRead(BLECharacteristic *pChar) override {
        JsonDocument doc;
        doc["foldPoint"] = controller->ledState->foldPoint;

        String jsonString;
        serializeJson(doc, jsonString);
        pChar->setValue(jsonString.c_str());
    }
};

// Callback sincronizzazione tempo
class TimeSyncCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit TimeSyncCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();
        Serial.printf("[BLE TIME SYNC] Received: %s\n", value.c_str());

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            uint32_t receivedEpoch = doc["epoch"] | 0;
            Serial.printf("[BLE TIME SYNC] Parsed epoch: %lu\n", receivedEpoch);

            controller->ledState->epochBase = receivedEpoch;
            controller->ledState->millisAtSync = millis();

            Serial.printf("[BLE] Time synced: epoch=%lu at millis=%lu\n",
                controller->ledState->epochBase,
                controller->ledState->millisAtSync);
        } else {
            Serial.printf("[BLE ERROR] Invalid JSON for time sync: %s\n", error.c_str());
        }
    }
};

// Callback Device Control (ignition, retract, reboot, sleep)
class DeviceControlCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit DeviceControlCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();
        Serial.printf("[BLE DEVICE_CONTROL] Received: %s\n", value.c_str());

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            String command = doc["command"] | "";
            Serial.printf("[BLE DEVICE_CONTROL] Parsed command: %s\n", command.c_str());

            if (command == "ignition") {
                if (controller->effectEngine) {
                    Serial.println("[BLE] Power ON (ignition with animation)");
                    controller->effectEngine->powerOn();
                } else {
                    Serial.println("[BLE ERROR] EffectEngine not set!");
                }
            } else if (command == "boot_config") {
                bool updated = false;

                if (!doc["autoIgnitionOnBoot"].isNull()) {
                    controller->ledState->autoIgnitionOnBoot = doc["autoIgnitionOnBoot"] | controller->ledState->autoIgnitionOnBoot;
                    updated = true;
                }

                if (!doc["autoIgnitionDelayMs"].isNull()) {
                    const uint32_t requestedDelayMs = doc["autoIgnitionDelayMs"] | controller->ledState->autoIgnitionDelayMs;
                    controller->ledState->autoIgnitionDelayMs = min<uint32_t>(requestedDelayMs, 60000);
                    updated = true;
                }

                if (updated) {
                    controller->setConfigDirty(true);
                    Serial.printf("[BLE] Boot config updated: autoIgnitionOnBoot=%d, autoIgnitionDelayMs=%lu\n",
                        controller->ledState->autoIgnitionOnBoot ? 1 : 0,
                        controller->ledState->autoIgnitionDelayMs);
                } else {
                    Serial.println("[BLE] Boot config command received but no fields provided");
                }
            } else if (command == "retract") {
                if (controller->effectEngine) {
                    Serial.println("[BLE] Power OFF (retraction with animation, no deep sleep)");
                    controller->effectEngine->powerOff(false);  // No deep sleep via BLE
                } else {
                    Serial.println("[BLE ERROR] EffectEngine not set!");
                }
            } else if (command == "reboot") {
                Serial.println("[BLE] Rebooting ESP32 in 1 second...");
                delay(1000);
                esp_restart();
            } else if (command == "sleep") {
                if (controller->effectEngine) {
                    Serial.println("[BLE] Power OFF with deep sleep");
                    controller->effectEngine->powerOff(true);  // Deep sleep mode
                } else {
                    Serial.println("[BLE] Entering deep sleep directly (no animation)...");
                    delay(500);
                    esp_deep_sleep_start();
                }
            } else {
                Serial.printf("[BLE ERROR] Unknown device control command: %s\n", command.c_str());
            }
        } else {
            Serial.printf("[BLE ERROR] Invalid JSON for device control: %s\n", error.c_str());
        }
    }
};

// Callback Effects List (READ only)
class EffectsListCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit EffectsListCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onRead(BLECharacteristic *pChar) override {
        // Costruisci JSON con lista effetti e parametri
        const char* effectsList = R"({
  "version": "1.0.0",
  "effects": [
    {"id":"solid","name":"Solid Color","params":["color"],"icon":"🟢"},
    {"id":"rainbow","name":"Rainbow","params":["speed"],"icon":"🌈"},
    {"id":"pulse","name":"Pulse Wave","params":["speed","color"],"icon":"⚡"},
    {"id":"breathe","name":"Breathing","params":["speed"],"icon":"💨"},
    {"id":"flicker","name":"Kylo Ren Flicker","params":["speed"],"icon":"🔥"},
    {"id":"unstable","name":"Kylo Ren Advanced","params":["speed"],"icon":"💥"},
    {"id":"dual_pulse","name":"Dual Pulse","params":["speed"],"icon":"⚔️"},
    {"id":"dual_pulse_simple","name":"Dual Pulse Simple","params":["speed"],"icon":"⚔️"},
    {"id":"rainbow_blade","name":"Rainbow Blade","params":["speed"],"icon":"🌟"},
    {"id":"chrono_hybrid","name":"Chrono Clock","params":["chronoHourTheme","chronoSecondTheme"],"themes":{"hour":["Classic","Neon","Plasma","Digital","Inferno","Storm"],"second":["Classic","Spiral","Fire","Lightning","Particle","Quantum"]},"icon":"🕐"}
  ]
})";

        pChar->setValue(effectsList);
        Serial.println("[BLE] Effects list sent to client");
    }
};

// Costruttore
BLELedController::BLELedController(LedState* state) {
    ledState = state;
    deviceConnected = false;
    configDirty = false;
    pServer = nullptr;
    pCharState = nullptr;
    pCharColor = nullptr;
    pCharEffect = nullptr;
    pCharBrightness = nullptr;
    pCharStatusLed = nullptr;
    pCharFoldPoint = nullptr;
    pCharTimeSync = nullptr;
    pCharDeviceControl = nullptr;
    pCharEffectsList = nullptr;
    effectEngine = nullptr;
}

// Inizializzazione BLE
void BLELedController::begin(BLEServer* server) {
    pServer = server;

    // Crea service con handles sufficienti (10 char * 4 handles ciascuna = 40, +10 margine)
    BLEService *pService = pServer->createService(BLEUUID(LED_SERVICE_UUID), 50);

    auto logCreate = [](const char* name, const char* uuid, BLECharacteristic* chr) {
        if (chr == nullptr) {
            Serial.printf("[BLE ERROR] Failed to create characteristic %s (%s)\n", name, uuid);
        }
    };

    // Characteristic 1: State (READ + NOTIFY)
    pCharState = pService->createCharacteristic(
        CHAR_LED_STATE_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    logCreate("State", CHAR_LED_STATE_UUID, pCharState);
    pCharState->addDescriptor(new BLE2902());  // Abilita notifiche
    BLEDescriptor* descState = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descState->setValue("LED State");
    pCharState->addDescriptor(descState);

    // Characteristic 2: Color (WRITE)
    pCharColor = pService->createCharacteristic(
        CHAR_LED_COLOR_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    logCreate("Color", CHAR_LED_COLOR_UUID, pCharColor);
    pCharColor->setCallbacks(new ColorCallbacks(this));
    BLEDescriptor* descColor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descColor->setValue("LED Color");
    pCharColor->addDescriptor(descColor);

    // Characteristic 3: Effect (WRITE)
    pCharEffect = pService->createCharacteristic(
        CHAR_LED_EFFECT_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    logCreate("Effect", CHAR_LED_EFFECT_UUID, pCharEffect);
    pCharEffect->setCallbacks(new EffectCallbacks(this));
    BLEDescriptor* descEffect = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descEffect->setValue("LED Effect");
    pCharEffect->addDescriptor(descEffect);

    // Characteristic 4: Brightness (WRITE)
    pCharBrightness = pService->createCharacteristic(
        CHAR_LED_BRIGHTNESS_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    logCreate("Brightness", CHAR_LED_BRIGHTNESS_UUID, pCharBrightness);
    pCharBrightness->setCallbacks(new BrightnessCallbacks(this));
    BLEDescriptor* descBrightness = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descBrightness->setValue("LED Brightness");
    pCharBrightness->addDescriptor(descBrightness);

    // Characteristic 5: Status LED integrato su pin 4 (READ + WRITE)
    pCharStatusLed = pService->createCharacteristic(
        CHAR_STATUS_LED_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );
    logCreate("StatusLED", CHAR_STATUS_LED_UUID, pCharStatusLed);
    pCharStatusLed->setCallbacks(new StatusLedCallbacks(this));
    BLEDescriptor* descStatusLed = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descStatusLed->setValue("Status LED Pin 4");
    pCharStatusLed->addDescriptor(descStatusLed);

    // Characteristic 6: Fold Point (READ + WRITE)
    pCharFoldPoint = pService->createCharacteristic(
        CHAR_FOLD_POINT_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );
    logCreate("FoldPoint", CHAR_FOLD_POINT_UUID, pCharFoldPoint);
    pCharFoldPoint->setCallbacks(new FoldPointCallbacks(this));
    BLEDescriptor* descFoldPoint = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descFoldPoint->setValue("LED Strip Fold Point");
    pCharFoldPoint->addDescriptor(descFoldPoint);

    // Characteristic 7: Time Sync (WRITE)
    pCharTimeSync = pService->createCharacteristic(
        CHAR_TIME_SYNC_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    logCreate("TimeSync", CHAR_TIME_SYNC_UUID, pCharTimeSync);
    pCharTimeSync->setCallbacks(new TimeSyncCallbacks(this));
    BLEDescriptor* descTimeSync = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descTimeSync->setValue("Time Sync");
    pCharTimeSync->addDescriptor(descTimeSync);

    // Characteristic 8: Device Control (WRITE) - ignition, retract, reboot, sleep
    pCharDeviceControl = pService->createCharacteristic(
        CHAR_DEVICE_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    logCreate("DeviceControl", CHAR_DEVICE_CONTROL_UUID, pCharDeviceControl);
    pCharDeviceControl->setCallbacks(new DeviceControlCallbacks(this));
    BLEDescriptor* descDeviceControl = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descDeviceControl->setValue("Device Control");
    pCharDeviceControl->addDescriptor(descDeviceControl);

    // Characteristic 9: Effects List (READ) - lista effetti disponibili
    pCharEffectsList = pService->createCharacteristic(
        CHAR_EFFECTS_LIST_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    logCreate("EffectsList", CHAR_EFFECTS_LIST_UUID, pCharEffectsList);
    pCharEffectsList->setCallbacks(new EffectsListCallbacks(this));
    BLEDescriptor* descEffectsList = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descEffectsList->setValue("Effects List");
    pCharEffectsList->addDescriptor(descEffectsList);

    // Avvia service
    pService->start();

    Serial.println("[BLE DEBUG] LED GATT UUIDs:");
    Serial.printf("  Service:        %s\n", LED_SERVICE_UUID);
    Serial.printf("  State:          %s\n", CHAR_LED_STATE_UUID);
    Serial.printf("  Color:          %s\n", CHAR_LED_COLOR_UUID);
    Serial.printf("  Effect:         %s\n", CHAR_LED_EFFECT_UUID);
    Serial.printf("  Bright:         %s\n", CHAR_LED_BRIGHTNESS_UUID);
    Serial.printf("  Status:         %s\n", CHAR_STATUS_LED_UUID);
    Serial.printf("  Fold:           %s\n", CHAR_FOLD_POINT_UUID);
    Serial.printf("  Time:           %s\n", CHAR_TIME_SYNC_UUID);
    Serial.printf("  DeviceControl:  %s\n", CHAR_DEVICE_CONTROL_UUID);
    Serial.printf("  EffectsList:    %s\n", CHAR_EFFECTS_LIST_UUID);

    Serial.println("[BLE OK] LED Service initialized with 9 characteristics!");
}

// Notifica stato LED ai client connessi
void BLELedController::notifyState() {
    if (!deviceConnected) return;

    JsonDocument doc;
    doc["r"] = ledState->r;
    doc["g"] = ledState->g;
    doc["b"] = ledState->b;
    doc["brightness"] = ledState->brightness;
    doc["effect"] = ledState->effect;
    doc["speed"] = ledState->speed;
    doc["enabled"] = ledState->enabled;

    // Campo bladeState: "off" | "igniting" | "on" | "retracting"
    String bladeState = "off";
    if (effectEngine) {
        LedEffectEngine::Mode mode = effectEngine->getMode();
        if (mode == LedEffectEngine::Mode::IGNITION_ACTIVE) {
            bladeState = "igniting";
        } else if (mode == LedEffectEngine::Mode::RETRACT_ACTIVE) {
            bladeState = "retracting";
        } else if (ledState->enabled) {
            bladeState = "on";
        }
    } else if (ledState->enabled) {
        bladeState = "on";
    }
    doc["bladeState"] = bladeState;

    doc["statusLedEnabled"] = ledState->statusLedEnabled;
    doc["statusLedBrightness"] = ledState->statusLedBrightness;
    doc["foldPoint"] = ledState->foldPoint;
    doc["autoIgnitionOnBoot"] = ledState->autoIgnitionOnBoot;
    doc["autoIgnitionDelayMs"] = ledState->autoIgnitionDelayMs;

    String jsonString;
    serializeJson(doc, jsonString);

    pCharState->setValue(jsonString.c_str());
    pCharState->notify();
}

bool BLELedController::isConnected() {
    return deviceConnected;
}

void BLELedController::setConnected(bool connected) {
    deviceConnected = connected;
}

void BLELedController::setConfigDirty(bool dirty) {
    configDirty = dirty;
}

bool BLELedController::isConfigDirty() {
    return configDirty;
}

void BLELedController::setEffectEngine(LedEffectEngine* engine) {
    effectEngine = engine;
    Serial.println("[BLE] EffectEngine linked for device control commands");
}
