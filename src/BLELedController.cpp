#include "BLELedController.h"

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

            Serial.printf("[BLE] Effect set to %s (speed: %d)\n",
                controller->ledState->effect.c_str(),
                controller->ledState->speed);

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
            // IMPORTANTE: Limita brightness a MAX_SAFE_BRIGHTNESS (60) per sicurezza alimentatore
            static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 60;
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

            if (doc.containsKey("enabled")) {
                controller->ledState->statusLedEnabled = doc["enabled"] | controller->ledState->statusLedEnabled;
                updated = true;
            }

            if (doc.containsKey("brightness")) {
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

    // Avvia service
    pService->start();

    Serial.println("[BLE DEBUG] LED GATT UUIDs:");
    Serial.printf("  Service: %s\n", LED_SERVICE_UUID);
    Serial.printf("  State:   %s\n", CHAR_LED_STATE_UUID);
    Serial.printf("  Color:   %s\n", CHAR_LED_COLOR_UUID);
    Serial.printf("  Effect:  %s\n", CHAR_LED_EFFECT_UUID);
    Serial.printf("  Bright:  %s\n", CHAR_LED_BRIGHTNESS_UUID);
    Serial.printf("  Status:  %s\n", CHAR_STATUS_LED_UUID);
    Serial.printf("  Fold:    %s\n", CHAR_FOLD_POINT_UUID);
    Serial.printf("  Time:    %s\n", CHAR_TIME_SYNC_UUID);

    Serial.println("[BLE OK] LED Service initialized!");
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
    doc["statusLedEnabled"] = ledState->statusLedEnabled;
    doc["statusLedBrightness"] = ledState->statusLedBrightness;
    doc["foldPoint"] = ledState->foldPoint;

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
