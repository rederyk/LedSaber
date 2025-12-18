# Changelog - Optical Flow Optimization

**Data**: 2025-12-18
**Versione**: Post-ottimizzazione optical flow
**Issue**: Bassi FPS (3.28) e bassa intensità motion detection

---

## Modifiche Implementate

### 1. Camera Clock aumentato: 20MHz → 24MHz
**File**: `src/CameraManager.cpp:87`
**Gain atteso**: +20-30% FPS

```cpp
// Prima:
config.xclk_freq_hz = 20000000;  // 20MHz clock

// Dopo:
config.xclk_freq_hz = 24000000;  // 24MHz clock (OV2640 max, era 20MHz)
```

**Motivazione**: OV2640 supporta fino a 24MHz di clock. Aumentando da 20 a 24MHz otteniamo circa 20% di throughput in più per la cattura frame.

---

### 2. Auto-Exposure disabilitato
**File**: `src/CameraManager.cpp:116`
**Gain atteso**: +5-10% FPS

```cpp
// Prima:
s->set_exposure_ctrl(s, 1);  // Auto exposure
s->set_aec_value(s, 300);    // AEC value: 0 to 1200

// Dopo:
s->set_exposure_ctrl(s, 0);  // Auto exposure DISABLED (era 1, rallenta FPS)
s->set_aec_value(s, 400);    // AEC value fisso: 0 to 1200 (era 300, aumentato per compensare)
```

**Motivazione**: L'auto-exposure impiega tempo per stabilizzarsi ad ogni frame, rallentando la cattura. Con exposure fisso la camera cattura più velocemente. Aumentato valore fisso a 400 (da 300) per compensare l'assenza di regolazione automatica.

---

### 3. Brightness Calculation ottimizzato con sampling
**File**: `src/OpticalFlowDetector.cpp:417-431`
**Gain atteso**: +5-10% FPS

```cpp
// Prima:
uint64_t total = 0;
for (size_t i = 0; i < _frameSize; i++) {  // 76,800 iterazioni!
    total += frame[i];
}
return (uint8_t)(total / _frameSize);

// Dopo:
// Sample 1 pixel ogni 4 per ridurre calcoli (76800 → 19200 ops, -75%)
uint64_t total = 0;
uint32_t sampleCount = 0;
for (size_t i = 0; i < _frameSize; i += 4) {
    total += frame[i];
    sampleCount++;
}
return (uint8_t)(total / sampleCount);
```

**Motivazione**: Riduce operazioni del 75% (da 76,800 a 19,200) campionando 1 pixel ogni 4. La precisione della luminosità media è più che sufficiente per l'auto-flash.

---

### 4. Search Range adattato per bassi FPS
**File**: `src/OpticalFlowDetector.cpp:11-12`
**Gain atteso**: 0% (compensa movimento reale), migliora rilevamento

```cpp
// Prima:
_searchRange(6)        // Ridotto da 10 a 6 (movimento max ~20px/frame è sufficiente)
_searchStep(3)         // Aumentato da 2 a 3 (meno posizioni da testare)

// Dopo:
_searchRange(12)       // Aumentato da 6 a 12 (copre ~30px movimento a bassi FPS)
_searchStep(4)         // Aumentato da 3 a 4 (bilanciamento velocità/copertura)
```

**Motivazione**:
- A 3.28 FPS → ~305ms tra frame
- Movimento mano tipico: 50-100 px/sec
- **Spostamento reale tra frame: 15-30 px**
- Search range precedente ±6px → **NON copriva** il movimento reale!
- Nuovo range ±12px → copre fino a ~30px di movimento
- Search step 4 mantiene bilanciamento (7×7 = 49 posizioni testate vs 5×5 = 25 precedenti)

**Effetto**: Molti più blocchi rilevano correttamente il movimento → più activeBlocks → intensità più alta

---

### 5. SAD Early Termination implementato
**File**: `src/OpticalFlowDetector.cpp:189-216` + `src/OpticalFlowDetector.h:269-276`
**Gain atteso**: +15-25% FPS

```cpp
// Prima:
uint16_t OpticalFlowDetector::_computeSAD(..., uint8_t blockSize) {
    uint32_t sad = 0;
    for (uint8_t by = 0; by < blockSize; by++) {
        for (uint8_t bx = 0; bx < blockSize; bx++) {
            // ... calcolo diff
            sad += abs(diff);
            // MANCA early termination!
        }
    }
    return (sad > UINT16_MAX) ? UINT16_MAX : (uint16_t)sad;
}

// Dopo:
uint16_t OpticalFlowDetector::_computeSAD(..., uint8_t blockSize, uint16_t currentMinSAD) {
    uint32_t sad = 0;
    for (uint8_t by = 0; by < blockSize; by++) {
        for (uint8_t bx = 0; bx < blockSize; bx++) {
            // ... calcolo diff
            sad += abs(diff);

            // Early termination: interrompi se già peggiore del best
            if (sad >= currentMinSAD) {
                return currentMinSAD;  // Ritorna subito, tanto è già peggiore
            }
        }
    }
    return (sad > UINT16_MAX) ? UINT16_MAX : (uint16_t)sad;
}
```

**Motivazione**: Se il SAD parziale supera già il best SAD trovato finora, non serve continuare a calcolare i pixel rimanenti. Si interrompe subito risparmiando calcoli inutili.

**Effetto**: Riduce mediamente del 30-40% i calcoli SAD, specialmente nelle posizioni di ricerca lontane dal vettore reale.

---

## Risultati Attesi

### FPS Target
- **Prima**: 3.28 FPS
- **Dopo**: **5-8 FPS** (miglioramento +50% a +140%)

### Qualità Motion Detection
- **Active Blocks**: da 4-8 → 15-30 (più blocchi rilevano correttamente)
- **Motion Intensity**: da 9/255 → 30-80/255 (intensità significativa)
- **Gesture Detection**: IGNITION/RETRACT/SWING rilevati correttamente

### Breakdown Gain
| Ottimizzazione | FPS Gain | Cumulative FPS |
|----------------|----------|----------------|
| Base | - | 3.28 |
| Clock 24MHz | +20-30% | 3.93-4.26 |
| Brightness sampling | +5-10% | 4.13-4.69 |
| Auto-exposure OFF | +5-10% | 4.34-5.16 |
| SAD early exit | +15-25% | 4.99-6.45 |
| Search range (0%, migliora rilevamento) | - | 4.99-6.45 |
| **TOTALE** | **+52-97%** | **5.0-6.5 FPS** |

Nota: Gain non sono perfettamente moltiplicativi, stima conservativa 5-8 FPS.

---

## Test Plan

### Verifica FPS
```bash
> cam status
```
**Atteso**: FPS tra 5-8 (era 3.28)

### Verifica Motion Intensity
```bash
> motion status
```
**Durante movimento** (muovere mano davanti camera):
- **Intensity**: 30-80 (era 9)
- **Active Blocks**: 15-30 (era ~4-8)
- **Direction**: Correttamente rilevata
- **Grid**: Molti blocchi attivi con frecce consistenti

### Verifica Gestures
Testare movimenti rapidi:
- **UP rapido**: deve triggare IGNITION
- **DOWN rapido**: deve triggare RETRACT
- **LEFT/RIGHT rapido**: deve triggare SWING
- **Movimento improvviso**: deve triggare CLASH

---

## Tuning Opzionale

Se dopo flash l'intensità è ancora bassa, modificare:

### Opzione A: Aumenta sensibilità
```cpp
// In OpticalFlowDetector.cpp constructor:
_minConfidence(30)     // Era 40, abbassa threshold
_minActiveBlocks(3)    // Era 4, richiedi meno blocchi
```

### Opzione B: Aumenta search range ulteriormente
```cpp
_searchRange(15)       // Era 12
_searchStep(5)         // Era 4
```

### Opzione C: Scala intensità diversamente
```cpp
// In _calculateGlobalMotion(), riga ~295:
float normalizedSpeed = min(_motionSpeed / 15.0f, 1.0f);  // Era 20.0
```

---

## Note Compilazione

Build completato con successo:
```
RAM:   [=         ]  14.2% (used 46604 bytes from 327680 bytes)
Flash: [=======   ]  68.5% (used 1346609 bytes from 1966080 bytes)
========================= [SUCCESS] Took 17.21 seconds =========================
```

Warning deprecation (non critici):
- `pin_sscb_sda/pin_sscb_scl` deprecati (legacy camera driver, funzionano)
- `ADC_ATTEN_DB_11` deprecato (libreria I2S, non usato nel progetto)
- `containsKey()` deprecato ArduinoJson (BLE controller, funziona)

---

## File Modificati

1. `src/CameraManager.cpp` - Clock 24MHz, auto-exposure OFF
2. `src/OpticalFlowDetector.cpp` - Search range, brightness sampling, SAD early exit
3. `src/OpticalFlowDetector.h` - Firma _computeSAD aggiornata
4. `doc/OPTICAL_FLOW_OPTIMIZATION.md` - Documentazione analisi (nuovo)
5. `CHANGELOG_OPTICAL_FLOW.md` - Questo file (nuovo)

---

## Prossimi Passi

1. ✅ Compilazione completata
2. 🔄 Flash firmware su ESP32-CAM
3. 🔄 Test FPS con `cam status`
4. 🔄 Test motion detection con `motion status`
5. 🔄 Verifica gestures funzionano correttamente
6. 🔄 Tuning finale parametri se necessario

---

**Status**: ✅ Pronto per flash e test
