#include "MotionProcessor.h"
#include <cmath>

MotionProcessor::MotionProcessor() :
    _lastMotionTime(0),
    _lastIntensity(0),
    _lastDirection(OpticalFlowDetector::Direction::NONE),
    _directionStartTime(0),
    _gestureCooldown(false),
    _gestureCooldownEnd(0),
    _clashCooldownEnd(0),
    _lastGestureConfidence(0)
{
}

MotionProcessor::ProcessedMotion MotionProcessor::process(
    uint8_t motionIntensity,
    OpticalFlowDetector::Direction direction,
    float speed,
    uint32_t timestamp,
    const OpticalFlowDetector& detector)
{
    ProcessedMotion result;
    result.motionIntensity = motionIntensity;
    result.direction = direction;
    result.speed = speed;
    result.timestamp = timestamp;
    result.gesture = GestureType::NONE;
    result.gestureConfidence = 0;

    // Calculate perturbation grid
    if (_config.perturbationEnabled) {
        _calculatePerturbationGrid(detector, result.perturbationGrid);
    } else {
        memset(result.perturbationGrid, 0, sizeof(result.perturbationGrid));
    }

    // Detect gestures
    if (_config.gesturesEnabled) {
        result.gesture = _detectGesture(motionIntensity, direction, speed, timestamp);
        result.gestureConfidence = (result.gesture != GestureType::NONE) ? _lastGestureConfidence : 0;
    }

    return result;
}

MotionProcessor::GestureType MotionProcessor::_detectGesture(
    uint8_t intensity,
    OpticalFlowDetector::Direction direction,
    float speed,
    uint32_t timestamp)
{
    _lastGestureConfidence = 0;

    auto max16 = [](uint16_t a, uint16_t b) { return (a > b) ? a : b; };
    auto min16 = [](uint16_t a, uint16_t b) { return (a < b) ? a : b; };

    // Per-gesture thresholds tuned for current optical flow noise/profile
    const uint8_t ignitionIntensityThreshold = (uint8_t)max(16, (int)_config.gestureThreshold + 4);
    const uint16_t ignitionDurationThreshold = max16((uint16_t)140, (uint16_t)(_config.gestureDurationMs + 60));
    const float ignitionSpeedThreshold = 0.9f;

    const uint8_t retractIntensityThreshold = (uint8_t)max(14, (int)_config.gestureThreshold + 4);   // Cleaner gesture
    const uint16_t retractDurationThreshold = max16((uint16_t)120, (uint16_t)(_config.gestureDurationMs + 40));
    const float retractSpeedThreshold = 0.9f;

    const uint8_t clashIntensityThreshold = (uint8_t)max(12, (int)_config.gestureThreshold);
    const uint16_t clashCooldown = max16(_config.clashCooldownMs, (uint16_t)500);

    // Cooldown management: prevent gesture spam
    if (_gestureCooldown) {
        if (timestamp < _gestureCooldownEnd) {
            return GestureType::NONE;
        }
        _gestureCooldown = false;
    }

    // Intensity threshold check (use the lowest meaningful threshold)
    uint8_t baseThreshold = retractIntensityThreshold;
    baseThreshold = min(baseThreshold, clashIntensityThreshold);
    if (intensity < baseThreshold) {
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;
        return GestureType::NONE;
    }

    // CLASH detection: sudden spike in intensity
    // Check for large delta in a window suitable for low FPS (default: 350ms)
    const bool clashOnCooldown = (timestamp < _clashCooldownEnd);
    if (!clashOnCooldown &&
        _lastMotionTime > 0 &&
        (timestamp - _lastMotionTime) <= _config.clashWindowMs &&
        intensity >= clashIntensityThreshold)
    {
        int16_t deltaIntensity = intensity - _lastIntensity;
        if (deltaIntensity > _config.clashDeltaThreshold) {
            // Clash detected!
            _gestureCooldown = true;
            uint16_t cooldownHalf = (uint16_t)(_config.gestureCooldownMs / 2);
            if (cooldownHalf < 250) cooldownHalf = 250;
            const uint16_t appliedCooldown = max16(cooldownHalf, clashCooldown);
            _gestureCooldownEnd = timestamp + appliedCooldown;
            _clashCooldownEnd = timestamp + clashCooldown;

            _lastIntensity = intensity;
            _lastDirection = direction;
            _lastMotionTime = timestamp;

            // Confidence: più delta = più confidence
            int16_t over = deltaIntensity - _config.clashDeltaThreshold;
            int conf = 50 + (over * 3);
            if (conf < 0) conf = 0;
            if (conf > 100) conf = 100;
            _lastGestureConfidence = (uint8_t)conf;

            Serial.printf("[MOTION] CLASH detected! (delta=%d)\n", deltaIntensity);
            return GestureType::CLASH;
        }
    }

    // Sustained direction tracking (use the smallest per-gesture requirement to start counting)
    uint16_t sustainThreshold = min16(min16(retractDurationThreshold, ignitionDurationThreshold), _config.gestureDurationMs);
    bool isSustained = _isSustainedDirection(direction, timestamp, sustainThreshold);

    if (!isSustained) {
        // Direction just changed or started
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;
        return GestureType::NONE;
    }

    // At this point: high intensity + sustained direction
    uint32_t duration = timestamp - _directionStartTime;

    const bool directionUpClean = (direction == OpticalFlowDetector::Direction::UP);
    const bool directionUpLoose = (direction == OpticalFlowDetector::Direction::UP_LEFT ||
                                   direction == OpticalFlowDetector::Direction::UP_RIGHT);

    const bool directionDownClean = (direction == OpticalFlowDetector::Direction::DOWN);
    const bool directionDownLoose = (direction == OpticalFlowDetector::Direction::DOWN_LEFT ||
                                     direction == OpticalFlowDetector::Direction::DOWN_RIGHT);

    // IGNITION: UP veloce e sostenuto
    if ((directionUpClean || directionUpLoose) &&
        speed >= ignitionSpeedThreshold &&
        intensity >= ignitionIntensityThreshold &&
        duration >= ignitionDurationThreshold &&
        (directionUpClean || duration >= (ignitionDurationThreshold + 40)))
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;

        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

        float speedScore = speed / 2.0f;
        if (speedScore > 1.0f) speedScore = 1.0f;
        float intScore = (float)(intensity - ignitionIntensityThreshold) / 60.0f;
        if (intScore > 1.0f) intScore = 1.0f;
        float durScore = (float)duration / (float)max16((uint16_t)300, ignitionDurationThreshold);
        if (durScore > 1.0f) durScore = 1.0f;
        int conf = (int)roundf((0.30f * speedScore + 0.30f * intScore + 0.40f * durScore) * 100.0f);
        if (conf < 0) conf = 0;
        if (conf > 100) conf = 100;
        _lastGestureConfidence = (uint8_t)conf;

        Serial.printf("[MOTION] IGNITION detected! (speed=%.1f, duration=%ums)\n", speed, duration);
        return GestureType::IGNITION;
    }

    // RETRACT: DOWN veloce e sostenuto
    if ((directionDownClean || directionDownLoose) &&
        speed >= retractSpeedThreshold &&
        intensity >= retractIntensityThreshold &&
        duration >= retractDurationThreshold &&
        (directionDownClean || duration >= (retractDurationThreshold + 40)))
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;

        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

        float speedScore = speed / 2.0f;
        if (speedScore > 1.0f) speedScore = 1.0f;
        float intScore = (float)(intensity - retractIntensityThreshold) / 60.0f;
        if (intScore > 1.0f) intScore = 1.0f;
        float durScore = (float)duration / (float)max16((uint16_t)250, retractDurationThreshold);
        if (durScore > 1.0f) durScore = 1.0f;
        int conf = (int)roundf((0.30f * speedScore + 0.30f * intScore + 0.40f * durScore) * 100.0f);
        if (conf < 0) conf = 0;
        if (conf > 100) conf = 100;
        _lastGestureConfidence = (uint8_t)conf;

        Serial.printf("[MOTION] RETRACT detected! (speed=%.1f, duration=%ums)\n", speed, duration);
        return GestureType::RETRACT;
    }

    // SWING: LEFT/RIGHT veloce
    // Include diagonali per catturare movimenti circolari (mano vicina alla camera)
    if ((direction == OpticalFlowDetector::Direction::LEFT ||
         direction == OpticalFlowDetector::Direction::RIGHT ||
         direction == OpticalFlowDetector::Direction::UP_LEFT ||
         direction == OpticalFlowDetector::Direction::UP_RIGHT ||
         direction == OpticalFlowDetector::Direction::DOWN_LEFT ||
         direction == OpticalFlowDetector::Direction::DOWN_RIGHT) &&
        intensity >= (uint8_t)max(8, (int)_config.gestureThreshold - 4) &&
        speed > 0.5f)
    {
        // Note: SWING non ha cooldown lungo, può ripetersi
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

        float speedScore = speed / 2.0f;
        if (speedScore > 1.0f) speedScore = 1.0f;
        uint8_t swingThreshold = (uint8_t)max(1, (int)_config.gestureThreshold - 4);
        float intScore = (float)(intensity - swingThreshold) / 50.0f;
        if (intScore > 1.0f) intScore = 1.0f;
        int conf = (int)roundf((0.55f * speedScore + 0.45f * intScore) * 100.0f);
        if (conf < 0) conf = 0;
        if (conf > 100) conf = 100;
        _lastGestureConfidence = (uint8_t)conf;

        Serial.printf("[MOTION] SWING detected! (dir=%s, speed=%.1f)\n",
                     OpticalFlowDetector::directionToString(direction), speed);
        return GestureType::SWING;
    }

    // No gesture detected
    _lastIntensity = intensity;
    _lastDirection = direction;
    _lastMotionTime = timestamp;

    return GestureType::NONE;
}

bool MotionProcessor::_isSustainedDirection(
    OpticalFlowDetector::Direction direction,
    uint32_t timestamp,
    uint16_t minDurationMs)
{
    if (direction == OpticalFlowDetector::Direction::NONE) {
        _directionStartTime = 0;
        return false;
    }

    if (direction != _lastDirection) {
        // Direction changed
        _directionStartTime = timestamp;
        return false;
    }

    // Same direction as last frame
    if (_directionStartTime == 0) {
        _directionStartTime = timestamp;
        return false;
    }

    // Check duration
    uint32_t duration = timestamp - _directionStartTime;
    return duration >= minDurationMs;
}

void MotionProcessor::_calculatePerturbationGrid(
    const OpticalFlowDetector& detector,
    uint8_t perturbationGrid[6][8])
{
    for (uint8_t row = 0; row < 6; row++) {
        for (uint8_t col = 0; col < 8; col++) {
            int8_t dx, dy;
            uint8_t confidence;

            if (detector.getBlockVector(row, col, &dx, &dy, &confidence)) {
                // Calculate magnitude of motion vector
                float magnitude = sqrtf(dx * dx + dy * dy);

                // AMPLIFIED SCALING: Motion vectors are small (±1-2 px typical at 5fps)
                // Old: magnitude / 20.0f = troppo conservativo
                // New: magnitude / 3.0f = molto più sensibile
                float normalized = magnitude / 3.0f;  // 0.0-3.0+ range
                if (normalized > 1.0f) normalized = 1.0f;

                // Apply confidence weighting (but boost it)
                // Confidence è spesso basso con optical flow grossolano
                float confidenceBoost = (confidence / 255.0f) * 0.5f + 0.5f;  // Min 50%
                normalized *= confidenceBoost;

                // Apply config scale (default 128 = 50%, ma lo applichiamo su range già amplificato)
                normalized *= (_config.perturbationScale / 255.0f);

                // AGGRESSIVE BOOST: Quadratic scaling for dramatic effect
                // x^0.7 = softer curve, più valori medi/alti
                normalized = powf(normalized, 0.7f);

                uint8_t value = (uint8_t)(normalized * 255.0f);
                perturbationGrid[row][col] = value;

                // Debug periodico (ogni 100 frame con motion)
                static uint32_t debugCounter = 0;
                if (magnitude > 0.5f && (debugCounter++ % 100) == 0) {
                    Serial.printf("[PERTURB] row=%d col=%d mag=%.1f conf=%d → value=%d\n",
                                 row, col, magnitude, confidence, value);
                }
            } else {
                perturbationGrid[row][col] = 0;
            }
        }
    }
}

void MotionProcessor::reset() {
    _lastMotionTime = 0;
    _lastIntensity = 0;
    _lastDirection = OpticalFlowDetector::Direction::NONE;
    _directionStartTime = 0;
    _gestureCooldown = false;
    _gestureCooldownEnd = 0;
    _clashCooldownEnd = 0;
    _lastGestureConfidence = 0;
}

const char* MotionProcessor::gestureToString(GestureType gesture) {
    switch (gesture) {
        case GestureType::NONE:      return "none";
        case GestureType::IGNITION:  return "ignition";
        case GestureType::RETRACT:   return "retract";
        case GestureType::CLASH:     return "clash";
        case GestureType::SWING:     return "swing";
        case GestureType::THRUST:    return "thrust";
        default:                     return "unknown";
    }
}
