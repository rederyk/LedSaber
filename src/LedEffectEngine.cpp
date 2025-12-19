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
    _pulseCharge(0),
    _pulseCharging(true),
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
                renderRainbow(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "breathe") {
                renderBreathe(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "flicker") {
                renderFlicker(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "unstable") {
                renderUnstable(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "pulse") {
                renderPulse(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "dual_pulse") {
                renderDualPulse(state, motion ? motion->perturbationGrid : nullptr);
            } else if (state.effect == "rainbow_blade") {
                renderRainbowBlade(state, motion ? motion->perturbationGrid : nullptr);
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

    // Apply DYNAMIC perturbations - blade breathes with motion
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

        // Sample multiple rows for fuller effect
        uint8_t maxPerturbation = 0;
        for (uint8_t row = 2; row <= 4; row++) {
            maxPerturbation = max(maxPerturbation, perturbationGrid[row][col]);
        }

        if (maxPerturbation > 10) {
            // BREATHING EFFECT: motion makes blade pulse locally
            // Slightly more reactive with subtle shimmer
            uint8_t breathEffect = scale8(maxPerturbation, 80);  // Reduced from 100 for less fade
            uint8_t randomNoise = random8(breathEffect / 4);     // Less random variation

            CRGB perturbedColor = baseColor;
            perturbedColor.fadeToBlackBy(breathEffect / 3 + randomNoise);  // Less fade = more visible

            perturbedColor = scaleColorByBrightness(perturbedColor, safeBrightness);
            setLedPair(i, state.foldPoint, perturbedColor);
        } else {
            // Stable area: full color
            CRGB scaledColor = scaleColorByBrightness(baseColor, safeBrightness);
            setLedPair(i, state.foldPoint, scaledColor);
        }
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderRainbow(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    uint8_t step = map(state.speed, 1, 255, 1, 15);
    if (step == 0) step = 1;

    if (perturbationGrid == nullptr) {
        // No motion: classic rainbow
        fill_rainbow(_leds, _numLeds, _hue, 256 / _numLeds);
    } else {
        // MOTION SHIMMER: movement creates saturation/brightness waves
        for (uint16_t i = 0; i < _numLeds; i++) {
            uint8_t hue = _hue + (i * 256 / _numLeds);

            // Map physical LED to grid column
            uint16_t logicalPos = (i < _numLeds / 2) ? i : (_numLeds - 1 - i);
            uint8_t col = map(logicalPos, 0, state.foldPoint - 1, 0, 7);

            // Average perturbation
            uint16_t perturbSum = 0;
            for (uint8_t row = 1; row <= 4; row++) {
                perturbSum += perturbationGrid[row][col];
            }
            uint8_t avgPerturbation = perturbSum / 4;

            // Motion creates shimmer: vary saturation and brightness
            uint8_t saturation = 255;
            uint8_t brightness = 255;

            if (avgPerturbation > 15) {
                // Shimmer effect: reduce saturation, boost brightness
                saturation = 255 - scale8(avgPerturbation, 80);
                brightness = 255;  // Keep bright when moving
            }

            _leds[i] = CHSV(hue, saturation, brightness);
        }
    }

    _hue += step;
}

void LedEffectEngine::renderBreathe(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    uint8_t breath = beatsin8(state.speed, 0, 255);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    if (perturbationGrid == nullptr) {
        // No motion: classic breathe
        fill_solid(_leds, _numLeds, CRGB(state.r, state.g, state.b));
        _breathOverride = scale8(breath, safeBrightness);
    } else {
        // MOTION BREATHING: movement adds local breath variations
        CRGB baseColor = CRGB(state.r, state.g, state.b);

        for (uint16_t i = 0; i < state.foldPoint; i++) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

            // Sample perturbation
            uint16_t perturbSum = 0;
            for (uint8_t row = 2; row <= 4; row++) {
                perturbSum += perturbationGrid[row][col];
            }
            uint8_t avgPerturbation = perturbSum / 3;

            // Motion modulates breath: creates wave-like breathing
            uint8_t localBreath = breath;
            if (avgPerturbation > 20) {
                // Phase shift based on perturbation (creates traveling waves)
                uint8_t phaseShift = scale8(avgPerturbation, 60);
                localBreath = qadd8(localBreath, phaseShift);
            }

            CRGB breathColor = baseColor;
            breathColor.fadeToBlackBy(255 - localBreath);

            setLedPair(i, state.foldPoint, breathColor);
        }

        _breathOverride = scale8(breath, safeBrightness);
    }
}

void LedEffectEngine::renderFlicker(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    CRGB baseColor = CRGB(state.r, state.g, state.b);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    uint8_t flickerIntensity = state.speed;

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        uint8_t noise = random8(flickerIntensity);

        // KYLO REN STYLE: Motion perturbations VIOLENTLY disturb the blade
        if (perturbationGrid != nullptr) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

            // Sample multiple rows to create wider perturbation effect
            uint8_t perturbSum = 0;
            for (uint8_t row = 2; row <= 4; row++) {  // Center 3 rows
                perturbSum = qadd8(perturbSum, perturbationGrid[row][col] / 3);
            }

            // AGGRESSIVE: Motion adds MAJOR instability (up to 200% of base flicker)
            uint8_t motionBoost = scale8(perturbSum, 255);  // Maximum amplify perturbation
            noise = qadd8(noise, motionBoost);

            // Add random spikes when motion is high (mimics unstable plasma)
            if (perturbSum > 60 && random8() < 80) {
                noise = qadd8(noise, random8(60, 140));
            }
        }

        // Clamp noise and convert to brightness - darker blacks for more contrast
        uint8_t brightness = 255 - min(noise, (uint8_t)220);  // Allow darker blacks

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

    // Add sparks (base randomness + MOTION-TRIGGERED PLASMA ERUPTIONS)
    uint8_t sparkChance = state.speed / 2;

    if (perturbationGrid != nullptr) {
        // MOTION CREATES PLASMA CHAOS: perturbation triggers eruptions
        for (uint8_t col = 0; col < 8; col++) {
            uint8_t maxPerturbation = 0;
            for (uint8_t row = 0; row < 6; row++) {
                maxPerturbation = max(maxPerturbation, perturbationGrid[row][col]);
            }

            // Motion-triggered eruptions: MORE AGGRESSIVE
            if (maxPerturbation > 30) {
                uint16_t pos = map(col, 0, 7, 0, maxIndex - 1);

                // High motion = EXPLOSIVE sparks
                uint8_t eruptionChance = sparkChance + scale8(maxPerturbation, 200);  // Increased from 150
                if (random8() < eruptionChance) {
                    // VIOLENT eruption: add major heat
                    _unstableHeat[pos] = qadd8(_unstableHeat[pos], random8(180, 255));  // Increased minimum

                    // Spread chaos to neighbors (plasma arc effect)
                    if (pos > 0) {
                        _unstableHeat[pos - 1] = qadd8(_unstableHeat[pos - 1], random8(100, 180));  // Increased
                    }
                    if (pos < maxIndex - 1) {
                        _unstableHeat[pos + 1] = qadd8(_unstableHeat[pos + 1], random8(100, 180));  // Increased
                    }
                }
            }
        }
    }

    // Base random sparks (balanced frequency)
    if (random8() < sparkChance) {
        uint16_t pos = random16(maxIndex);
        _unstableHeat[pos] = qadd8(_unstableHeat[pos], random8(120, 200));  // Increased range
    }

    // Render with INVERTED mapping: high heat = DARK (perturbations are dark/off)
    for (uint16_t i = 0; i < maxIndex; i++) {
        // INVERTED: heat makes LED darker (perturbations = dark spots)
        uint8_t heatBrightness = _unstableHeat[i];
        // Inverted mapping: 0 heat = bright (255), max heat = dark (60)
        uint8_t brightness = 255 - scale8(heatBrightness, 195);  // 255 -> 60 range (high contrast)

        CRGB unstableColor = baseColor;
        unstableColor.fadeToBlackBy(255 - brightness);
        unstableColor = scaleColorByBrightness(unstableColor, safeBrightness);

        setLedPair(i, state.foldPoint, unstableColor);
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderPulse(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    const unsigned long now = millis();

    // Speed calculation based on folded index for synchronization
    // Total travel distance = foldPoint + pulseWidth (to exit completely from tip)
    const uint8_t pulseWidth = 15;
    uint16_t totalDistance = state.foldPoint + pulseWidth;

    // Calculate timing for seamless cycle: charge + travel time
    uint16_t chargeSpeed = map(state.speed, 1, 255, 30, 3);   // Slower charge (piano)
    uint16_t travelSpeed = map(state.speed, 1, 255, 20, 1);   // Travel speed

    if (now - _lastPulseUpdate > (_pulseCharging ? chargeSpeed : travelSpeed)) {
        if (_pulseCharging) {
            // CHARGING PHASE: accumulate plasma at base
            _pulseCharge += 15;  // Gradual charge increase
            if (_pulseCharge >= 255) {
                // Charge complete, launch pulse
                _pulseCharging = false;
                _pulsePosition = 0;
                _pulseCharge = 255;
            }
        } else {
            // TRAVELING PHASE: move pulse along blade
            _pulsePosition++;
            if (_pulsePosition >= totalDistance) {
                // Pulse has exited tip, restart charge cycle (SEAMLESS)
                _pulseCharging = true;
                _pulsePosition = 0;
                _pulseCharge = 0;
            }
        }
        _lastPulseUpdate = now;
    }

    CRGB baseColor = CRGB(state.r, state.g, state.b);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        uint8_t brightness = 80;  // Dark base

        if (_pulseCharging) {
            // CHARGING: show growing plasma ball at base
            if (i < 10) {  // Charge zone at base (first 10 LEDs)
                uint8_t chargeDistance = i;
                // Brighter at base, fades toward position 10
                uint8_t chargeBrightness = map(chargeDistance, 0, 10, 255, 100);
                // Scale by charge level (0-255)
                chargeBrightness = scale8(chargeBrightness, _pulseCharge);
                brightness = max(brightness, chargeBrightness);
            }
        } else {
            // TRAVELING: pulse moves along blade
            int16_t distance = abs((int16_t)i - (int16_t)_pulsePosition);

            if (distance < pulseWidth) {
                // Main pulse body
                brightness = map(distance, 0, pulseWidth, 255, 150);
            } else if (distance < pulseWidth + 5) {
                // Trailing halo
                brightness = map(distance, pulseWidth, pulseWidth + 5, 150, 80);
            }
        }

        // MOTION RIPPLES: movement creates brightness waves
        if (perturbationGrid != nullptr) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

            uint16_t perturbSum = 0;
            for (uint8_t row = 1; row <= 4; row++) {
                perturbSum += perturbationGrid[row][col];
            }
            uint8_t avgPerturbation = perturbSum / 4;

            if (avgPerturbation > 20) {
                uint8_t rippleBoost = scale8(avgPerturbation, 100);
                brightness = qadd8(brightness, rippleBoost);
            }
        }

        CRGB pulseColor = baseColor;
        pulseColor.fadeToBlackBy(255 - brightness);
        pulseColor = scaleColorByBrightness(pulseColor, safeBrightness);
        setLedPair(i, state.foldPoint, pulseColor);
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderDualPulse(const LedState& state, const uint8_t perturbationGrid[6][8]) {
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

        // Higher base brightness (180 instead of 150) - more difficult to darken
        uint8_t brightness = 180;
        if (dist1 < pulseWidth) {
            // Brighter peak (255 -> 200 instead of 255 -> 150)
            brightness = max(brightness, (uint8_t)map(dist1, 0, pulseWidth, 255, 200));
        }
        if (dist2 < pulseWidth) {
            brightness = max(brightness, (uint8_t)map(dist2, 0, pulseWidth, 255, 200));
        }

        // DUAL MOTION RIPPLES: movement creates interference patterns
        if (perturbationGrid != nullptr) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

            // Sample perturbation
            uint16_t perturbSum = 0;
            for (uint8_t row = 1; row <= 4; row++) {
                perturbSum += perturbationGrid[row][col];
            }
            uint8_t avgPerturbation = perturbSum / 4;

            // Motion creates interference: BOOST brightness more, darken LESS
            if (avgPerturbation > 25) {
                // Alternating pattern based on position
                if ((i + (now / 100)) % 2 == 0) {
                    // Increased brightening (80 instead of 60)
                    brightness = qadd8(brightness, scale8(avgPerturbation, 80));
                } else {
                    // Reduced darkening (15 instead of 30) - harder to darken
                    brightness = qsub8(brightness, scale8(avgPerturbation, 15));
                }
            }
        }

        CRGB dualPulseColor = baseColor;
        dualPulseColor.fadeToBlackBy(255 - brightness);
        dualPulseColor = scaleColorByBrightness(dualPulseColor, safeBrightness);
        setLedPair(i, state.foldPoint, dualPulseColor);
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

void LedEffectEngine::renderRainbowBlade(const LedState& state, const uint8_t perturbationGrid[6][8]) {
    uint8_t hueStep = map(state.speed, 1, 255, 1, 15);
    if (hueStep == 0) hueStep = 1;

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        uint8_t hue = _rainbowHue + (i * 256 / state.foldPoint);
        uint8_t saturation = 255;
        uint8_t brightness = 255;

        // CHROMATIC ABERRATION: motion creates color shifts and sparkles
        if (perturbationGrid != nullptr) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

            // Sample perturbation
            uint16_t perturbSum = 0;
            for (uint8_t row = 2; row <= 4; row++) {
                perturbSum += perturbationGrid[row][col];
            }
            uint8_t avgPerturbation = perturbSum / 3;

            if (avgPerturbation > 8) {  // More sensitive threshold
                // Motion creates chromatic shimmer: hue shift + saturation pulse
                int8_t hueShift = (avgPerturbation / 4) - 32;  // ±32 hue shift
                hue = hue + hueShift;

                // Sparkle effect: reduce saturation = more white (increased effect)
                saturation = 255 - scale8(avgPerturbation, 160);

                // Random sparkles on high motion (increased probability)
                if (avgPerturbation > 50 && random8() < 100) {
                    saturation = random8(100, 200);  // Deep sparkle
                    brightness = 255;
                }
            }
        }

        CRGB rainbowColor = CHSV(hue, saturation, brightness);
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
