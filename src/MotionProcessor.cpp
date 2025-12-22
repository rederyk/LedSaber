#include "MotionProcessor.h"
#include <cmath>

MotionProcessor::MotionProcessor() :
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

    // Per-gesture thresholds (configurable via BLE)
    const uint8_t retractIntensityThreshold = _config.retractIntensityThreshold;
    const float retractSpeedMax = _config.retractSpeedMax; // Slow movement = retract

    const uint8_t clashIntensityThreshold = _config.clashIntensityThreshold;
    const float clashSpeedMin = _config.clashSpeedMin;   // Fast movement = clash
    const uint16_t clashCooldown = max16(_config.clashCooldownMs, (uint16_t)400);

    // Cooldown management: prevent gesture spam
    if (_gestureCooldown) {
        if (timestamp < _gestureCooldownEnd) {
            return GestureType::NONE;
        }
        _gestureCooldown = false;
    }

    const bool clashOnCooldown = (timestamp < _clashCooldownEnd);

    auto dirIsDown = [](OpticalFlowDetector::Direction dir) {
        using D = OpticalFlowDetector::Direction;
        return (dir == D::DOWN || dir == D::DOWN_LEFT || dir == D::DOWN_RIGHT);
    };
    auto dirIsLeftRight = [](OpticalFlowDetector::Direction dir) {
        using D = OpticalFlowDetector::Direction;
        return (dir == D::LEFT || dir == D::RIGHT || dir == D::UP_LEFT || dir == D::UP_RIGHT);
    };

    // "Cerchio a spicchi": giu' lento = retract, sinistra/destra veloce = clash
    if (dirIsLeftRight(direction) &&
        speed >= clashSpeedMin &&
        intensity >= clashIntensityThreshold &&
        !clashOnCooldown)
    {
        _gestureCooldown = true;
        uint16_t cooldownHalf = (uint16_t)(_config.gestureCooldownMs / 2);
        if (cooldownHalf < 200) cooldownHalf = 200;
        const uint16_t appliedCooldown = max16(cooldownHalf, clashCooldown);
        _gestureCooldownEnd = timestamp + appliedCooldown;
        _clashCooldownEnd = timestamp + clashCooldown;
        _lastGestureConfidence = 70;
        Serial.println("[MOTION] CLASH detected (direction: left/right, fast).");
        _lastDirection = direction;
        return GestureType::CLASH;
    }

    if (dirIsDown(direction) &&
        speed <= retractSpeedMax &&
        intensity >= retractIntensityThreshold)
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;
        _lastGestureConfidence = 80;
        Serial.println("[MOTION] RETRACT detected (direction: down, slow).");
        _lastDirection = direction;
        return GestureType::RETRACT;
    }

    // No gesture detected
    _lastDirection = direction;
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
    uint8_t perturbationGrid[OpticalFlowDetector::GRID_ROWS][OpticalFlowDetector::GRID_COLS])
{
    for (uint8_t row = 0; row < OpticalFlowDetector::GRID_ROWS; row++) {
        for (uint8_t col = 0; col < OpticalFlowDetector::GRID_COLS; col++) {
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
                float confidenceBoost = (confidence / 255.0f) * 0.2f + 0.5f;  // Min 50%
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
        default:                     return "unknown";
    }
}
