#include "LedEffectEngine.h"

static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 112;

LedEffectEngine::LedEffectEngine(CRGB* leds, uint16_t numLeds) :
    _leds(leds),
    _numLeds(numLeds),
    _mode(Mode::IDLE),
    _modeStartTime(0),
    _hue(0),
    _ignitionProgress(0),
    _lastIgnitionUpdate(0),
    _retractionProgress(0),
    _lastRetractionUpdate(0),
    _pulsePosition(0),
    _lastPulseUpdate(0),
    _pulse1Pos(0),
    _pulse2Pos(0),
    _lastDualPulseUpdate(0),
    _clashBrightness(0),
    _lastClashTrigger(0),
    _clashActive(false),
    _rainbowHue(0),
    _breathOverride(255),
    _lastUpdate(0)
{
    memset(_unstableHeat, 0, sizeof(_unstableHeat));
}

void LedEffectEngine::render(const LedState& state, const MotionProcessor::ProcessedMotion* motion) {
    const unsigned long now = millis();

    // Rate limiting: 20ms per frame
    if (now - _lastUpdate < 20) {
        return;
    }

    if (!state.enabled) {
        fill_solid(_leds, _numLeds, CRGB::Black);
        FastLED.show();
        _lastUpdate = now;
        return;
    }

    // Handle gesture triggers (if motion available)
    if (motion != nullptr) {
        handleGestureTriggers(motion->gesture, now);
    }

    // Check mode timeout
    if (_mode != Mode::IDLE) {
        if (checkModeTimeout(now)) {
            _mode = Mode::IDLE;
        }
    }

    // Reset breathe override (will be set by renderBreathe if needed)
    _breathOverride = 255;

    // Render based on mode
    switch (_mode) {
        case Mode::IGNITION_ACTIVE:
            renderIgnition(state);
            break;

        case Mode::RETRACT_ACTIVE:
            renderRetraction(state);
            break;

        case Mode::CLASH_ACTIVE:
            renderClash(state);
            break;

        case Mode::IDLE:
        default:
            // Render base effect with optional perturbations
            if (state.effect == "solid") {
                renderSolid(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "rainbow") {
                renderRainbow(state);
            } else if (state.effect == "breathe") {
                renderBreathe(state);
            } else if (state.effect == "flicker") {
                renderFlicker(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "unstable") {
                renderUnstable(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "pulse") {
                renderPulse(state);
            } else if (state.effect == "dual_pulse") {
                renderDualPulse(state);
            } else if (state.effect == "rainbow_blade") {
                renderRainbowBlade(state);
            } else if (state.effect == "ignition") {
                // Manual ignition effect (user triggered)
                renderIgnition(state);
            } else if (state.effect == "retraction") {
                // Manual retraction effect (user triggered)
                renderRetraction(state);
            } else if (state.effect == "clash") {
                // Manual clash effect (user triggered)
                renderClash(state);
            } else {
                // Unknown effect: fallback to solid
                renderSolid(state, nullptr);
            }
            break;
    }

    // Apply brightness (use override if breathe effect set it)
    uint8_t finalBrightness = min(_breathOverride, MAX_SAFE_BRIGHTNESS);
    if (state.effect != "breathe") {
        finalBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    }

    FastLED.setBrightness(finalBrightness);
    FastLED.show();
    _lastUpdate = now;
}

// ═══════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════

inline CRGB LedEffectEngine::scaleColorByBrightness(CRGB color, uint8_t brightness) {
    return CRGB(
        scale8(color.r, brightness),
        scale8(color.g, brightness),
        scale8(color.b, brightness)
    );
}

void LedEffectEngine::setLedPair(uint16_t logicalIndex, uint16_t foldPoint, CRGB color) {
    if (logicalIndex >= foldPoint) {
        return;
    }

    uint16_t led1 = logicalIndex;
    uint16_t led2 = (_numLeds - 1) - logicalIndex;

    _leds[led1] = color;
    _leds[led2] = color;
}

// ═══════════════════════════════════════════════════════════
// BASE EFFECT RENDERERS
// ═══════════════════════════════════════════════════════════

void LedEffectEngine::renderSolid(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    CRGB baseColor = CRGB(state.r, state.g, state.b);

    if (perturbationGrid == nullptr) {
        // No perturbations: simple solid fill
        fill_solid(_leds, _numLeds, baseColor);
        return;
    }

    // Apply perturbations
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        // Map LED logical position i to grid column
        // foldPoint LEDs -> 8 columns
        uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);
        uint8_t row = 3;  // Center row (simulates blade axis)

        uint8_t perturbation = perturbationGrid[row][col];

        if (perturbation > 15) {  // Threshold minimum
            // Apply local perturbation: add noise to brightness
            uint8_t noise = random8(perturbation / 2);
            CRGB perturbedColor = baseColor;
            perturbedColor.fadeToBlackBy(noise);

            perturbedColor = scaleColorByBrightness(perturbedColor, safeBrightness);
            setLedPair(i, state.foldPoint, perturbedColor);
        } else {
            // No perturbation: base color
            CRGB scaledColor = scaleColorByBrightness(baseColor, safeBrightness);
            setLedPair(i, state.foldPoint, scaledColor);
        }
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderRainbow(const LedState& state) {
    uint8_t step = map(state.speed, 1, 255, 1, 15);
    if (step == 0) step = 1;
    fill_rainbow(_leds, _numLeds, _hue, 256 / _numLeds);
    _hue += step;
}

void LedEffectEngine::renderBreathe(const LedState& state) {
    uint8_t breath = beatsin8(state.speed, 0, 255);
    fill_solid(_leds, _numLeds, CRGB(state.r, state.g, state.b));
    // Brightness will be applied in render() as scale8(breath, brightness)
    // Store breath value in a temporary variable for now - apply directly via brightness override
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    _breathOverride = scale8(breath, safeBrightness);
}

void LedEffectEngine::renderFlicker(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    CRGB baseColor = CRGB(state.r, state.g, state.b);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    uint8_t flickerIntensity = state.speed;

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        uint8_t noise = random8(flickerIntensity);

        // Enhance with perturbation if available
        if (perturbationGrid != nullptr) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);
            uint8_t row = 3;
            uint8_t perturbation = perturbationGrid[row][col];
            noise = qadd8(noise, perturbation / 4);  // Add 25% of perturbation
        }

        uint8_t brightness = 255 - (noise / 2);

        CRGB flickeredColor = baseColor;
        flickeredColor.fadeToBlackBy(255 - brightness);
        flickeredColor = scaleColorByBrightness(flickeredColor, safeBrightness);

        setLedPair(i, state.foldPoint, flickeredColor);
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderUnstable(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    CRGB baseColor = CRGB(state.r, state.g, state.b);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    uint16_t maxIndex = min((uint16_t)state.foldPoint, (uint16_t)72);

    // Decay heat
    for (uint16_t i = 0; i < maxIndex; i++) {
        _unstableHeat[i] = qsub8(_unstableHeat[i], random8(5, 15));
    }

    // Add sparks (base randomness + perturbation boost)
    uint8_t sparkChance = state.speed / 2;

    if (perturbationGrid != nullptr) {
        // Increase spark chance in high-perturbation areas
        for (uint8_t col = 0; col < 8; col++) {
            uint8_t perturbation = perturbationGrid[3][col];
            if (perturbation > 50 && random8() < (sparkChance + perturbation / 4)) {
                uint16_t pos = map(col, 0, 7, 0, maxIndex - 1);
                _unstableHeat[pos] = qadd8(_unstableHeat[pos], random8(100, 200));
            }
        }
    }

    if (random8() < sparkChance) {
        uint16_t pos = random16(maxIndex);
        _unstableHeat[pos] = qadd8(_unstableHeat[pos], random8(100, 200));
    }

    // Render
    for (uint16_t i = 0; i < maxIndex; i++) {
        uint8_t brightness = scale8(255, 200 + (_unstableHeat[i] / 4));
        CRGB unstableColor = baseColor;
        unstableColor.fadeToBlackBy(255 - brightness);
        unstableColor = scaleColorByBrightness(unstableColor, safeBrightness);

        setLedPair(i, state.foldPoint, unstableColor);
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderPulse(const LedState& state) {
    const unsigned long now = millis();
    uint16_t pulseSpeed = map(state.speed, 1, 255, 80, 1);

    if (now - _lastPulseUpdate > pulseSpeed) {
        _pulsePosition = (_pulsePosition + 1) % state.foldPoint;
        _lastPulseUpdate = now;
    }

    CRGB baseColor = CRGB(state.r, state.g, state.b);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    const uint8_t pulseWidth = 15;

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        int16_t distance = abs((int16_t)i - (int16_t)_pulsePosition);

        if (distance < pulseWidth) {
            uint8_t brightness = map(distance, 0, pulseWidth, 255, 150);
            CRGB pulseColor = baseColor;
            pulseColor.fadeToBlackBy(255 - brightness);
            pulseColor = scaleColorByBrightness(pulseColor, safeBrightness);
            setLedPair(i, state.foldPoint, pulseColor);
        } else {
            CRGB dimColor = baseColor;
            dimColor.fadeToBlackBy(255 - 150);
            dimColor = scaleColorByBrightness(dimColor, safeBrightness);
            setLedPair(i, state.foldPoint, dimColor);
        }
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderDualPulse(const LedState& state) {
    const unsigned long now = millis();

    if (_pulse2Pos == 0 && _pulse1Pos == 0) {
        _pulse2Pos = state.foldPoint / 2;
    }

    uint16_t pulseSpeed = map(state.speed, 1, 255, 60, 1);

    if (now - _lastDualPulseUpdate > pulseSpeed) {
        _pulse1Pos = (_pulse1Pos + 1) % state.foldPoint;
        _pulse2Pos = (_pulse2Pos > 0) ? (_pulse2Pos - 1) : (state.foldPoint - 1);
        _lastDualPulseUpdate = now;
    }

    CRGB baseColor = CRGB(state.r, state.g, state.b);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    const uint8_t pulseWidth = 10;

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        int16_t dist1 = abs((int16_t)i - (int16_t)_pulse1Pos);
        int16_t dist2 = abs((int16_t)i - (int16_t)_pulse2Pos);

        uint8_t brightness = 150;
        if (dist1 < pulseWidth) {
            brightness = max(brightness, (uint8_t)map(dist1, 0, pulseWidth, 255, 150));
        }
        if (dist2 < pulseWidth) {
            brightness = max(brightness, (uint8_t)map(dist2, 0, pulseWidth, 255, 150));
        }

        CRGB dualPulseColor = baseColor;
        dualPulseColor.fadeToBlackBy(255 - brightness);
        dualPulseColor = scaleColorByBrightness(dualPulseColor, safeBrightness);
        setLedPair(i, state.foldPoint, dualPulseColor);
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderRainbowBlade(const LedState& state) {
    uint8_t hueStep = map(state.speed, 1, 255, 1, 15);
    if (hueStep == 0) hueStep = 1;

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        uint8_t hue = _rainbowHue + (i * 256 / state.foldPoint);
        CRGB rainbowColor = CHSV(hue, 255, 255);
        setLedPair(i, state.foldPoint, rainbowColor);
    }

    _rainbowHue += hueStep;
}

// ═══════════════════════════════════════════════════════════
// GESTURE-TRIGGERED EFFECTS
// ═══════════════════════════════════════════════════════════

void LedEffectEngine::renderIgnition(const LedState& state) {
    const unsigned long now = millis();
    uint16_t ignitionSpeed = map(state.speed, 1, 255, 100, 1);

    if (now - _lastIgnitionUpdate > ignitionSpeed) {
        if (_ignitionProgress < state.foldPoint) {
            _ignitionProgress++;
        } else {
            _ignitionProgress = 0;  // Loop
        }
        _lastIgnitionUpdate = now;
    }

    fill_solid(_leds, _numLeds, CRGB::Black);
    CRGB color = CRGB(state.r, state.g, state.b);

    for (uint16_t i = 0; i < _ignitionProgress; i++) {
        if (i >= _ignitionProgress - 5 && i < _ignitionProgress) {
            uint8_t fade = map(i, _ignitionProgress - 5, _ignitionProgress - 1, 100, 255);
            CRGB fadedColor = color;
            fadedColor.fadeToBlackBy(255 - fade);
            setLedPair(i, state.foldPoint, fadedColor);
        } else {
            setLedPair(i, state.foldPoint, color);
        }
    }
}

void LedEffectEngine::renderRetraction(const LedState& state) {
    const unsigned long now = millis();
    uint16_t retractionSpeed = map(state.speed, 1, 255, 100, 1);

    if (_retractionProgress == 0) {
        _retractionProgress = state.foldPoint;
    }

    if (now - _lastRetractionUpdate > retractionSpeed) {
        if (_retractionProgress > 0) {
            _retractionProgress--;
        } else {
            _retractionProgress = state.foldPoint;  // Loop
        }
        _lastRetractionUpdate = now;
    }

    fill_solid(_leds, _numLeds, CRGB::Black);
    CRGB color = CRGB(state.r, state.g, state.b);

    for (uint16_t i = 0; i < _retractionProgress; i++) {
        if (i >= _retractionProgress - 5 && i < _retractionProgress) {
            uint8_t fade = map(i, _retractionProgress - 5, _retractionProgress - 1, 100, 255);
            CRGB fadedColor = color;
            fadedColor.fadeToBlackBy(255 - fade);
            setLedPair(i, state.foldPoint, fadedColor);
        } else {
            setLedPair(i, state.foldPoint, color);
        }
    }
}

void LedEffectEngine::renderClash(const LedState& state) {
    const unsigned long now = millis();

    // Trigger clash periodically (for manual effect mode)
    if (!_clashActive && now - _lastClashTrigger > 3000) {
        _clashActive = true;
        _clashBrightness = 255;
        _lastClashTrigger = now;
    }

    if (_clashActive) {
        _clashBrightness = qsub8(_clashBrightness, 15);
        if (_clashBrightness == 0) {
            _clashActive = false;
        }
    }

    CRGB baseColor = CRGB(state.r, state.g, state.b);
    CRGB flashColor = CRGB(255, 255, 255);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        CRGB clashColor = blend(baseColor, flashColor, _clashBrightness);
        clashColor = scaleColorByBrightness(clashColor, safeBrightness);
        setLedPair(i, state.foldPoint, clashColor);
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

// ═══════════════════════════════════════════════════════════
// MODE MANAGEMENT
// ═══════════════════════════════════════════════════════════

bool LedEffectEngine::checkModeTimeout(uint32_t now) {
    switch (_mode) {
        case Mode::IGNITION_ACTIVE:
            return (now - _modeStartTime) > 2000;  // 2 seconds

        case Mode::RETRACT_ACTIVE:
            return (now - _modeStartTime) > 2000;  // 2 seconds

        case Mode::CLASH_ACTIVE:
            return (now - _modeStartTime) > 500;   // 500ms

        default:
            return false;
    }
}

void LedEffectEngine::handleGestureTriggers(MotionProcessor::GestureType gesture, uint32_t now) {
    if (_mode != Mode::IDLE) {
        // Already in override mode, ignore new gestures
        return;
    }

    switch (gesture) {
        case MotionProcessor::GestureType::IGNITION:
            _mode = Mode::IGNITION_ACTIVE;
            _modeStartTime = now;
            _ignitionProgress = 0;
            _lastIgnitionUpdate = now;
            Serial.println("[LED] IGNITION effect triggered by gesture!");
            break;

        case MotionProcessor::GestureType::RETRACT:
            _mode = Mode::RETRACT_ACTIVE;
            _modeStartTime = now;
            _retractionProgress = 0;  // Will be initialized in render
            _lastRetractionUpdate = now;
            Serial.println("[LED] RETRACT effect triggered by gesture!");
            break;

        case MotionProcessor::GestureType::CLASH:
            _mode = Mode::CLASH_ACTIVE;
            _modeStartTime = now;
            _clashActive = true;
            _clashBrightness = 255;
            _lastClashTrigger = now;
            Serial.println("[LED] CLASH effect triggered by gesture!");
            break;

        case MotionProcessor::GestureType::SWING:
            // SWING doesn't override mode, just enhances current effect
            // (perturbations already applied in base rendering)
            break;

        default:
            break;
    }
}
