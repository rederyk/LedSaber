#include "BLEMotionService.h"

BLEMotionService::BLEMotionService(OpticalFlowDetector* motionDetector, MotionProcessor* motionProcessor)
    : _motion(motionDetector)
    , _processor(motionProcessor)
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
    , _lastGesture(MotionProcessor::GestureType::NONE)
    , _lastGestureConfidence(0)
    , _lastGestureTime(0)
    , _motionCandidateSince(0)
    , _stillCandidateSince(0)
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

    // Debouncing: evita notifiche troppo frequenti (ridotto a 200ms per reattività)
    unsigned long now = millis();
    static unsigned long lastNotifyTime = 0;

    if (now - lastNotifyTime < 200) {
        return;  // Troppo presto, skip
    }

    lastNotifyTime = now;

    String statusJson = _getStatusJson();
    _pCharStatus->setValue(statusJson.c_str());
    _pCharStatus->notify();
}

void BLEMotionService::notifyEvent(const String& eventType, bool includeGesture) {
    if (!_eventsNotifyEnabled) {
        return;
    }

    unsigned long now = millis();

    // Debouncing: ridotto a 100ms per eventi più rapidi
    if (now - _lastEventTime < 100) {
        return;
    }

    JsonDocument doc;
    OpticalFlowDetector::Metrics metrics = _motion->getMetrics();
    doc["event"] = eventType;
    doc["timestamp"] = now;
    doc["intensity"] = metrics.currentIntensity;
    doc["direction"] = OpticalFlowDetector::directionToString(metrics.dominantDirection);
    doc["speed"] = round(metrics.avgSpeed * 10.0f) / 10.0f;
    doc["confidence"] = round(metrics.avgConfidence * 100.0f);
    doc["activeBlocks"] = metrics.avgActiveBlocks;
    doc["frameDiff"] = metrics.frameDiff;
    if (includeGesture) {
        doc["gesture"] = MotionProcessor::gestureToString(_lastGesture);
        doc["gestureConfidence"] = _lastGestureConfidence;
        doc["gestureTimestamp"] = _lastGestureTime;
    } else {
        doc["gesture"] = "none";
        doc["gestureConfidence"] = 0;
        doc["gestureTimestamp"] = 0;
    }

    String output;
    serializeJson(doc, output);

    _pCharEvents->setValue(output.c_str());
    _pCharEvents->notify();

    _lastEventTime = now;

    Serial.printf("[MOTION BLE] Event notified: %s (len=%u)\n",
                  eventType.c_str(), (unsigned)output.length());
}

void BLEMotionService::update(bool motionDetected, bool shakeDetected, const MotionProcessor::ProcessedMotion* processed) {
    if (!_motionEnabled) {
        return;
    }

    // Track last gesture from processor (if available)
    if (processed) {
        const MotionProcessor::GestureType gesture = processed->gesture;
        const uint8_t confidence = processed->gestureConfidence;
        if (gesture != MotionProcessor::GestureType::NONE && confidence > 0) {
            const unsigned long now = millis();
            const bool changed = (gesture != _lastGesture);
            _lastGesture = gesture;
            _lastGestureConfidence = confidence;
            _lastGestureTime = now;

            // Optional: emit a specific gesture event (debounced)
            if (changed && (now - _lastEventTime) >= 120) {
                notifyEvent("gesture_detected", true);
            }
        }
    }

    // Motion hysteresis to reduce rapid start/end chatter
    const unsigned long now = millis();
    static constexpr unsigned long START_STABLE_MS = 120;
    static constexpr unsigned long END_STABLE_MS = 180;

    if (motionDetected) {
        _stillCandidateSince = 0;
        if (!_wasMotionActive) {
            if (_motionCandidateSince == 0) _motionCandidateSince = now;
            if ((now - _motionCandidateSince) >= START_STABLE_MS) {
                notifyEvent("motion_started", false);
                _wasMotionActive = true;
                _motionCandidateSince = 0;
            }
        } else {
            _motionCandidateSince = 0;
        }
    } else {
        _motionCandidateSince = 0;
        if (_wasMotionActive) {
            if (_stillCandidateSince == 0) _stillCandidateSince = now;
            if ((now - _stillCandidateSince) >= END_STABLE_MS) {
                notifyEvent("motion_ended", false);
                _wasMotionActive = false;
                _stillCandidateSince = 0;
            }
        } else {
            _stillCandidateSince = 0;
        }
    }

    // Rileva shake
    if (shakeDetected && !_wasShakeDetected) {
        notifyEvent("shake_detected", false);
        _wasShakeDetected = true;
    }

    // Reset shake flag dopo 500ms
    if (_wasShakeDetected && !shakeDetected) {
        _wasShakeDetected = false;
    }
}

String BLEMotionService::_getStatusJson() {
    JsonDocument doc;
    OpticalFlowDetector::Metrics metrics = _motion->getMetrics();

    doc["enabled"] = _motionEnabled;
    doc["motionDetected"] = _wasMotionActive;
    doc["intensity"] = metrics.currentIntensity;
    doc["avgBrightness"] = metrics.avgBrightness;
    doc["flashIntensity"] = metrics.flashIntensity;
    doc["trajectoryLength"] = metrics.trajectoryLength;

    doc["totalFrames"] = metrics.totalFramesProcessed;
    doc["motionFrames"] = metrics.motionFrameCount;

    // NUOVI campi optical flow
    doc["direction"] = OpticalFlowDetector::directionToString(metrics.dominantDirection);
    doc["speed"] = round(metrics.avgSpeed * 10.0f) / 10.0f;  // 1 decimal
    doc["confidence"] = round(metrics.avgConfidence * 100.0f);  // 0-100%
    doc["activeBlocks"] = metrics.avgActiveBlocks;
    doc["computeTimeMs"] = metrics.avgComputeTimeMs;
    doc["frameDiff"] = metrics.frameDiff;

    // Gesture fields (from MotionProcessor via update()) with expiry to avoid "stuck" UI
    const unsigned long now = millis();
    static constexpr unsigned long GESTURE_TTL_MS = 700;
    if (_lastGesture != MotionProcessor::GestureType::NONE && (now - _lastGestureTime) <= GESTURE_TTL_MS) {
        doc["gesture"] = MotionProcessor::gestureToString(_lastGesture);
        doc["gestureConfidence"] = _lastGestureConfidence;
        doc["gestureTimestamp"] = _lastGestureTime;
    } else {
        doc["gesture"] = "none";
        doc["gestureConfidence"] = 0;
        doc["gestureTimestamp"] = 0;
    }

    // 8x6 grid tags for quick visualization via BLE
    doc["gridRows"] = OpticalFlowDetector::GRID_ROWS;
    doc["gridCols"] = OpticalFlowDetector::GRID_COLS;
    doc["blockSize"] = OpticalFlowDetector::BLOCK_SIZE;
    JsonArray gridArray = doc["grid"].to<JsonArray>();
    for (uint8_t row = 0; row < OpticalFlowDetector::GRID_ROWS; row++) {
        String rowStr;
        rowStr.reserve(OpticalFlowDetector::GRID_COLS);
        for (uint8_t col = 0; col < OpticalFlowDetector::GRID_COLS; col++) {
            rowStr += _motion->getBlockDirectionTag(row, col);
        }
        gridArray.add(rowStr);
    }

    // NON includere trajectory nello status (troppo grande per BLE notification)
    // La trajectory è disponibile solo tramite eventi

    String output;
    serializeJson(doc, output);
    return output;
}

String BLEMotionService::_getConfigJson() {
    JsonDocument doc;

    doc["enabled"] = _motionEnabled;
    doc["sensitivity"] = _motion->getSensitivity();

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
    uint8_t sensitivity = doc["sensitivity"] | _service->_motion->getSensitivity();

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
