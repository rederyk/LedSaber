#include "MotionProcessor.h"
#include <cmath>

MotionProcessor::MotionProcessor() :
    _lastMotionTime(0),
    _lastIntensity(0),
    _lastDirection(OpticalFlowDetector::Direction::NONE),
    _directionStartTime(0),
    _gestureCooldown(false),
    _gestureCooldownEnd(0),
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

    // Cooldown management: prevent gesture spam
    if (_gestureCooldown) {
        if (timestamp < _gestureCooldownEnd) {
            return GestureType::NONE;
        }
        _gestureCooldown = false;
    }

    // Intensity threshold check
    if (intensity < _config.gestureThreshold) {
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;
        return GestureType::NONE;
    }

    // CLASH detection: sudden spike in intensity
    // Check for large delta in a window suitable for low FPS (default: 350ms)
    if (_lastMotionTime > 0 && (timestamp - _lastMotionTime) <= _config.clashWindowMs) {
        int16_t deltaIntensity = intensity - _lastIntensity;
        if (deltaIntensity > _config.clashDeltaThreshold) {
            // Clash detected!
            _gestureCooldown = true;
            uint16_t cooldownHalf = (uint16_t)(_config.gestureCooldownMs / 2);
            if (cooldownHalf < 250) cooldownHalf = 250;
            _gestureCooldownEnd = timestamp + cooldownHalf;

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

    // Sustained direction tracking
    bool isSustained = _isSustainedDirection(direction, timestamp);

    if (!isSustained) {
        // Direction just changed or started
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;
        return GestureType::NONE;
    }

    // At this point: high intensity + sustained direction
    uint32_t duration = timestamp - _directionStartTime;

    // IGNITION: UP veloce e sostenuto
    if (direction == OpticalFlowDetector::Direction::UP &&
        speed > 0.6f &&  // Più permissivo (5fps + tracking grossolano)
        intensity > _config.gestureThreshold &&
        duration >= _config.gestureDurationMs)
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;

        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

        // Confidence: intensity + speed + duration
        float speedScore = speed / 2.0f;
        if (speedScore > 1.0f) speedScore = 1.0f;
        float intScore = (float)(intensity - _config.gestureThreshold) / 60.0f;
        if (intScore > 1.0f) intScore = 1.0f;
        float durScore = (float)duration / 300.0f;
        if (durScore > 1.0f) durScore = 1.0f;
        int conf = (int)roundf((0.45f * speedScore + 0.35f * intScore + 0.20f * durScore) * 100.0f);
        if (conf < 0) conf = 0;
        if (conf > 100) conf = 100;
        _lastGestureConfidence = (uint8_t)conf;

        Serial.printf("[MOTION] IGNITION detected! (speed=%.1f, duration=%ums)\n", speed, duration);
        return GestureType::IGNITION;
    }

    // RETRACT: DOWN veloce e sostenuto
    if (direction == OpticalFlowDetector::Direction::DOWN &&
        speed > 0.6f &&
        intensity > _config.gestureThreshold &&
        duration >= _config.gestureDurationMs)
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;

        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

        float speedScore = speed / 2.0f;
        if (speedScore > 1.0f) speedScore = 1.0f;
        float intScore = (float)(intensity - _config.gestureThreshold) / 60.0f;
        if (intScore > 1.0f) intScore = 1.0f;
        float durScore = (float)duration / 300.0f;
        if (durScore > 1.0f) durScore = 1.0f;
        int conf = (int)roundf((0.45f * speedScore + 0.35f * intScore + 0.20f * durScore) * 100.0f);
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
    uint32_t timestamp)
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
    return duration >= _config.gestureDurationMs;
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
