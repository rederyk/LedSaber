# 🗺️ Roadmap: Migrazione a Optical Flow Motion Detection

## 📋 Executive Summary

Questo documento descrive la migrazione del sistema di motion detection da **Frame Differencing** (metodo attuale) a **Optical Flow** (block-based Lucas-Kanade semplificato).

**Obiettivo:** Migliorare drasticamente la precisione del tracking del movimento, aggiungere rilevamento direzionale e aumentare la robustezza al rumore.

**Timeline stimata:** 5-7 ore di sviluppo
**Rischio:** Medio (performance ESP32 da validare)

---

## 🔍 Analisi Situazione Attuale

### Problemi del Metodo Attuale (Frame Differencing)

Il sistema attuale in `MotionDetector.cpp` usa una **sottrazione pixel-by-pixel** tra frame consecutivi:

```cpp
// Codice attuale (linea 90-96)
for (size_t i = 0; i < _frameSize; i++) {
    int16_t diff = abs((int16_t)frameBuffer[i] - (int16_t)_previousFrame[i]);
    if (diff > _motionThreshold) {
        changedPixels++;
        totalDelta += diff;
    }
}
```

**Problemi identificati:**

1. ❌ **Sensibilità al rumore** - Auto-exposure e rumore del sensore causano falsi positivi
2. ❌ **Nessuna direzionalità** - Non distingue movimento UP/DOWN/LEFT/RIGHT
3. ❌ **Centroide impreciso** - Media pesata semplice senza context spaziale
4. ❌ **Smoothing eccessivo** - Fattore 0.3 rallenta risposta al movimento reale
5. ❌ **Perdita di dettagli** - Piccoli movimenti controllati non vengono rilevati bene

**Risultato:** Nei log di test, il movimento verticale della mano viene rilevato in modo sporadico e impreciso.

---

## ✨ Vantaggi di Optical Flow

### Perché Optical Flow è Superiore

**Optical Flow** calcola vettori di movimento analizzando il gradiente temporale dell'immagine:

```
Motion Vector (dx, dy) = argmin { SAD(Block_t0, Block_t1[x+dx, y+dy]) }
```

**Vantaggi:**

1. ✅ **Direzione movimento** - Vettori 2D indicano esattamente dove va l'oggetto
2. ✅ **Velocità movimento** - Magnitude del vettore = velocità in px/frame
3. ✅ **Robusto al rumore** - Analisi locale su blocchi invece di pixel singoli
4. ✅ **Tracking preciso** - Centroide basato su vettori filtrati
5. ✅ **Confidence metric** - Ogni vettore ha una confidence (0-255)

**Applicazioni abilitate:**

- Gesture recognition (swipe up/down/left/right)
- Velocity-based LED effects (più veloce = più luminoso)
- Motion heatmap (zone con più movimento)
- Predictive tracking (previsione traiettoria)

---

## 🏗️ Architettura Proposta

### Componenti del Sistema

```
┌─────────────────────────────────────────────────────────┐
│                   CameraManager                         │
│              (320x240 GRAYSCALE @ 15-20 FPS)            │
└────────────────────┬────────────────────────────────────┘
                     │ uint8_t* frameBuffer
                     ▼
┌─────────────────────────────────────────────────────────┐
│              OpticalFlowDetector                        │
│                                                          │
│  ┌────────────────────────────────────────────┐        │
│  │  1. Block Grid Division (8x6 = 48 blocks)  │        │
│  └────────────────────────────────────────────┘        │
│                     │                                    │
│  ┌────────────────────────────────────────────┐        │
│  │  2. Block Matching (SAD, ±10px window)     │        │
│  └────────────────────────────────────────────┘        │
│                     │                                    │
│  ┌────────────────────────────────────────────┐        │
│  │  3. Outlier Filtering (median filter)      │        │
│  └────────────────────────────────────────────┘        │
│                     │                                    │
│  ┌────────────────────────────────────────────┐        │
│  │  4. Global Motion Calculation              │        │
│  │     - Direction (8-way + NONE)              │        │
│  │     - Speed (px/frame)                      │        │
│  │     - Intensity (0-255)                     │        │
│  └────────────────────────────────────────────┘        │
│                     │                                    │
│  ┌────────────────────────────────────────────┐        │
│  │  5. Trajectory Tracking                     │        │
│  │     - Kalman-like smoothing                 │        │
│  │     - Confidence-weighted centroid          │        │
│  └────────────────────────────────────────────┘        │
└────────────────────┬────────────────────────────────────┘
                     │ Motion Data
                     ▼
┌─────────────────────────────────────────────────────────┐
│              BLEMotionService                           │
│  - intensity, direction, speed, trajectory              │
└─────────────────────────────────────────────────────────┘
```

---

## 📐 Specifiche Tecniche

### Parametri Griglia Optical Flow

```cpp
// Frame resolution
const uint16_t FRAME_WIDTH = 320;
const uint16_t FRAME_HEIGHT = 240;
const uint32_t FRAME_SIZE = 76800;  // bytes

// Block grid
const uint8_t BLOCK_SIZE = 40;      // 40x40 pixel per block
const uint8_t GRID_COLS = 8;        // 320 / 40 = 8
const uint8_t GRID_ROWS = 6;        // 240 / 40 = 6
const uint8_t TOTAL_BLOCKS = 48;    // 8 × 6

// Search parameters
const int8_t SEARCH_RANGE = 10;     // ±10 pixel search window
const uint8_t SEARCH_STEP = 2;      // Sample every 2 pixels (10x speedup)

// Thresholds
const uint8_t MIN_CONFIDENCE = 50;       // 0-255
const uint8_t MIN_ACTIVE_BLOCKS = 6;     // Min 6 blocks with motion
const float OUTLIER_SIGMA = 2.5f;        // Std deviations for outlier removal
```

### Strutture Dati

```cpp
// Motion vector per singolo blocco
struct BlockMotionVector {
    int8_t dx;              // Horizontal displacement (-127 to +127)
    int8_t dy;              // Vertical displacement (-127 to +127)
    uint8_t confidence;     // Match quality (0=bad, 255=perfect)
    uint16_t sad;           // Sum of Absolute Differences
};

// Direzioni movimento
enum class MotionDirection : uint8_t {
    NONE = 0,
    UP = 1,
    DOWN = 2,
    LEFT = 3,
    RIGHT = 4,
    UP_LEFT = 5,
    UP_RIGHT = 6,
    DOWN_LEFT = 7,
    DOWN_RIGHT = 8
};

// Metriche globali movimento
struct GlobalMotion {
    MotionDirection direction;
    float speed;                // px/frame
    uint8_t intensity;          // 0-255
    float centroidX;            // Normalized 0.0-1.0
    float centroidY;            // Normalized 0.0-1.0
    float confidence;           // Average confidence 0.0-1.0
    uint8_t activeBlocks;       // Number of blocks with motion
};
```

---

## 🎯 Roadmap di Implementazione

### FASE 1: Preparazione (30 min)

#### 1.1 Backup File Esistenti

```bash
cd /home/reder/Documenti/PlatformIO/Projects/ledSaber

# Backup
cp src/MotionDetector.h src/MotionDetector.h.backup
cp src/MotionDetector.cpp src/MotionDetector.cpp.backup

# Git checkpoint
git add -A
git commit -m "checkpoint: pre optical flow migration"
git checkout -b feature/optical-flow-detector
```

#### 1.2 Documentare API Esistente

Creare `MOTION_DETECTOR_API.md` con:
- Lista completa metodi pubblici
- Formato output BLE
- Test cases esistenti
- Performance baseline (FPS, RAM, CPU)

#### 1.3 Setup Feature Toggle

Per testare in parallelo vecchio e nuovo sistema:

```cpp
// src/config.h (nuovo file)
#ifndef CONFIG_H
#define CONFIG_H

// Feature flags
#define USE_OPTICAL_FLOW 1  // 0 = Frame Diff, 1 = Optical Flow

#endif
```

---

### FASE 2: Implementazione Core (2-3 ore)

#### 2.1 Creare OpticalFlowDetector.h

**File:** `src/OpticalFlowDetector.h`

```cpp
#ifndef OPTICAL_FLOW_DETECTOR_H
#define OPTICAL_FLOW_DETECTOR_H

#include <Arduino.h>

/**
 * @brief Optical Flow Motion Detector per ESP32-CAM
 *
 * Implementa block-based optical flow usando Sum of Absolute Differences (SAD)
 * per calcolare vettori di movimento tra frame consecutivi.
 *
 * Features:
 * - Direction tracking (8-way + NONE)
 * - Speed calculation (px/frame)
 * - Confidence per motion vector
 * - Outlier filtering
 * - Smooth trajectory tracking
 */
class OpticalFlowDetector {
public:
    OpticalFlowDetector();
    ~OpticalFlowDetector();

    // ═══════════════════════════════════════════════════════════
    // PUBLIC API (compatibile con MotionDetector)
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Inizializza detector
     * @param frameWidth Larghezza frame (default: 320)
     * @param frameHeight Altezza frame (default: 240)
     * @return true se successo
     */
    bool begin(uint16_t frameWidth = 320, uint16_t frameHeight = 240);

    /**
     * @brief Processa frame per optical flow
     * @param frameBuffer Buffer grayscale (1 byte/pixel)
     * @param frameLength Lunghezza in bytes
     * @return true se movimento rilevato
     */
    bool processFrame(const uint8_t* frameBuffer, size_t frameLength);

    /**
     * @brief Verifica se c'è movimento attivo
     */
    bool isMotionActive() const { return _motionActive; }

    /**
     * @brief Intensità movimento (0-255)
     */
    uint8_t getMotionIntensity() const { return _motionIntensity; }

    /**
     * @brief Flash LED consigliato (0-255)
     */
    uint8_t getRecommendedFlashIntensity() const { return _flashIntensity; }

    /**
     * @brief Configura sensibilità
     * @param sensitivity 0-255 (0=insensibile, 255=molto sensibile)
     */
    void setSensitivity(uint8_t sensitivity);

    /**
     * @brief Reset stato detector
     */
    void reset();

    // ═══════════════════════════════════════════════════════════
    // OPTICAL FLOW SPECIFIC API (nuove features)
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Direzioni movimento possibili
     */
    enum class Direction : uint8_t {
        NONE = 0,
        UP = 1,
        DOWN = 2,
        LEFT = 3,
        RIGHT = 4,
        UP_LEFT = 5,
        UP_RIGHT = 6,
        DOWN_LEFT = 7,
        DOWN_RIGHT = 8
    };

    /**
     * @brief Ottieni direzione movimento dominante
     */
    Direction getMotionDirection() const { return _motionDirection; }

    /**
     * @brief Ottieni velocità movimento (px/frame)
     */
    float getMotionSpeed() const { return _motionSpeed; }

    /**
     * @brief Ottieni confidence media (0.0-1.0)
     */
    float getMotionConfidence() const { return _motionConfidence; }

    /**
     * @brief Ottieni numero blocchi attivi
     */
    uint8_t getActiveBlocks() const { return _activeBlocks; }

    // ═══════════════════════════════════════════════════════════
    // TRAJECTORY TRACKING (compatibile + migliorato)
    // ═══════════════════════════════════════════════════════════

    struct TrajectoryPoint {
        float x;                // Normalized 0.0-1.0
        float y;                // Normalized 0.0-1.0
        uint32_t timestamp;     // Milliseconds
        uint8_t intensity;      // 0-255
        float speed;            // NEW: px/frame
        Direction direction;    // NEW: direzione in quel punto
    };

    static constexpr uint8_t MAX_TRAJECTORY_POINTS = 20;

    /**
     * @brief Ottieni traiettoria corrente
     * @param outPoints Array output (min MAX_TRAJECTORY_POINTS)
     * @return Numero punti validi
     */
    uint8_t getTrajectory(TrajectoryPoint* outPoints) const;

    // ═══════════════════════════════════════════════════════════
    // METRICHE E DEBUG
    // ═══════════════════════════════════════════════════════════

    struct Metrics {
        // Compatibili con MotionDetector
        uint32_t totalFramesProcessed;
        uint32_t motionFrameCount;
        uint8_t currentIntensity;
        uint8_t avgBrightness;
        uint8_t flashIntensity;
        uint8_t trajectoryLength;
        bool motionActive;

        // Nuove metriche optical flow
        uint32_t avgComputeTimeMs;      // Tempo calcolo per frame
        float avgConfidence;             // Confidence media
        uint8_t avgActiveBlocks;         // Blocchi attivi medi
        Direction dominantDirection;     // Direzione prevalente
        float avgSpeed;                  // Velocità media
    };

    Metrics getMetrics() const;

private:
    // ═══════════════════════════════════════════════════════════
    // PRIVATE DATA STRUCTURES
    // ═══════════════════════════════════════════════════════════

    struct BlockMotionVector {
        int8_t dx;              // -127 to +127
        int8_t dy;              // -127 to +127
        uint8_t confidence;     // 0-255
        uint16_t sad;           // Sum of Absolute Differences
        bool valid;             // Outlier filter flag
    };

    // ═══════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════

    bool _initialized;
    uint16_t _frameWidth;
    uint16_t _frameHeight;
    uint32_t _frameSize;

    // Grid configuration
    static constexpr uint8_t BLOCK_SIZE = 40;
    static constexpr uint8_t GRID_COLS = 8;   // 320/40 = 8
    static constexpr uint8_t GRID_ROWS = 6;   // 240/40 = 6
    static constexpr uint8_t TOTAL_BLOCKS = 48;

    // Search parameters
    int8_t _searchRange;        // ±pixels (default: 10)
    uint8_t _searchStep;        // Sample step (default: 2)
    uint8_t _minConfidence;     // Threshold (default: 50)
    uint8_t _minActiveBlocks;   // Min blocks (default: 6)

    // ═══════════════════════════════════════════════════════════
    // STATE
    // ═══════════════════════════════════════════════════════════

    // Frame buffers
    uint8_t* _previousFrame;    // PSRAM allocated

    // Motion vectors grid
    BlockMotionVector _motionVectors[GRID_ROWS][GRID_COLS];

    // Global motion state
    bool _motionActive;
    uint8_t _motionIntensity;
    Direction _motionDirection;
    float _motionSpeed;
    float _motionConfidence;
    uint8_t _activeBlocks;

    // Centroid tracking
    float _centroidX;
    float _centroidY;
    bool _centroidValid;

    // Trajectory
    TrajectoryPoint _trajectory[MAX_TRAJECTORY_POINTS];
    uint8_t _trajectoryLength;

    // Auto flash
    uint8_t _flashIntensity;
    uint8_t _avgBrightness;

    // Timing & metrics
    unsigned long _lastMotionTime;
    uint32_t _totalFramesProcessed;
    uint32_t _motionFrameCount;
    uint32_t _totalComputeTime;

    // ═══════════════════════════════════════════════════════════
    // CORE ALGORITHM
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Calcola optical flow completo
     */
    void _computeOpticalFlow(const uint8_t* currentFrame);

    /**
     * @brief Calcola motion vector per singolo blocco
     */
    void _calculateBlockMotion(uint8_t row, uint8_t col, const uint8_t* currentFrame);

    /**
     * @brief Calcola SAD tra due blocchi
     * @return Sum of Absolute Differences
     */
    uint16_t _computeSAD(
        const uint8_t* block1,
        const uint8_t* block2,
        uint16_t x1, uint16_t y1,
        uint16_t x2, uint16_t y2,
        uint8_t blockSize
    );

    /**
     * @brief Filtra outliers usando median filter
     */
    void _filterOutliers();

    /**
     * @brief Calcola movimento globale da vettori
     */
    void _calculateGlobalMotion();

    /**
     * @brief Calcola centroide pesato per confidence
     */
    void _calculateCentroid();

    /**
     * @brief Aggiorna traiettoria con smoothing Kalman-like
     */
    void _updateTrajectory();

    /**
     * @brief Calcola luminosità media (per auto flash)
     */
    uint8_t _calculateAverageBrightness(const uint8_t* frame);

    /**
     * @brief Aggiorna flash intensity
     */
    void _updateFlashIntensity();

    // ═══════════════════════════════════════════════════════════
    // UTILITY
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Converte vettore (dx, dy) in direzione
     */
    static Direction _vectorToDirection(float dx, float dy);

    /**
     * @brief Converte direzione in stringa (per debug)
     */
    static const char* _directionToString(Direction dir);
};

#endif // OPTICAL_FLOW_DETECTOR_H
```

#### 2.2 Implementare Core Algorithm

**File:** `src/OpticalFlowDetector.cpp`

Implementazione completa con:
1. Constructor/destructor con gestione PSRAM
2. `begin()` - allocazione buffer
3. `processFrame()` - entry point principale
4. `_computeOpticalFlow()` - loop sui blocchi
5. `_calculateBlockMotion()` - SAD search
6. `_computeSAD()` - ottimizzato con `__builtin_abs()`
7. `_filterOutliers()` - median filtering
8. `_calculateGlobalMotion()` - aggregazione vettori

**Pseudocodice chiave:**

```cpp
void OpticalFlowDetector::_calculateBlockMotion(uint8_t row, uint8_t col, const uint8_t* current) {
    uint16_t blockX = col * BLOCK_SIZE;
    uint16_t blockY = row * BLOCK_SIZE;

    int8_t bestDx = 0, bestDy = 0;
    uint16_t minSAD = UINT16_MAX;

    // Search window: ±10px in step di 2px
    for (int8_t dy = -_searchRange; dy <= _searchRange; dy += _searchStep) {
        for (int8_t dx = -_searchRange; dx <= _searchRange; dx += _searchStep) {
            // Bounds check
            int16_t searchX = blockX + dx;
            int16_t searchY = blockY + dy;

            if (searchX < 0 || searchY < 0 ||
                searchX + BLOCK_SIZE > _frameWidth ||
                searchY + BLOCK_SIZE > _frameHeight) {
                continue;
            }

            // Compute SAD
            uint16_t sad = _computeSAD(
                _previousFrame, current,
                blockX, blockY,
                searchX, searchY,
                BLOCK_SIZE
            );

            if (sad < minSAD) {
                minSAD = sad;
                bestDx = dx;
                bestDy = dy;
            }
        }
    }

    // Store vector
    BlockMotionVector& vec = _motionVectors[row][col];
    vec.dx = bestDx;
    vec.dy = bestDy;
    vec.sad = minSAD;

    // Confidence: inverso di SAD normalizzato
    // SAD range tipico: 0-10000 per blocco 40x40
    uint32_t maxSAD = BLOCK_SIZE * BLOCK_SIZE * 255;
    vec.confidence = 255 - min((uint32_t)255, (minSAD * 255) / (maxSAD / 10));
    vec.valid = (vec.confidence >= _minConfidence);
}
```

---

### FASE 3: Features Avanzate (1 ora)

#### 3.1 Direction Calculation

```cpp
OpticalFlowDetector::Direction OpticalFlowDetector::_vectorToDirection(float dx, float dy) {
    // Magnitude threshold
    float magnitude = sqrtf(dx * dx + dy * dy);
    if (magnitude < 2.0f) {  // < 2px/frame = fermo
        return Direction::NONE;
    }

    // Calcola angolo in gradi
    float angle = atan2f(dy, dx) * 180.0f / PI;

    // Normalizza 0-360
    if (angle < 0) angle += 360.0f;

    // Quantizza in 8 direzioni + NONE
    //   UP:         67.5 - 112.5  (90°)
    //   DOWN:      247.5 - 292.5  (270°)
    //   RIGHT:     337.5 -  22.5  (0°/360°)
    //   LEFT:      157.5 - 202.5  (180°)
    //   UP_RIGHT:   22.5 -  67.5  (45°)
    //   UP_LEFT:   112.5 - 157.5  (135°)
    //   DOWN_LEFT: 202.5 - 247.5  (225°)
    //   DOWN_RIGHT:292.5 - 337.5  (315°)

    if (angle >= 67.5f && angle < 112.5f) return Direction::UP;
    if (angle >= 247.5f && angle < 292.5f) return Direction::DOWN;
    if (angle >= 157.5f && angle < 202.5f) return Direction::LEFT;
    if (angle < 22.5f || angle >= 337.5f) return Direction::RIGHT;
    if (angle >= 22.5f && angle < 67.5f) return Direction::UP_RIGHT;
    if (angle >= 112.5f && angle < 157.5f) return Direction::UP_LEFT;
    if (angle >= 202.5f && angle < 247.5f) return Direction::DOWN_LEFT;
    if (angle >= 292.5f && angle < 337.5f) return Direction::DOWN_RIGHT;

    return Direction::NONE;
}
```

#### 3.2 Outlier Filtering

Usa **median filter 3x3** sui vettori:

```cpp
void OpticalFlowDetector::_filterOutliers() {
    for (uint8_t row = 1; row < GRID_ROWS - 1; row++) {
        for (uint8_t col = 1; col < GRID_COLS - 1; col++) {
            // Raccogli vettori 3x3 vicini
            int8_t dxValues[9], dyValues[9];
            uint8_t count = 0;

            for (int8_t dr = -1; dr <= 1; dr++) {
                for (int8_t dc = -1; dc <= 1; dc++) {
                    BlockMotionVector& vec = _motionVectors[row + dr][col + dc];
                    if (vec.valid) {
                        dxValues[count] = vec.dx;
                        dyValues[count] = vec.dy;
                        count++;
                    }
                }
            }

            if (count < 5) continue;  // Troppo pochi dati

            // Calcola mediana
            int8_t medianDx = _median(dxValues, count);
            int8_t medianDy = _median(dyValues, count);

            // Verifica se blocco centrale è outlier
            BlockMotionVector& center = _motionVectors[row][col];
            int8_t diffDx = abs(center.dx - medianDx);
            int8_t diffDy = abs(center.dy - medianDy);

            // Threshold: 3x la varianza tipica
            if (diffDx > 8 || diffDy > 8) {
                center.valid = false;  // Mark come outlier
            }
        }
    }
}
```

#### 3.3 Kalman-like Smoothing

Per traiettoria fluida:

```cpp
void OpticalFlowDetector::_updateTrajectory() {
    if (!_centroidValid) return;

    unsigned long now = millis();

    // Predizione Kalman semplificata
    const float ALPHA = 0.7f;  // Smoothing factor (0=molto smooth, 1=reattivo)

    if (_trajectoryLength > 0) {
        TrajectoryPoint& last = _trajectory[_trajectoryLength - 1];

        // Smooth position
        float predictedX = last.x + (last.speed * cosf(last.direction * PI / 4.0f)) * 0.05f;
        float predictedY = last.y + (last.speed * sinf(last.direction * PI / 4.0f)) * 0.05f;

        _centroidX = ALPHA * _centroidX + (1.0f - ALPHA) * predictedX;
        _centroidY = ALPHA * _centroidY + (1.0f - ALPHA) * predictedY;
    }

    // Aggiungi punto se movimento significativo
    float distance = 0.0f;
    if (_trajectoryLength > 0) {
        TrajectoryPoint& last = _trajectory[_trajectoryLength - 1];
        float dx = _centroidX - last.x;
        float dy = _centroidY - last.y;
        distance = sqrtf(dx * dx + dy * dy);
    }

    const float MIN_DISTANCE = 0.03f;  // 3% del frame
    if (_trajectoryLength == 0 || distance >= MIN_DISTANCE) {
        // Aggiungi nuovo punto
        if (_trajectoryLength < MAX_TRAJECTORY_POINTS) {
            TrajectoryPoint& point = _trajectory[_trajectoryLength];
            point.x = _centroidX;
            point.y = _centroidY;
            point.timestamp = now;
            point.intensity = _motionIntensity;
            point.speed = _motionSpeed;
            point.direction = _motionDirection;
            _trajectoryLength++;
        }
    }
}
```

---

### FASE 4: Integrazione BLE (1 ora)

#### 4.1 Estendere BLEMotionService

**Modifica:** `src/BLEMotionService.cpp`

Aggiornare JSON di stato per includere nuovi campi:

```cpp
String BLEMotionService::_getStatusJson() {
    JsonDocument doc;

    auto metrics = _motion->getMetrics();

    // Campi esistenti (compatibilità)
    doc["active"] = metrics.motionActive;
    doc["intensity"] = metrics.currentIntensity;
    doc["brightness"] = metrics.avgBrightness;
    doc["flash"] = metrics.flashIntensity;
    doc["trajectory_len"] = metrics.trajectoryLength;

    // NUOVI campi optical flow
    #if USE_OPTICAL_FLOW
    doc["direction"] = _directionToString(metrics.dominantDirection);
    doc["speed"] = round(metrics.avgSpeed * 10) / 10.0f;  // 1 decimal
    doc["confidence"] = round(metrics.avgConfidence * 100);  // 0-100%
    doc["active_blocks"] = metrics.avgActiveBlocks;
    #endif

    String output;
    serializeJson(doc, output);
    return output;
}
```

#### 4.2 Aggiornare Client Python

**Modifica:** Script Python controller

```python
def on_motion_notify(sender, data):
    """Handler notifiche motion"""
    try:
        status = json.loads(data.decode('utf-8'))

        # Campi esistenti
        intensity = status.get('intensity', 0)
        active = status.get('active', False)

        # NUOVI campi optical flow
        direction = status.get('direction', 'none')  # NEW
        speed = status.get('speed', 0.0)              # NEW
        confidence = status.get('confidence', 0)      # NEW

        # Print migliorato
        if active:
            print(f"🔍 Motion: {direction.upper()} "
                  f"| Speed: {speed:.1f}px/f "
                  f"| Intensity: {intensity} "
                  f"| Confidence: {confidence}%")
        else:
            print("🔍 Motion: IDLE")

    except Exception as e:
        print(f"Error parsing motion data: {e}")
```

---

### FASE 5: Testing & Tuning (1-2 ore)

#### 5.1 Test Suite

Creare `tests/test_optical_flow.cpp`:

```cpp
// Test 1: Movimento verticale UP
void test_vertical_up() {
    // Simula mano che si muove verso l'alto
    // Frame 0: mano in basso
    // Frame 1: mano in centro
    // Frame 2: mano in alto

    // Expected: direction=UP, speed>0, confidence>70%
}

// Test 2: Movimento orizzontale LEFT
void test_horizontal_left() {
    // Expected: direction=LEFT, speed>0
}

// Test 3: Oggetto fermo
void test_no_motion() {
    // Expected: direction=NONE, intensity=0
}

// Test 4: Rumore camera
void test_noise_rejection() {
    // Frame con solo rumore gaussiano
    // Expected: Nessun falso positivo
}

// Test 5: Performance
void test_performance() {
    // Misura tempo processFrame()
    // Expected: < 60ms @ 160MHz
}
```

#### 5.2 Metriche da Raccogliere

Durante test manuali, loggare:

```cpp
[OPTICAL FLOW TEST]
Frame: 1234
Compute time: 42ms
Active blocks: 12 / 48
Avg confidence: 78%
Direction: UP
Speed: 8.3 px/frame
Intensity: 127
Centroid: (0.52, 0.31)
Outliers filtered: 3
```

#### 5.3 Tuning Parametri

Parametri da ottimizzare:

| Parametro | Valore iniziale | Range test | Obiettivo |
|-----------|----------------|------------|-----------|
| `SEARCH_RANGE` | 10px | 6-15px | Bilanciare speed/accuracy |
| `SEARCH_STEP` | 2px | 1-3px | Max performance |
| `MIN_CONFIDENCE` | 50 | 30-80 | Ridurre falsi positivi |
| `MIN_ACTIVE_BLOCKS` | 6 | 4-10 | Sensibilità movimento |
| `OUTLIER_THRESHOLD` | 8px | 5-12px | Robustezza rumore |

**Processo tuning:**

1. **Test baseline** - Registra performance con valori default
2. **Vary one parameter** - Cambia solo un parametro alla volta
3. **Measure impact** - FPS, precision, false positives
4. **Document results** - Tabella comparativa
5. **Choose optimal** - Trade-off performance/accuracy

---

### FASE 6: Cleanup (30 min)

#### 6.1 Rimuovere Codice Vecchio

Dopo validazione che optical flow funziona:

```bash
# Rimuovi backup
rm src/MotionDetector.h.backup
rm src/MotionDetector.cpp.backup

# Opzionale: mantieni per storico
mkdir -p deprecated/
mv src/MotionDetector.* deprecated/
```

#### 6.2 Aggiornare Documentazione

**README.md:**

```markdown
## Motion Detection

Il sistema usa **Optical Flow** per tracking preciso del movimento:

- ✅ Direction tracking (8-way: UP, DOWN, LEFT, RIGHT + diagonali)
- ✅ Speed measurement (px/frame)
- ✅ Confidence metric (0-100%)
- ✅ Auto flash LED basato su luminosità
- ✅ Trajectory smoothing con Kalman-like filter

### Performance

- Frame rate: 15-20 FPS @ 160MHz
- Latenza: 40-60ms per frame
- RAM usage: ~85KB (PSRAM)
- Precisione direzione: >95%
```

#### 6.3 Git Commit Finale

```bash
git add -A
git commit -m "feat: migrate to optical flow motion detection

BREAKING CHANGE: Motion detection now uses optical flow instead of frame differencing

Added:
- Block-based optical flow (8x6 grid)
- Direction tracking (8-way + NONE)
- Speed measurement (px/frame)
- Confidence metric per motion vector
- Outlier filtering (median 3x3)
- Kalman-like trajectory smoothing

Improved:
- Robustness to camera noise
- Tracking precision (+40% accuracy)
- False positive rejection

Performance:
- Frame rate: 15-20 FPS (vs 30-40 FPS with frame diff)
- Compute time: ~45ms/frame @ 160MHz
- RAM usage: +8KB

API changes:
- Added getMotionDirection() -> Direction enum
- Added getMotionSpeed() -> float px/frame
- Added getMotionConfidence() -> 0.0-1.0
- BLE status JSON now includes: direction, speed, confidence

Backward compatible:
- Existing API preserved (intensity, trajectory, flash)
- BLE clients work without changes
- Python controller extended with new fields

Tested:
- Vertical hand movement: ✅ (UP/DOWN detected)
- Horizontal movement: ✅ (LEFT/RIGHT detected)
- Static scene: ✅ (NONE, no false positives)
- Performance: ✅ (45ms avg @ 160MHz)
"
```

---

## 📊 Comparativa Prestazioni

### Metriche Attese

| Metrica | Frame Diff (old) | Optical Flow (new) | Delta |
|---------|------------------|-------------------|-------|
| **FPS** | 30-40 | 15-20 | -50% |
| **Latenza** | 10-15ms | 40-60ms | +3x |
| **RAM** | 77KB | 85KB | +8KB |
| **Precisione direzione** | N/A | 95%+ | +∞ |
| **Falsi positivi** | Alto | Basso | -80% |
| **Tracking smoothness** | Medio | Eccellente | +200% |
| **Robustezza rumore** | Bassa | Alta | +300% |

### Trade-off

**Sacrifici:**
- ⚠️ FPS ridotto di ~50% (ma 15 FPS è ancora ottimo per motion detection)
- ⚠️ Compute time +3x (ma margine c'è, ESP32 @ 240MHz disponibile)
- ⚠️ RAM +10% (ma solo 85KB su 4MB disponibili)

**Benefici:**
- ✅ Direction awareness (enable gesture recognition)
- ✅ Speed measurement (velocity-based effects)
- ✅ Confidence metric (quality control)
- ✅ Tracking precision +40%
- ✅ False positives -80%

**Verdict:** Trade-off molto favorevole! La riduzione FPS è accettabile per i benefici ottenuti.

---

## ⚠️ Rischi e Mitigazioni

### Rischio 1: Performance Insufficiente

**Sintomo:** FPS < 10, latenza > 100ms

**Mitigazioni:**
1. Ridurre griglia a 6x4 (24 blocchi) invece di 8x6
2. Aumentare `SEARCH_STEP` a 3px invece di 2px
3. Ridurre `SEARCH_RANGE` a ±8px invece di ±10px
4. Overclock ESP32 da 160MHz a 240MHz
5. Ottimizzare `_computeSAD()` con ESP32 SIMD instructions

**Fallback:** Mantenere flag `USE_OPTICAL_FLOW` per tornare a frame diff se necessario

---

### Rischio 2: RAM Overflow

**Sintomo:** ESP32 crash, heap corruption

**Mitigazioni:**
1. Verificare tutti buffer allocati in PSRAM (non DRAM interna)
2. Ridurre `MAX_TRAJECTORY_POINTS` da 20 a 15
3. Usare `uint8_t` invece di `uint16_t` dove possibile
4. Monitorare `ESP.getFreeHeap()` a runtime

**Diagnostics:**
```cpp
Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
Serial.printf("Min free heap: %u bytes\n", ESP.getMinFreeHeap());
```

---

### Rischio 3: Falsi Positivi

**Sintomo:** Motion rilevato quando scena è ferma

**Mitigazioni:**
1. Aumentare `MIN_CONFIDENCE` threshold
2. Aumentare `MIN_ACTIVE_BLOCKS` (più blocchi richiesti)
3. Migliorare outlier filtering (mediana 5x5 invece di 3x3)
4. Aggiungere temporal filtering (movimento confermato solo dopo 3 frame)

---

### Rischio 4: Breaking API BLE

**Sintomo:** Client Python non funziona più

**Mitigazioni:**
1. **Mantenere campi esistenti** nel JSON (intensity, active, trajectory_len)
2. **Aggiungere solo nuovi campi opzionali** (direction, speed, confidence)
3. Client vecchi ignorano campi unknown (JSON parsing robusto)
4. Testare con client prima di merge

---

## ✅ Acceptance Criteria

Prima di considerare la migrazione completa:

### Funzionalità
- [ ] Direction tracking funziona (8 direzioni + NONE)
- [ ] Speed measurement accurato (±10% su test controllato)
- [ ] Confidence > 70% su movimenti chiari
- [ ] Auto flash LED continua a funzionare
- [ ] Trajectory tracking smooth (no jitter)

### Performance
- [ ] FPS ≥ 15 @ 160MHz (accettabile: 12-15)
- [ ] Latenza ≤ 60ms/frame
- [ ] RAM usage ≤ 100KB
- [ ] Nessun crash dopo 1 ora di runtime
- [ ] Free heap stabile (no memory leak)

### Qualità
- [ ] Falsi positivi < 5% su scene ferme
- [ ] True positive rate > 90% su movimento chiaro
- [ ] No false negative su movimento verticale (problema originale)
- [ ] Robusto a variazioni luce

### Integrazione
- [ ] API pubblica compatibile
- [ ] BLE service funziona
- [ ] Client Python esteso con nuovi campi
- [ ] Tutti i test passano

### Documentazione
- [ ] README.md aggiornato
- [ ] API documentata (Doxygen comments)
- [ ] Commit message descrittivo
- [ ] Migration guide scritto (questo doc!)

---

## 🚀 Go/No-Go Decision

### GO se:
- ✅ Performance accettabili (≥12 FPS)
- ✅ Precision migliorata vs frame diff
- ✅ Nessun regression su features esistenti
- ✅ RAM usage sostenibile

### NO-GO se:
- ❌ FPS < 10 anche dopo ottimizzazioni
- ❌ RAM overflow irrisolvibile
- ❌ Breaking changes su BLE API
- ❌ Regression su flash LED auto

---

## 📞 Contatti & Support

**Maintainer:** @reder
**Branch:** `feature/optical-flow-detector`
**Docs:** `OPTICAL_FLOW_MIGRATION.md`

Per domande o problemi durante l'implementazione, documentare in:
- GitHub Issues con tag `optical-flow`
- Log di debug in `logs/optical_flow_test_YYYYMMDD.log`

---

## 📚 Riferimenti

### Papers & Algorithms
- Lucas-Kanade Optical Flow (1981)
- Horn-Schunck Dense Optical Flow (1981)
- Block Matching Algorithms (survey)

### ESP32 Resources
- ESP32 Camera Driver Docs
- ESP32 PSRAM Management
- ESP32 Performance Optimization Guide

### Similar Projects
- ESP32-CAM Optical Flow (GitHub)
- OpenCV OpticalFlow implementation
- Ardupilot OpticalFlow library

---

**Fine Documento**

_Ultimo aggiornamento: 2025-12-18_
_Versione: 1.0_
