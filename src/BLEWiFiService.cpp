#include "BLEWiFiService.h"
#include <ArduinoJson.h>
#include <BLE2902.h>

// Callback per CHAR_WIFI_CONTROL
class WiFiControlCallbacks : public BLECharacteristicCallbacks {
public:
    WiFiControlCallbacks(BLEWiFiService* service) : _service(service) {}

    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            _service->_handleControlCommand(value);
        }
    }

private:
    BLEWiFiService* _service;
};

BLEWiFiService::BLEWiFiService()
    : _pCharControl(nullptr)
    , _pCharStatus(nullptr)
    , _pCharSSID(nullptr)
    , _pCharPassword(nullptr)
    , _wifiEnabled(false)
    , _wasConnected(false)
    , _lastStatusUpdate(0)
{
}

void BLEWiFiService::begin(BLEServer* pServer) {
    Serial.println("[BLE WiFi] Initializing service...");

    // Crea servizio
    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Characteristic: WiFi Control (Write)
    _pCharControl = pService->createCharacteristic(
        CHAR_WIFI_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pCharControl->setCallbacks(new WiFiControlCallbacks(this));

    // Characteristic: WiFi Status (Notify)
    _pCharStatus = pService->createCharacteristic(
        CHAR_WIFI_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _pCharStatus->addDescriptor(new BLE2902());  // Enable notifications

    // Characteristic: SSID (Read/Write)
    _pCharSSID = pService->createCharacteristic(
        CHAR_WIFI_SSID_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

    // Characteristic: Password (Write Only - sicurezza)
    _pCharPassword = pService->createCharacteristic(
        CHAR_WIFI_PASSWORD_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    // Avvia servizio
    pService->start();

    Serial.println("[BLE WiFi] Service started!");
}

void BLEWiFiService::update() {
    // Verifica cambio stato connessione
    bool connected = isConnected();
    if (connected != _wasConnected) {
        _wasConnected = connected;
        _notifyStatus();
    }

    // Notifica periodica ogni 5s se WiFi attivo
    if (_wifiEnabled && (millis() - _lastStatusUpdate > 5000)) {
        _notifyStatus();
    }
}

void BLEWiFiService::_handleControlCommand(const std::string& value) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value.c_str());

    if (error) {
        Serial.printf("[BLE WiFi] JSON parse error: %s\n", error.c_str());
        return;
    }

    String cmd = doc["cmd"].as<String>();
    Serial.printf("[BLE WiFi] Command received: %s\n", cmd.c_str());

    if (cmd == "enable") {
        _connectWiFi();
    } else if (cmd == "disable") {
        _disconnectWiFi();
    } else if (cmd == "status") {
        _notifyStatus();
    } else if (cmd == "configure") {
        _ssid = doc["ssid"].as<String>();
        _password = doc["pass"].as<String>();
        Serial.printf("[BLE WiFi] Credentials configured: SSID=%s\n", _ssid.c_str());

        // Salva in characteristic per persistenza
        _pCharSSID->setValue(_ssid.c_str());

        // Auto-connect se WiFi già abilitato
        if (_wifiEnabled) {
            _connectWiFi();
        }
    }
}

void BLEWiFiService::_connectWiFi() {
    if (_ssid.isEmpty()) {
        Serial.println("[BLE WiFi] ERROR: No SSID configured!");
        _notifyStatus();
        return;
    }

    Serial.printf("[BLE WiFi] Connecting to: %s\n", _ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    _wifiEnabled = true;

    // Timeout 15s per connessione
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 15000)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[BLE WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[BLE WiFi] Connection failed!");
    }

    _notifyStatus();
}

void BLEWiFiService::_disconnectWiFi() {
    Serial.println("[BLE WiFi] Disconnecting...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _wifiEnabled = false;
    _notifyStatus();
}

void BLEWiFiService::_notifyStatus() {
    JsonDocument doc;

    doc["enabled"] = _wifiEnabled;
    doc["connected"] = isConnected();
    doc["ssid"] = _ssid;

    if (isConnected()) {
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["url"] = "http://" + WiFi.localIP().toString();
    } else {
        doc["ip"] = "";
        doc["rssi"] = 0;
        doc["url"] = "";
    }

    String jsonStr;
    serializeJson(doc, jsonStr);

    _pCharStatus->setValue(jsonStr.c_str());
    _pCharStatus->notify();

    _lastStatusUpdate = millis();

    Serial.printf("[BLE WiFi] Status notified: %s\n", jsonStr.c_str());
}

String BLEWiFiService::getIPAddress() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "";
}
