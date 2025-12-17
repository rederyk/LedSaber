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
    doc["changedPixels"] = _motion->getChangedPixels();
    doc["direction"] = _motion->getMotionDirectionName();
    doc["motionVectorX"] = _motion->getMotionVectorX();
    doc["motionVectorY"] = _motion->getMotionVectorY();
    doc["centroidValid"] = _motion->hasValidCentroid();
    if (_motion->hasValidCentroid()) {
        doc["centroidX"] = _motion->getCentroidX();
        doc["centroidY"] = _motion->getCentroidY();
    }

    // Aggiungi gesture info
    doc["gesture"] = _motion->getGestureName();
    doc["gestureConfidence"] = _motion->getGestureConfidence();

    String output;
    serializeJson(doc, output);

    _pCharEvents->setValue(output.c_str());
    _pCharEvents->notify();

    _lastEventTime = now;

    Serial.printf("[MOTION BLE] 📢 Event notified: %s (gesture: %s, confidence: %u%%)\n",
                  eventType.c_str(), _motion->getGestureName(), _motion->getGestureConfidence());
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

    // Rileva still (fermo per 3 secondi)
    if (_motion->isStill(3000)) {
        static unsigned long lastStillNotify = 0;
        unsigned long now = millis();

        // Notifica still solo ogni 10 secondi per non spammare
        if (now - lastStillNotify > 10000) {
            notifyEvent("still_detected");
            lastStillNotify = now;
        }
    }
}

String BLEMotionService::_getStatusJson() {
    JsonDocument doc;
    MotionDetector::MotionMetrics metrics = _motion->getMetrics();

    doc["enabled"] = _motionEnabled;
    doc["motionDetected"] = _wasMotionActive;
    doc["intensity"] = _motion->getMotionIntensity();
    doc["changedPixels"] = _motion->getChangedPixels();
    doc["intensityFloor"] = metrics.intensityFloor;
    doc["shakeDetected"] = _motion->isShakeDetected();
    doc["direction"] = _motion->getMotionDirectionName();

    // Gesture recognition
    doc["gesture"] = _motion->getGestureName();
    doc["gestureConfidence"] = _motion->getGestureConfidence();

    // Proximity detection
    doc["proximity"] = _motion->getMotionProximityName();

    doc["totalFrames"] = metrics.totalFramesProcessed;
    doc["motionFrames"] = metrics.motionFrameCount;
    doc["shakeCount"] = metrics.shakeCount;
    doc["proximityBrightness"] = metrics.proximityBrightness;
    doc["centroidValid"] = metrics.centroidValid;
    doc["centroidX"] = metrics.centroidX;
    doc["centroidY"] = metrics.centroidY;
    doc["motionVectorX"] = metrics.motionVectorX;
    doc["motionVectorY"] = metrics.motionVectorY;
    doc["vectorConfidence"] = metrics.vectorConfidence;

    // Non inviamo zones via BLE (troppo pesante - 81 valori)
    // Se necessario, usare un comando separato per richiedere le zone

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
