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
    _mainPulseWidth(20)
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
    static unsigned long lastCollisionTime = 0;  // Timestamp dell'ultima collisione
    static uint16_t collisionCount = 0;         // Conta le collisioni per debug
    static float ball1_tempMass = 0.0f;         // Massa temporanea aggiunta da motion
    static float ball2_tempMass = 0.0f;         // Massa temporanea aggiunta da motion

    // FIXED BASE SPEED (independent of state.speed parameter)
    const float FIXED_BASE_SPEED = 0.10f;  // pixel/ms (rallentata per più controllo)

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
        initialized = true;

        Serial.println("[DUAL_PONG] Initialized with MASS-based physics");
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

        if (globalPerturbation > 30) {
            perturbAccumulator = min(255, perturbAccumulator + (globalPerturbation / 6));

            // Soglia di attivazione
            if (perturbAccumulator > 60) {
                // Scegli quale palla aumentare di massa (se non già scelta)
                if (perturbTarget == 0) {
                    perturbTarget = (random8() < 128) ? -1 : 1;
                    Serial.print("[DUAL_PONG] Mass boost target: Ball ");
                    Serial.println((perturbTarget == -1) ? "1" : "2");
                }

                // CALCOLA MASSA TEMPORANEA in base al motion (può diventare MOLTO alta!)
                // Range: 0 (motion basso) -> 20.0 (motion altissimo!)
                float tempMassFromMotion = (globalPerturbation / 255.0f) * (globalPerturbation / 255.0f) * 20.0f;

                if (perturbTarget == -1 && ball1_active) {
                    // Aggiorna massa temporanea smoothly
                    ball1_tempMass = ball1_tempMass * 0.7f + tempMassFromMotion * 0.3f; // Smooth transition
                    ball1_tempMass = min(ball1_tempMass, 20.0f);  // Cap massimo

                    // AGGIUNGI anche massa permanente (molto più lentamente)
                    float permMassIncrement = (globalPerturbation / 255.0f) * 0.003f;
                    ball1_mass += permMassIncrement;
                    ball1_mass = min(ball1_mass, 5.0f);  // Massa permanente max 5x
                } else if (perturbTarget == 1 && ball2_active) {
                    ball2_tempMass = ball2_tempMass * 0.7f + tempMassFromMotion * 0.3f;
                    ball2_tempMass = min(ball2_tempMass, 20.0f);

                    float permMassIncrement = (globalPerturbation / 255.0f) * 0.003f;
                    ball2_mass += permMassIncrement;
                    ball2_mass = min(ball2_mass, 5.0f);
                }
            }
        } else {
            // Decay VELOCE della massa temporanea quando motion cala
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

        // Update positions (moto con drag proporzionale alla massa temporanea)
        if (ball1_active) {
            ball1_pos += ball1_vel * dt * 1000.0f;

            // DRAG/ATTRITO proporzionale alla massa temporanea
            // Più massa temporanea = più rallenta (effetto "frenata")
            if (ball1_tempMass > 0.5f) {
                // Calcola drag factor: più massa = più attrito
                // Range: 0.0 (no drag) -> 0.95 (massimo attrito)
                float dragFactor = min(ball1_tempMass / 25.0f, 0.95f);
                ball1_vel *= (1.0f - dragFactor * dt * 2.0f);  // Rallenta gradualmente
            }
        }
        if (ball2_active) {
            ball2_pos += ball2_vel * dt * 1000.0f;

            // DRAG/ATTRITO proporzionale alla massa temporanea
            if (ball2_tempMass > 0.5f) {
                float dragFactor = min(ball2_tempMass / 25.0f, 0.95f);
                ball2_vel *= (1.0f - dragFactor * dt * 2.0f);  // Rallenta gradualmente
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
            float distance = abs(ball1_pos - ball2_pos);
            const float collisionRadius = 8.0f;  // Raggio di collisione (aumentato per rilevamento migliore)

            // Previeni collisioni multiple: almeno 200ms tra una collisione e l'altra
            bool canCollide = (now - lastCollisionTime) > 200;

            if (distance < collisionRadius && canCollide) {
                // Check se le palle si stanno avvicinando (non allontanando)
                // Se ball1 è a sinistra di ball2 e si stanno avvicinando:
                // ball1_vel deve essere > ball2_vel
                bool approaching = false;
                if (ball1_pos < ball2_pos) {
                    approaching = (ball1_vel > ball2_vel);  // ball1 più veloce verso destra
                } else {
                    approaching = (ball1_vel < ball2_vel);  // ball2 più veloce verso destra
                }

                if (approaching) {
                    // COLLISIONE ELASTICA 1D CON MASSE DIVERSE
                    // Conservazione momento: m1*v1 + m2*v2 = m1*v1' + m2*v2'
                    // Conservazione energia: (1/2)*m1*v1² + (1/2)*m2*v2² = (1/2)*m1*v1'² + (1/2)*m2*v2'²
                    //
                    // Formule (derivate da conservazione):
                    // v1' = ((m1 - m2)*v1 + 2*m2*v2) / (m1 + m2)
                    // v2' = ((m2 - m1)*v2 + 2*m1*v1) / (m1 + m2)

                    // USA MASSA TOTALE = massa base + massa temporanea da motion!
                    float m1 = ball1_mass + ball1_tempMass;
                    float m2 = ball2_mass + ball2_tempMass;
                    float v1 = ball1_vel;
                    float v2 = ball2_vel;

                    float totalMass = m1 + m2;

                    // Calcola nuove velocità usando formule fisiche corrette
                    float v1_new = ((m1 - m2) * v1 + 2.0f * m2 * v2) / totalMass;
                    float v2_new = ((m2 - m1) * v2 + 2.0f * m1 * v1) / totalMass;

                    ball1_vel = v1_new;
                    ball2_vel = v2_new;

                    // Separa FORZATAMENTE le palle
                    float midPoint = (ball1_pos + ball2_pos) / 2.0f;

                    if (ball1_pos < ball2_pos) {
                        ball1_pos = midPoint - (collisionRadius / 2.0f + 1.0f);
                        ball2_pos = midPoint + (collisionRadius / 2.0f + 1.0f);
                    } else {
                        ball1_pos = midPoint + (collisionRadius / 2.0f + 1.0f);
                        ball2_pos = midPoint - (collisionRadius / 2.0f + 1.0f);
                    }

                    lastCollisionTime = now;
                    collisionCount++;

                    Serial.print("[DUAL_PONG] Collision #");
                    Serial.print(collisionCount);
                    Serial.print(" | m1=");
                    Serial.print(m1, 2);  // Stampa massa TOTALE
                    Serial.print(" (");
                    Serial.print(ball1_mass, 1);
                    Serial.print("+");
                    Serial.print(ball1_tempMass, 1);
                    Serial.print(") m2=");
                    Serial.print(m2, 2);
                    Serial.print(" (");
                    Serial.print(ball2_mass, 1);
                    Serial.print("+");
                    Serial.print(ball2_tempMass, 1);
                    Serial.print(") | v1=");
                    Serial.print(ball1_vel, 3);
                    Serial.print(" v2=");
                    Serial.println(ball2_vel, 3);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    // STABILITY CHECK - Collasso quando velocità critiche
    // ═══════════════════════════════════════════════════════════

    // COLLASSO quando una palla diventa troppo lenta (quasi ferma)
    // Dopo ~π collisioni, la palla leggera si ferma
    const float minVelocity = 0.02f;  // pixel/ms (quasi ferma)

    if (!singleBallMode && (now - lastCollapseTime) > 2000) {
        // Check se una palla è quasi ferma (dominata dall'altra)
        bool ball1_stopped = ball1_active && (abs(ball1_vel) < minVelocity);
        bool ball2_stopped = ball2_active && (abs(ball2_vel) < minVelocity);

        // OPPURE se una è troppo veloce (oltre limite rendering)
        float maxRenderSpeed = 1.2f;
        bool ball1_critical = ball1_active && (abs(ball1_vel) > maxRenderSpeed);
        bool ball2_critical = ball2_active && (abs(ball2_vel) > maxRenderSpeed);

        if (ball1_stopped || ball2_stopped || ball1_critical || ball2_critical) {
            // COLLASSO DEL SISTEMA!
            singleBallMode = true;
            lastCollapseTime = now;

            // Determina quale palla vince (la più veloce o quella in movimento)
            bool ball1_wins = false;

            if (ball1_stopped && !ball2_stopped) {
                ball1_wins = false;  // Ball 2 vince (ball 1 ferma)
                Serial.print("[DUAL_PONG] COLLAPSE after ");
                Serial.print(collisionCount);
                Serial.println(" collisions! Ball 1 STOPPED - Ball 2 WINS");
            } else if (ball2_stopped && !ball1_stopped) {
                ball1_wins = true;   // Ball 1 vince (ball 2 ferma)
                Serial.print("[DUAL_PONG] COLLAPSE after ");
                Serial.print(collisionCount);
                Serial.println(" collisions! Ball 2 STOPPED - Ball 1 WINS");
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
                ball1_hue = ball1_hue + random8(80, 160);
                ball1_vel = (ball1_vel > 0) ? FIXED_BASE_SPEED : -FIXED_BASE_SPEED;
                ball1_mass = 1.0f;  // Reset massa
                nextBallHue = ball1_hue + 128;
            } else {
                ball1_active = false;
                ball2_hue = ball2_hue + random8(80, 160);
                ball2_vel = (ball2_vel > 0) ? FIXED_BASE_SPEED : -FIXED_BASE_SPEED;
                ball2_mass = 1.0f;  // Reset massa
                nextBallHue = ball2_hue + 128;
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
        // Condizione di respawn: palla solitaria è alla punta (>85%) e torna verso base (vel < 0)
        float tipThreshold = state.foldPoint * 0.85f;

        bool ball1_at_tip_returning = ball1_active && (ball1_pos > tipThreshold) && (ball1_vel < 0);
        bool ball2_at_tip_returning = ball2_active && (ball2_pos > tipThreshold) && (ball2_vel < 0);

        if (ball1_at_tip_returning || ball2_at_tip_returning) {
            // SPAWN NUOVA PALLA DALLA BASE CON LAMPO!
            singleBallMode = false;
            spawnFlashBrightness = 255;

            if (!ball2_active) {
                // Respawn ball2
                ball2_active = true;
                ball2_pos = 0.0f;  // Base della lama
                ball2_vel = FIXED_BASE_SPEED;  // Va verso la punta
                ball2_mass = 1.0f;  // Reset massa a valore base
                ball2_hue = nextBallHue;
                Serial.println("[DUAL_PONG] *** FLASH! Ball 2 SPAWNED from base ***");
            } else {
                // Respawn ball1
                ball1_active = true;
                ball1_pos = 0.0f;
                ball1_vel = FIXED_BASE_SPEED;  // Va verso la punta
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
            if (dist1 < ballRadius * 2.0f) {
                // Profilo gaussiano: I = I_max * exp(-dist²/(2σ²))
                // σ = ballRadius / √(2π) per normalizzazione visiva
                const float sigma = ballRadius / 2.5066f;  // √(2π) ≈ 2.5066
                float gaussian = expf(-(dist1 * dist1) / (2.0f * sigma * sigma));
                uint8_t ballBrightness = 255 * gaussian;

                // EFFETTO PULSANTE TREMOLANTE proporzionale alla massa temporanea
                // Più massa temporanea = più pulsa e tremola (effetto "carica di energia")
                if (ball1_tempMass > 1.0f) {
                    // Calcola intensità del pulsing in base alla massa temporanea
                    float massRatio = min(ball1_tempMass / 20.0f, 1.0f);  // 0.0 -> 1.0

                    // Pulsing: usa tempo per oscillazione continua
                    uint8_t pulsePhase = (now >> 3) & 0xFF;  // Cambia ogni ~8ms
                    uint8_t pulseBrightness = 128 + scale8(sin8(pulsePhase), 127);  // Range 128-255

                    // Tremore casuale aumenta con la massa
                    uint8_t jitter = random8(massRatio * 60);  // Tremore fino a 60

                    ballBrightness = scale8(ballBrightness, pulseBrightness);
                    ballBrightness = qadd8(ballBrightness, jitter);
                }

                if (ballBrightness > 30) {  // Soglia visibilità
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
            if (dist2 < ballRadius * 2.0f) {
                const float sigma = ballRadius / 2.5066f;
                float gaussian = expf(-(dist2 * dist2) / (2.0f * sigma * sigma));
                uint8_t ballBrightness = 255 * gaussian;

                // EFFETTO PULSANTE TREMOLANTE proporzionale alla massa temporanea
                // Più massa temporanea = più pulsa e tremola (effetto "carica di energia")
                if (ball2_tempMass > 1.0f) {
                    // Calcola intensità del pulsing in base alla massa temporanea
                    float massRatio = min(ball2_tempMass / 20.0f, 1.0f);  // 0.0 -> 1.0

                    // Pulsing: usa tempo per oscillazione continua
                    uint8_t pulsePhase = (now >> 3) & 0xFF;  // Cambia ogni ~8ms
                    uint8_t pulseBrightness = 128 + scale8(sin8(pulsePhase), 127);  // Range 128-255

                    // Tremore casuale aumenta con la massa
                    uint8_t jitter = random8(massRatio * 60);  // Tremore fino a 60

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
    uint16_t ignitionSpeed = map(state.speed, 1, 255, 100, 1);

    if (now - _lastIgnitionUpdate > ignitionSpeed) {
        if (_ignitionProgress < state.foldPoint) {
            _ignitionProgress++;
        } else {
            // Progress complete
            if (_ignitionOneShot) {
                // One-shot mode: mark as completed and return to IDLE
                _ignitionCompleted = true;
                _mode = Mode::IDLE;
            } else {
                // Normal mode: loop
                _ignitionProgress = 0;
            }
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
    // If one-shot mode and already completed, all LEDs off
    if (_retractionOneShot && _retractionCompleted) {
        fill_solid(_leds, _numLeds, CRGB::Black);
        return;
    }

    const unsigned long now = millis();
    uint16_t retractionSpeed = map(state.speed, 1, 255, 100, 1);

    if (_retractionProgress == 0) {
        _retractionProgress = state.foldPoint;
    }

    if (now - _lastRetractionUpdate > retractionSpeed) {
        if (_retractionProgress > 0) {
            _retractionProgress--;
        } else {
            // Progress complete (reached 0)
            if (_retractionOneShot) {
                // One-shot mode: mark as completed
                _retractionCompleted = true;
                _mode = Mode::IDLE;
                Serial.println("[LED] Retraction complete - all LEDs off");
            } else {
                // Normal mode: loop
                _retractionProgress = state.foldPoint;
            }
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
