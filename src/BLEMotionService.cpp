#include "BLEMotionService.h"

BLEMotionService::BLEMotionService(MotionDetector* motionDetector)
    : _motion(motionDetector)
    , _pService(nullptr)
    , _pCharStatus(nullptr)
    , _pCharControl(nullptr)
    , _pCharEvents(nullptr)
    , _pCharConfig(nullptr)
    , _statusNotifyEnabled(false)
    , _eventsNotifyEnabled(false)
    , _motionEnabled(false)
    , _wasMotionActive(false)
    , _wasShakeDetected(false)
    , _lastEventTime(0)
{
}

void BLEMotionService::begin(BLEServer* pServer) {
    Serial.println("[MOTION BLE] Creating Motion Service...");

    // Crea servizio
    _pService = pServer->createService(MOTION_SERVICE_UUID);

    // Characteristic STATUS (Read + Notify)
    _pCharStatus = _pService->createCharacteristic(
        CHAR_MOTION_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    BLE2902* pStatusDescriptor = new BLE2902();
    pStatusDescriptor->setCallbacks(new StatusDescriptorCallbacks(this));
    _pCharStatus->addDescriptor(pStatusDescriptor);
    _pCharStatus->setValue("{}");

    // Characteristic CONTROL (Write)
    _pCharControl = _pService->createCharacteristic(
        CHAR_MOTION_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pCharControl->setCallbacks(new ControlCallbacks(this));

    // Characteristic EVENTS (Notify)
    _pCharEvents = _pService->createCharacteristic(
        CHAR_MOTION_EVENTS_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    BLE2902* pEventsDescriptor = new BLE2902();
    pEventsDescriptor->setCallbacks(new EventsDescriptorCallbacks(this));
    _pCharEvents->addDescriptor(pEventsDescriptor);
    _pCharEvents->setValue("{}");

    // Characteristic CONFIG (Read + Write)
    _pCharConfig = _pService->createCharacteristic(
        CHAR_MOTION_CONFIG_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _pCharConfig->setCallbacks(new ConfigCallbacks(this));
    _pCharConfig->setValue(_getConfigJson().c_str());

    // Avvia servizio
    _pService->start();

    Serial.println("[MOTION BLE] ✓ Motion Service started");
}

void BLEMotionService::notifyStatus() {
    if (!_statusNotifyEnabled) {
        return;
    }

    // Debouncing: evita notifiche troppo frequenti (max 1 ogni 500ms)
    unsigned long now = millis();
    static unsigned long lastNotifyTime = 0;

    if (now - lastNotifyTime < 500) {
        return;  // Troppo presto, skip
    }

    lastNotifyTime = now;

    String statusJson = _getStatusJson();
    _pCharStatus->setValue(statusJson.c_str());
    _pCharStatus->notify();
}

void BLEMotionService::notifyEvent(const String& eventType) {
    if (!_eventsNotifyEnabled) {
        return;
    }

    unsigned long now = millis();

    // Debouncing: max 1 evento ogni 200ms
    if (now - _lastEventTime < 200) {
        return;
    }

    JsonDocument doc;
    doc["event"] = eventType;
    doc["timestamp"] = now;
    doc["intensity"] = _motion->getMotionIntensity();

    // Aggiungi traiettoria se presente
    MotionDetector::TrajectoryPoint trajectory[MotionDetector::MAX_TRAJECTORY_POINTS];
    uint8_t points = _motion->getTrajectory(trajectory);

    if (points > 0) {
        JsonArray trajArray = doc["trajectory"].to<JsonArray>();
        for (uint8_t i = 0; i < points; i++) {
            JsonObject point = trajArray.add<JsonObject>();
            point["x"] = trajectory[i].x;
            point["y"] = trajectory[i].y;
        }
    }

    String output;
    serializeJson(doc, output);

    _pCharEvents->setValue(output.c_str());
    _pCharEvents->notify();

    _lastEventTime = now;

    Serial.printf("[MOTION BLE] Event notified: %s (trajectory: %u points)\n",
                  eventType.c_str(), points);
}

void BLEMotionService::update(bool motionDetected, bool shakeDetected) {
    if (!_motionEnabled) {
        return;
    }

    // Rileva inizio movimento
    if (motionDetected && !_wasMotionActive) {
        notifyEvent("motion_started");
        _wasMotionActive = true;
    }

    // Rileva fine movimento
    if (!motionDetected && _wasMotionActive) {
        notifyEvent("motion_ended");
        _wasMotionActive = false;
    }

    // Rileva shake
    if (shakeDetected && !_wasShakeDetected) {
        notifyEvent("shake_detected");
        _wasShakeDetected = true;
    }

    // Reset shake flag dopo 500ms
    if (_wasShakeDetected && !shakeDetected) {
        _wasShakeDetected = false;
    }
}

String BLEMotionService::_getStatusJson() {
    JsonDocument doc;
    MotionDetector::Metrics metrics = _motion->getMetrics();

    doc["enabled"] = _motionEnabled;
    doc["motionDetected"] = _wasMotionActive;
    doc["intensity"] = metrics.currentIntensity;
    doc["avgBrightness"] = metrics.avgBrightness;
    doc["flashIntensity"] = metrics.flashIntensity;
    doc["trajectoryLength"] = metrics.trajectoryLength;

    doc["totalFrames"] = metrics.totalFramesProcessed;
    doc["motionFrames"] = metrics.motionFrameCount;

    // NON includere trajectory nello status (troppo grande per BLE notification)
    // La trajectory è disponibile solo tramite eventi

    String output;
    serializeJson(doc, output);
    return output;
}

String BLEMotionService::_getConfigJson() {
    JsonDocument doc;

    doc["enabled"] = _motionEnabled;
    // Note: threshold non esposti direttamente, usare sensitivity
    doc["sensitivity"] = 128;  // Default medio

    String output;
    serializeJson(doc, output);
    return output;
}

void BLEMotionService::_executeCommand(const String& command) {
    Serial.printf("[MOTION BLE] Command received: %s\n", command.c_str());

    if (command == "enable") {
        _motionEnabled = true;
        Serial.println("[MOTION BLE] ✓ Motion detection enabled");

    } else if (command == "disable") {
        _motionEnabled = false;
        Serial.println("[MOTION BLE] ✓ Motion detection disabled");

    } else if (command == "reset") {
        _motion->reset();
        _wasMotionActive = false;
        _wasShakeDetected = false;
        Serial.println("[MOTION BLE] ✓ Motion detector reset");

    } else if (command.startsWith("sensitivity ")) {
        // Comando: "sensitivity 128"
        int sensitivity = command.substring(12).toInt();
        if (sensitivity >= 0 && sensitivity <= 255) {
            _motion->setSensitivity((uint8_t)sensitivity);
            Serial.printf("[MOTION BLE] ✓ Sensitivity set: %d\n", sensitivity);
        } else {
            Serial.printf("[MOTION BLE] ✗ Invalid sensitivity: %d (must be 0-255)\n", sensitivity);
        }

    } else {
        Serial.printf("[MOTION BLE] ✗ Unknown command: %s\n", command.c_str());
    }

    // Aggiorna config characteristic
    _pCharConfig->setValue(_getConfigJson().c_str());

    // Notifica stato aggiornato
    notifyStatus();
}

// ============================================================================
// CALLBACKS
// ============================================================================

void BLEMotionService::ControlCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        String command = String(value.c_str());
        _service->_executeCommand(command);
    }
}

void BLEMotionService::ConfigCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) return;

    // Parse JSON payload: {"enabled": bool, "sensitivity": 0-255}
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value.c_str());

    if (error) {
        Serial.printf("[MOTION BLE] Config JSON parse error: %s\n", error.c_str());
        return;
    }

    bool enabled = doc["enabled"] | _service->_motionEnabled;
    uint8_t sensitivity = doc["sensitivity"] | 128;

    Serial.printf("[MOTION BLE] Config update: enabled=%s, sensitivity=%u\n",
                  enabled ? "true" : "false", sensitivity);

    _service->_motionEnabled = enabled;
    _service->_motion->setSensitivity(sensitivity);

    // Aggiorna characteristic con nuovo stato
    String response = _service->_getConfigJson();
    pCharacteristic->setValue(response.c_str());

    // Notifica stato aggiornato
    _service->notifyStatus();
}
