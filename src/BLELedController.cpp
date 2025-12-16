#include "BLELedController.h"
#include <esp_gap_ble_api.h>

// Callback connessione BLE
class ServerCallbacks: public BLEServerCallbacks {
    BLELedController* controller;
public:
    explicit ServerCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onConnect(BLEServer* pServer) override {
        controller->deviceConnected = true;
        Serial.println("[BLE] Client connected!");
    }

    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
        (void)pServer;
        controller->deviceConnected = true;
        Serial.println("[BLE] Client connected!");

        // Richiedi parametri di connessione più aggressivi per throughput OTA (unità: 1.25ms)
        // min=0x06 => 7.5ms, max=0x0C => 15ms, latency=0, timeout=400 => 4s
        esp_ble_conn_update_params_t connParams = {};
        memcpy(connParams.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        connParams.min_int = 0x06;
        connParams.max_int = 0x0C;
        connParams.latency = 0;
        connParams.timeout = 400;
        esp_err_t err = esp_ble_gap_update_conn_params(&connParams);
        Serial.printf("[BLE] Conn params update req: %s\n", esp_err_to_name(err));

        // Richiedi Data Length Extension (251 payload) se supportato
        err = esp_ble_gap_set_pkt_data_len(param->connect.remote_bda, 251);
        Serial.printf("[BLE] Data len update req: %s\n", esp_err_to_name(err));
    }

    void onDisconnect(BLEServer* pServer) override {
        controller->deviceConnected = false;
        Serial.println("[BLE] Client disconnected!");
        pServer->startAdvertising();  // Riprendi advertising
    }

    void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override {
        (void)pServer;
        Serial.printf("[BLE] MTU changed: %u\n", param->mtu.mtu);
    }
};

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
            controller->ledState->brightness = doc["brightness"] | 255;
            controller->ledState->enabled = doc["enabled"] | true;

            Serial.printf("[BLE] Brightness set to %d (enabled: %d)\n",
                controller->ledState->brightness,
                controller->ledState->enabled);
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
    pServer = nullptr;
    pCharState = nullptr;
    pCharColor = nullptr;
    pCharEffect = nullptr;
    pCharBrightness = nullptr;
    pCharStatusLed = nullptr;
}

// Inizializzazione BLE
void BLELedController::begin(const char* deviceName) {
    BLEDevice::init(deviceName);

    // Crea server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));

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

    // Avvia advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(LED_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // iPhone compatibility
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();

    Serial.println("[BLE OK] BLE GATT Server started!");
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
