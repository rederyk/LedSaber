#include "BLECameraService.h"

BLECameraService::BLECameraService(CameraManager* cameraManager)
    : _camera(cameraManager)
    , _pService(nullptr)
    , _pCharStatus(nullptr)
    , _pCharControl(nullptr)
    , _pCharMetrics(nullptr)
    , _pCharFlash(nullptr)
    , _statusNotifyEnabled(false)
    , _cameraActive(false)
{
}

void BLECameraService::begin(BLEServer* pServer) {
    Serial.println("[CAM BLE] Creating Camera Service...");

    // Crea servizio
    _pService = pServer->createService(CAMERA_SERVICE_UUID);

    // Characteristic STATUS (Read + Notify)
    _pCharStatus = _pService->createCharacteristic(
        CHAR_CAMERA_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    BLE2902* pDescriptor = new BLE2902();
    pDescriptor->setCallbacks(new StatusDescriptorCallbacks(this));
    _pCharStatus->addDescriptor(pDescriptor);
    _pCharStatus->setValue("{}");

    // Characteristic CONTROL (Write)
    _pCharControl = _pService->createCharacteristic(
        CHAR_CAMERA_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pCharControl->setCallbacks(new ControlCallbacks(this));

    // Characteristic METRICS (Read)
    _pCharMetrics = _pService->createCharacteristic(
        CHAR_CAMERA_METRICS_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    _pCharMetrics->setValue("{}");

    // Characteristic FLASH (Read + Write)
    _pCharFlash = _pService->createCharacteristic(
        CHAR_CAMERA_FLASH_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pCharFlash->setCallbacks(new FlashCallbacks(this));
    _pCharFlash->setValue("{\"enabled\":false,\"brightness\":0}");

    // Avvia servizio
    _pService->start();

    Serial.println("[CAM BLE] ✓ Camera Service started");
}

void BLECameraService::notifyStatus() {
    if (!_statusNotifyEnabled) {
        return;
    }

    String statusJson = _getStatusJson();
    _pCharStatus->setValue(statusJson.c_str());
    _pCharStatus->notify();
}

void BLECameraService::updateMetrics() {
    String metricsJson = _getMetricsJson();
    _pCharMetrics->setValue(metricsJson.c_str());
}

void BLECameraService::setCameraActive(bool active) {
    if (_cameraActive == active) {
        return;
    }

    _cameraActive = active;
    Serial.printf("[CAM BLE] Continuous capture %s\n", active ? "started" : "stopped");
    notifyStatus();
}

String BLECameraService::_getStatusJson() {
    JsonDocument doc;

    doc["initialized"] = _camera->isInitialized();
    doc["active"] = _cameraActive;

    CameraManager::CameraMetrics metrics = _camera->getMetrics();
    doc["fps"] = metrics.currentFps;
    doc["totalFrames"] = metrics.totalFramesCaptured;
    doc["failedCaptures"] = metrics.failedCaptures;

    String output;
    serializeJson(doc, output);
    return output;
}

String BLECameraService::_getMetricsJson() {
    JsonDocument doc;

    CameraManager::CameraMetrics metrics = _camera->getMetrics();
    doc["totalFramesCaptured"] = metrics.totalFramesCaptured;
    doc["failedCaptures"] = metrics.failedCaptures;
    doc["lastFrameSize"] = metrics.lastFrameSize;
    doc["lastCaptureTime"] = metrics.lastCaptureTime;
    doc["currentFps"] = metrics.currentFps;

    // Aggiungi info memoria
    doc["heapFree"] = ESP.getFreeHeap();
    doc["psramTotal"] = ESP.getPsramSize();
    doc["psramFree"] = ESP.getFreePsram();

    String output;
    serializeJson(doc, output);
    return output;
}

void BLECameraService::_executeCommand(const String& command) {
    Serial.printf("[CAM BLE] Command received: %s\n", command.c_str());

    if (command == "init") {
        // Inizializza camera
        if (!_camera->isInitialized()) {
            if (_camera->begin(4)) {
                Serial.println("[CAM BLE] ✓ Camera initialized");
                _cameraActive = false;
            } else {
                Serial.println("[CAM BLE] ✗ Camera init failed");
            }
        } else {
            Serial.println("[CAM BLE] Camera already initialized");
            // Assicurati che lo stato sia corretto anche se già inizializzata
            _cameraActive = false;
        }

    } else if (command == "capture") {
        // Test capture singolo
        if (_cameraActive) {
            Serial.println("[CAM BLE] ✗ Cannot capture while continuous mode is active");
        } else if (_camera->isInitialized()) {
            uint8_t* buffer = nullptr;
            size_t length = 0;

            if (_camera->captureFrame(&buffer, &length)) {
                Serial.printf("[CAM BLE] ✓ Frame captured: %u bytes\n", length);
                _camera->releaseFrame();
            } else {
                Serial.println("[CAM BLE] ✗ Frame capture failed");
            }
        } else {
            Serial.println("[CAM BLE] Camera not initialized!");
        }

    } else if (command == "start") {
        // Avvia cattura continua
        _cameraActive = true;
        Serial.println("[CAM BLE] ✓ Continuous capture started");

    } else if (command == "stop") {
        // Ferma cattura continua
        _cameraActive = false;
        Serial.println("[CAM BLE] ✓ Continuous capture stopped");

    } else if (command == "reset_metrics") {
        // Reset metriche
        _camera->resetMetrics();
        Serial.println("[CAM BLE] ✓ Metrics reset");

    } else {
        Serial.printf("[CAM BLE] ✗ Unknown command: %s\n", command.c_str());
    }

    // Notifica stato aggiornato
    notifyStatus();
}

// ============================================================================
// CALLBACKS
// ============================================================================

void BLECameraService::ControlCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        String command = String(value.c_str());
        _service->_executeCommand(command);
    }
}

void BLECameraService::FlashCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) return;

    // Parse JSON payload: {"enabled": bool, "brightness": 0-255}
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value.c_str());

    if (error) {
        Serial.printf("[CAM BLE] Flash JSON parse error: %s\n", error.c_str());
        return;
    }

    bool enabled = doc["enabled"] | false;
    uint8_t brightness = doc["brightness"] | 0;

    Serial.printf("[CAM BLE] Flash: %s (brightness: %u)\n",
                  enabled ? "ON" : "OFF", brightness);

    _service->_camera->setFlash(enabled, brightness);

    // Aggiorna characteristic con nuovo stato
    String response;
    serializeJson(doc, response);
    pCharacteristic->setValue(response.c_str());
}
