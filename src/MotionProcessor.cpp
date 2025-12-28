#include "MotionProcessor.h"
#include <cmath>
#include <cstring>

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
    _lastEffectRequest[0] = '\0';
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
    result.effectRequest[0] = '\0';

    // Calculate perturbation grid based on algorithm
    if (_config.perturbationEnabled) {
        // Use different perturbation calculation based on motion algorithm
        if (detector.getAlgorithm() == OpticalFlowDetector::Algorithm::CENTROID_TRACKING) {
            // Centroid mode: create radial gradient from centroid position
            _calculatePerturbationFromCentroid(detector, result.perturbationGrid);
        } else {
            // Optical flow mode: use motion vectors for local perturbations
            _calculatePerturbationGrid(detector, result.perturbationGrid);
        }
    } else {
        memset(result.perturbationGrid, 0, sizeof(result.perturbationGrid));
    }

    // Detect gestures
    if (_config.gesturesEnabled) {
        result.gesture = _detectGesture(motionIntensity, direction, speed, timestamp, detector);
        result.gestureConfidence = (result.gesture != GestureType::NONE) ? _lastGestureConfidence : 0;
    }
    if (_lastEffectRequest[0] != '\0') {
        strncpy(result.effectRequest, _lastEffectRequest, sizeof(result.effectRequest) - 1);
        result.effectRequest[sizeof(result.effectRequest) - 1] = '\0';
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
    _lastEffectRequest[0] = '\0';

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
    enum class CardinalDirection : uint8_t { NONE, UP, DOWN, LEFT, RIGHT };
    CardinalDirection dir4 = CardinalDirection::NONE;
    if (absDx > 0 || absDy > 0) {
        if (absDx >= absDy) {
            dir4 = (sumDx >= 0) ? CardinalDirection::RIGHT : CardinalDirection::LEFT;
        } else {
            dir4 = (sumDy >= 0) ? CardinalDirection::DOWN : CardinalDirection::UP;
        }
    }

    const bool effectTrigger = (intensity >= _config.gestureThreshold ||
                                speed >= _config.ignitionSpeedThreshold);
    if (effectTrigger) {
        const String* effectName = nullptr;
        switch (dir4) {
            case CardinalDirection::UP:
                effectName = &_config.effectOnUp;
                break;
            case CardinalDirection::DOWN:
                effectName = &_config.effectOnDown;
                break;
            case CardinalDirection::LEFT:
                effectName = &_config.effectOnLeft;
                break;
            case CardinalDirection::RIGHT:
                effectName = &_config.effectOnRight;
                break;
            default:
                break;
        }
        if (effectName && effectName->length() > 0) {
            _gestureCooldown = true;
            _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;
            _lastGestureConfidence = 40;
            strncpy(_lastEffectRequest, effectName->c_str(), sizeof(_lastEffectRequest) - 1);
            _lastEffectRequest[sizeof(_lastEffectRequest) - 1] = '\0';
            if (_config.debugLogsEnabled) {
                Serial.printf("[MOTION] EFFECT change requested: %s\n", _lastEffectRequest);
            }
        }
    }

    GestureType mappedGesture = GestureType::NONE;
    switch (dir4) {
        case CardinalDirection::UP:
            mappedGesture = GestureType::IGNITION;
            break;
        case CardinalDirection::DOWN:
            mappedGesture = GestureType::RETRACT;
            break;
        case CardinalDirection::LEFT:
        case CardinalDirection::RIGHT:
            mappedGesture = GestureType::CLASH;
            break;
        default:
            break;
    }

    if (mappedGesture == GestureType::RETRACT &&
        (intensity >= retractIntensityThreshold || speed >= _config.retractSpeedThreshold))
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;
        _lastGestureConfidence = 60;
        if (_config.debugLogsEnabled) {
            Serial.println("[MOTION] RETRACT detected (4-way mapping).");
        }
        _lastDirection = direction;
        return GestureType::RETRACT;
    }

    if (mappedGesture == GestureType::IGNITION &&
        (intensity >= _config.ignitionIntensityThreshold || speed >= _config.ignitionSpeedThreshold))
    {
        _gestureCooldown = true;
        _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;
        _lastGestureConfidence = 85;
        if (_config.debugLogsEnabled) {
            Serial.println("[MOTION] IGNITION detected (4-way mapping).");
        }
        _lastDirection = direction;
        return GestureType::IGNITION;
    }

    if (mappedGesture == GestureType::CLASH &&
        (intensity >= clashIntensityThreshold || speed >= _config.clashSpeedThreshold) &&
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
            Serial.println("[MOTION] CLASH detected (4-way mapping).");
        }
        _lastDirection = direction;
        return GestureType::CLASH;
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

void MotionProcessor::_calculatePerturbationFromCentroid(
    const OpticalFlowDetector& detector,
    uint8_t perturbationGrid[OpticalFlowDetector::GRID_ROWS][OpticalFlowDetector::GRID_COLS])
{
    // Reset grid to zero first
    memset(perturbationGrid, 0, sizeof(uint8_t) * OpticalFlowDetector::GRID_ROWS * OpticalFlowDetector::GRID_COLS);

    // Get centroid normalized position (0.0-1.0)
    float cx, cy;
    if (!detector.getCentroidNormalized(&cx, &cy)) {
        // No valid centroid: no perturbation
        return;
    }

    // Get motion intensity to scale the perturbation
    uint8_t motionIntensity = detector.getMotionIntensity();
    if (motionIntensity < 5) {
        // Motion too weak: no perturbation
        return;
    }

    // Map centroid to grid coordinates (floating point for precision)
    const float centroidCol = cx * (OpticalFlowDetector::GRID_COLS - 1);
    const float centroidRow = cy * (OpticalFlowDetector::GRID_ROWS - 1);

    // Apply perturbation scale from config
    const float scaleNorm = _config.perturbationScale / 255.0f;

    // Create radial gradient centered on centroid
    // The perturbation is stronger at the centroid and fades with distance
    const float radiusInfluence = 4.0f;  // Grid cells of influence (tunable)

    for (uint8_t row = 0; row < OpticalFlowDetector::GRID_ROWS; row++) {
        for (uint8_t col = 0; col < OpticalFlowDetector::GRID_COLS; col++) {
            // Calculate distance from centroid to this cell
            float dx = (float)col - centroidCol;
            float dy = (float)row - centroidRow;
            float distance = sqrtf(dx * dx + dy * dy);

            // Calculate falloff: 1.0 at center, 0.0 at radiusInfluence distance
            float falloff = 1.0f - (distance / radiusInfluence);
            if (falloff < 0.0f) falloff = 0.0f;

            // Apply quadratic falloff for smoother gradient (x^2)
            falloff = falloff * falloff;

            // Scale by motion intensity and config scale
            float value = falloff * motionIntensity * scaleNorm;

            // Clamp to 0-255
            if (value > 255.0f) value = 255.0f;

            perturbationGrid[row][col] = (uint8_t)value;

            // Debug logging (periodic)
            if (_config.debugLogsEnabled && value > 10.0f) {
                static uint32_t debugCounter = 0;
                if ((debugCounter++ % 50) == 0) {
                    Serial.printf("[PERTURB_CENTROID] cx=%.2f cy=%.2f -> [%d,%d] dist=%.1f falloff=%.2f value=%d\n",
                                 centroidCol, centroidRow, row, col, distance, falloff, (uint8_t)value);
                }
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
    _lastEffectRequest[0] = '\0';
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
