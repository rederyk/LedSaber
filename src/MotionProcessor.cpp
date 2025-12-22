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
    _lastGestureConfidence(0),
    _deducedRotation(Rotation::ROT_0),
    _deducedRotationValid(false),
    _trackedAxis(Axis::NONE),
    _axisStartTime(0),
    _axisEmaWeightUp(0),
    _axisEmaWeightDown(0),
    _axisEmaWeightLeft(0),
    _axisEmaWeightRight(0),
    _axisEmaWeightTotal(0)
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

    auto max16 = [](uint16_t a, uint16_t b) { return (a > b) ? a : b; };

    // Per-gesture thresholds tuned for current optical flow noise/profile
    const uint8_t ignitionIntensityThreshold = (uint8_t)max(14, (int)_config.gestureThreshold + 2);
    const uint16_t ignitionDurationThreshold = max16((uint16_t)110, (uint16_t)(_config.gestureDurationMs + 30));
    const float ignitionSpeedThreshold = 0.60f;

    const uint8_t retractIntensityThreshold = (uint8_t)max(14, (int)_config.gestureThreshold + 4);   // Cleaner gesture
    const uint16_t retractDurationThreshold = max16((uint16_t)120, (uint16_t)(_config.gestureDurationMs + 40));
    const float retractSpeedThreshold = 0.65f;

    const uint8_t clashIntensityThreshold = (uint8_t)max(12, (int)_config.gestureThreshold);
    const uint16_t clashCooldown = max16(_config.clashCooldownMs, (uint16_t)500);

    // Cooldown management: prevent gesture spam
    if (_gestureCooldown) {
        if (timestamp < _gestureCooldownEnd) {
            return GestureType::NONE;
        }
        _gestureCooldown = false;
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

    // ─────────────────────────────────────────────────────────────
    // 4-axis gesture detection (no diagonals)
    // ─────────────────────────────────────────────────────────────

    auto rotateDir8 = [](OpticalFlowDetector::Direction dir, Rotation rot) -> OpticalFlowDetector::Direction {
        using D = OpticalFlowDetector::Direction;
        if (dir == D::NONE) return D::NONE;
        switch (rot) {
            case Rotation::ROT_0: return dir;
            case Rotation::ROT_180:
                switch (dir) {
                    case D::UP: return D::DOWN;
                    case D::DOWN: return D::UP;
                    case D::LEFT: return D::RIGHT;
                    case D::RIGHT: return D::LEFT;
                    case D::UP_LEFT: return D::DOWN_RIGHT;
                    case D::UP_RIGHT: return D::DOWN_LEFT;
                    case D::DOWN_LEFT: return D::UP_RIGHT;
                    case D::DOWN_RIGHT: return D::UP_LEFT;
                    default: return D::NONE;
                }
            case Rotation::ROT_90_CW:
                switch (dir) {
                    case D::UP: return D::RIGHT;
                    case D::UP_RIGHT: return D::DOWN_RIGHT;
                    case D::RIGHT: return D::DOWN;
                    case D::DOWN_RIGHT: return D::DOWN_LEFT;
                    case D::DOWN: return D::LEFT;
                    case D::DOWN_LEFT: return D::UP_LEFT;
                    case D::LEFT: return D::UP;
                    case D::UP_LEFT: return D::UP_RIGHT;
                    default: return D::NONE;
                }
            case Rotation::ROT_90_CCW:
                switch (dir) {
                    case D::UP: return D::LEFT;
                    case D::UP_LEFT: return D::DOWN_LEFT;
                    case D::LEFT: return D::DOWN;
                    case D::DOWN_LEFT: return D::DOWN_RIGHT;
                    case D::DOWN: return D::RIGHT;
                    case D::DOWN_RIGHT: return D::UP_RIGHT;
                    case D::RIGHT: return D::UP;
                    case D::UP_RIGHT: return D::UP_LEFT;
                    default: return D::NONE;
                }
            default: return dir;
        }
    };

    auto rotateVector = [](int16_t dx, int16_t dy, Rotation rot, int16_t* outDx, int16_t* outDy) {
        switch (rot) {
            case Rotation::ROT_0:      *outDx = dx;  *outDy = dy;  break;
            case Rotation::ROT_90_CW:  *outDx = dy;  *outDy = -dx; break;
            case Rotation::ROT_180:    *outDx = -dx; *outDy = -dy; break;
            case Rotation::ROT_90_CCW: *outDx = -dy; *outDy = dx;  break;
        }
    };

    // Deduce rotation between detector raw direction and the direction passed to us (often rotated in main.cpp)
    const OpticalFlowDetector::Direction rawDir = detector.getMotionDirection();
    if (rawDir != OpticalFlowDetector::Direction::NONE && direction != OpticalFlowDetector::Direction::NONE) {
        Rotation best = _deducedRotationValid ? _deducedRotation : Rotation::ROT_0;
        bool found = false;
        for (Rotation rot : {Rotation::ROT_0, Rotation::ROT_90_CW, Rotation::ROT_180, Rotation::ROT_90_CCW}) {
            if (rotateDir8(rawDir, rot) == direction) {
                best = rot;
                found = true;
                break;
            }
        }
        if (found) {
            _deducedRotation = best;
            _deducedRotationValid = true;
        }
    }
    const Rotation rot = _deducedRotationValid ? _deducedRotation : Rotation::ROT_0;

    // Per-frame weights
    uint16_t wUp = 0, wDown = 0, wLeft = 0, wRight = 0, wTotal = 0;
    uint8_t cUp = 0, cDown = 0, cLeft = 0, cRight = 0;

    for (uint8_t row = 0; row < OpticalFlowDetector::GRID_ROWS; row++) {
        for (uint8_t col = 0; col < OpticalFlowDetector::GRID_COLS; col++) {
            int8_t dx8 = 0, dy8 = 0;
            uint8_t conf = 0;
            if (!detector.getBlockVector(row, col, &dx8, &dy8, &conf)) {
                continue;
            }

            int16_t dx = dx8;
            int16_t dy = dy8;
            int16_t rdx = 0, rdy = 0;
            rotateVector(dx, dy, rot, &rdx, &rdy);

            const int absDx = abs((int)rdx);
            const int absDy = abs((int)rdy);
            if (absDx <= 1 && absDy <= 1) {
                continue;
            }

            // Only 4 axes: accept a vector only if one component clearly dominates the other.
            // This rejects diagonals/noise.
            static constexpr int DOM_RATIO_NUM = 2;
            static constexpr int DOM_RATIO_DEN = 1;
            const bool vertical = (absDy * DOM_RATIO_DEN) >= (absDx * DOM_RATIO_NUM);
            const bool horizontal = (absDx * DOM_RATIO_DEN) >= (absDy * DOM_RATIO_NUM);
            if (!vertical && !horizontal) {
                continue;
            }

            // Weight: manhattan magnitude * confidence (scaled to keep uint16 range small)
            const uint16_t mag = (uint16_t)min(255, absDx + absDy);
            const uint16_t w = (uint16_t)((mag * (uint16_t)conf) / 255U);
            if (w == 0) continue;
            wTotal = (uint16_t)min(65535, (int)wTotal + (int)w);

            if (vertical) {
                if (rdy < 0) {  // UP (dy negative in image coordinates)
                    wUp = (uint16_t)min(65535, (int)wUp + (int)w);
                    cUp++;
                } else {
                    wDown = (uint16_t)min(65535, (int)wDown + (int)w);
                    cDown++;
                }
            } else {
                if (rdx < 0) {
                    wLeft = (uint16_t)min(65535, (int)wLeft + (int)w);
                    cLeft++;
                } else {
                    wRight = (uint16_t)min(65535, (int)wRight + (int)w);
                    cRight++;
                }
            }
        }
    }

    // Smooth weights over time (EMA on integer domain)
    const uint16_t alpha = _config.axisEmaAlpha;              // 0..255
    const uint16_t invA = (uint16_t)(255 - alpha);
    auto emaU16 = [&](uint16_t prev, uint16_t cur) -> uint16_t {
        // prev*(1-a)+cur*a where a=alpha/255
        const uint32_t mixed = (uint32_t)prev * invA + (uint32_t)cur * alpha;
        uint32_t v = mixed / 255U;
        if (v > 65535U) v = 65535U;
        return (uint16_t)v;
    };

    _axisEmaWeightUp = emaU16(_axisEmaWeightUp, wUp);
    _axisEmaWeightDown = emaU16(_axisEmaWeightDown, wDown);
    _axisEmaWeightLeft = emaU16(_axisEmaWeightLeft, wLeft);
    _axisEmaWeightRight = emaU16(_axisEmaWeightRight, wRight);
    _axisEmaWeightTotal = emaU16(_axisEmaWeightTotal, wTotal);

    const uint16_t totalEma = _axisEmaWeightTotal;
    if (totalEma < _config.axisMinTotalWeight) {
        _trackedAxis = Axis::NONE;
        _axisStartTime = 0;
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;
        return GestureType::NONE;
    }

    struct AxisCandidate {
        Axis axis;
        uint16_t w;
        uint8_t blocks;
    };
    AxisCandidate candidates[4] = {
        {Axis::UP, _axisEmaWeightUp, cUp},
        {Axis::DOWN, _axisEmaWeightDown, cDown},
        {Axis::LEFT, _axisEmaWeightLeft, cLeft},
        {Axis::RIGHT, _axisEmaWeightRight, cRight},
    };

    // Find best + second
    AxisCandidate best = candidates[0];
    AxisCandidate second = candidates[1];
    if (second.w > best.w) { AxisCandidate tmp = best; best = second; second = tmp; }
    for (int i = 2; i < 4; i++) {
        if (candidates[i].w > best.w) {
            second = best;
            best = candidates[i];
        } else if (candidates[i].w > second.w) {
            second = candidates[i];
        }
    }

    const uint16_t totalDen = (totalEma == 0) ? 1 : totalEma;
    uint32_t fracTmp = ((uint32_t)best.w * 100U) / (uint32_t)totalDen;
    if (fracTmp > 100U) fracTmp = 100U;
    const uint8_t fracBest = (uint8_t)fracTmp;

    const uint16_t secondDen = (second.w == 0) ? 1 : second.w;
    const uint16_t dominanceX10 = (uint16_t)(((uint32_t)best.w * 10U) / (uint32_t)secondDen);

    // Enforce minimum coherent blocks on the winning axis
    if (best.blocks < _config.axisMinBlocks) {
        _trackedAxis = Axis::NONE;
        _axisStartTime = 0;
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;
        return GestureType::NONE;
    }

    const bool axisStrong = (fracBest >= _config.axisOnFraction && dominanceX10 >= _config.axisMinDominance);

    // Track axis with hysteresis: latch on strong, release when it weakens enough
    bool axisLockedThisFrame = false;
    if (_trackedAxis == Axis::NONE) {
        if (axisStrong) {
            _trackedAxis = best.axis;
            _axisStartTime = timestamp;
            axisLockedThisFrame = true;
        }
    } else {
        // If still same axis, keep tracking. If changes, require a strong latch to switch.
        if (_trackedAxis != best.axis) {
            if (axisStrong) {
                _trackedAxis = best.axis;
                _axisStartTime = timestamp;
                axisLockedThisFrame = true;
            }
        } else {
            const bool axisWeak = (fracBest < _config.axisOffFraction);
            if (axisWeak) {
                _trackedAxis = Axis::NONE;
                _axisStartTime = 0;
            }
        }
    }

    if (axisLockedThisFrame) {
        const char* axisStr =
            (_trackedAxis == Axis::UP) ? "up" :
            (_trackedAxis == Axis::DOWN) ? "down" :
            (_trackedAxis == Axis::LEFT) ? "left" :
            (_trackedAxis == Axis::RIGHT) ? "right" : "none";
        Serial.printf("[MOTION] Axis lock: %s (frac=%u%%, dom=%.1fx, total=%u)\n",
                      axisStr, fracBest, (double)dominanceX10 / 10.0, (unsigned)totalEma);
    }

    if (_trackedAxis == Axis::NONE || _axisStartTime == 0) {
        _lastIntensity = intensity;
        _lastDirection = direction;
        _lastMotionTime = timestamp;
        return GestureType::NONE;
    }

    const uint32_t heldMs = timestamp - _axisStartTime;

    // Convert tracked axis into gestures
    const uint8_t swingIntensityThreshold = (uint8_t)max(6, (int)_config.gestureThreshold - 4);

    auto confidenceFromAxis = [&](uint32_t requiredMs) -> uint8_t {
        // Confidence combines: fraction, dominance, duration over requirement (clamped)
        float fracScore = (float)fracBest / 100.0f;
        uint16_t domClamped = dominanceX10;
        if (domClamped > 30) domClamped = 30;
        float domScore = (float)domClamped / 30.0f; // 0..1 where 3.0x == full

        uint32_t durDen = requiredMs * 2U;
        if (durDen == 0) durDen = 1;
        uint32_t durNum = heldMs;
        if (durNum > durDen) durNum = durDen;
        float durScore = (float)durNum / (float)durDen;
        float confF = (0.50f * fracScore + 0.25f * domScore + 0.25f * durScore) * 100.0f;
        int conf = (int)roundf(confF);
        if (conf < 0) conf = 0;
        if (conf > 100) conf = 100;
        return (uint8_t)conf;
    };

    // Up/Down = "major" gestures with cooldown; Left/Right = swing (no long cooldown)
    if (_trackedAxis == Axis::UP) {
        const bool strongAxisNow = (fracBest >= (uint8_t)min(100, (int)_config.axisOnFraction + 10));
        const bool okSpeed = (speed >= ignitionSpeedThreshold) || strongAxisNow;
        const bool okIntensity = (intensity >= swingIntensityThreshold) || (intensity >= ignitionIntensityThreshold);
        if (okIntensity && okSpeed && heldMs >= ignitionDurationThreshold)
        {
            _gestureCooldown = true;
            _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;
            _lastGestureConfidence = confidenceFromAxis(ignitionDurationThreshold);
            Serial.printf("[MOTION] IGNITION detected! (4-axis, frac=%u%%, dom=%.1fx, held=%lums)\n",
                          fracBest, (double)dominanceX10 / 10.0, (unsigned long)heldMs);
            _lastIntensity = intensity;
            _lastDirection = direction;
            _lastMotionTime = timestamp;
            return GestureType::IGNITION;
        }
    } else if (_trackedAxis == Axis::DOWN) {
        const bool strongAxisNow = (fracBest >= (uint8_t)min(100, (int)_config.axisOnFraction + 10));
        const bool okSpeed = (speed >= retractSpeedThreshold) || strongAxisNow;
        const bool okIntensity = (intensity >= swingIntensityThreshold) || (intensity >= retractIntensityThreshold);
        if (okIntensity && okSpeed && heldMs >= retractDurationThreshold)
        {
            _gestureCooldown = true;
            _gestureCooldownEnd = timestamp + _config.gestureCooldownMs;
            _lastGestureConfidence = confidenceFromAxis(retractDurationThreshold);
            Serial.printf("[MOTION] RETRACT detected! (4-axis, frac=%u%%, dom=%.1fx, held=%lums)\n",
                          fracBest, (double)dominanceX10 / 10.0, (unsigned long)heldMs);
            _lastIntensity = intensity;
            _lastDirection = direction;
            _lastMotionTime = timestamp;
            return GestureType::RETRACT;
        }
    } else if (_trackedAxis == Axis::LEFT || _trackedAxis == Axis::RIGHT) {
        if (intensity >= swingIntensityThreshold &&
            speed > 0.45f &&
            heldMs >= _config.swingDurationMs)
        {
            _lastGestureConfidence = confidenceFromAxis(_config.swingDurationMs);
            Serial.printf("[MOTION] SWING detected! (4-axis, axis=%s, frac=%u%%, dom=%.1fx, held=%lums)\n",
                          (_trackedAxis == Axis::LEFT) ? "left" : "right",
                          fracBest, (double)dominanceX10 / 10.0, (unsigned long)heldMs);
            _lastIntensity = intensity;
            _lastDirection = direction;
            _lastMotionTime = timestamp;
            return GestureType::SWING;
        }
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
    _lastMotionTime = 0;
    _lastIntensity = 0;
    _lastDirection = OpticalFlowDetector::Direction::NONE;
    _directionStartTime = 0;
    _gestureCooldown = false;
    _gestureCooldownEnd = 0;
    _clashCooldownEnd = 0;
    _lastGestureConfidence = 0;
    _deducedRotation = Rotation::ROT_0;
    _deducedRotationValid = false;
    _trackedAxis = Axis::NONE;
    _axisStartTime = 0;
    _axisEmaWeightUp = 0;
    _axisEmaWeightDown = 0;
    _axisEmaWeightLeft = 0;
    _axisEmaWeightRight = 0;
    _axisEmaWeightTotal = 0;
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
