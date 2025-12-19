# 🗺️ ROADMAP DETTAGLIATA - LED SABER EFFECTS

## ✅ GRUPPO 1: EFFETTI RENDERING (COMPLETATO)

### Status: 100% Completato - Build SUCCESS ✓

**Task completati:**
1. ✅ **Unstable Effect** - Mapping invertito e perturbazioni scure
2. ✅ **Dual Pulse Effect** - Più luminoso e difficile da scurire
3. ✅ **Pulse Effect** - Seamless con carica plasma e folded index

**File modificati:**
- `src/LedEffectEngine.h` - Aggiunte variabili `_pulseCharge`, `_pulseCharging`
- `src/LedEffectEngine.cpp` - Modifiche a `renderUnstable()`, `renderDualPulse()`, `renderPulse()`

---

## ✅ GRUPPO 2: LIFECYCLE MANAGEMENT (COMPLETATO)

### Status: 100% Completato - Build SUCCESS ✓

### Task 2.1: Ignition - Un solo ciclo all'avvio e connessione BLE ✅

**Obiettivo:** ✅ COMPLETATO
Evitare che l'effetto ignition vada in loop. Deve eseguire un solo ciclo completo quando:
- Sistema si avvia (boot) ✓
- Connessione BLE viene stabilita ✓

**Modifiche implementate:**

#### File 1: `src/LedEffectEngine.h`
```cpp
// Aggiungere nuove variabili membro:
bool _ignitionOneShot;        // true = ignition runs once
bool _ignitionCompleted;      // true = cycle completed
```

#### File 2: `src/LedEffectEngine.cpp`
```cpp
// Modifiche al costruttore:
LedEffectEngine::LedEffectEngine(...) :
    ...
    _ignitionOneShot(false),
    _ignitionCompleted(false)
{ ... }

// Modifiche a renderIgnition():
void LedEffectEngine::renderIgnition(const LedState& state) {
    // Se one-shot mode e già completato, non renderizzare
    if (_ignitionOneShot && _ignitionCompleted) {
        return;
    }

    // ... existing rendering code ...

    // Se progresso completato e in one-shot mode
    if (_ignitionProgress >= state.foldPoint && _ignitionOneShot) {
        _ignitionCompleted = true;
        _mode = Mode::IDLE;  // Torna a IDLE
    }
}

// Nuova funzione pubblica:
void triggerIgnitionOneShot() {
    _ignitionOneShot = true;
    _ignitionCompleted = false;
    _ignitionProgress = 0;
    _mode = Mode::IGNITION_ACTIVE;
    _modeStartTime = millis();
}
```

#### File 3: `src/main.cpp`
```cpp
// In setup(), dopo inizializzazione:
void setup() {
    // ... existing setup code ...

    // Trigger ignition all'avvio
    effectEngine.triggerIgnitionOneShot();
}
```

#### File 4: `src/BLELedController.cpp`
```cpp
// Nel callback di connessione BLE:
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("[BLE] Client connected");

        // Trigger ignition on connection
        if (effectEnginePtr) {
            effectEnginePtr->triggerIgnitionOneShot();
        }
    }
};
```

**File da modificare:**
- ✏️ `src/LedEffectEngine.h` - Aggiungere variabili e metodo pubblico
- ✏️ `src/LedEffectEngine.cpp` - Modificare `renderIgnition()`, aggiungere `triggerIgnitionOneShot()`
- ✏️ `src/main.cpp` - Chiamare trigger all'avvio
- ✏️ `src/BLELedController.cpp` - Aggiungere callback connessione e passare puntatore effectEngine

**Implementazione:** ✅ COMPLETATO
- File modificati: `LedEffectEngine.h`, `LedEffectEngine.cpp`, `main.cpp`
- Nuove variabili: `_ignitionOneShot`, `_ignitionCompleted`
- Nuovo metodo pubblico: `triggerIgnitionOneShot()`
- Trigger implementati: setup() all'avvio, MainServerCallbacks::onConnect()

---

### Task 2.2: Retraction - Implementazione per spegnimento ✅

**Obiettivo:** ✅ COMPLETATO
Completare l'effetto retraction per spegnimento futuro del dispositivo.
Deve funzionare come l'opposto di ignition. ✓

**Modifiche implementate:**

#### File 1: `src/LedEffectEngine.h`
```cpp
// Aggiungere:
void triggerRetractionOneShot();  // Trigger retraction singolo
bool _retractionOneShot;
bool _retractionCompleted;
```

#### File 2: `src/LedEffectEngine.cpp`
```cpp
// Modificare renderRetraction():
void LedEffectEngine::renderRetraction(const LedState& state) {
    // Simile a ignition, ma count-down
    // Se one-shot e completato, stop

    if (_retractionOneShot && _retractionCompleted) {
        // Tutto spento
        fill_solid(_leds, _numLeds, CRGB::Black);
        return;
    }

    // ... existing code ma fix loop ...

    if (_retractionProgress == 0 && _retractionOneShot) {
        _retractionCompleted = true;
        // Opzionale: disabilitare LEDs completamente
        // state.enabled = false;
    }
}

void LedEffectEngine::triggerRetractionOneShot() {
    _retractionOneShot = true;
    _retractionCompleted = false;
    _retractionProgress = state.foldPoint;  // Start from tip
    _mode = Mode::RETRACT_ACTIVE;
    _modeStartTime = millis();
}
```

#### File 3: `src/main.cpp`
```cpp
// Aggiungere funzione shutdown:
void shutdownDevice() {
    Serial.println("[SHUTDOWN] Triggering retraction...");
    effectEngine.triggerRetractionOneShot();

    // Aspettare che retraction finisca
    delay(2000);

    // Spegnere LEDs
    FastLED.clear();
    FastLED.show();

    // Deep sleep o power off
    // esp_deep_sleep_start();
}

// Opzionale: trigger su BLE disconnect
```

**Implementazione:** ✅ COMPLETATO
- File modificati: `LedEffectEngine.h`, `LedEffectEngine.cpp`
- Nuove variabili: `_retractionOneShot`, `_retractionCompleted`
- Nuovo metodo pubblico: `triggerRetractionOneShot()`
- Rendering modificato: retraction si ferma a 0 in one-shot mode
- Pronto per uso futuro (shutdown/disconnect)

---

## 🔮 GRUPPO 3: RAINBOW REFACTOR (FUTURO)

### Task 3.1: Rainbow Effect - Lama bianca con perturbazioni colorate

**Obiettivo:**
Deprecare il vecchio `rainbow` senza blade. Creare nuovo effetto `rainbow_effect`:
- Base: lama bianca (RGB 255,255,255)
- Perturbazioni: tutti i colori in base alla direzione del movimento
- Colori risultanti più luminosi grazie alla base bianca

**Modifiche necessarie:**

#### File 1: `src/LedEffectEngine.cpp`
```cpp
// Nuova funzione:
void LedEffectEngine::renderRainbowEffect(const LedState& state, const uint8_t perturbationGrid[6][8], const MotionProcessor::ProcessedMotion* motion) {
    CRGB whiteBase = CRGB(255, 255, 255);
    uint8_t safeBrightness = min(state.brightness, MAX_SAFE_BRIGHTNESS);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        CRGB ledColor = whiteBase;

        if (perturbationGrid != nullptr && motion != nullptr) {
            uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);

            // Calcolare direzione media nella colonna
            // UP = rosso, DOWN = blu, LEFT = verde, RIGHT = giallo
            // Diagonali = mix

            // Sample perturbation
            uint16_t perturbSum = 0;
            for (uint8_t row = 2; row <= 4; row++) {
                perturbSum += perturbationGrid[row][col];
            }
            uint8_t avgPerturbation = perturbSum / 3;

            if (avgPerturbation > 15) {
                // Determinare colore in base alla direzione
                uint8_t hue = getHueFromDirection(motion->direction);

                // Creare colore saturato
                CRGB perturbColor = CHSV(hue, 255, 255);

                // Blend con base bianca (più perturbazione = più colore)
                uint8_t blendAmount = scale8(avgPerturbation, 180);
                ledColor = blend(whiteBase, perturbColor, blendAmount);
            }
        }

        ledColor = scaleColorByBrightness(ledColor, safeBrightness);
        setLedPair(i, state.foldPoint, ledColor);
    }
}

// Helper function:
uint8_t LedEffectEngine::getHueFromDirection(MotionDirection dir) {
    switch(dir) {
        case MotionDirection::UP:        return 0;    // Rosso
        case MotionDirection::DOWN:      return 160;  // Blu
        case MotionDirection::LEFT:      return 96;   // Verde
        case MotionDirection::RIGHT:     return 64;   // Giallo
        case MotionDirection::UP_LEFT:   return 48;   // Arancione
        case MotionDirection::UP_RIGHT:  return 32;   // Giallo-rosso
        case MotionDirection::DOWN_LEFT: return 128;  // Ciano
        case MotionDirection::DOWN_RIGHT:return 192;  // Viola
        default:                         return 0;
    }
}
```

#### File 2: `src/BLELedController.cpp`
```cpp
// Modificare callback effect per supportare "rainbow_effect":
if (effect == "rainbow_effect") {
    _ledState.effect = "rainbow_effect";
} else if (effect == "rainbow") {
    // Deprecato - redirect a rainbow_effect
    Serial.println("[WARN] 'rainbow' is deprecated, use 'rainbow_effect'");
    _ledState.effect = "rainbow_effect";
}
```

#### File 3: `src/LedEffectEngine.h`
```cpp
// Aggiungere:
void renderRainbowEffect(const LedState& state, const uint8_t perturbationGrid[6][8], const MotionProcessor::ProcessedMotion* motion);
uint8_t getHueFromDirection(MotionDirection dir);
```

#### File 4: `src/LedEffectEngine.cpp` (render switch)
```cpp
// In render(), aggiungere case:
} else if (state.effect == "rainbow_effect") {
    renderRainbowEffect(state, motion ? motion->perturbationGrid : nullptr, motion);
}
```

**File da modificare:**
- ✏️ `src/LedEffectEngine.h` - Aggiungere metodi
- ✏️ `src/LedEffectEngine.cpp` - Implementare `renderRainbowEffect()` e helper
- ✏️ `src/BLELedController.cpp` - Supportare nuovo effect name
- ✏️ `src/MotionProcessor.h` - Verificare enum `MotionDirection` disponibile

**Stima complessità:** MEDIA-ALTA
**Dipendenze:** Richiede accesso a `motion->direction` (verificare disponibilità)

---

## 📊 PRIORITÀ E SEQUENZA CONSIGLIATA

1. **PRIMA:** Task 2.1 (Ignition one-shot) - Quick win, importante per UX
2. **POI:** Task 2.2 (Retraction) - Completa il lifecycle management
3. **INFINE:** Task 3.1 (Rainbow Effect) - Feature enhancement, non critica

## 🎯 METRICHE DI SUCCESSO

### Gruppo 2
- ✅ Ignition si attiva solo all'avvio e su BLE connect
- ✅ Nessun loop infinito
- ✅ Retraction spegne completamente la lama
- ✅ Build senza warning/errori

### Gruppo 3
- ✅ Lama appare bianca senza movimento
- ✅ Colori appaiono in base alla direzione
- ✅ Colori risultano più luminosi rispetto a rainbow classico
- ✅ Transizioni smooth e responsive

## ⚠️ ATTENZIONI

1. **BLE Callback:** Per Task 2.1, verificare che `BLELedController` abbia accesso al puntatore `effectEngine`
2. **Motion Direction:** Per Task 3.1, verificare che `ProcessedMotion` includa campo `direction`
3. **Memory:** Rainbow Effect potrebbe aumentare uso RAM - testare su dispositivo reale

## 📝 NOTE TECNICHE

- **Max Safe Brightness:** Tutti gli effetti rispettano `MAX_SAFE_BRIGHTNESS = 112`
- **Folded Strip:** Tutti gli effetti usano `setLedPair()` per compatibilità LED strip folded
- **Frame Rate:** Limite globale 20ms (50 FPS) mantenuto
- **Perturbation Grid:** 8x6 grid da optical flow detector
