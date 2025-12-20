#include "LedEffectEngine.h"
#include <esp_sleep.h>

static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 112;

LedEffectEngine::LedEffectEngine(CRGB* leds, uint16_t numLeds) :
    _leds(leds),
    _numLeds(numLeds),
    _mode(Mode::IDLE),
    _modeStartTime(0),
    _suppressGestureOverrides(false),
    _deepSleepRequested(false),
    _ledStateRef(nullptr),
    _hue(0),
    _ignitionProgress(0),
    _lastIgnitionUpdate(0),
    _ignitionOneShot(false),
    _ignitionCompleted(false),
    _retractionProgress(0),
    _lastRetractionUpdate(0),
    _retractionOneShot(false),
    _retractionCompleted(false),
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
    _lastUpdate(0),
    _lastSecondarySpawn(0),
    _mainPulseWidth(20),
    _visualOffset(0.0f),
    _lastMotionTime(0)
{
    memset(_unstableHeat, 0, sizeof(_unstableHeat));
    // Initialize secondary pulses
    for (uint8_t i = 0; i < 5; i++) {
        _secondaryPulses[i].active = false;
        _secondaryPulses[i].position = 0;
        _secondaryPulses[i].birthTime = 0;
        _secondaryPulses[i].velocityPhase = 0;
        _secondaryPulses[i].size = 1;          // Normal size
        _secondaryPulses[i].brightness = 200;  // Normal brightness
        _secondaryPulses[i].width = 20;        // Default width
    }
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

    // CRITICAL: If blade is disabled, only allow IGNITION or RETRACT animations
    // All other modes should show black (blade off)
    if (!state.bladeEnabled) {
        // Allow ignition and retraction animations even when blade is disabled
        if (_mode == Mode::IGNITION_ACTIVE) {
            renderIgnition(state);
            FastLED.show();
            _lastUpdate = now;
            return;
        } else if (_mode == Mode::RETRACT_ACTIVE) {
            renderRetraction(state);
            FastLED.show();
            _lastUpdate = now;
            return;
        } else {
            // Blade is off and no animation running - keep LEDs off
            fill_solid(_leds, _numLeds, CRGB::Black);
            FastLED.show();
            _lastUpdate = now;
            return;
        }
    }

    // In some modes we don't want gestures to override the running effect
    // (e.g. Dual Pong/Dual Pulse manages its own "collision clash").
    _suppressGestureOverrides = (state.effect == "dual_pulse");

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
            } else if (state.effect == "rainbow_effect") {
                renderRainbowEffect(state, motion ? motion->perturbationGrid : nullptr, motion);
            } else if (state.effect == "chrono_hybrid" ||"chrono_hybrid" || "clock" ) {
                renderChronoHybrid(state, motion ? motion->perturbationGrid : nullptr, motion);
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

uint8_t LedEffectEngine::getHueFromDirection(OpticalFlowDetector::Direction dir) {
    switch(dir) {
        case OpticalFlowDetector::Direction::UP:         return 0;    // Rosso
        case OpticalFlowDetector::Direction::DOWN:       return 160;  // Blu
        case OpticalFlowDetector::Direction::LEFT:       return 96;   // Verde
        case OpticalFlowDetector::Direction::RIGHT:      return 64;   // Giallo
        case OpticalFlowDetector::Direction::UP_LEFT:    return 48;   // Arancione
        case OpticalFlowDetector::Direction::UP_RIGHT:   return 32;   // Giallo-rosso
        case OpticalFlowDetector::Direction::DOWN_LEFT:  return 128;  // Ciano
        case OpticalFlowDetector::Direction::DOWN_RIGHT: return 192;  // Viola
        case OpticalFlowDetector::Direction::NONE:
        default:                                          return 0;    // Default rosso
    }
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

    // PERTURBATION ACCELERATION: calculate average motion intensity
    uint8_t globalPerturbation = 0;
    if (perturbationGrid != nullptr) {
        uint16_t totalPerturb = 0;
        uint8_t samples = 0;
        for (uint8_t row = 1; row <= 4; row++) {
            for (uint8_t col = 0; col < 8; col++) {
                totalPerturb += perturbationGrid[row][col];
                samples++;
            }
        }
        globalPerturbation = totalPerturb / samples;
    }

    // ACCELERAZIONE PROGRESSIVA NATURALE
    // Il pulse accelera in modo fluido dalla velocità base alla velocità massima

    // Calcola velocità massima raggiungibile con motion
    uint16_t baseSpeed = state.speed;
    uint16_t maxSpeed = baseSpeed;

    if (globalPerturbation > 3) {
        // Motion boost: aumenta la velocità massima raggiungibile
        // Uso scaling quadratico per rendere il motion più impattante
        uint16_t motionBoost = (globalPerturbation * globalPerturbation) / 4;
        uint16_t boostedSpeed = baseSpeed + motionBoost;
        maxSpeed = (boostedSpeed > 255) ? 255 : boostedSpeed;
    }

    // PULSE WIDTH inversely proportional to max speed: faster = narrower pulse
    // Speed 1 (slow) = wide pulse (60 pixels), Speed 255 (fast) = narrow pulse (2 pixels)
    uint8_t targetPulseWidth = map(maxSpeed, 1, 255, 30, 2);

    // Update main pulse width ONLY at start of cycle to prevent flickering
    if (_pulsePosition == 0) {
        _mainPulseWidth = targetPulseWidth;
    }

    // Travel distance for pulse to completely exit from tip
    // Add extra width for entry phase (pulse starts "outside" the blade)
    uint16_t totalDistance = state.foldPoint + 2 * _mainPulseWidth;

    // NATURAL ACCELERATION CURVE: velocità aumenta progressivamente lungo il percorso
    // Usa una curva ease-out esponenziale: v(x) = v_base + (v_max - v_base) * (1 - e^(-k*x))

    // Normalizza la posizione corrente (0.0 = inizio, 1.0 = fine)
    float normalizedPos = (float)_pulsePosition / (float)totalDistance;

    // Coefficiente di accelerazione (maggiore = accelera più rapidamente)
    // 3.0 = accelerazione moderata, naturale
    float accelCoeff = 3.0f;

    // Calcola il fattore di accelerazione usando curva esponenziale
    // 1 - e^(-k*x) parte da 0 e arriva asintoticamente a 1
    float accelFactor = 1.0f - expf(-accelCoeff * normalizedPos);

    // Interpola tra velocità base e velocità massima
    uint16_t currentSpeed = baseSpeed + (uint16_t)((maxSpeed - baseSpeed) * accelFactor);

    // Converti velocità in delay (ms): velocità alta = delay basso
    // Range: speed 1 -> 120ms, speed 255 -> 1ms
    uint16_t travelSpeed;
    if (currentSpeed < 20) {
        travelSpeed = map(currentSpeed, 1, 20, 120, 50);
    } else if (currentSpeed < 100) {
        travelSpeed = map(currentSpeed, 20, 100, 50, 10);
    } else {
        travelSpeed = map(currentSpeed, 100, 255, 10, 1);
    }

    // CONTINUOUS PULSE FLOW: always moving, no charging phase
    if (now - _lastPulseUpdate >= travelSpeed) {
        // Calculate steps based on elapsed time to ensure speed is independent of frame rate
        uint16_t steps = (now - _lastPulseUpdate) / travelSpeed;
        _pulsePosition += steps;
        while (_pulsePosition >= totalDistance) {
            _pulsePosition -= totalDistance;
        }
        // Keep phase alignment
        _lastPulseUpdate = now - ((now - _lastPulseUpdate) % travelSpeed);
    }

    // SPAWN SECONDARY PULSES (plasma discharge effect) - SOLO CON MOVIMENTO
    // I pulsi secondari creano un effetto "scarica al plasma" scenografico
    uint8_t spawnChance = 0;

    // Spawn DIPENDE COMPLETAMENTE dal movimento
    if (globalPerturbation > 15) {  // Soglia minima: DEVE esserci movimento
        // Spawn chance aumenta esponenzialmente con il movimento
        // Da 15% (movimento leggero) a 120% (movimento intenso)
        spawnChance = map(globalPerturbation, 15, 255, 15, 120);

        // Boost aggiuntivo per movimento molto intenso (effetto "tempesta al plasma")
        if (globalPerturbation > 60) {
            uint8_t extraBoost = scale8(globalPerturbation - 60, 100);
            spawnChance = qadd8(spawnChance, extraBoost);
        }
    }
    // NESSUNO spawn se globalPerturbation <= 15 (nessun movimento)

    // Try to spawn secondary pulses around main pulse area - solo se c'è movimento!
    if (spawnChance > 0 && now - _lastSecondarySpawn > 80 && random8() < spawnChance) {
        // Find an inactive slot
        for (uint8_t i = 0; i < 5; i++) {
            if (!_secondaryPulses[i].active) {
                // Spawn intorno all'area del pulse principale (±30 LED)
                // Calculate effective center considering the entry offset
                int16_t spawnCenter = (int16_t)_pulsePosition - _mainPulseWidth;
                int16_t spawnOffset = random16(60) - 30;  // -30 a +30 LED dall'impulso principale
                int16_t spawnPos = spawnCenter + spawnOffset;

                // Clamp alla blade (evita spawn forzato in punta che crea accumulo)
                if (spawnPos < 0) spawnPos = 0;
                if (spawnPos >= state.foldPoint) continue; // Skip spawn se fuori dalla lama

                _secondaryPulses[i].position = spawnPos;
                _secondaryPulses[i].width = targetPulseWidth; // Lock width at spawn time
                _secondaryPulses[i].birthTime = now & 0xFFFF;
                _secondaryPulses[i].velocityPhase = random8();  // Random starting phase
                _secondaryPulses[i].active = true;
                _secondaryPulses[i].size = 1;          // Normal size initially
                // Brightness variabile: alcuni spawn più luminosi (più scenografico)
                _secondaryPulses[i].brightness = random8(180, 240);
                _lastSecondarySpawn = now;
                break;
            }
        }
    }

    // UPDATE SECONDARY PULSES with same velocity as main pulse
    for (uint8_t i = 0; i < 5; i++) {
        if (_secondaryPulses[i].active) {
            // Secondary pulses use SAME speed as main pulse (synchronized movement)
            // Questo garantisce che tutti i pulse si muovano insieme in modo coerente
            // Use individual timing to handle spawn time offsets
            uint16_t timeSinceLast = (uint16_t)((now & 0xFFFF) - _secondaryPulses[i].birthTime);
            
            if (timeSinceLast >= travelSpeed) {
                uint16_t secSteps = timeSinceLast / travelSpeed;
                _secondaryPulses[i].position += secSteps;
                
                // Update last update time (birthTime is reused as lastUpdate)
                _secondaryPulses[i].birthTime = (uint16_t)(now & 0xFFFF) - (timeSinceLast % travelSpeed);

                // Kill pulse if it exits the blade
                if (_secondaryPulses[i].position >= state.foldPoint + _secondaryPulses[i].width) {
                    _secondaryPulses[i].active = false;
                }
            }
        }
    }

    // FUSION LOGIC: check for collisions and merge plasmoidi
    for (uint8_t i = 0; i < 5; i++) {
        if (!_secondaryPulses[i].active) continue;

        for (uint8_t j = i + 1; j < 5; j++) {
            if (!_secondaryPulses[j].active) continue;

            // Check if pulses are close enough to merge (within 8 LEDs)
            int16_t distance = abs((int16_t)_secondaryPulses[i].position - (int16_t)_secondaryPulses[j].position);

            if (distance <= 8) {
                // FUSION! Merge j into i, make it bigger and brighter
                _secondaryPulses[i].size = min(3, _secondaryPulses[i].size + 1);  // Increase size (max 3x)
                _secondaryPulses[i].brightness = 255;  // MAXIMUM BRIGHTNESS!

                // FIX: Non usare max() per la posizione! Questo causa un effetto "surfing" dove i pulse
                // saltano in avanti a catena, accelerando verso l'uscita e svuotando la striscia.
                // Manteniamo la posizione del pulse "predatore" (i), assorbendo solo energia da j.
                // _secondaryPulses[i].position = _secondaryPulses[i].position; // (Implicito)

                // Average velocity phase (keeps moving smoothly)
                _secondaryPulses[i].velocityPhase = (_secondaryPulses[i].velocityPhase + _secondaryPulses[j].velocityPhase) / 2;

                // Deactivate the absorbed pulse
                _secondaryPulses[j].active = false;
            }
        }
    }

    CRGB baseColor = CRGB(state.r, state.g, state.b);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        // CONTRASTO DINAMICO: base scura per evidenziare i pulsi
        // Con motion intenso, la base diventa ancora più scura (più drammatico)
        uint8_t baseBrightness = 100;
        if (globalPerturbation > 40) {
            // Motion intenso: base ancora più scura per contrasto maggiore
            baseBrightness = map(globalPerturbation, 40, 255, 50, 20);
        }
        uint8_t brightness = baseBrightness;

        // TRAVELING MAIN PULSE: gradiente fluido con core brillante
        int16_t effectiveCenter = (int16_t)_pulsePosition - _mainPulseWidth;
        int16_t distance = abs((int16_t)i - effectiveCenter);

        if (distance < _mainPulseWidth) {
            // Main pulse body: core ultra-brillante con falloff morbido
            // Il core è sempre a 255, il falloff dipende dalla larghezza
            brightness = map(distance, 0, _mainPulseWidth, 255, baseBrightness + 30);
        }

        // SECONDARY PULSES: più visibili e drammatici con FUSION SUPPORT
        for (uint8_t p = 0; p < 5; p++) {
            if (_secondaryPulses[p].active) {
                int16_t secDistance = abs((int16_t)i - (int16_t)_secondaryPulses[p].position);

                // Size increases with fusion: 1x, 1.5x, 2x per size 1, 2, 3
                uint8_t basePulseWidth = _secondaryPulses[p].width / 2;
                uint8_t secPulseWidth = (basePulseWidth * _secondaryPulses[p].size * 3) / 4;
                if (secPulseWidth < basePulseWidth) secPulseWidth = basePulseWidth;

                if (secDistance < secPulseWidth) {
                    // Brightness con contrasto aumentato per i secondari
                    uint8_t maxBrightness = _secondaryPulses[p].brightness;
                    // Falloff più lento = pulsi secondari più visibili
                    uint8_t secBrightness = map(secDistance, 0, secPulseWidth, maxBrightness, baseBrightness + 20);
                    brightness = max(brightness, secBrightness);
                }
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

    // ═══════════════════════════════════════════════════════════
    // DUAL PONG - ELEGANT PHYSICS SIMULATION
    // ═══════════════════════════════════════════════════════════
    // Due sfere rimbalzano elasticamente sulle estremità della lama
    // Il movimento è regolato dalla fisica del moto armonico con collisioni elastiche
    // La perturbazione imprime energia SOLO a UNA palla alla volta
    // Quando il sistema diventa troppo instabile, collassa e si rigenera

    // Static state variables (persistent across calls)
    static float ball1_pos = 0.0f;              // Posizione [0, foldPoint-1]
    static float ball2_pos = 0.0f;
    static float ball1_vel = 0.0f;              // Velocità (signed, pixel/ms)
    static float ball2_vel = 0.0f;
    static float ball1_mass = 1.0f;             // Massa (variabile con perturbazione!)
    static float ball2_mass = 1.0f;
    static uint8_t ball1_hue = 0;               // Colore HSV
    static uint8_t ball2_hue = 160;             // Colore complementare iniziale
    static bool ball1_active = true;
    static bool ball2_active = true;
    static bool singleBallMode = false;         // Modalità palla singola (post-collasso)
    static uint8_t nextBallHue = 0;             // Colore della prossima palla da spawnare
    static uint8_t spawnFlashBrightness = 0;    // Intensità del lampo di spawn
    static unsigned long lastCollapseTime = 0;
    static bool initialized = false;
    static int8_t perturbTarget = 0;            // -1 = ball1, +1 = ball2, 0 = none
    static uint8_t perturbAccumulator = 0;      // Accumula perturbazioni per sensibilità bassa
    static uint16_t collisionCount = 0;         // Conta le collisioni per debug
    static float ball1_tempMass = 0.0f;         // Massa temporanea aggiunta da motion
    static float ball2_tempMass = 0.0f;         // Massa temporanea aggiunta da motion
    static unsigned long ball1_invulnTime = 0;  // Tempo di invulnerabilità (grace period)
    static unsigned long ball2_invulnTime = 0;  // Tempo di invulnerabilità (grace period)
    static float collisionFlashPos = 0.0f;      // Centro flash collisione (pixel)
    static uint8_t collisionFusionFlash = 0;    // Intensità flash fusione (0-255)
    static uint8_t collisionWhiteCore = 0;      // Core bianco (solo impatti forti)
    static uint8_t collisionHueA = 0;
    static uint8_t collisionHueB = 160;
    static unsigned long ball1_crackleLast = 0;
    static unsigned long ball2_crackleLast = 0;
    static uint8_t ball1_crackle = 0;
    static uint8_t ball2_crackle = 0;
    static unsigned long ball1_lastEdgeSave = 0;
    static unsigned long ball2_lastEdgeSave = 0;
    static unsigned long ball1_edgeStuckSince = 0;
    static unsigned long ball2_edgeStuckSince = 0;

    // FIXED BASE SPEED (independent of state.speed parameter)
    const float FIXED_BASE_SPEED = 0.14f;  // pixel/ms (Aumentata per dinamicità)

    // First run initialization
    if (!initialized || now < 500) {
        // Posizioni iniziali simmetriche (1/4 e 3/4 della lama)
        ball1_pos = state.foldPoint * 0.25f;
        ball2_pos = state.foldPoint * 0.75f;

        // Velocità iniziali FISSE (opposte per equilibrio)
        ball1_vel = FIXED_BASE_SPEED;
        ball2_vel = -FIXED_BASE_SPEED;

        // Masse iniziali UGUALI (equilibrio perfetto)
        ball1_mass = 1.0f;
        ball2_mass = 1.0f;

        ball1_hue = 0;      // Rosso
        ball2_hue = 160;    // Ciano
        ball1_active = true;
        ball2_active = true;
        singleBallMode = false;
        spawnFlashBrightness = 0;
        perturbTarget = 0;
        perturbAccumulator = 0;
        collisionCount = 0;
        ball1_invulnTime = 0;
        ball2_invulnTime = 0;
        ball1_crackleLast = 0;
        ball2_crackleLast = 0;
        ball1_crackle = 0;
        ball2_crackle = 0;
        ball1_edgeStuckSince = 0;
        ball2_edgeStuckSince = 0;
        collisionFusionFlash = 0;
        collisionWhiteCore = 0;
        initialized = true;

        Serial.println("[DUAL_PONG] Initialized with MASS-based physics (EASY MODE)");
    }

    // ═══════════════════════════════════════════════════════════
    // PERTURBATION SYSTEM - Bassa sensibilità, una palla alla volta
    // ═══════════════════════════════════════════════════════════

    uint8_t globalPerturbation = 0;
    if (perturbationGrid != nullptr && !singleBallMode) {
        // Calcola perturbazione media su tutta la lama
        uint16_t totalPerturb = 0;
        uint8_t samples = 0;
        for (uint8_t row = 1; row <= 4; row++) {
            for (uint8_t col = 0; col < 8; col++) {
                totalPerturb += perturbationGrid[row][col];
                samples++;
            }
        }
        globalPerturbation = totalPerturb / samples;

        // PERTURBAZIONE AGGIUNGE MASSA TEMPORANEA (fisica realistica!)
        // Il motion aumenta DRASTICAMENTE la massa in modo temporaneo
        // Alta massa = palla difficile da spostare (si muove poco se colpita)
        // Bassa massa = palla normale (si muove normalmente)

        if (globalPerturbation > 15) {  // Soglia più alta: trigger più Difficile!
            perturbAccumulator = min(255, perturbAccumulator + (globalPerturbation / 4));  // Accumula più velocemente

            // Soglia di attivazione ridotta
            if (perturbAccumulator > 50) {  // Era 60, ora 40: si attiva prima! meglio 60
                // Scegli quale palla aumentare di massa (se non già scelta)
                if (perturbTarget == 0) {
                    perturbTarget = (random8() < 128) ? -1 : 1;
                    Serial.print("[DUAL_PONG] Mass boost target: Ball ");
                    Serial.println((perturbTarget == -1) ? "1" : "2");
                }

                // CALCOLA MASSA TEMPORANEA - MODALITÀ FACILE (drasticamente ridotta!)
                // Range: 0 (motion basso) -> 3.0 (motion altissimo!) - Era 20.0, ora 3.0 (6.6x meno!)
                float tempMassFromMotion = (globalPerturbation / 255.0f) * (globalPerturbation / 255.0f) * 3.0f;

                if (perturbTarget == -1 && ball1_active) {
                    // Aggiorna massa temporanea smoothly
                    ball1_tempMass = ball1_tempMass * 0.7f + tempMassFromMotion * 0.3f; // Smooth transition
                    ball1_tempMass = min(ball1_tempMass, 7.0f);  // Cap massimo ridotto da 20 a 3

                    // AGGIUNGI anche massa permanente (MOLTO più lentamente - quasi disabled)
                    // Era 0.003, ora 0.0005 (6x più lento)
                    float permMassIncrement = (globalPerturbation / 255.0f) * 0.005f;
                    ball1_mass += permMassIncrement;
                    ball1_mass = min(ball1_mass, 4.0f);  // Massa permanente max 1.8x (era 5x)
                } else if (perturbTarget == 1 && ball2_active) {
                    ball2_tempMass = ball2_tempMass * 0.7f + tempMassFromMotion * 0.3f;
                    ball2_tempMass = min(ball2_tempMass, 7.0f);

                    float permMassIncrement = (globalPerturbation / 255.0f) * 0.0005f;
                    ball2_mass += permMassIncrement;
                    ball2_mass = min(ball2_mass, 4.0f);
                }
            }
        } else {
            // Decay VELOCE della massa temporanea quando motion cala

            // MODALITÀ FACILE: SLINGSHOT BONUS BASSO
            // La palla accelera leggermente quando rilasci, ma MOLTO meno rispetto alla modalità normale
            // Bonus ridotto drasticamente per gameplay più prevedibile

            // Ball 1 Release - BONUS BASSO (era 0.08, ora 0.015)
            if (ball1_tempMass > 0.5f) {
                float bonus = ball1_tempMass * 0.1f; // Aumentato per premiare il colpo
                float targetSpeed = FIXED_BASE_SPEED + bonus;
                float dir = 1.0f; // Sempre verso l'avversario (Destra)

                // Riaccelerazione rapida verso la direzione corretta
                float snapFactor = 0.4f;
                ball1_vel = ball1_vel * (1.0f - snapFactor) + (dir * targetSpeed) * snapFactor;
            }

            // Ball 2 Release - BONUS BASSO
            if (ball2_tempMass > 0.5f) {
                float bonus = ball2_tempMass * 0.1f; // Aumentato per premiare il colpo
                float targetSpeed = FIXED_BASE_SPEED + bonus;
                float dir = -1.0f; // Sempre verso l'avversario (Sinistra)

                // Riaccelerazione rapida verso la direzione corretta
                float snapFactor = 0.4f;
                ball2_vel = ball2_vel * (1.0f - snapFactor) + (dir * targetSpeed) * snapFactor;
            }

            ball1_tempMass *= 0.85f;  // Decade rapidamente
            ball2_tempMass *= 0.85f;

            if (ball1_tempMass < 0.1f) ball1_tempMass = 0.0f;
            if (ball2_tempMass < 0.1f) ball2_tempMass = 0.0f;

            // Decay lento dell'accumulatore
            if (perturbAccumulator > 0) {
                perturbAccumulator = qsub8(perturbAccumulator, 3);
            }
            if (perturbAccumulator < 30) {
                perturbTarget = 0;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    // PHYSICS UPDATE - Collisioni elastiche con conservazione energia
    // ═══════════════════════════════════════════════════════════

    unsigned long deltaTime = now - _lastDualPulseUpdate;
    if (deltaTime > 100) deltaTime = 20;  // Clamp per evitare salti enormi

    if (deltaTime > 0) {
        float dt = deltaTime / 1000.0f;  // Converti in secondi per fisica

        // NESSUN impulso diretto! La massa influenza solo le collisioni
        // Le velocità rimangono costanti tra una collisione e l'altra (moto inerziale)

        // Store previous positions for collision detection (anti-tunneling)
        float old_b1 = ball1_pos;
        float old_b2 = ball2_pos;

        // Update positions (moto con drag proporzionale alla massa temporanea)
        if (ball1_active) {
            ball1_pos += ball1_vel * dt * 1000.0f;

            // DRAG/ATTRITO proporzionale alla massa temporanea - MODALITÀ FACILE (ridotto!)
            // Più massa temporanea = più rallenta, ma MOLTO meno rispetto a prima
            if (ball1_tempMass > 0.5f) {
                // Calcola drag factor: ridotto drasticamente
                // Max drag ora = 0.60 (era 0.98) - 60% invece di 98%!
                float dragFactor = min(ball1_tempMass / 5.0f, 0.98f);  // Era /20 max 0.98
                ball1_vel *= (1.0f - dragFactor * dt * 5.0f);  // Era 5.0, ora 2.0 (2.5x meno aggressivo)
            }

            // INVULNERABILITÀ: Se la palla è quasi ferma, attiva grace period
            if (abs(ball1_vel) < 0.03f && ball1_invulnTime == 0) {
                ball1_invulnTime = now;
                ball1_crackleLast = now;
                ball1_crackle = 0;
                Serial.println("[DUAL_PONG] Ball 1 SLOW - Grace period activated!");
            }
        }
        if (ball2_active) {
            ball2_pos += ball2_vel * dt * 1000.0f;

            // DRAG/ATTRITO proporzionale alla massa temporanea - MODALITÀ FACILE (ridotto!)
            if (ball2_tempMass > 0.5f) {
                float dragFactor = min(ball2_tempMass / 5.0f, 0.98f);  // Era /20 max 0.98
                ball2_vel *= (1.0f - dragFactor * dt * 5.0f);  // Era 5.0, ora 2.0
            }

            // INVULNERABILITÀ: Se la palla è quasi ferma, attiva grace period
            if (abs(ball2_vel) < 0.03f && ball2_invulnTime == 0) {
                ball2_invulnTime = now;
                ball2_crackleLast = now;
                ball2_crackle = 0;
                Serial.println("[DUAL_PONG] Ball 2 SLOW - Grace period activated!");
            }
        }

        // ELASTIC BOUNDARY COLLISIONS (coefficiente di restituzione = 1.0, energia conservata)
        if (ball1_active) {
            if (ball1_pos < 0.0f) {
                ball1_pos = -ball1_pos;  // Riflessione speculare
                ball1_vel = -ball1_vel;
                Serial.println("[DUAL_PONG] Ball 1 bounced at base");
            }
            if (ball1_pos >= state.foldPoint - 1) {
                float excess = ball1_pos - (state.foldPoint - 1);
                ball1_pos = (state.foldPoint - 1) - excess;
                ball1_vel = -ball1_vel;
                Serial.println("[DUAL_PONG] Ball 1 bounced at tip");
            }
        }

        if (ball2_active) {
            if (ball2_pos < 0.0f) {
                ball2_pos = -ball2_pos;
                ball2_vel = -ball2_vel;
                Serial.println("[DUAL_PONG] Ball 2 bounced at base");
            }
            if (ball2_pos >= state.foldPoint - 1) {
                float excess = ball2_pos - (state.foldPoint - 1);
                ball2_pos = (state.foldPoint - 1) - excess;
                ball2_vel = -ball2_vel;
                Serial.println("[DUAL_PONG] Ball 2 bounced at tip");
            }
        }

        // BALL-TO-BALL ELASTIC COLLISION (conservazione momento e energia)
        if (ball1_active && ball2_active) {
            // CHECK: Sospendi collisioni se una palla è triggerata (massa alta)
            // La palla triggerata diventa un "muro immobile" - nessuno scambio di massa!
            bool ball1_triggered = (ball1_tempMass > 1.5f);  // Soglia alta = triggerata
            bool ball2_triggered = (ball2_tempMass > 1.5f);

            // Rilevamento collisione robusto (previene tunneling)
            bool was_left = (old_b1 < old_b2);
            bool is_left = (ball1_pos < ball2_pos);
            bool crossed = (was_left != is_left);

            float dist = abs(ball1_pos - ball2_pos);
            const float collisionRadius = 8.0f;
            bool overlap = (dist < collisionRadius);

            if (crossed || overlap) {
                // 1. SEPARAZIONE POSIZIONALE (Evita compenetrazione)
                float midPoint = (ball1_pos + ball2_pos) / 2.0f;
                float sepDist = collisionRadius / 2.0f + 0.1f;

                if (was_left) {
                    ball1_pos = midPoint - sepDist;
                    ball2_pos = midPoint + sepDist;
                } else {
                    ball1_pos = midPoint + sepDist;
                    ball2_pos = midPoint - sepDist;
                }

                // 2. RISOLUZIONE VELOCITÀ - MODALITÀ SPECIALE SE UNA PALLA È TRIGGERATA
                bool approaching = false;
                if (was_left) {
                    approaching = (ball1_vel > ball2_vel);
                } else {
                    approaching = (ball2_vel > ball1_vel);
                }

                if (approaching) {
                    // CASO SPECIALE: Una palla triggerata (carica) = rimbalzo perfetto
                    if (ball1_triggered || ball2_triggered) {
                        // Sospendi scambio massa: la palla triggerata è "immobile", l'altra rimbalza
                        if (ball1_triggered && !ball2_triggered) {
                            ball2_vel = -ball2_vel; // Ball 2 rimbalza
                            // Ball 1 continua (immovable)
                        } else if (ball2_triggered && !ball1_triggered) {
                            ball1_vel = -ball1_vel; // Ball 1 rimbalza
                            // Ball 2 continua (immovable)
                        } else {
                            // Entrambe triggerate: rimbalzano entrambe
                            ball1_vel = -ball1_vel;
                            ball2_vel = -ball2_vel;
                        }

                        Serial.print("[DUAL_PONG] TRIGGERED Collision #");
                        Serial.print(collisionCount);
                        Serial.println(" - Immovable object interaction");
                    } else {
                        // COLLISIONE NORMALE: scambio massa standard
                        float m1 = ball1_mass + ball1_tempMass;
                        float m2 = ball2_mass + ball2_tempMass;
                        float totalMass = m1 + m2;

                        float v1 = ball1_vel;
                        float v2 = ball2_vel;

                        ball1_vel = ((m1 - m2) * v1 + 2.0f * m2 * v2) / totalMass;
                        ball2_vel = ((m2 - m1) * v2 + 2.0f * m1 * v1) / totalMass;

                        Serial.print("[DUAL_PONG] Normal Collision #");
                        Serial.print(collisionCount);
                        Serial.print(" | v1="); Serial.print(ball1_vel);
                        Serial.print(" v2="); Serial.println(ball2_vel);
                    }

                    // Collisione: "fusion flash" + core bianco solo per impatti forti
                    float relativeSpeed = abs(ball1_vel - ball2_vel);
                    float impact = min(1.0f, relativeSpeed / 0.25f);  // 0..1
                    uint8_t impactBrightness = (uint8_t)(60.0f + impact * 140.0f); // 60..200
                    collisionFlashPos = midPoint;
                    collisionHueA = ball1_hue;
                    collisionHueB = ball2_hue;
                    collisionFusionFlash = max(collisionFusionFlash, impactBrightness);

                    const float strongImpactThreshold = 0.18f;
                    if (relativeSpeed >= strongImpactThreshold) {
                        float strongT = min(1.0f, (relativeSpeed - strongImpactThreshold) / 0.20f);
                        uint8_t core = (uint8_t)(70.0f + strongT * 160.0f); // 70..230
                        collisionWhiteCore = max(collisionWhiteCore, core);
                    }

                    collisionCount++;
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    // STABILITY CHECK - Collasso con GRACE PERIOD (invulnerabilità)
    // ═══════════════════════════════════════════════════════════

    // GRACE PERIOD: 1500ms (1.5 secondi) per recuperare velocità
    const unsigned long GRACE_PERIOD = 300; // Ridotto a 1s per rendere la sconfitta più probabile

    // COLLASSO quando una palla diventa troppo lenta E il grace period scade
    const float minVelocity = 0.05f;  // pixel/ms (Soglia alzata: muore prima)

    // Reset grace period se la palla recupera velocità
    if (ball1_invulnTime > 0 && abs(ball1_vel) >= 0.06f) {
        ball1_invulnTime = 0;  // Recuperata! Resetta grace period
        ball1_crackle = 0;
        Serial.println("[DUAL_PONG] Ball 1 RECOVERED - Grace period cancelled");
    }
    if (ball2_invulnTime > 0 && abs(ball2_vel) >= 0.06f) {
        ball2_invulnTime = 0;
        ball2_crackle = 0;
        Serial.println("[DUAL_PONG] Ball 2 RECOVERED - Grace period cancelled");
    }

    // "SALVATAGGIO IN EXTREMIS":
    // - Se una palla lampeggia molto (crackle alto) e NON è tenuta (tempMass bassa),
    //   e si trova vicino a una estremità, riceve un impulso di salvataggio.
    // - Se invece viene "tenuta" (tempMass alta) mentre è in grace period, recupera vitalità.
    const float HOLD_TRIGGER_MASS = 0.9f;
    const float EDGE_SAVE_DISTANCE = 0.0f;  // pixel dalla base/punta
    const uint8_t EDGE_SAVE_CRACKLE = 120;   // lampeggio "molto"
    const unsigned long EDGE_SAVE_COOLDOWN = 30000;
    // Edge-save SOLO se la palla è davvero "bloccata" sugli ultimi 2 pixel:
    // - molto lenta
    // - ferma lì per un minimo di tempo
    const float EDGE_SAVE_MAX_VELOCITY = 0.010f;       // pixel/ms
    const unsigned long EDGE_SAVE_STUCK_TIME = 2000;    // ms

    auto reviveFromHold = [&](bool active,
                              float& pos,
                              float& vel,
                              float tempMass,
                              unsigned long& invulnTime,
                              uint8_t& crackle,
                              const char* label) {
        if (!active || invulnTime == 0) return;
        // Aiuto extra: se l'utente "ritocca e tiene" poco prima che la palla sparisca,
        // abbassiamo dinamicamente la soglia richiesta (late-save).
        float t = min(1.0f, (float)(now - invulnTime) / (float)GRACE_PERIOD); // 0..1
        float requiredHoldMass = max(0.50f, HOLD_TRIGGER_MASS - t * 0.40f);   // 0.90 -> ~0.50 (Più difficile salvare)
        if (tempMass < requiredHoldMass) return;

        float sign = (abs(vel) > 0.005f) ? ((vel > 0.0f) ? 1.0f : -1.0f)
                                         : ((pos < (state.foldPoint * 0.5f)) ? 1.0f : -1.0f);

        float reviveSpeed = FIXED_BASE_SPEED + min(0.08f, tempMass * 0.03f + t * 0.03f);
        vel = sign * reviveSpeed;

        invulnTime = 0;
        crackle = 0;
        Serial.print("[DUAL_PONG] ");
        Serial.print(label);
        Serial.println(" HELD - Vitality restored (grace cancelled)");
    };

    auto updateEdgeStuckSince = [&](bool active,
                                    float pos,
                                    float vel,
                                    unsigned long& stuckSince) {
        if (!active) {
            stuckSince = 0;
            return;
        }

        bool nearBase = (pos <= EDGE_SAVE_DISTANCE);
        bool nearTip = (pos >= (state.foldPoint - 1.0f - EDGE_SAVE_DISTANCE));
        bool inLastTwoPixels = (nearBase || nearTip);
        bool verySlow = (abs(vel) <= EDGE_SAVE_MAX_VELOCITY);

        if (inLastTwoPixels && verySlow) {
            if (stuckSince == 0) stuckSince = now;
        } else {
            stuckSince = 0;
        }
    };

    auto edgeSave = [&](bool active,
                        float& pos,
                        float& vel,
                        float tempMass,
                        unsigned long& invulnTime,
                        uint8_t crackle,
                        unsigned long& lastEdgeSave,
                        unsigned long& stuckSince,
                        const char* label) {
        if (!active || invulnTime == 0) return;
        if (tempMass >= 0.30f) return;  // se l'utente sta "tenendo" anche poco, non interferire con kick
        if (crackle < EDGE_SAVE_CRACKLE) return;
        if (now - lastEdgeSave < EDGE_SAVE_COOLDOWN) return;

        bool nearBase = (pos <= EDGE_SAVE_DISTANCE);
        bool nearTip = (pos >= (state.foldPoint - 1.0f - EDGE_SAVE_DISTANCE));
        if (!nearBase && !nearTip) return;

        // Richiede che sia davvero bloccata sugli ultimi 2 pixel.
        if (abs(vel) > EDGE_SAVE_MAX_VELOCITY) return;
        if (stuckSince == 0 || (now - stuckSince) < EDGE_SAVE_STUCK_TIME) return;

        vel = nearBase ? fabs(FIXED_BASE_SPEED) : -fabs(FIXED_BASE_SPEED);
        invulnTime = 0;
        lastEdgeSave = now;
        stuckSince = 0;
        Serial.print("[DUAL_PONG] ");
        Serial.print(label);
        Serial.println(" EDGE SAVE (extremis) - Kick applied");
    };

    updateEdgeStuckSince(ball1_active, ball1_pos, ball1_vel, ball1_edgeStuckSince);
    updateEdgeStuckSince(ball2_active, ball2_pos, ball2_vel, ball2_edgeStuckSince);

    reviveFromHold(ball1_active, ball1_pos, ball1_vel, ball1_tempMass, ball1_invulnTime, ball1_crackle, "Ball 1");
    reviveFromHold(ball2_active, ball2_pos, ball2_vel, ball2_tempMass, ball2_invulnTime, ball2_crackle, "Ball 2");
    edgeSave(ball1_active, ball1_pos, ball1_vel, ball1_tempMass, ball1_invulnTime, ball1_crackle, ball1_lastEdgeSave, ball1_edgeStuckSince, "Ball 1");
    edgeSave(ball2_active, ball2_pos, ball2_vel, ball2_tempMass, ball2_invulnTime, ball2_crackle, ball2_lastEdgeSave, ball2_edgeStuckSince, "Ball 2");

    // Check trigger status (massa temporanea attiva)
    // Impedisce il collasso se c'è stata perturbazione recente (assorbimento bloccato)
    bool isTriggered = (ball1_tempMass > 0.8f || ball2_tempMass > 0.8f);

    if (!singleBallMode && (now - lastCollapseTime) > 2000 && !isTriggered) {
        // Check se una palla è quasi ferma E il grace period è scaduto
        bool ball1_stopped = ball1_active && (abs(ball1_vel) < minVelocity) &&
                             (ball1_invulnTime > 0) && ((now - ball1_invulnTime) > GRACE_PERIOD);
        bool ball2_stopped = ball2_active && (abs(ball2_vel) < minVelocity) &&
                             (ball2_invulnTime > 0) && ((now - ball2_invulnTime) > GRACE_PERIOD);

        // OPPURE se una è troppo veloce (oltre limite rendering)
        float maxRenderSpeed = 1.2f;
        bool ball1_critical = ball1_active && (abs(ball1_vel) > maxRenderSpeed);
        bool ball2_critical = ball2_active && (abs(ball2_vel) > maxRenderSpeed);

        if (ball1_stopped || ball2_stopped || ball1_critical || ball2_critical) {
            // COLLASSO DEL SISTEMA!
            singleBallMode = true;
            lastCollapseTime = now;

            // Reset grace periods
            ball1_invulnTime = 0;
            ball2_invulnTime = 0;
            ball1_crackle = 0;
            ball2_crackle = 0;

            // Determina quale palla vince (la più veloce o quella in movimento)
            bool ball1_wins = false;

            if (ball1_stopped && !ball2_stopped) {
                ball1_wins = false;  // Ball 2 vince (ball 1 ferma)
                Serial.print("[DUAL_PONG] COLLAPSE after ");
                Serial.print(collisionCount);
                Serial.println(" collisions! Ball 1 STOPPED (grace period expired) - Ball 2 WINS");
            } else if (ball2_stopped && !ball1_stopped) {
                ball1_wins = true;   // Ball 1 vince (ball 2 ferma)
                Serial.print("[DUAL_PONG] COLLAPSE after ");
                Serial.print(collisionCount);
                Serial.println(" collisions! Ball 2 STOPPED (grace period expired) - Ball 1 WINS");
            } else if (ball1_critical && !ball2_critical) {
                ball1_wins = true;   // Ball 1 troppo veloce: vince
                Serial.println("[DUAL_PONG] COLLAPSE! Ball 1 too fast - Ball 2 DELETED");
            } else if (ball2_critical && !ball1_critical) {
                ball1_wins = false;  // Ball 2 troppo veloce: vince
                Serial.println("[DUAL_PONG] COLLAPSE! Ball 2 too fast - Ball 1 DELETED");
            } else {
                ball1_wins = (abs(ball1_vel) > abs(ball2_vel));  // Vince la più veloce
            }

            if (ball1_wins) {
                ball2_active = false;
                // Vincitore mantiene il colore
                // Forza direzione coerente (evita riprese invertite quando la velocità è quasi zero)
                ball1_vel = fabs(FIXED_BASE_SPEED);  // Ball 1 "attacca" verso la punta
                ball1_mass = 1.0f;  // Reset massa

                // Genera colore OPPOSTO con minimo 90° di differenza (garantisce contrasto)
                // Range: 90-180° = da 1/4 a 1/2 della ruota cromatica (sempre visibile)
                uint8_t hueOffset = random8(90, 180);
                nextBallHue = ball2_hue + hueOffset;
                Serial.print("[DUAL_PONG] New color: old_hue=");
                Serial.print(ball2_hue);
                Serial.print(" new_hue=");
                Serial.println(nextBallHue);
            } else {
                ball1_active = false;
                // Vincitore mantiene il colore
                ball2_vel = -fabs(FIXED_BASE_SPEED);  // Ball 2 "attacca" verso la base
                ball2_mass = 1.0f;  // Reset massa

                // Genera colore OPPOSTO con minimo 90° di differenza
                uint8_t hueOffset = random8(90, 180);
                nextBallHue = ball1_hue + hueOffset;
                Serial.print("[DUAL_PONG] New color: old_hue=");
                Serial.print(ball1_hue);
                Serial.print(" new_hue=");
                Serial.println(nextBallHue);
            }

            // Reset sistema
            perturbTarget = 0;
            perturbAccumulator = 0;
            collisionCount = 0;
        }
    }

    // ═══════════════════════════════════════════════════════════
    // RESPAWN LOGIC - Generazione nuova palla con LAMPO
    // ═══════════════════════════════════════════════════════════

    if (singleBallMode) {
        // Condizione di respawn:
        // - Ball 1 (base-side) quando è vicino alla base e sta ripartendo verso la punta.
        // - Ball 2 (tip-side) quando è vicino alla punta e sta ripartendo verso la base.
        float tipThreshold = state.foldPoint * 0.85f;
        float baseThreshold = state.foldPoint * 0.15f;

        bool ball1_ready = ball1_active && (ball1_pos < baseThreshold) && (ball1_vel > 0);
        bool ball2_ready = ball2_active && (ball2_pos > tipThreshold) && (ball2_vel < 0);

        if (ball1_ready || ball2_ready) {
            // SPAWN NUOVA PALLA dalla sua estremità con LAMPO!
            singleBallMode = false;
            spawnFlashBrightness = 255;

            if (!ball2_active) {
                // Respawn ball2
                ball2_active = true;
                // Ball 2 "appartiene" alla punta: respawn dalla punta verso la base
                ball2_pos = (float)(state.foldPoint - 1);  // Punta della lama
                ball2_vel = -fabs(FIXED_BASE_SPEED);       // Va verso la base
                ball2_mass = 1.0f;  // Reset massa a valore base
                ball2_hue = nextBallHue;
                Serial.println("[DUAL_PONG] *** FLASH! Ball 2 SPAWNED from tip ***");
            } else {
                // Respawn ball1
                ball1_active = true;
                ball1_pos = 0.0f;
                // Ball 1 "appartiene" alla base: respawn dalla base verso la punta
                ball1_vel = fabs(FIXED_BASE_SPEED);  // Va verso la punta
                ball1_mass = 1.0f;  // Reset massa a valore base
                ball1_hue = nextBallHue;
                Serial.println("[DUAL_PONG] *** FLASH! Ball 1 SPAWNED from base ***");
            }
        }
    }

    // Decay del lampo di spawn
    if (spawnFlashBrightness > 0) {
        spawnFlashBrightness = qsub8(spawnFlashBrightness, 20);  // Fade veloce
    }

    // Decay del flash collisione
    if (collisionFusionFlash > 0) {
        collisionFusionFlash = qsub8(collisionFusionFlash, 28);
    }
    if (collisionWhiteCore > 0) {
        collisionWhiteCore = qsub8(collisionWhiteCore, 45);
    }

    // "Plasma held": crackle/clash ripetuti quando una palla è quasi ferma.
    // Aumentano intensità e velocità man mano che ci si avvicina alla fine del GRACE_PERIOD.
    if (ball1_invulnTime > 0) {
        float t = min(1.0f, (float)(now - ball1_invulnTime) / (float)GRACE_PERIOD); // 0..1
        uint16_t periodMs = (uint16_t)(420.0f - t * 330.0f); // 420ms -> 90ms
        if (now - ball1_crackleLast >= periodMs) {
            ball1_crackleLast = now;
            uint8_t base = (uint8_t)(30.0f + t * 140.0f); // 30..170
            base = qadd8(base, random8((uint8_t)(20.0f + t * 60.0f)));
            ball1_crackle = qadd8(ball1_crackle, base);
            if (ball1_crackle > 160) ball1_crackle = 160;
        }
    }
    if (ball2_invulnTime > 0) {
        float t = min(1.0f, (float)(now - ball2_invulnTime) / (float)GRACE_PERIOD);
        uint16_t periodMs = (uint16_t)(420.0f - t * 330.0f);
        if (now - ball2_crackleLast >= periodMs) {
            ball2_crackleLast = now;
            uint8_t base = (uint8_t)(30.0f + t * 140.0f);
            base = qadd8(base, random8((uint8_t)(20.0f + t * 60.0f)));
            ball2_crackle = qadd8(ball2_crackle, base);
            if (ball2_crackle > 160) ball2_crackle = 160;
        }
    }

    // Decay crackle (molto breve, tipo "clash ripetuti")
    if (ball1_crackle > 0) ball1_crackle = qsub8(ball1_crackle, 28);
    if (ball2_crackle > 0) ball2_crackle = qsub8(ball2_crackle, 28);

    _lastDualPulseUpdate = now;

    // ═══════════════════════════════════════════════════════════
    // RENDERING - Palle colorate su sfondo scuro
    // ═══════════════════════════════════════════════════════════

    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);
    const float ballRadius = 6.0f;  // Raggio rendering palla (pixel)

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        CRGB color = CRGB::Black;
        uint8_t brightness = 15;  // Base scura per contrasto

        // LAMPO DI SPAWN (flash bianco alla base)
        if (spawnFlashBrightness > 0 && i < 20) {
            // Gradiente del flash dalla base (gaussiano)
            float flashDist = i / 20.0f;  // [0, 1]
            uint8_t flashIntensity = spawnFlashBrightness * (1.0f - flashDist * flashDist);

            CRGB flashColor = CRGB(flashIntensity, flashIntensity, flashIntensity);
            color.r = qadd8(color.r, flashColor.r);
            color.g = qadd8(color.g, flashColor.g);
            color.b = qadd8(color.b, flashColor.b);
        }

        // BALL 1 RENDERING (gradiente gaussiano per smoothness)
        if (ball1_active) {
            float dist1 = abs((float)i - ball1_pos);
            float totalMass1 = ball1_mass + ball1_tempMass;

            // ALONE COLORATO (colore inverso) - più grande con massa maggiore
            float haloRadius = ballRadius * (1.5f + totalMass1 * 0.15f);  // Cresce con massa
            if (dist1 < haloRadius && ball1_tempMass > 1.0f) {
                // Alone con colore complementare (opposto)
                uint8_t haloHue = ball1_hue + 128;  // Colore inverso!
                float haloDist = (dist1 - ballRadius) / (haloRadius - ballRadius);  // 0.0 al bordo palla, 1.0 all'esterno
                haloDist = max(0.0f, haloDist);

                // Alone più intenso con massa maggiore
                float haloIntensity = (ball1_tempMass / 20.0f) * (1.0f - haloDist);
                uint8_t haloBrightness = haloIntensity * 180;  // Alone visibile

                if (haloBrightness > 20) {
                    CRGB haloColor = CHSV(haloHue, 255, haloBrightness);
                    color.r = qadd8(color.r, haloColor.r);
                    color.g = qadd8(color.g, haloColor.g);
                    color.b = qadd8(color.b, haloColor.b);
                }
            }

            // CORE della palla
            if (dist1 < ballRadius * 2.0f) {
                // Profilo gaussiano: I = I_max * exp(-dist²/(2σ²))
                const float sigma = ballRadius / 2.5066f;
                float gaussian = expf(-(dist1 * dist1) / (2.0f * sigma * sigma));
                uint8_t ballBrightness = 255 * gaussian;

                // BOOST LUMINOSITÀ in base alla massa TOTALE (sempre visibile!)
                float massBoost = 1.0f + (totalMass1 - 1.0f) * 0.15f;  // +15% per ogni unità di massa
                ballBrightness = min(255, (int)(ballBrightness * massBoost));

                // EFFETTO PULSANTE TREMOLANTE quando triggerata
                if (ball1_tempMass > 1.0f) {
                    float massRatio = min(ball1_tempMass / 20.0f, 1.0f);

                    // Pulsing ultra-luminoso
                    uint8_t pulsePhase = (now >> 2) & 0xFF;  // Più veloce
                    uint8_t pulseBrightness = 200 + scale8(sin8(pulsePhase), 55);  // Range 200-255 (MOLTO luminoso!)

                    // Tremore drammatico
                    uint8_t jitter = random8(massRatio * 80);  // Tremore aumentato

                    ballBrightness = scale8(ballBrightness, pulseBrightness);
                    ballBrightness = qadd8(ballBrightness, jitter);
                }

                if (ballBrightness > 30) {
                    CRGB ball1Color = CHSV(ball1_hue, 255, ballBrightness);

                    if (ballBrightness > brightness) {
                        color = ball1Color;
                        brightness = ballBrightness;
                    }
                }
            }
        }

        // BALL 2 RENDERING
        if (ball2_active) {
            float dist2 = abs((float)i - ball2_pos);
            float totalMass2 = ball2_mass + ball2_tempMass;

            // ALONE COLORATO (colore inverso) - più grande con massa maggiore
            float haloRadius = ballRadius * (1.5f + totalMass2 * 0.15f);  // Cresce con massa
            if (dist2 < haloRadius && ball2_tempMass > 1.0f) {
                // Alone con colore complementare (opposto)
                uint8_t haloHue = ball2_hue + 128;  // Colore inverso!
                float haloDist = (dist2 - ballRadius) / (haloRadius - ballRadius);  // 0.0 al bordo palla, 1.0 all'esterno
                haloDist = max(0.0f, haloDist);

                // Alone più intenso con massa maggiore
                float haloIntensity = (ball2_tempMass / 20.0f) * (1.0f - haloDist);
                uint8_t haloBrightness = haloIntensity * 180;  // Alone visibile

                if (haloBrightness > 20) {
                    CRGB haloColor = CHSV(haloHue, 255, haloBrightness);
                    color.r = qadd8(color.r, haloColor.r);
                    color.g = qadd8(color.g, haloColor.g);
                    color.b = qadd8(color.b, haloColor.b);
                }
            }

            // CORE della palla
            if (dist2 < ballRadius * 2.0f) {
                const float sigma = ballRadius / 2.5066f;
                float gaussian = expf(-(dist2 * dist2) / (2.0f * sigma * sigma));
                uint8_t ballBrightness = 255 * gaussian;

                // BOOST LUMINOSITÀ in base alla massa TOTALE (sempre visibile!)
                float massBoost = 1.0f + (totalMass2 - 1.0f) * 0.15f;  // +15% per ogni unità di massa
                ballBrightness = min(255, (int)(ballBrightness * massBoost));

                // EFFETTO PULSANTE TREMOLANTE quando triggerata
                if (ball2_tempMass > 1.0f) {
                    float massRatio = min(ball2_tempMass / 20.0f, 1.0f);

                    // Pulsing ultra-luminoso
                    uint8_t pulsePhase = (now >> 2) & 0xFF;  // Più veloce
                    uint8_t pulseBrightness = 200 + scale8(sin8(pulsePhase), 55);  // Range 200-255 (MOLTO luminoso!)

                    // Tremore drammatico
                    uint8_t jitter = random8(massRatio * 80);  // Tremore aumentato

                    ballBrightness = scale8(ballBrightness, pulseBrightness);
                    ballBrightness = qadd8(ballBrightness, jitter);
                }

                if (ballBrightness > 30) {
                    CRGB ball2Color = CHSV(ball2_hue, 255, ballBrightness);

                    // Se le palle si sovrappongono: blend additivo
                    if (ballBrightness > brightness / 2) {
                        color.r = qadd8(color.r, ball2Color.r);
                        color.g = qadd8(color.g, ball2Color.g);
                        color.b = qadd8(color.b, ball2Color.b);
                    }
                }
            }
        }

        // Collisione: fusion flash (colore) + core bianco sottile (solo impatti forti)
        if (collisionFusionFlash > 0) {
            float dist = abs((float)i - collisionFlashPos);
            const float radius = 12.0f;
            if (dist < radius) {
                float t = 1.0f - (dist / radius);
                uint8_t boost = (uint8_t)(collisionFusionFlash * t * t);
                CRGB fusionA = CHSV(collisionHueA, 255, 255);
                CRGB fusionB = CHSV(collisionHueB, 255, 255);
                CRGB fusion = blend(fusionA, fusionB, 128);
                fusion.r = scale8(fusion.r, boost);
                fusion.g = scale8(fusion.g, boost);
                fusion.b = scale8(fusion.b, boost);
                color.r = qadd8(color.r, fusion.r);
                color.g = qadd8(color.g, fusion.g);
                color.b = qadd8(color.b, fusion.b);
            }
        }
        if (collisionWhiteCore > 0) {
            float dist = abs((float)i - collisionFlashPos);
            const float radius = 4.0f;
            if (dist < radius) {
                float t = 1.0f - (dist / radius);
                uint8_t boost = (uint8_t)(collisionWhiteCore * t * t);
                color.r = qadd8(color.r, boost);
                color.g = qadd8(color.g, boost);
                color.b = qadd8(color.b, boost);
            }
        }

        // Grabbing: alone complementare + micro-sparkle bianco sporadico (clampato)
        if (ball1_active && ball1_invulnTime > 0) {
            float t = min(1.0f, (float)(now - ball1_invulnTime) / (float)GRACE_PERIOD);
            float dist = abs((float)i - ball1_pos);
            float radius = 8.0f + t * 10.0f; // 8..18
            if (dist < radius) {
                float k = 1.0f - (dist / radius);
                uint8_t haloHue = ball1_hue + 128;
                uint8_t haloV = (uint8_t)(40.0f + t * 120.0f);
                haloV = scale8(haloV, 180 + scale8(sin8((now >> 2) & 0xFF), 75));
                CRGB halo = CHSV(haloHue, 255, (uint8_t)(scale8(haloV, (uint8_t)(k * k * 255.0f))));
                color.r = qadd8(color.r, halo.r);
                color.g = qadd8(color.g, halo.g);
                color.b = qadd8(color.b, halo.b);
            }

            if (ball1_crackle > 0 && dist < (5.0f + t * 7.0f)) {
                uint8_t p = (uint8_t)(18.0f + t * 90.0f); // probabilità sparkles 7%..42%
                if (random8() < p) {
                    float r = 5.0f + t * 7.0f;
                    float k = 1.0f - (dist / r);
                    uint8_t crackle = (ball1_crackle > 120) ? 120 : ball1_crackle;
                    uint8_t boost = (uint8_t)(crackle * k * k);
                    color.r = qadd8(color.r, boost);
                    color.g = qadd8(color.g, boost);
                    color.b = qadd8(color.b, boost);
                }
            }
        }
        if (ball2_active && ball2_invulnTime > 0) {
            float t = min(1.0f, (float)(now - ball2_invulnTime) / (float)GRACE_PERIOD);
            float dist = abs((float)i - ball2_pos);
            float radius = 8.0f + t * 10.0f;
            if (dist < radius) {
                float k = 1.0f - (dist / radius);
                uint8_t haloHue = ball2_hue + 128;
                uint8_t haloV = (uint8_t)(40.0f + t * 120.0f);
                haloV = scale8(haloV, 180 + scale8(sin8((now >> 2) & 0xFF), 75));
                CRGB halo = CHSV(haloHue, 255, (uint8_t)(scale8(haloV, (uint8_t)(k * k * 255.0f))));
                color.r = qadd8(color.r, halo.r);
                color.g = qadd8(color.g, halo.g);
                color.b = qadd8(color.b, halo.b);
            }

            if (ball2_crackle > 0 && dist < (5.0f + t * 7.0f)) {
                uint8_t p = (uint8_t)(18.0f + t * 90.0f);
                if (random8() < p) {
                    float r = 5.0f + t * 7.0f;
                    float k = 1.0f - (dist / r);
                    uint8_t crackle = (ball2_crackle > 120) ? 120 : ball2_crackle;
                    uint8_t boost = (uint8_t)(crackle * k * k);
                    color.r = qadd8(color.r, boost);
                    color.g = qadd8(color.g, boost);
                    color.b = qadd8(color.b, boost);
                }
            }
        }

        // Apply global brightness scaling
        color = scaleColorByBrightness(color, safeBrightness);
        setLedPair(i, state.foldPoint, color);
    }
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

void LedEffectEngine::renderRainbowEffect(const LedState& state, const uint8_t perturbationGrid[6][8], const MotionProcessor::ProcessedMotion* motion) {
    CRGB whiteBase = CRGB(255, 255, 255);  // Lama bianca come base
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        CRGB ledColor = whiteBase;  // Start with white

        // RAINBOW PERTURBATIONS: motion adds colors based on direction
        if (perturbationGrid != nullptr && motion != nullptr) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

            // Sample perturbation in this column
            uint16_t perturbSum = 0;
            for (uint8_t row = 2; row <= 4; row++) {
                perturbSum += perturbationGrid[row][col];
            }
            uint8_t avgPerturbation = perturbSum / 3;

            if (avgPerturbation > 12) {  // Sensitive threshold for color appearance
                // Get color based on motion direction
                uint8_t hue = getHueFromDirection(motion->direction);

                // Create saturated color from direction
                CRGB perturbColor = CHSV(hue, 255, 255);

                // Blend white base with perturbation color
                // Higher perturbation = more color visible
                // Scale to 200 max (not 255) to keep some white for luminosity
                uint8_t blendAmount = scale8(avgPerturbation, 200);
                ledColor = blend(whiteBase, perturbColor, blendAmount);

                // LUMINOSITY BOOST: colors on white base should be brighter
                // Add back some white to increase perceived brightness
                uint8_t whiteBoost = scale8(avgPerturbation, 80);  // Add white glow
                ledColor.r = qadd8(ledColor.r, whiteBoost);
                ledColor.g = qadd8(ledColor.g, whiteBoost);
                ledColor.b = qadd8(ledColor.b, whiteBoost);
            }
        }

        ledColor = scaleColorByBrightness(ledColor, safeBrightness);
        setLedPair(i, state.foldPoint, ledColor);
    }
}

// ═══════════════════════════════════════════════════════════
// GESTURE-TRIGGERED EFFECTS
// ═══════════════════════════════════════════════════════════

void LedEffectEngine::renderIgnition(const LedState& state) {
    // If one-shot mode and already completed, skip rendering
    if (_ignitionOneShot && _ignitionCompleted) {
        return;
    }

    const unsigned long now = millis();
    const uint32_t IGNITION_DURATION_MS = 1000;  // Fixed 1 second duration

    // Calculate progress based on elapsed time (0.0 to 1.0)
    uint32_t elapsed = now - _modeStartTime;
    float progress = min(1.0f, (float)elapsed / IGNITION_DURATION_MS);

    // Convert progress to LED count
    _ignitionProgress = (uint16_t)(progress * state.foldPoint);

    // Check if animation is complete
    if (progress >= 1.0f) {
        _ignitionProgress = state.foldPoint;  // Ensure full brightness
        if (_ignitionOneShot) {
            // One-shot mode: mark as completed and return to IDLE
            _ignitionCompleted = true;
            _mode = Mode::IDLE;
            Serial.println("[LED] Ignition complete - blade fully ignited");
        } else {
            // Normal mode: loop
            _modeStartTime = now;  // Restart animation
        }
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
    // If one-shot mode and already completed, all LEDs off
    if (_retractionOneShot && _retractionCompleted) {
        fill_solid(_leds, _numLeds, CRGB::Black);

        // Handle blade state and deep sleep after animation completes
        if (_ledStateRef && _ledStateRef->bladeEnabled) {
            _ledStateRef->bladeEnabled = false;
            Serial.println("[LED POWER] Blade disabled");
        }

        // Trigger deep sleep if requested
        if (_deepSleepRequested) {
            Serial.println("[LED POWER] Entering deep sleep in 500ms...");
            Serial.println("[LED POWER] Wake-up sources:");
            Serial.println("[LED POWER]   - EXT0 (GPIO 0 / BOOT button) LOW level");
            Serial.println("[LED POWER]   - Timer wake-up disabled (wake only via reset/button)");

            FastLED.show();  // Ensure LEDs are off
            delay(500);

            // Configure wake-up source: GPIO 0 (BOOT button) on LOW level
            // This allows wake-up by pressing the BOOT button
            esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // 0 = LOW level trigger

            // Optional: Configure timer wake-up (disabled by default)
            // Uncomment to enable wake-up after X seconds:
            // esp_sleep_enable_timer_wakeup(60 * 1000000ULL);  // 60 seconds

            Serial.println("[LED POWER] Entering deep sleep NOW!");
            Serial.flush();  // Ensure serial buffer is flushed

            esp_deep_sleep_start();
        }

        return;
    }

    const unsigned long now = millis();
    const uint32_t RETRACTION_DURATION_MS = 800;  // Fixed 800ms duration

    // Calculate progress based on elapsed time (1.0 to 0.0, reverse)
    uint32_t elapsed = now - _modeStartTime;
    float progress = 1.0f - min(1.0f, (float)elapsed / RETRACTION_DURATION_MS);

    // Convert progress to LED count (counts down from foldPoint to 0)
    _retractionProgress = (uint16_t)(progress * state.foldPoint);

    // Check if animation is complete
    if (elapsed >= RETRACTION_DURATION_MS) {
        _retractionProgress = 0;  // Ensure all LEDs off
        if (_retractionOneShot) {
            // One-shot mode: mark as completed
            _retractionCompleted = true;
            _mode = Mode::IDLE;
            Serial.println("[LED] Retraction complete - all LEDs off");
        } else {
            // Normal mode: loop
            _modeStartTime = now;  // Restart animation
        }
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
// PUBLIC TRIGGER METHODS
// ═══════════════════════════════════════════════════════════

void LedEffectEngine::triggerIgnitionOneShot() {
    _ignitionOneShot = true;
    _ignitionCompleted = false;
    _ignitionProgress = 0;
    _mode = Mode::IGNITION_ACTIVE;
    _modeStartTime = millis();
    _lastIgnitionUpdate = millis();
    Serial.println("[LED] Ignition ONE-SHOT triggered!");
}

void LedEffectEngine::triggerRetractionOneShot() {
    _retractionOneShot = true;
    _retractionCompleted = false;
    _retractionProgress = 0;  // Will be initialized in renderRetraction
    _mode = Mode::RETRACT_ACTIVE;
    _modeStartTime = millis();
    _lastRetractionUpdate = millis();
    Serial.println("[LED] Retraction ONE-SHOT triggered!");
}

void LedEffectEngine::powerOn() {
    // Check if blade is already on or ignition is in progress
    if (_ledStateRef && _ledStateRef->bladeEnabled) {
        Serial.println("[LED POWER] Blade already ON - ignoring powerOn()");
        return;
    }

    if (_mode == Mode::IGNITION_ACTIVE) {
        Serial.println("[LED POWER] Ignition already in progress - ignoring powerOn()");
        return;
    }

    Serial.println("[LED POWER] Power ON sequence initiated");

    // Step 1: Enable blade state FIRST
    if (_ledStateRef) {
        _ledStateRef->bladeEnabled = true;
        Serial.println("[LED POWER] Blade enabled");
    } else {
        Serial.println("[LED POWER ERROR] LedState reference not set!");
    }

    // Step 2: Trigger ignition animation (one-shot)
    _ignitionOneShot = true;
    _ignitionCompleted = false;
    _ignitionProgress = 0;
    _mode = Mode::IGNITION_ACTIVE;
    _modeStartTime = millis();
    _lastIgnitionUpdate = millis();

    Serial.println("[LED POWER] Ignition animation started");
    // Step 3: After animation completes, mode will return to IDLE automatically
    // and normal effects will render
}

void LedEffectEngine::powerOff(bool deepSleep) {
    // Check if blade is already off or retraction is in progress
    if (_ledStateRef && !_ledStateRef->bladeEnabled && _mode != Mode::RETRACT_ACTIVE) {
        Serial.println("[LED POWER] Blade already OFF - ignoring powerOff()");
        return;
    }

    if (_mode == Mode::RETRACT_ACTIVE) {
        Serial.println("[LED POWER] Retraction already in progress - ignoring powerOff()");
        return;
    }

    Serial.printf("[LED POWER] Power OFF sequence initiated (deep sleep: %s)\n",
                  deepSleep ? "YES" : "NO");

    // Step 1: Store deep sleep request
    _deepSleepRequested = deepSleep;

    // Step 2: Trigger retraction animation (one-shot)
    _retractionOneShot = true;
    _retractionCompleted = false;
    _retractionProgress = 0;
    _mode = Mode::RETRACT_ACTIVE;
    _modeStartTime = millis();
    _lastRetractionUpdate = millis();

    Serial.println("[LED POWER] Retraction animation started");
    // Step 3: After animation completes, blade will be disabled and deep sleep triggered
    // (handled in renderRetraction when _retractionCompleted becomes true)
}

// ═══════════════════════════════════════════════════════════
// MODE MANAGEMENT
// ═══════════════════════════════════════════════════════════

bool LedEffectEngine::checkModeTimeout(uint32_t now) {
    switch (_mode) {
        case Mode::IGNITION_ACTIVE:
            return (now - _modeStartTime) > 5000;  // 5 seconds (enough for full animation)

        case Mode::RETRACT_ACTIVE:
            return (now - _modeStartTime) > 5000;  // 5 seconds (enough for full animation)

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

    if (_suppressGestureOverrides) {
        switch (gesture) {
            case MotionProcessor::GestureType::IGNITION:
            case MotionProcessor::GestureType::RETRACT:
            case MotionProcessor::GestureType::CLASH:
                return;
            default:
                break;
        }
    }

    switch (gesture) {
        case MotionProcessor::GestureType::IGNITION:
            _mode = Mode::IGNITION_ACTIVE;
            _modeStartTime = now;
            _ignitionProgress = 0;
            _lastIgnitionUpdate = now;
            // Reset one-shot flags to allow gesture-triggered ignition to run normally
            _ignitionOneShot = false;
            _ignitionCompleted = false;
            Serial.println("[LED] IGNITION effect triggered by gesture!");
            break;

        case MotionProcessor::GestureType::RETRACT:
            _mode = Mode::RETRACT_ACTIVE;
            _modeStartTime = now;
            _retractionProgress = 0;  // Will be initialized in render
            _lastRetractionUpdate = now;
            // Reset one-shot flags to allow gesture-triggered retraction to run normally
            _retractionOneShot = false;
            _retractionCompleted = false;
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

// ═══════════════════════════════════════════════════════════
// CHRONO HYBRID EFFECT - Orologio ibrido con motion reactive
// ═══════════════════════════════════════════════════════════

void LedEffectEngine::renderChronoHybrid(const LedState& state, const uint8_t perturbationGrid[6][8], const MotionProcessor::ProcessedMotion* motion) {
    const unsigned long now = millis();

    // ═══ STEP 1: CALCOLO TEMPO REALE ═══
    if (state.epochBase == 0) {
        // Nessun sync ancora: mostra pattern "waiting for sync"
        static unsigned long lastDebugPrint = 0;
        if (now - lastDebugPrint > 5000) {
            Serial.println("[CHRONO] Waiting for time sync (epochBase == 0)");
            lastDebugPrint = now;
        }
        fill_solid(_leds, _numLeds, CRGB(20, 0, 20));  // Viola dim
        return;
    }

    uint32_t elapsed = (now - state.millisAtSync) / 1000;  // Secondi da sync
    uint32_t currentEpoch = state.epochBase + elapsed;

    // Debug time sync (ogni 10 secondi)
    static unsigned long lastTimePrint = 0;
    if (now - lastTimePrint > 10000) {
        Serial.printf("[CHRONO] epochBase=%lu, millisAtSync=%lu, elapsed=%lu sec\n",
            state.epochBase, state.millisAtSync, elapsed);
        lastTimePrint = now;
    }

    // Converti epoch in ore/minuti/secondi locali
    uint32_t timeOfDay = currentEpoch % 86400;  // Secondi dalla mezzanotte UTC
    uint8_t hours = (timeOfDay / 3600) % 12;    // Ore in formato 12h
    uint8_t minutes = (timeOfDay / 60) % 60;
    uint8_t seconds = timeOfDay % 60;

    // Debug (ogni 10 secondi)
    static unsigned long lastRenderPrint = 0;
    if (now - lastRenderPrint > 10000) {
        Serial.printf("[CHRONO] Time: %02d:%02d:%02d, foldPoint=%d, RGB=(%d,%d,%d)\n",
            hours, minutes, seconds, state.foldPoint, state.r, state.g, state.b);
        lastRenderPrint = now;
    }

    // ═══ STEP 2: GESTIONE OFFSET MOTION ═══
    float targetOffset = 0.0f;

    if (motion != nullptr && motion->gesture != MotionProcessor::GestureType::NONE) {
        // Motion attivo: applica perturbazione
        targetOffset = motion->motionIntensity * 30.0f / 255.0f;  // Max ±30 secondi virtuali
        _lastMotionTime = now;
    } else if (now - _lastMotionTime < 3000) {
        // Motion cessato da <3s: decadimento esponenziale
        float decay = 1.0f - ((now - _lastMotionTime) / 3000.0f);
        targetOffset = _visualOffset * decay;
    } else {
        targetOffset = 0.0f;  // Tornato a riposo
    }

    // Interpolazione fluida (lerp 10% per frame)
    _visualOffset = _visualOffset * 0.9f + targetOffset * 0.1f;

    // Applica offset al tempo visuale
    int visualSeconds = seconds + (int)_visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    // ═══ STEP 3: RENDERING VISUALE MODULARE ═══
    fill_solid(_leds, _numLeds, CRGB::Black);

    CRGB baseColor = CRGB(state.r, state.g, state.b);

    // A. MARKER ORE (tema selezionabile)
    switch (state.chronoHourTheme) {
        case 0:  // Classic
            renderChronoHours_Classic(state.foldPoint, baseColor);
            break;
        case 1:  // Neon Cyberpunk
            renderChronoHours_Neon(state.foldPoint, baseColor, hours);
            break;
        case 2:  // Plasma
            renderChronoHours_Plasma(state.foldPoint, hours, minutes);
            break;
        case 3:  // Digital Glitch
            renderChronoHours_Digital(state.foldPoint, baseColor, seconds);
            break;
        case 4:  // Inferno
            renderChronoHours_Inferno(state.foldPoint, baseColor, hours);
            break;
        case 5:  // Storm
            renderChronoHours_Storm(state.foldPoint, baseColor, hours);
            break;
        default:
            renderChronoHours_Classic(state.foldPoint, baseColor);
            break;
    }

    // B. CURSORI SECONDI/MINUTI (tema selezionabile)
    switch (state.chronoSecondTheme) {
        case 0:  // Classic
            renderChronoSeconds_Classic(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
            break;
        case 1:  // Time Spiral
            renderChronoSeconds_TimeSpiral(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
            break;
        case 2:  // Fire Clock
            renderChronoSeconds_FireClock(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
            break;
        case 3:  // Lightning
            renderChronoSeconds_Lightning(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
            break;
        case 4:  // Particle Flow
            renderChronoSeconds_Particle(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
            break;
        case 5:  // Quantum Wave
            renderChronoSeconds_Quantum(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
            break;
        default:
            renderChronoSeconds_Classic(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
            break;
    }

    // C. BATTITO GLOBALE (opzionale, solo per temi classic)
    if (state.chronoSecondTheme == 0) {
        uint16_t millisInSecond = (now % 1000);
        uint8_t pulseBrightness;

        if (millisInSecond < 100) {
            pulseBrightness = map(millisInSecond, 0, 100, 255, 180);
        } else {
            pulseBrightness = map(millisInSecond, 100, 1000, 180, 200);
        }

        for (uint16_t i = 0; i < _numLeds; i++) {
            _leds[i].nscale8(pulseBrightness);
        }
    }

    // Note: Brightness scaling already applied, will be set globally in render()
}

// ═══════════════════════════════════════════════════════════
// CHRONO THEME RENDERERS - HOUR MARKERS
// ═══════════════════════════════════════════════════════════

void LedEffectEngine::renderChronoHours_Classic(uint16_t foldPoint, CRGB baseColor) {
    // Marker statici uniformi (attenuati al 15%)
    for (uint8_t i = 0; i < 12; i++) {
        uint16_t pos = map(i, 0, 12, 0, foldPoint);
        CRGB markerColor = baseColor;
        markerColor.nscale8(40);  // 15% brightness
        setLedPair(pos, foldPoint, markerColor);
    }
}

void LedEffectEngine::renderChronoHours_Neon(uint16_t foldPoint, CRGB baseColor, uint8_t hours) {
    // Colori vibranti con glow: l'ora corrente brilla di più
    for (uint8_t i = 0; i < 12; i++) {
        uint16_t pos = map(i, 0, 12, 0, foldPoint);
        CRGB markerColor = baseColor;

        if (i == hours) {
            // Ora corrente: glow intenso + shift colore
            markerColor.nscale8(200);  // 78% brightness
            // Aggiungi componente cyan per effetto neon
            markerColor += CRGB(0, 50, 80);
        } else {
            markerColor.nscale8(60);  // 23% brightness per gli altri
        }

        // Glow esteso (3 LED per marker)
        for (int8_t j = -1; j <= 1; j++) {
            int16_t glowPos = pos + j;
            if (glowPos >= 0 && glowPos < foldPoint) {
                CRGB glow = markerColor;
                glow.nscale8(abs(j) == 1 ? 128 : 255);  // Fade sui lati
                setLedPair(glowPos, foldPoint, glow);
            }
        }
    }
}

void LedEffectEngine::renderChronoHours_Plasma(uint16_t foldPoint, uint8_t hours, uint8_t minutes) {
    // Blend arcobaleno fluido che ruota nel tempo
    uint8_t baseHue = (hours * 21 + minutes / 3) % 256;  // Ruota lentamente

    for (uint16_t i = 0; i < foldPoint; i++) {
        // Crea onda sinusoidale di colore
        uint8_t wave = sin8(i * 8 + baseHue);
        uint8_t hue = baseHue + wave / 4;
        CRGB plasmaColor = CHSV(hue, 255, 60);  // Saturo ma dim

        // Intensifica sui marker delle ore
        for (uint8_t h = 0; h < 12; h++) {
            uint16_t markerPos = map(h, 0, 12, 0, foldPoint);
            if (abs((int16_t)i - (int16_t)markerPos) <= 2) {
                plasmaColor.nscale8(200);  // Boost sui marker
                break;
            }
        }

        setLedPair(i, foldPoint, plasmaColor);
    }
}

void LedEffectEngine::renderChronoHours_Digital(uint16_t foldPoint, CRGB baseColor, uint8_t seconds) {
    // Scan lines RGB digitali che pulsano
    uint8_t scanPhase = (seconds * 4) % 256;  // Ciclo ogni 64 secondi

    for (uint8_t i = 0; i < 12; i++) {
        uint16_t pos = map(i, 0, 12, 0, foldPoint);

        // Colori primari RGB che si alternano
        CRGB digitColor;
        switch (i % 3) {
            case 0: digitColor = CRGB::Red; break;
            case 1: digitColor = CRGB::Green; break;
            case 2: digitColor = CRGB::Blue; break;
        }

        // Pulse sincronizzato con scanPhase
        uint8_t pulse = sin8(scanPhase + i * 20);
        digitColor.nscale8(40 + pulse / 4);  // 15-78% brightness

        // Scan line effect (3 pixel)
        for (int8_t j = -1; j <= 1; j++) {
            int16_t scanPos = pos + j;
            if (scanPos >= 0 && scanPos < foldPoint) {
                setLedPair(scanPos, foldPoint, digitColor);
            }
        }
    }
}

void LedEffectEngine::renderChronoHours_Inferno(uint16_t foldPoint, CRGB baseColor, uint8_t hours) {
    // TEMA INFERNO: Letto di braci ardenti e magma
    // Ideale come sfondo per Fire Clock

    unsigned long now = millis();
    
    // Sfondo: Rumore di calore a bassa frequenza
    for (uint16_t i = 0; i < foldPoint; i++) {
        // Combina due onde per simulare il movimento del magma
        uint8_t wave1 = sin8(i * 4 + now / 15);
        uint8_t wave2 = sin8(i * 9 - now / 22);
        uint8_t heat = (wave1 + wave2) / 2;
        
        // Mappa su colori scuri rosso/arancio (braci)
        // Hue: 0 (Red) -> 32 (Orange)
        uint8_t hue = map(heat, 0, 255, 0, 25);
        uint8_t bri = map(heat, 0, 255, 15, 50); // Dim background
        
        CRGB emberColor = CHSV(hue, 255, bri);
        setLedPair(i, foldPoint, emberColor);
    }

    // Marker Ore: Punti caldi che pulsano
    for (uint8_t h = 0; h < 12; h++) {
        uint16_t pos = map(h, 0, 12, 0, foldPoint);
        bool isCurrent = (h == hours);
        
        // Pulsazione lenta "respiro del drago"
        uint8_t pulse = beatsin8(isCurrent ? 40 : 15, 120, 255);
        
        CRGB markerColor = CHSV(0, 255, isCurrent ? pulse : 80);
        if (isCurrent) markerColor += CRGB(60, 20, 0); // Core giallo/bianco per l'ora corrente
        
        setLedPair(pos, foldPoint, markerColor);
    }
}

void LedEffectEngine::renderChronoHours_Storm(uint16_t foldPoint, CRGB baseColor, uint8_t hours) {
    // TEMA STORM: Nuvole scure ed elettricità statica
    // Ideale come sfondo per Lightning

    unsigned long now = millis();

    // Sfondo: Nuvole temporalesche (Blu scuro/Viola)
    for (uint16_t i = 0; i < foldPoint; i++) {
        uint8_t cloud = sin8(i * 3 + now / 40); // Movimento lento
        uint8_t hue = map(cloud, 0, 255, 155, 185); // Blue -> Purple
        uint8_t bri = map(cloud, 0, 255, 5, 30); // Molto scuro
        setLedPair(i, foldPoint, CHSV(hue, 200, bri));
    }

    // Marker Ore: Nodi elettrici
    for (uint8_t h = 0; h < 12; h++) {
        uint16_t pos = map(h, 0, 12, 0, foldPoint);
        bool isCurrent = (h == hours);
        // Jitter elettrico per l'ora corrente
        uint8_t jitter = isCurrent ? random8(100, 255) : 60;
        setLedPair(pos, foldPoint, CHSV(140, 180, jitter)); // Cyan elettrico
    }
}

// ═══════════════════════════════════════════════════════════
// CHRONO THEME RENDERERS - SECOND/MINUTE CURSORS
// ═══════════════════════════════════════════════════════════

void LedEffectEngine::renderChronoSeconds_Classic(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    // Cursore lineare solido (implementazione attuale)

    // B. CURSORE MINUTI (position)
    uint16_t minutePos = map(minutes, 0, 60, 0, foldPoint);
    CRGB minuteColor = baseColor;
    minuteColor.nscale8(180);  // 70% brightness
    for (int8_t i = -2; i <= 2; i++) {
        int16_t pos = minutePos + i;
        if (pos >= 0 && pos < foldPoint) {
            setLedPair(pos, foldPoint, minuteColor);
        }
    }

    // C. CURSORE SECONDI (scanner)
    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    uint16_t secondPos = map(visualSeconds, 0, 60, 0, foldPoint);
    CRGB secondColor = (abs(visualOffset) > 1.0f) ? CRGB::Cyan : CRGB::White;  // Glitch ciano
    for (int8_t i = -1; i <= 1; i++) {
        int16_t pos = secondPos + i;
        if (pos >= 0 && pos < foldPoint) {
            setLedPair(pos, foldPoint, secondColor);
        }
    }
}

void LedEffectEngine::renderChronoSeconds_TimeSpiral(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    // Spirale colorata rotante: i secondi creano un pattern a spirale con trail

    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    // La spirale è composta da più "bracci" che ruotano
    uint8_t numArms = 3;
    uint8_t baseHue = (visualSeconds * 4) % 256;  // Hue ruota con i secondi

    for (uint8_t arm = 0; arm < numArms; arm++) {
        // Calcola posizione di ogni braccio della spirale
        uint16_t armOffset = (visualSeconds * foldPoint / 60 + arm * foldPoint / numArms) % foldPoint;
        uint8_t armHue = (baseHue + arm * 85) % 256;  // Offset 120° tra bracci

        // Disegna trail dietro ogni braccio (effetto scia)
        for (int8_t trail = 0; trail < 8; trail++) {
            int16_t pos = armOffset - trail;
            if (pos < 0) pos += foldPoint;
            if (pos >= foldPoint) pos -= foldPoint;

            uint8_t brightness = 255 - (trail * 30);  // Fade della scia
            CRGB spiralColor = CHSV(armHue, 255, brightness);

            // Blend additivo per sovrapposizione
            uint16_t physicalIdx = pos < foldPoint ? pos : (2 * foldPoint - pos - 1);
            if (physicalIdx < _numLeds) {
                _leds[physicalIdx] += spiralColor;
            }
        }
    }

    // Cursore minuti: punto brillante fisso
    uint16_t minutePos = map(minutes, 0, 60, 0, foldPoint);
    CRGB minuteColor = CHSV((minutes * 4) % 256, 180, 255);
    for (int8_t i = -1; i <= 1; i++) {
        int16_t pos = minutePos + i;
        if (pos >= 0 && pos < foldPoint) {
            setLedPair(pos, foldPoint, minuteColor);
        }
    }
}

void LedEffectEngine::renderChronoSeconds_FireClock(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    // Fuoco che cresce dalla base: altezza = minuti, intensità fiamme = secondi

    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    // Calcola altezza del fuoco basata sui minuti
    uint16_t fireHeight = map(minutes, 0, 60, 5, foldPoint);

    // Palette fuoco: rosso -> arancione -> giallo -> bianco
    for (uint16_t i = 0; i < fireHeight; i++) {
        // Intensità variabile per simulare fiamme tremolanti
        uint8_t flicker = random8(40);  // Variazione casuale
        uint8_t secondPulse = sin8(visualSeconds * 4 + i * 10);  // Pulse sincronizzato

        // Colore fuoco che diventa più caldo verso la base
        uint8_t heat = map(i, 0, fireHeight, 255, 80);
        heat = qadd8(heat, secondPulse / 4);
        heat = qadd8(heat, flicker);

        CRGB fireColor;
        if (heat > 200) {
            fireColor = CRGB(255, 255, heat - 100);  // Bianco-giallo
        } else if (heat > 140) {
            fireColor = CRGB(255, heat, 0);  // Giallo-arancione
        } else {
            fireColor = CRGB(heat * 2, heat / 2, 0);  // Rosso-arancione
        }

        // Motion: se c'è offset, il fuoco diventa blu (fuoco "freddo" glitch)
        if (abs(visualOffset) > 2.0f) {
            fireColor = CRGB(fireColor.b, fireColor.r / 2, fireColor.g);
        }

        setLedPair(i, foldPoint, fireColor);
    }
}

void LedEffectEngine::renderChronoSeconds_Lightning(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    // Fulmini pulsanti: frequenza dipende dai secondi, posizioni dai minuti

    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    // Posizione principale del fulmine (basata sui minuti)
    uint16_t strikePos = map(minutes, 0, 60, 0, foldPoint);

    // Pulse del fulmine (rapido nei primi millisecondi del secondo)
    unsigned long now = millis();
    uint16_t millisInSecond = now % 1000;

    // Strike multipli per secondo (frequenza aumenta con i secondi)
    uint8_t strikesPerSecond = 1 + (visualSeconds / 20);  // 1-3 strike/sec
    uint16_t strikePeriod = 1000 / strikesPerSecond;
    uint16_t phaseInPeriod = millisInSecond % strikePeriod;

    if (phaseInPeriod < 80) {
        // Lightning flash attivo
        uint8_t flashBrightness = map(phaseInPeriod, 0, 80, 255, 0);

        // Fulmine principale
        CRGB boltColor = baseColor;
        boltColor.nscale8(flashBrightness);
        boltColor += CRGB(flashBrightness, flashBrightness, 255);  // Tint blu elettrico

        // Ramificazioni casuali del fulmine
        for (int8_t i = -10; i <= 10; i++) {
            if (random8(100) < 30) {  // 30% probabilità di ramificazione
                int16_t pos = strikePos + i;
                if (pos >= 0 && pos < foldPoint) {
                    CRGB branchColor = boltColor;
                    branchColor.nscale8(random8(128, 255));
                    setLedPair(pos, foldPoint, branchColor);
                }
            }
        }

        // Core del fulmine (più brillante)
        for (int8_t i = -2; i <= 2; i++) {
            int16_t pos = strikePos + i;
            if (pos >= 0 && pos < foldPoint) {
                setLedPair(pos, foldPoint, boltColor);
            }
        }
    } else {
        // Afterglow (bagliore residuo)
        uint8_t glowBrightness = map(phaseInPeriod, 80, strikePeriod, 60, 0);
        CRGB glowColor = CRGB(0, 0, glowBrightness / 2);

        for (int8_t i = -5; i <= 5; i++) {
            int16_t pos = strikePos + i;
            if (pos >= 0 && pos < foldPoint) {
                uint8_t distFade = map(abs(i), 0, 5, 255, 64);
                CRGB fade = glowColor;
                fade.nscale8(distFade);
                setLedPair(pos, foldPoint, fade);
            }
        }
    }
}

void LedEffectEngine::renderChronoSeconds_Particle(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    // Particelle che fluiscono: densità = secondi, velocità = minuti

    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    // Numero di particelle aumenta con i secondi
    uint8_t numParticles = 3 + (visualSeconds / 10);  // 3-9 particelle

    // Velocità base determinata dai minuti
    uint8_t baseSpeed = 1 + (minutes / 10);  // 1-7 unità/frame

    unsigned long now = millis();
    uint8_t timePhase = (now / 50) % 256;  // Fase temporale per animazione

    for (uint8_t p = 0; p < numParticles; p++) {
        // Posizione di ogni particella (offset per distribuzione)
        uint16_t particlePos = ((timePhase * baseSpeed + p * (256 / numParticles)) % 256);
        particlePos = map(particlePos, 0, 256, 0, foldPoint);

        // Colore unico per ogni particella (hue spacing)
        uint8_t particleHue = (p * (256 / numParticles) + visualSeconds * 2) % 256;
        CRGB particleColor = CHSV(particleHue, 255, 200);

        // Trail dietro la particella (3 LED)
        for (int8_t trail = 0; trail < 4; trail++) {
            int16_t pos = particlePos - trail;
            if (pos < 0) pos += foldPoint;

            CRGB trailColor = particleColor;
            trailColor.nscale8(255 - trail * 60);  // Fade del trail

            setLedPair(pos, foldPoint, trailColor);
        }
    }
}

void LedEffectEngine::renderChronoSeconds_Quantum(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    // Onda sinusoidale: frequenza = secondi, ampiezza = minuti

    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    // Parametri dell'onda
    uint8_t frequency = 1 + (visualSeconds / 10);  // 1-6 Hz
    uint8_t amplitude = map(minutes, 0, 60, 20, 200);  // Ampiezza brightness

    unsigned long now = millis();
    uint8_t timePhase = (now / 16) % 256;  // Fase temporale animazione

    for (uint16_t i = 0; i < foldPoint; i++) {
        // Calcola onda sinusoidale per questa posizione
        uint8_t wave = sin8(i * frequency * 8 + timePhase);
        uint8_t brightness = map(wave, 0, 255, 255 - amplitude, 255);

        // Colore basato sulla posizione e tempo (effetto "quantum shimmer")
        uint8_t hue = (i * 4 + visualSeconds * 2) % 256;
        CRGB quantumColor = CHSV(hue, 200, brightness);

        // Overlay pattern di interferenza (seconda onda)
        uint8_t wave2 = sin8(i * 12 - timePhase * 2);
        quantumColor.nscale8(wave2);

        // Motion offset: shift di fase dell'onda
        if (abs(visualOffset) > 1.0f) {
            quantumColor += CRGB(0, 0, abs(visualOffset) * 20);  // Tint blu
        }

        setLedPair(i, foldPoint, quantumColor);
    }
}
