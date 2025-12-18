# Optical Flow Detector - Analisi e Ottimizzazioni

**Data**: 2025-12-18
**Issue**: Bassi FPS (3.28) e bassa intensità motion detection
**Target**: 5-8 FPS con migliore rilevamento movimento

---

## Analisi Problemi Identificati

### 1. CONFIGURAZIONE CAMERA SUB-OTTIMALE

**File**: `src/CameraManager.cpp:87`

**Problema**:
```cpp
config.xclk_freq_hz = 20000000;  // 20MHz clock - TROPPO BASSO
```

**Impatto**:
- OV2640 supporta fino a 24MHz
- FPS attuale: 3.28 (misurato)
- FPS teorico a 24MHz: ~4.5-5.0

**Soluzione**: Aumentare a 24MHz → **+20-30% FPS**

---

### 2. SEARCH RANGE INADEGUATO PER BASSI FPS

**File**: `src/OpticalFlowDetector.cpp:11-12`

**Problema**:
```cpp
_searchRange(6)        // ±6 px = finestra 13×13
_searchStep(3)         // step 3 = 25 posizioni testate
```

**Calcolo movimento reale**:
- A 3.28 FPS → ~305ms tra frame
- Movimento mano: 50-100 px/sec
- **Spostamento tra frame: 15-30 px**
- Search range attuale: ±6px → **NON COPRE il movimento!**

**Risultato**: Molti blocchi "perdono" il target → bassa intensità

**Soluzione**:
- Search range: ±6 → **±12 px**
- Search step: 3 → **4 px**
- Trade-off: più test (25 → 49) ma copre movimento reale

---

### 3. CALCOLO BRIGHTNESS INEFFICIENTE

**File**: `src/OpticalFlowDetector.cpp:417-427`

**Problema**:
```cpp
uint8_t OpticalFlowDetector::_calculateAverageBrightness(const uint8_t* frame) {
    uint64_t total = 0;
    for (size_t i = 0; i < _frameSize; i++) {  // 76,800 iterazioni!
        total += frame[i];
    }
    return (uint8_t)(total / _frameSize);
}
```

**Impatto**: 76,800 iterazioni per frame, chiamato ad ogni frame

**Soluzione**: Sample ogni N pixel (es. 1 ogni 4) → **-75% operazioni, +5-10% FPS**

---

### 4. CALCOLO SAD SENZA EARLY TERMINATION

**File**: `src/OpticalFlowDetector.cpp:189-209`

**Problema**: SAD calcola sempre tutti i 1600 pixel (40×40) anche se SAD parziale > bestSAD

**Esempio**:
```cpp
uint16_t sad = 0;
for (uint8_t by = 0; by < blockSize; by++) {
    for (uint8_t bx = 0; bx < blockSize; bx++) {
        // ... calcolo diff
        sad += abs(diff);
        // MANCA: if (sad > minSAD) break;  ← EARLY EXIT
    }
}
```

**Soluzione**: Interrompi se `sad > minSAD` → **-30-40% calcoli inutili**

---

### 5. AUTO-EXPOSURE CAMERA ATTIVO

**File**: `src/CameraManager.cpp:116`

**Problema**:
```cpp
s->set_exposure_ctrl(s, 1);  // Auto exposure ENABLED
```

**Impatto**: Auto-exposure impiega tempo per stabilizzarsi, rallenta cattura

**Soluzione**: Disabilita e usa exposure fisso → **+5-10% FPS**

---

### 6. MEDIAN FILTER CON BUBBLE SORT

**File**: `src/OpticalFlowDetector.cpp:569-585`

**Problema**: Bubble sort O(n²) chiamato 48 volte/frame

**Impatto**: Minore, ma evitabile

**Soluzione**: Selection (trova solo mediana) o disabilita per blocchi bordo

---

## Piano Ottimizzazioni Implementate

### Priority 1 - Quick Wins (Implementate)

| # | Ottimizzazione | File | Gain Stimato | Linee Codice |
|---|----------------|------|--------------|--------------|
| 1 | Clock camera 20→24MHz | CameraManager.cpp:87 | +20-30% FPS | 1 |
| 2 | Brightness sampling 1:4 | OpticalFlowDetector.cpp:417 | +5-10% FPS | 5 |
| 3 | Disabilita auto-exposure | CameraManager.cpp:116 | +5-10% FPS | 1 |
| 4 | Search range ±6→±12, step 3→4 | OpticalFlowDetector.cpp:11-12 | 0% (compensa) | 2 |
| 5 | SAD early termination | OpticalFlowDetector.cpp:196 | +15-25% FPS | 10 |

**Total FPS gain stimato: 3.28 → 5-8 FPS (50-140% improvement)**

---

### Priority 2 - Advanced (Non implementate)

| # | Ottimizzazione | Gain Stimato | Effort |
|---|----------------|--------------|--------|
| 6 | Risoluzione QQVGA (160×120) | +200-300% FPS | Alto, riduce risoluzione |
| 7 | SIMD assembly per SAD | +100-200% FPS | Molto alto, assembly |
| 8 | Multi-scale search | +20-30% FPS | Alto, complesso |
| 9 | Frame skipping dinamico | Variabile | Medio |

---

## Metriche Pre-Ottimizzazione

```
FPS: 3.28
Total Frames: 50289
Motion Frames: 46
Motion Intensity: 9 (bassa)
Optical Flow Grid: Alcuni blocchi LEFT rilevati
```

**Problema evidenziato**: Movimento visibile (frecce LEFT nella grid) ma intensità bassa (9/255)

**Causa root**: Search range insufficiente perde molti blocchi → pochi activeBlocks → bassa intensità globale

---

## Metriche Post-Ottimizzazione (Attese)

```
FPS: 5-8 (target)
Motion Intensity: 30-80 (buona)
Active Blocks: 15-30 (vs 4-8 attuali)
```

---

## Test Plan

1. Compilare e flashare firmware ottimizzato
2. Verificare FPS con `cam status`
3. Verificare intensità motion con `motion status` durante movimento
4. Verificare grid optical flow mostra blocchi attivi consistenti
5. Confermare che gestures (IGNITION/RETRACT/SWING) vengono rilevati

---

## Note Implementazione

### Parametri Tuning

Se dopo ottimizzazioni l'intensità è ancora bassa:

**Opzione A - Aumenta sensibilità**:
```cpp
_minConfidence = 30;      // Era 40, abbassa threshold
_minActiveBlocks = 3;     // Era 4, richiedi meno blocchi
```

**Opzione B - Aumenta search range ulteriormente**:
```cpp
_searchRange = 15;        // ±15px invece di ±12px
_searchStep = 5;          // Step più largo per compensare
```

**Opzione C - Ridimensiona intensità**:
```cpp
// In _calculateGlobalMotion(), riga 295
float normalizedSpeed = min(_motionSpeed / 15.0f, 1.0f);  // Era 20.0
```

### Monitoring

Aggiungere log diagnostici temporanei in `_calculateGlobalMotion()`:

```cpp
Serial.printf("[OPTICAL] activeBlocks=%u, speed=%.1f, intensity=%u, conf=%.2f\n",
              validBlocks, _motionSpeed, _motionIntensity, _motionConfidence);
```

---

## Riferimenti

- **OV2640 Datasheet**: Clock max 24MHz, typical 20-24MHz
- **ESP32 Camera Driver**: `esp_camera.h` config options
- **Optical Flow Literature**: Lucas-Kanade, block matching, SAD optimization

---

## Autore

Claude Code - Analisi automatizzata 2025-12-18
