# LedSaber Cam - Design Document

## 🎯 Obiettivo

Creare un sistema **coerente** ed **elastico** che collega i dati di movimento rilevati dalla camera (optical flow) agli effetti LED della lightsaber, rispettando la fisica e l'estetica delle spade laser classiche.

---

## 📊 Stato Attuale

### Motion Detection
- **Optical Flow**: Block-based SAD (8×6 grid)
- **Dati disponibili**:
  - `intensity` (0-255): velocità × confidence
  - `direction` (8-way): UP, DOWN, LEFT, RIGHT, diagonali
  - `speed` (px/frame): magnitudine movimento
  - `confidence` (0.0-1.0): affidabilità vettori
  - `trajectory`: fino a 20 punti con timestamp
  - `activeBlocks` (0-48): blocchi con movimento valido

### LED Effects (11 effetti)
| Effetto | Trigger Attuale | Note |
|---------|----------------|------|
| `solid` | Manuale | Colore statico |
| `rainbow` | Manuale | Animazione autonoma |
| `breathe` | Manuale | Pulsing autonomo |
| `ignition` | Manuale | ⚠️ Candidato per gesto |
| `retraction` | Manuale | ⚠️ Candidato per gesto |
| `flicker` | Manuale | Instabilità plasma (Kylo Ren) |
| `unstable` | Manuale | Flicker + sparks |
| `pulse` | Manuale | Onde di energia |
| `dual_pulse` | Manuale | Onde opposte |
| `clash` | Auto (3s loop) | ⚠️ Candidato per gesto |
| `rainbow_blade` | Manuale | Rainbow lineare |

---

## 🎨 Design Proposto: "Responsive Lightsaber"

### Filosofia

1. **Movimento NON classificato** (movimento "grezzo") → **Perturbazioni locali**
   - Simula instabilità del plasma come `flicker`, ma localizzata
   - Mantiene la lama "viva" e reattiva senza sembrare casuale

2. **Gesti classificati** → **Azioni specifiche**
   - Ignition: attivazione lama
   - Retraction: spegnimento lama
   - Clash: impatto/difesa

3. **Elasticità**: il sistema si adatta al contesto
   - Motion continuo → perturba effetti in corso
   - Gesti specifici → override temporaneo
   - Idle → ritorna allo stato base

---

## 🔧 Implementazione Tecnica

### 1. Motion Processing Layer (`MotionProcessor`)

**Nuovo file**: `src/MotionProcessor.h/cpp`

```cpp
class MotionProcessor {
public:
    enum class GestureType {
        NONE,
        IGNITION,      // UP veloce
        RETRACT,       // DOWN veloce
        CLASH,         // Spike improvviso di intensity
        SWING,         // LEFT/RIGHT veloce
        THRUST,        // UP rapido + corto
    };

    struct ProcessedMotion {
        GestureType gesture;
        uint8_t motionIntensity;      // 0-255 (motion grezzo)
        OpticalFlowDetector::Direction direction;
        float speed;
        uint32_t timestamp;

        // Localized perturbation data
        uint8_t perturbationGrid[6][8];  // Per-block intensity map
    };

    ProcessedMotion process(const MotionTaskResult& rawMotion,
                           const OpticalFlowDetector& detector);
};
```

#### Gesture Detection Logic

```cpp
GestureType _detectGesture() {
    // IGNITION: UP veloce e sostenuto (>150ms)
    if (direction == UP && speed > 8.0 && intensity > 180 && duration > 150) {
        return GestureType::IGNITION;
    }

    // RETRACT: DOWN veloce e sostenuto (>150ms)
    if (direction == DOWN && speed > 8.0 && intensity > 180 && duration > 150) {
        return GestureType::RETRACT;
    }

    // CLASH: spike improvviso (delta intensity > 100 in <50ms)
    if (deltaIntensity > 100 && deltaTime < 50) {
        return GestureType::CLASH;
    }

    // SWING: LEFT/RIGHT veloce (>120 intensity)
    if ((direction == LEFT || direction == RIGHT) && intensity > 120) {
        return GestureType::SWING;
    }

    return GestureType::NONE;
}
```

#### Perturbation Grid Calculation

```cpp
void _calculatePerturbationGrid(const OpticalFlowDetector& detector) {
    for (uint8_t row = 0; row < 6; row++) {
        for (uint8_t col = 0; col < 8; col++) {
            int8_t dx, dy;
            uint8_t confidence;

            if (detector.getBlockVector(row, col, &dx, &dy, &confidence)) {
                // Mappa motion block -> LED position
                // Griglia 8×6 blocchi -> 144 LED (72 logici)
                // Ogni blocco influenza ~9 LED vicini

                float magnitude = sqrt(dx*dx + dy*dy);
                perturbationGrid[row][col] = min(255, magnitude * confidence / 10);
            } else {
                perturbationGrid[row][col] = 0;
            }
        }
    }
}
```

---

### 2. Enhanced LED Effects (`LedEffectEngine`)

**Modifica**: `src/main.cpp` → estrai in `src/LedEffectEngine.h/cpp`

```cpp
class LedEffectEngine {
public:
    enum class Mode {
        IDLE,              // Effetto base (solid, breathe, ecc)
        IGNITION_ACTIVE,   // Override: ignition in corso
        RETRACT_ACTIVE,    // Override: retraction in corso
        CLASH_ACTIVE,      // Override: clash flash
    };

    void render(const LedState& state,
                const MotionProcessor::ProcessedMotion& motion);

private:
    Mode _mode = Mode::IDLE;
    uint32_t _modeStartTime = 0;

    // Render functions
    void _renderWithPerturbation(const LedState& state,
                                 const uint8_t perturbationGrid[6][8]);
    void _renderIgnition();
    void _renderRetract();
    void _renderClash();
};
```

#### Perturbation Application (Esempio con `solid`)

```cpp
void _renderWithPerturbation(const LedState& state, const uint8_t grid[6][8]) {
    CRGB baseColor = CRGB(state.r, state.g, state.b);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        // Mappa LED logico i -> blocco griglia (row, col)
        uint8_t col = map(i, 0, state.foldPoint, 0, GRID_COLS);
        uint8_t row = 3;  // Centro griglia (simula asse lama)

        uint8_t perturbation = grid[row][col];

        if (perturbation > 15) {  // Threshold minimo
            // Perturba localmente: aggiungi rumore luminosità
            uint8_t noise = random8(perturbation / 2);
            CRGB perturbedColor = baseColor;
            perturbedColor.fadeToBlackBy(noise);

            setLedPair(i, state.foldPoint, perturbedColor);
        } else {
            // No perturbation: colore base
            setLedPair(i, state.foldPoint, baseColor);
        }
    }
}
```

#### Gesture-Triggered Effects

```cpp
void render(const LedState& state, const ProcessedMotion& motion) {
    uint32_t now = millis();

    // Gesture overrides
    if (motion.gesture == GestureType::IGNITION && _mode == Mode::IDLE) {
        _mode = Mode::IGNITION_ACTIVE;
        _modeStartTime = now;
        _renderIgnition();  // Riusa codice esistente (linee 211-245)
        return;
    }

    if (motion.gesture == GestureType::RETRACT && _mode == Mode::IDLE) {
        _mode = Mode::RETRACT_ACTIVE;
        _modeStartTime = now;
        _renderRetract();   // Riusa codice esistente (linee 247-281)
        return;
    }

    if (motion.gesture == GestureType::CLASH) {
        _mode = Mode::CLASH_ACTIVE;
        _modeStartTime = now;
        _renderClash();     // Riusa codice esistente (linee 457-499)
        return;
    }

    // Check mode timeout (ritorna a IDLE)
    if (_mode == Mode::IGNITION_ACTIVE && now - _modeStartTime > 2000) {
        _mode = Mode::IDLE;
    }
    if (_mode == Mode::RETRACT_ACTIVE && now - _modeStartTime > 2000) {
        _mode = Mode::IDLE;
    }
    if (_mode == Mode::CLASH_ACTIVE && now - _modeStartTime > 500) {
        _mode = Mode::IDLE;
    }

    // IDLE mode: render effetto base + perturbazioni
    if (_mode == Mode::IDLE) {
        if (state.effect == "solid") {
            _renderWithPerturbation(state, motion.perturbationGrid);
        } else if (state.effect == "flicker") {
            // Flicker già ha rumore: esalta con perturbazione
            _renderFlickerEnhanced(state, motion.perturbationGrid);
        } else {
            // Altri effetti: rendering classico (no perturbazione per non rovinare animazioni)
            renderLedStrip();  // Codice esistente
        }
    }
}
```

---

### 3. Integration in `main.cpp`

```cpp
// Aggiungi globals
MotionProcessor motionProcessor;
LedEffectEngine effectEngine;

void loop() {
    // ... codice esistente ...

    if (gCachedMotionResult.valid && bleMotionService.isMotionEnabled()) {
        // Process raw motion -> gestures + perturbations
        auto processedMotion = motionProcessor.process(gCachedMotionResult, motionDetector);

        // Render LED con motion data
        effectEngine.render(ledState, processedMotion);

        // BLE updates (esistente)
        bleMotionService.update(gCachedMotionResult.motionDetected, false);
        if (now - lastMotionStatusNotify > 300) {
            bleMotionService.notifyStatus();
            lastMotionStatusNotify = now;
        }
    } else {
        // No motion: render classico
        renderLedStrip();
    }
}
```

---

## 🎭 Esempi di Comportamento

### Scenario 1: Idle + Movimento Lieve
**Input**: `intensity=40`, `direction=LEFT`, `speed=3.0`
**Output**:
- Nessun gesture rilevato
- Perturbazione leggera su LED lato sinistro (blocchi LEFT attivi)
- Effetto base (`solid`, `breathe`) continua con micro-flicker locale

### Scenario 2: Ignition Gesture
**Input**: `intensity=220`, `direction=UP`, `speed=12.0`, durata 200ms
**Output**:
- Gesture `IGNITION` rilevato
- Override mode → `IGNITION_ACTIVE`
- Animazione ignition (base→tip, 2 secondi)
- Dopo 2s: ritorna a IDLE con effetto base

### Scenario 3: Clash Gesture
**Input**: `intensity` spike 50→180 in 30ms
**Output**:
- Gesture `CLASH` rilevato
- Override mode → `CLASH_ACTIVE`
- Flash bianco 500ms (dissolvenza)
- Ritorna a effetto base dopo 500ms

### Scenario 4: Swing Continuo
**Input**: `intensity=150`, `direction=RIGHT`, `speed=9.0` sostenuto
**Output**:
- Gesture `SWING` rilevato (no override, solo enhancer)
- Perturbazione marcata su LED lato destro
- Effetto "trail" visivo: LED ritardano leggermente lo spegnimento
- Può combinare con `pulse` per effetto "swoosh"

---

## 🔧 Configurazione Utente (Opzionale)

### BLE Config Characteristic (estensione)

```json
{
    "motionEnabled": true,
    "motionSensitivity": 128,
    "gesturesEnabled": true,           // NEW
    "gestureThreshold": 180,           // NEW: intensity per gesture (0-255)
    "perturbationEnabled": true,       // NEW
    "perturbationIntensity": 128       // NEW: 0=off, 255=max
}
```

### Script Python Extension

```python
# Nuovo comando
> motion config gesture-threshold 200
✓ Gesture threshold impostato a 200

> motion config perturbation on
✓ Perturbazioni LED abilitate
```

---

## 📈 Vantaggi del Design

### ✅ Coerenza
- **Motion grezzo** = perturbazioni locali (fisica: plasma instabile)
- **Gesti** = azioni cinematiche (ignition, clash, swing)
- Separazione chiara tra "rumore ambientale" e "intent"

### ✅ Elasticità
- Sistema modulare: `MotionProcessor` + `LedEffectEngine` indipendenti
- Gesture detection configurabile (threshold, durata)
- Perturbazioni possono essere disattivate per effetti "puliti" (rainbow, breathe)

### ✅ Compatibilità
- **Backward compatible**: senza motion detection, tutto funziona come prima
- Riusa codice esistente per ignition/retract/clash
- BLE API estesa ma non breaking

### ✅ Performance
- Perturbation grid pre-calcolata (8×6 = 48 valori)
- Gesture detection: solo 5 confronti per frame
- No overhead se motion detection disabilitata

---

## 🚧 Roadmap Implementazione

### Phase 1: Foundation (1-2 ore)
1. Crea `MotionProcessor.h/cpp` con gesture detection
2. Aggiungi perturbation grid calculation
3. Unit test gesture logic (mock data)

### Phase 2: LED Integration (2-3 ore)
4. Estrai rendering in `LedEffectEngine.h/cpp`
5. Implementa `_renderWithPerturbation()` per `solid`
6. Hook in `main.cpp` loop

### Phase 3: Gesture Effects (1-2 ore)
7. Collega gesture IGNITION/RETRACT/CLASH a override modes
8. Test timing e transitions

### Phase 4: Enhanced Effects (2-3 ore)
9. Estendi perturbazioni a `flicker`, `unstable`
10. Aggiungi "trail effect" per SWING
11. Tuning parametri (threshold, decay rates)

### Phase 5: Configuration (1 ora)
12. Estendi BLE config characteristic
13. Aggiorna script Python con nuovi comandi

### Phase 6: Testing & Polish (2 ore)
14. Test end-to-end con lightsaber fisica
15. Tuning quality e thresholds
16. Documentazione finale

**Totale stimato**: 9-13 ore di sviluppo attivo

---

## 🔍 Note Tecniche

### Memory Usage
- `MotionProcessor`: ~150 bytes (perturbation grid + stato)
- `LedEffectEngine`: ~100 bytes (mode state)
- **Totale overhead**: ~250 bytes (OK per ESP32 con 520KB RAM)

### Latency Budget
- Optical flow: ~30-50ms/frame (esistente)
- Gesture detection: <1ms (confronti semplici)
- Perturbation calc: ~2-3ms (48 blocchi)
- LED rendering: 20ms (esistente, frame limiter)
- **Totale**: ~55ms → ~18 FPS (accettabile)

### Flash Memory
- Nuovo codice: ~8-10KB (MotionProcessor + LedEffectEngine)
- ESP32: 4MB flash → impatto minimo

---

## 📚 References

### File da Modificare
- [main.cpp](src/main.cpp) linee 184-527 (rendering loop)
- [main.cpp](src/main.cpp) linee 601-728 (main loop)

### File da Creare
- `src/MotionProcessor.h` (nuovo)
- `src/MotionProcessor.cpp` (nuovo)
- `src/LedEffectEngine.h` (nuovo, estratto da main.cpp)
- `src/LedEffectEngine.cpp` (nuovo, estratto da main.cpp)

### API da Estendere
- [BLEMotionService](src/BLEMotionService.h) per config gesture (opzionale)
- Script Python `ledsaber_controller.py` per comandi motion (opzionale)

---

## 🎯 Success Criteria

Il sistema è **completo e funzionante** quando:

1. ✅ Motion detection attiva genera perturbazioni visibili su LED
2. ✅ Gesto UP veloce trigge ignition automaticamente
3. ✅ Gesto DOWN veloce trigger retraction automaticamente
4. ✅ Spike di movimento trigger clash flash
5. ✅ Sistema ritorna a IDLE dopo gesti completati
6. ✅ Perturbazioni non interferiscono con effetti animati (rainbow, pulse)
7. ✅ Performance: >15 FPS, <100ms latency gesture→LED
8. ✅ Configurabile via BLE (enable/disable gesture, threshold)

---

**Autore**: Claude Sonnet 4.5
**Data**: 2025-12-18
**Versione**: 1.0
**Status**: 📝 Design Document - Ready for Implementation
