#include "MotionProcessor.h"
#include <cmath>

MotionProcessor::MotionProcessor() :
    _lastMotionTime(0),
    _lastIntensity(0),
    _lastDirection(OpticalFlowDetector::Direction::NONE),
    _directionStartTime(0),
    _gestureCooldown(false),
    _gestureCooldownEnd(0)
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

    // Calculate perturbation grid
    if (_config.perturbationEnabled) {
        _calculatePerturbationGrid(detector, result.perturbationGrid);
    } else {
        memset(result.perturbationGrid, 0, sizeof(result.perturbationGrid));
    }

    // Detect gestures
    if (_config.gesturesEnabled) {
        result.gesture = _detectGesture(motionIntensity, direction, speed, timestamp);
    }

    return result;
}

MotionProcessor::GestureType MotionProcessor::_detectGesture(
    uint8_t intensity,
    OpticalFlowDetector::Direction direction,
    float speed,
    uint32_t timestamp)
{
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
    // Check for large delta in short time (< 100ms)
    if (_lastMotionTime > 0 && (timestamp - _lastMotionTime) < 100) {
        int16_t deltaIntensity = intensity - _lastIntensity;
        if (deltaIntensity > _config.clashDeltaThreshold) {
            // Clash detected!
            _gestureCooldown = true;
            _gestureCooldownEnd = timestamp + 1000;  // 1 second cooldown

            _lastIntensity = intensity;
            _lastDirection = direction;
            _lastMotionTime = timestamp;

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
        speed > 8.0f &&
        intensity > _config.gestureThreshold &&
        duration >= _config.gestureDurationMs)
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + 2000;  // 2 second cooldown

        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

        Serial.printf("[MOTION] IGNITION detected! (speed=%.1f, duration=%ums)\n", speed, duration);
        return GestureType::IGNITION;
    }

    // RETRACT: DOWN veloce e sostenuto
    if (direction == OpticalFlowDetector::Direction::DOWN &&
        speed > 8.0f &&
        intensity > _config.gestureThreshold &&
        duration >= _config.gestureDurationMs)
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + 2000;  // 2 second cooldown

        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

        Serial.printf("[MOTION] RETRACT detected! (speed=%.1f, duration=%ums)\n", speed, duration);
        return GestureType::RETRACT;
    }

    // SWING: LEFT/RIGHT veloce
    if ((direction == OpticalFlowDetector::Direction::LEFT ||
         direction == OpticalFlowDetector::Direction::RIGHT) &&
        intensity > 120 &&
        speed > 6.0f)
    {
        // Note: SWING non ha cooldown lungo, può ripetersi
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;

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

                // Scale by confidence and config scale
                // magnitude: 0-20 typical (±10 px max per direction)
                // Normalize to 0-255 range
                float normalized = magnitude / 20.0f;  // 0.0-1.0 (can exceed 1.0)
                if (normalized > 1.0f) normalized = 1.0f;

                // Apply confidence weighting
                normalized *= (confidence / 255.0f);

                // Apply config scale (0-255)
                normalized *= (_config.perturbationScale / 255.0f);

                perturbationGrid[row][col] = (uint8_t)(normalized * 255.0f);
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
