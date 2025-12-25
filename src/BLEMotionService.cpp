#include "BLEMotionService.h"
#include "BLELedController.h"

extern BLELedController bleController;

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
    OpticalFlowDetector::Metrics metrics = _motion->getMetrics();
    const char* directionStr = OpticalFlowDetector::directionToString(metrics.dominantDirection);
    const char* gestureStr = includeGesture ? MotionProcessor::gestureToString(_lastGesture) : "none";
    const uint8_t gestureConfidence = includeGesture ? _lastGestureConfidence : 0;

    const unsigned long now = millis();

    auto logMotionStats = [&](const char* reason) {
        Serial.printf("[MOTION BLE] %s: %s | I:%u dir=%s spd=%.1f conf=%u%% blocks=%u diff=%u gesture=%s(%u)\n",
                      reason,
                      eventType.c_str(),
                      metrics.currentIntensity,
                      directionStr,
                      round(metrics.avgSpeed * 10.0f) / 10.0f,
                      (uint8_t)round(metrics.avgConfidence * 100.0f),
                      metrics.avgActiveBlocks,
                      metrics.frameDiff,
                      gestureStr,
                      gestureConfidence);
    };

    if (!_eventsNotifyEnabled) {
        logMotionStats("notify skipped (disabled)");
        return;
    }

    // Debouncing: ridotto a 100ms per eventi più rapidi
    if (now - _lastEventTime < 100) {
        logMotionStats("notify skipped (debounce)");
        return;
    }

    JsonDocument doc;
    doc["event"] = eventType;
    doc["timestamp"] = now;
    doc["intensity"] = metrics.currentIntensity;
    doc["direction"] = directionStr;
    doc["speed"] = round(metrics.avgSpeed * 10.0f) / 10.0f;
    doc["confidence"] = round(metrics.avgConfidence * 100.0f);
    doc["activeBlocks"] = metrics.avgActiveBlocks;
    doc["frameDiff"] = metrics.frameDiff;
    if (includeGesture) {
        doc["gesture"] = gestureStr;
        doc["gestureConfidence"] = gestureConfidence;
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
    logMotionStats("notify sent");
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

    auto rotateDirectionCW = [](OpticalFlowDetector::Direction dir, uint16_t degrees) -> OpticalFlowDetector::Direction {
        switch (degrees % 360) {
            case 0:
                return dir;
            case 90:
                switch (dir) {
                    case OpticalFlowDetector::Direction::UP:         return OpticalFlowDetector::Direction::RIGHT;
                    case OpticalFlowDetector::Direction::UP_RIGHT:   return OpticalFlowDetector::Direction::DOWN_RIGHT;
                    case OpticalFlowDetector::Direction::RIGHT:      return OpticalFlowDetector::Direction::DOWN;
                    case OpticalFlowDetector::Direction::DOWN_RIGHT: return OpticalFlowDetector::Direction::DOWN_LEFT;
                    case OpticalFlowDetector::Direction::DOWN:       return OpticalFlowDetector::Direction::LEFT;
                    case OpticalFlowDetector::Direction::DOWN_LEFT:  return OpticalFlowDetector::Direction::UP_LEFT;
                    case OpticalFlowDetector::Direction::LEFT:       return OpticalFlowDetector::Direction::UP;
                    case OpticalFlowDetector::Direction::UP_LEFT:    return OpticalFlowDetector::Direction::UP_RIGHT;
                    default: return dir;
                }
            case 180:
                switch (dir) {
                    case OpticalFlowDetector::Direction::UP:         return OpticalFlowDetector::Direction::DOWN;
                    case OpticalFlowDetector::Direction::UP_RIGHT:   return OpticalFlowDetector::Direction::DOWN_LEFT;
                    case OpticalFlowDetector::Direction::RIGHT:      return OpticalFlowDetector::Direction::LEFT;
                    case OpticalFlowDetector::Direction::DOWN_RIGHT: return OpticalFlowDetector::Direction::UP_LEFT;
                    case OpticalFlowDetector::Direction::DOWN:       return OpticalFlowDetector::Direction::UP;
                    case OpticalFlowDetector::Direction::DOWN_LEFT:  return OpticalFlowDetector::Direction::UP_RIGHT;
                    case OpticalFlowDetector::Direction::LEFT:       return OpticalFlowDetector::Direction::RIGHT;
                    case OpticalFlowDetector::Direction::UP_LEFT:    return OpticalFlowDetector::Direction::DOWN_RIGHT;
                    default: return dir;
                }
            case 270:
                switch (dir) {
                    case OpticalFlowDetector::Direction::UP:         return OpticalFlowDetector::Direction::LEFT;
                    case OpticalFlowDetector::Direction::UP_RIGHT:   return OpticalFlowDetector::Direction::UP_LEFT;
                    case OpticalFlowDetector::Direction::RIGHT:      return OpticalFlowDetector::Direction::UP;
                    case OpticalFlowDetector::Direction::DOWN_RIGHT: return OpticalFlowDetector::Direction::UP_RIGHT;
                    case OpticalFlowDetector::Direction::DOWN:       return OpticalFlowDetector::Direction::RIGHT;
                    case OpticalFlowDetector::Direction::DOWN_LEFT:  return OpticalFlowDetector::Direction::DOWN_RIGHT;
                    case OpticalFlowDetector::Direction::LEFT:       return OpticalFlowDetector::Direction::DOWN;
                    case OpticalFlowDetector::Direction::UP_LEFT:    return OpticalFlowDetector::Direction::DOWN_LEFT;
                    default: return dir;
                }
            default:
                return dir;
        }
    };

    auto rotateTagCW = [](char tag, uint16_t degrees) -> char {
        switch (degrees % 360) {
            case 0:
                return tag;
            case 90:
                switch (tag) {
                    case '^': return '>';
                    case '>': return 'v';
                    case 'v': return '<';
                    case '<': return '^';
                    case 'A': return 'C'; // up-right -> down-right
                    case 'C': return 'D'; // down-right -> down-left
                    case 'D': return 'B'; // down-left -> up-left
                    case 'B': return 'A'; // up-left -> up-right
                    default: return tag;
                }
            case 180:
                switch (tag) {
                    case '^': return 'v';
                    case '>': return '<';
                    case 'v': return '^';
                    case '<': return '>';
                    case 'A': return 'D'; // up-right -> down-left
                    case 'C': return 'B'; // down-right -> up-left
                    case 'D': return 'A'; // down-left -> up-right
                    case 'B': return 'C'; // up-left -> down-right
                    default: return tag;
                }
            case 270:
                switch (tag) {
                    case '^': return '<';
                    case '>': return '^';
                    case 'v': return '>';
                    case '<': return 'v';
                    case 'A': return 'B'; // up-right -> up-left
                    case 'C': return 'A'; // down-right -> up-right
                    case 'D': return 'C'; // down-left -> down-right
                    case 'B': return 'D'; // up-left -> down-left
                    default: return tag;
                }
            default:
                return tag;
        }
    };

    doc["enabled"] = _motionEnabled;
    doc["motionDetected"] = _wasMotionActive;
    doc["quality"] = _motion->getQuality();
    doc["intensity"] = metrics.currentIntensity;
    doc["avgBrightness"] = metrics.avgBrightness;
    doc["flashIntensity"] = metrics.flashIntensity;
    doc["trajectoryLength"] = metrics.trajectoryLength;

    doc["totalFrames"] = metrics.totalFramesProcessed;
    doc["motionFrames"] = metrics.motionFrameCount;

    // NUOVI campi optical flow
    // Rotate for display to match gesture reference (main.cpp rotates motion direction before gesture processing)
    doc["direction"] = OpticalFlowDetector::directionToString(rotateDirectionCW(metrics.dominantDirection, 180));
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

    // 6x6 grid tags for quick visualization via BLE
    doc["gridRows"] = OpticalFlowDetector::GRID_ROWS;
    doc["gridCols"] = OpticalFlowDetector::GRID_COLS;
    doc["blockSize"] = OpticalFlowDetector::BLOCK_SIZE;
    JsonArray gridArray = doc["grid"].to<JsonArray>();
    for (uint8_t row = 0; row < OpticalFlowDetector::GRID_ROWS; row++) {
        String rowStr;
        rowStr.reserve(OpticalFlowDetector::GRID_COLS);
        for (uint8_t col = 0; col < OpticalFlowDetector::GRID_COLS; col++) {
            const char tag = _motion->getBlockDirectionTag(row, col);
            rowStr += rotateTagCW(tag, 0);
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
    doc["quality"] = _motion->getQuality();
    doc["motionIntensityMin"] = _motion->getMotionIntensityThreshold();
    doc["motionSpeedMin"] = _motion->getMotionSpeedThreshold();
    if (_processor) {
        const MotionProcessor::Config& cfg = _processor->getConfig();
        doc["gestureIgnitionIntensity"] = cfg.ignitionIntensityThreshold;
        doc["gestureRetractIntensity"] = cfg.retractIntensityThreshold;
        doc["gestureClashIntensity"] = cfg.clashIntensityThreshold;
        doc["debugLogs"] = cfg.debugLogsEnabled;
    }

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

    } else if (command.startsWith("quality ")) {
        // Comando: "quality 128"
        int quality = command.substring(8).toInt();
        if (quality >= 0 && quality <= 255) {
            _motion->setQuality((uint8_t)quality);
            Serial.printf("[MOTION BLE] ✓ Quality set: %d\n", quality);
        } else {
            Serial.printf("[MOTION BLE] ✗ Invalid quality: %d (must be 0-255)\n", quality);
        }

    } else if (command.startsWith("motionmin ")) {
        int minIntensity = command.substring(10).toInt();
        if (minIntensity >= 0 && minIntensity <= 255) {
            _motion->setMotionIntensityThreshold((uint8_t)minIntensity);
            Serial.printf("[MOTION BLE] ✓ Motion intensity min set: %d\n", minIntensity);
        } else {
            Serial.printf("[MOTION BLE] ✗ Invalid motionmin: %d (must be 0-255)\n", minIntensity);
        }

    } else if (command.startsWith("speedmin ")) {
        float minSpeed = command.substring(9).toFloat();
        if (minSpeed >= 0.0f && minSpeed <= 20.0f) {
            _motion->setMotionSpeedThreshold(minSpeed);
            Serial.printf("[MOTION BLE] ✓ Motion speed min set: %.2f\n", minSpeed);
        } else {
            Serial.printf("[MOTION BLE] ✗ Invalid speedmin: %.2f (must be 0-20)\n", minSpeed);
        }

    } else {
        Serial.printf("[MOTION BLE] ✗ Unknown command: %s\n", command.c_str());
    }
    bleController.setConfigDirty(true);

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

    // Parse JSON payload: {"enabled": bool, "quality": 0-255,
    // "motionIntensityMin": 0-255, "motionSpeedMin": 0-20,
    // "gestureIgnitionIntensity": 0-255, "gestureRetractIntensity": 0-255,
    // "gestureClashIntensity": 0-255, "debugLogs": bool}
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value.c_str());

    if (error) {
        Serial.printf("[MOTION BLE] Config JSON parse error: %s\n", error.c_str());
        return;
    }

    bool enabled = doc["enabled"] | _service->_motionEnabled;
    const bool hasQuality = !doc["quality"].isNull();
    const bool hasMotionIntensity = !doc["motionIntensityMin"].isNull();
    const bool hasMotionSpeed = !doc["motionSpeedMin"].isNull();
    const bool hasIgnitionIntensity = !doc["gestureIgnitionIntensity"].isNull();
    const bool hasRetractIntensity = !doc["gestureRetractIntensity"].isNull();
    const bool hasClashIntensity = !doc["gestureClashIntensity"].isNull();
    const bool hasDebugLogs = !doc["debugLogs"].isNull();

    uint8_t quality = doc["quality"] | _service->_motion->getQuality();
    uint8_t motionIntensityMin = doc["motionIntensityMin"] | _service->_motion->getMotionIntensityThreshold();
    float motionSpeedMin = doc["motionSpeedMin"] | _service->_motion->getMotionSpeedThreshold();

    Serial.printf("[MOTION BLE] Config update: enabled=%s, quality=%u, motionMin=%u, speedMin=%.2f\n",
                  enabled ? "true" : "false", quality, motionIntensityMin, motionSpeedMin);

    _service->_motionEnabled = enabled;
    if (hasQuality) {
        _service->_motion->setQuality(quality);
    }
    if (hasMotionIntensity) {
        _service->_motion->setMotionIntensityThreshold(motionIntensityMin);
    }
    if (hasMotionSpeed) {
        _service->_motion->setMotionSpeedThreshold(motionSpeedMin);
    }
    if (_service->_processor &&
        (hasIgnitionIntensity || hasRetractIntensity || hasClashIntensity || hasDebugLogs)) {
        MotionProcessor::Config cfg = _service->_processor->getConfig();
        if (hasIgnitionIntensity) {
            cfg.ignitionIntensityThreshold = (uint8_t)constrain((int)doc["gestureIgnitionIntensity"], 0, 255);
        }
        if (hasRetractIntensity) {
            cfg.retractIntensityThreshold = (uint8_t)constrain((int)doc["gestureRetractIntensity"], 0, 255);
        }
        if (hasClashIntensity) {
            cfg.clashIntensityThreshold = (uint8_t)constrain((int)doc["gestureClashIntensity"], 0, 255);
        }
        if (hasDebugLogs) {
            cfg.debugLogsEnabled = (bool)doc["debugLogs"];
        }
        _service->_processor->setConfig(cfg);
        Serial.printf("[MOTION BLE] Gesture update: ignition=%u retract=%u clash=%u\n",
                      cfg.ignitionIntensityThreshold,
                      cfg.retractIntensityThreshold,
                      cfg.clashIntensityThreshold);
    }
    bleController.setConfigDirty(true);

    // Aggiorna characteristic con nuovo stato
    String response = _service->_getConfigJson();
    pCharacteristic->setValue(response.c_str());

    // Notifica stato aggiornato
    _service->notifyStatus();
}
