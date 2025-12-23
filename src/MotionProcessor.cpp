#include "MotionProcessor.h"
#include <cmath>

namespace {
const uint8_t* gammaLut07() {
    static uint8_t lut[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) {
            const float x = (float)i / 255.0f;
            lut[i] = (uint8_t)lroundf(powf(x, 0.7f) * 255.0f);
        }
        init = true;
    }
    return lut;
}
}

MotionProcessor::MotionProcessor() :
    _lastDirection(OpticalFlowDetector::Direction::NONE),
    _directionStartTime(0),
    _lastFrameTime(0),
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
        result.gesture = _detectGesture(motionIntensity, direction, speed, timestamp, detector);
        result.gestureConfidence = (result.gesture != GestureType::NONE) ? _lastGestureConfidence : 0;
    }

    return result;
}

MotionProcessor::GestureType MotionProcessor::_detectGesture(
    uint8_t intensity,
    OpticalFlowDetector::Direction direction,
    float speed,
    uint32_t timestamp,
    const OpticalFlowDetector& detector)
{
    _lastGestureConfidence = 0;
    (void)speed;

    auto max16 = [](uint16_t a, uint16_t b) { return (a > b) ? a : b; };

    // Per-gesture thresholds (configurable via BLE)
    const uint8_t retractIntensityThreshold = _config.retractIntensityThreshold;

    const uint8_t clashIntensityThreshold = _config.clashIntensityThreshold;
    const uint16_t clashCooldown = max16(_config.clashCooldownMs, (uint16_t)400);

    // Cooldown management: prevent gesture spam
    if (_gestureCooldown) {
        if (timestamp < _gestureCooldownEnd) {
            return GestureType::NONE;
        }
        _gestureCooldown = false;
    }

    const bool clashOnCooldown = (timestamp < _clashCooldownEnd);

    int32_t sumDx = 0;
    int32_t sumDy = 0;
    int32_t sumW = 0;
    uint16_t weightScaleQ8 = 256; // 1.0 in Q8
    if (_lastFrameTime > 0 && timestamp > _lastFrameTime) {
        const uint32_t dtMs = timestamp - _lastFrameTime;
        if (dtMs > 0) {
            uint32_t scaled = (1000UL << 8) / dtMs;
            if (scaled < 64) scaled = 64;      // 0.25 in Q8
            if (scaled > 1024) scaled = 1024;  // 4.0 in Q8
            weightScaleQ8 = (uint16_t)scaled;
        }
    }
    _lastFrameTime = timestamp;
    for (uint8_t row = 0; row < OpticalFlowDetector::GRID_ROWS; row++) {
        for (uint8_t col = 0; col < OpticalFlowDetector::GRID_COLS; col++) {
            int8_t dx = 0;
            int8_t dy = 0;
            uint8_t conf = 0;
            if (!detector.getBlockVector(row, col, &dx, &dy, &conf)) {
                continue;
            }
            const int mag = abs((int)dx) + abs((int)dy);
            if (mag == 0 || conf == 0) {
                continue;
            }
            const int32_t temp = (int32_t)mag * (int32_t)conf * (int32_t)weightScaleQ8;
            const int32_t w = (temp + 128) >> 8;
            sumDx += (int32_t)dx * w;
            sumDy += (int32_t)dy * w;
            sumW += w;
        }
    }

    if (sumW == 0) {
        _lastDirection = direction;
        return GestureType::NONE;
    }

    int32_t absDx = (sumDx < 0) ? -sumDx : sumDx;
    int32_t absDy = (sumDy < 0) ? -sumDy : sumDy;
    const int32_t maxAbs = (absDx > absDy) ? absDx : absDy;
    if (maxAbs > 0) {
        const int32_t limit = 4800000; // keeps multiplications in 32-bit
        if (maxAbs > limit) {
            uint8_t shift = 0;
            int32_t scaled = maxAbs;
            while (scaled > limit && shift < 16) {
                scaled >>= 1;
                ++shift;
            }
            absDx >>= shift;
            absDy >>= shift;
        }
    }
    static constexpr int32_t TAN_DOWN_HALF_Q8 = 256; // tan(45 deg) * 256 (Era 443 / 60deg)
    static constexpr int32_t TAN_LR_HALF_Q8 = 215;   // tan(40 deg) * 256

    const bool isDown = (sumDy > 0) &&
        ((absDx << 8) <= (TAN_DOWN_HALF_Q8 * absDy));
    const bool isLeft = (sumDx < 0) &&
        ((absDy << 8) <= (TAN_LR_HALF_Q8 * absDx));
    const bool isRight = (sumDx > 0) &&
        ((absDy << 8) <= (TAN_LR_HALF_Q8 * absDx));

    // "Cerchio a spicchi": giu' lento? = retract, sinistra/destra veloce ?= clash
    if ((isLeft || isRight) &&
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
        if (_config.debugLogsEnabled) {
            Serial.println("[MOTION] CLASH detected (direction: left/right, fast).");
        }
        _lastDirection = direction;
        return GestureType::CLASH;
    }

    if (isDown &&
        intensity >= retractIntensityThreshold)
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;
        _lastGestureConfidence = 80;
        if (_config.debugLogsEnabled) {
            Serial.println("[MOTION] RETRACT detected (direction: down, slow).");
        }
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
    const uint8_t* gamma = gammaLut07();
    const float inv255 = 1.0f / 255.0f;
    const float confWeight = 0.2f * inv255;
    const float scale = _config.perturbationScale * inv255;

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
                float confidenceBoost = (confidence * confWeight) + 0.5f;  // Min 50%
                normalized *= confidenceBoost;

                // Apply config scale (default 128 = 50%, ma lo applichiamo su range già amplificato)
                normalized *= scale;

                // AGGRESSIVE BOOST: Quadratic scaling for dramatic effect
                // x^0.7 = softer curve, più valori medi/alti
                uint16_t idx = (uint16_t)(normalized * 255.0f + 0.5f);
                if (idx > 255) idx = 255;
                uint8_t value = gamma[idx];
                perturbationGrid[row][col] = value;

                // Debug periodico (ogni 100 frame con motion)
                if (_config.debugLogsEnabled) {
                    static uint32_t debugCounter = 0;
                    if (magnitude > 0.5f && (debugCounter++ % 100) == 0) {
                        Serial.printf("[PERTURB] row=%d col=%d mag=%.1f conf=%d -> value=%d\n",
                                     row, col, magnitude, confidence, value);
                    }
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
    _lastFrameTime = 0;
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
