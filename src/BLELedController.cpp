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
            controller->ledState->brightness = requestedBrightness;
            controller->ledState->enabled = doc["enabled"] | true;

            Serial.printf("[BLE] Brightness requested: %d (enabled: %d)\n",
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
            controller->ledState->statusLedEnabled = doc["enabled"] | true;

            Serial.printf("[BLE] Status LED (pin 4) set to %s\n",
                controller->ledState->statusLedEnabled ? "ON" : "OFF");

            controller->setConfigDirty(true);
        }
    }

    void onRead(BLECharacteristic *pChar) override {
        JsonDocument doc;
        doc["enabled"] = controller->ledState->statusLedEnabled;

        String jsonString;
        serializeJson(doc, jsonString);
        pChar->setValue(jsonString.c_str());
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
}

// Inizializzazione BLE
void BLELedController::begin(BLEServer* server) {
    pServer = server;

    // Crea service con handles sufficienti (10 char * 4 handles ciascuna = 40, +10 margine)
    BLEService *pService = pServer->createService(BLEUUID(LED_SERVICE_UUID), 50);

    // Characteristic 1: State (READ + NOTIFY)
    pCharState = pService->createCharacteristic(
        CHAR_LED_STATE_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharState->addDescriptor(new BLE2902());  // Abilita notifiche
    BLEDescriptor* descState = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descState->setValue("LED State");
    pCharState->addDescriptor(descState);

    // Characteristic 2: Color (WRITE)
    pCharColor = pService->createCharacteristic(
        CHAR_LED_COLOR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharColor->setCallbacks(new ColorCallbacks(this));
    BLEDescriptor* descColor = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descColor->setValue("LED Color");
    pCharColor->addDescriptor(descColor);

    // Characteristic 3: Effect (WRITE)
    pCharEffect = pService->createCharacteristic(
        CHAR_LED_EFFECT_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharEffect->setCallbacks(new EffectCallbacks(this));
    BLEDescriptor* descEffect = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descEffect->setValue("LED Effect");
    pCharEffect->addDescriptor(descEffect);

    // Characteristic 4: Brightness (WRITE)
    pCharBrightness = pService->createCharacteristic(
        CHAR_LED_BRIGHTNESS_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
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
    pCharStatusLed->setCallbacks(new StatusLedCallbacks(this));
    BLEDescriptor* descStatusLed = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descStatusLed->setValue("Status LED Pin 4");
    pCharStatusLed->addDescriptor(descStatusLed);

    // Avvia service
    pService->start();

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
