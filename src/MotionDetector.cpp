#include "MotionDetector.h"
#include <esp_heap_caps.h>
#include <math.h>

MotionDetector::MotionDetector()
    : _initialized(false)
    , _frameWidth(0)
    , _frameHeight(0)
    , _frameSize(0)
    , _previousFrame(nullptr)
    , _backgroundFrame(nullptr)
    , _motionThreshold(50)  // Aumentato da 30 a 50 per ridurre falsi positivi
    , _shakeThreshold(150)
    , _motionIntensity(0)
    , _changedPixels(0)
    , _lastTotalDelta(0)
    , _shakeDetected(false)
    , _motionDirection(DIRECTION_NONE)
    , _motionProximity(PROXIMITY_UNKNOWN)
    , _proximityTestRequested(false)
    , _proximityBrightness(0)
    , _currentGesture(GESTURE_NONE)
    , _gestureConfidence(0)
    , _backgroundInitialized(false)
    , _backgroundWarmupFrames(0)
    , _centroidValid(false)
    , _currentCentroidX(0.0f)
    , _currentCentroidY(0.0f)
    , _motionVectorX(0.0f)
    , _motionVectorY(0.0f)
    , _vectorConfidence(0)
    , _trajectoryIndex(0)
    , _trajectoryFull(false)
    , _historyIndex(0)
    , _historyFull(false)
    , _lastMotionTime(0)
    , _motionStartTime(0)
    , _lastStillTime(0)
    , _totalFramesProcessed(0)
    , _motionFrameCount(0)
    , _shakeCount(0)
{
    memset(_intensityHistory, 0, sizeof(_intensityHistory));
    memset(_zoneIntensities, 0, sizeof(_zoneIntensities));
    memset(_trajectoryBuffer, 0, sizeof(_trajectoryBuffer));
}

MotionDetector::~MotionDetector() {
    if (_previousFrame) {
        heap_caps_free(_previousFrame);
        _previousFrame = nullptr;
    }
    if (_backgroundFrame) {
        heap_caps_free(_backgroundFrame);
        _backgroundFrame = nullptr;
    }
    _initialized = false;
}

bool MotionDetector::begin(uint16_t frameWidth, uint16_t frameHeight) {
    if (_initialized) {
        Serial.println("[MOTION] Already initialized!");
        return true;
    }

    _frameWidth = frameWidth;
    _frameHeight = frameHeight;
    _frameSize = frameWidth * frameHeight;

    Serial.printf("[MOTION] Initializing for %ux%u frames (%u bytes)...\n",
                  _frameWidth, _frameHeight, _frameSize);

    // Alloca buffer frame precedente in PSRAM
    _previousFrame = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);
    _backgroundFrame = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);
    if (!_previousFrame || !_backgroundFrame) {
        Serial.println("[MOTION ERROR] Failed to allocate motion buffers!");
        if (_previousFrame) {
            heap_caps_free(_previousFrame);
            _previousFrame = nullptr;
        }
        if (_backgroundFrame) {
            heap_caps_free(_backgroundFrame);
            _backgroundFrame = nullptr;
        }
        return false;
    }

    // Inizializza a nero
    memset(_previousFrame, 0, _frameSize);
    memset(_backgroundFrame, 0, _frameSize);
    _backgroundInitialized = false;
    _backgroundWarmupFrames = 0;

    _initialized = true;
    _lastStillTime = millis();

    Serial.println("[MOTION] ✓ Initialized successfully!");
    Serial.printf("[MOTION] PSRAM allocated: %u bytes\n", _frameSize);
    Serial.printf("[MOTION] Motion threshold: %u\n", _motionThreshold);
    Serial.printf("[MOTION] Shake threshold: %u\n", _shakeThreshold);

    return true;
}

uint32_t MotionDetector::_calculateFrameDifference(const uint8_t* currentFrame) {
    uint32_t changedPixels = 0;
    uint32_t totalDelta = 0;
    const uint8_t* referenceFrame = (_backgroundInitialized && _backgroundFrame) ?
        _backgroundFrame : _previousFrame;

    // Confronta ogni pixel con frame di riferimento (background adattivo)
    for (size_t i = 0; i < _frameSize; i++) {
        int16_t diff = abs((int16_t)currentFrame[i] - (int16_t)referenceFrame[i]);
        if (diff > _motionThreshold) {
            changedPixels++;
            totalDelta += diff;
        }
    }

    _lastTotalDelta = totalDelta;
    return changedPixels;
}

uint8_t MotionDetector::_calculateMotionIntensity(uint32_t changedPixels) {
    if (changedPixels == 0 || _frameSize == 0) {
        return 0;
    }

    // Delta medio sui pixel variati
    uint32_t avgDelta = (_lastTotalDelta > 0) ? (_lastTotalDelta / changedPixels) : 0;
    if (avgDelta > 255) {
        avgDelta = 255;
    }

    // Copertura: quanta parte del frame è interessata
    uint32_t coverageNormalizer = _frameSize / 6;  // 16% pixel -> 255
    if (coverageNormalizer == 0) {
        coverageNormalizer = 1;
    }
    uint32_t coverageScore = (changedPixels * 255UL) / coverageNormalizer;
    if (coverageScore > 255) {
        coverageScore = 255;
    }

    // Intensità finale come media pesata tra ampiezza e copertura
    uint32_t amplitudeScore = avgDelta * 4;
    if (amplitudeScore > 255) {
        amplitudeScore = 255;
    }
    uint8_t intensity = static_cast<uint8_t>((coverageScore * 2 + amplitudeScore) / 3);

    return intensity;
}

void MotionDetector::_updateHistoryAndDetectShake(uint8_t intensity) {
    // Aggiungi intensità corrente all'history buffer
    _intensityHistory[_historyIndex] = intensity;
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;

    if (!_historyFull && _historyIndex == 0) {
        _historyFull = true;
    }

    // Rileva shake solo se history buffer pieno
    if (!_historyFull) {
        _shakeDetected = false;
        return;
    }

    // Calcola min/max intensità recente
    uint8_t minIntensity, maxIntensity;
    _getHistoryMinMax(minIntensity, maxIntensity);

    // Shake rilevato se swing intensità supera threshold
    uint8_t intensitySwing = maxIntensity - minIntensity;
    _shakeDetected = (intensitySwing > _shakeThreshold);

    if (_shakeDetected) {
        _shakeCount++;
        Serial.printf("[MOTION] 🔔 SHAKE DETECTED! Swing: %u (min: %u, max: %u)\n",
                      intensitySwing, minIntensity, maxIntensity);
    }
}

void MotionDetector::_getHistoryMinMax(uint8_t& outMin, uint8_t& outMax) const {
    if (!_historyFull) {
        outMin = outMax = 0;
        return;
    }

    outMin = 255;
    outMax = 0;

    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (_intensityHistory[i] < outMin) {
            outMin = _intensityHistory[i];
        }
        if (_intensityHistory[i] > outMax) {
            outMax = _intensityHistory[i];
        }
    }
}

bool MotionDetector::processFrame(const uint8_t* frameBuffer, size_t frameLength) {
    if (!_initialized) {
        Serial.println("[MOTION ERROR] Not initialized!");
        return false;
    }

    if (frameLength != _frameSize) {
        Serial.printf("[MOTION ERROR] Frame size mismatch! Expected %u, got %u\n",
                      _frameSize, frameLength);
        return false;
    }

    unsigned long now = millis();

    // Se richiesto test prossimità, analizza frame con flash
    if (_proximityTestRequested) {
        _motionProximity = _analyzeProximity(frameBuffer);
        _proximityTestRequested = false;

        Serial.printf("[MOTION] Proximity test result: %s (brightness: %u)\n",
                      getMotionProximityName(), _proximityBrightness);

        // Copia frame corrente in previous e termina (questo frame non è valido per motion detection)
        memcpy(_previousFrame, frameBuffer, _frameSize);
        _totalFramesProcessed++;
        return false;  // Frame con flash non conta come motion
    }

    // Inizializza modello di background al primo frame utile
    if (!_backgroundInitialized) {
        memcpy(_backgroundFrame, frameBuffer, _frameSize);
        memcpy(_previousFrame, frameBuffer, _frameSize);
        _backgroundInitialized = true;
        _backgroundWarmupFrames = 0;
        _totalFramesProcessed++;
        return false;
    }

    // Durante la fase di warmup accumuliamo frame per stabilizzare il background
    if (_backgroundWarmupFrames < BACKGROUND_WARMUP_FRAMES) {
        _backgroundWarmupFrames++;
        memcpy(_previousFrame, frameBuffer, _frameSize);
        _totalFramesProcessed++;
        return false;
    }

    // Calcola differenza frame
    _changedPixels = _calculateFrameDifference(frameBuffer);

    // Calcola intensità movimento
    _motionIntensity = _calculateMotionIntensity(_changedPixels);

    // Calcola intensità per zone e determina direzione
    _calculateZoneIntensities(frameBuffer);
    _motionDirection = _determineMotionDirection();

    // Aggiorna trajectory buffer con heatmap corrente
    _updateTrajectoryBuffer();

    // Riconosce gesture dalla trajectory
    _currentGesture = _recognizeGesture();

    // Aggiorna history e rileva shake
    _updateHistoryAndDetectShake(_motionIntensity);

    // Aggiorna timing
    // Richiedi almeno 2% di pixel cambiati per considerarlo movimento valido
    // (es: 320x240 = 76800 pixels, 2% = 1536 pixels)
    const uint32_t MIN_CHANGED_PIXELS = _frameSize / 50;  // 2% del frame
    bool motionDetected = (_motionIntensity > 0 && _changedPixels > MIN_CHANGED_PIXELS);

    if (motionDetected) {
        if (_lastMotionTime == 0) {
            _motionStartTime = now;
        }
        _lastMotionTime = now;
        _motionFrameCount++;
    } else {
        if (_lastMotionTime != 0) {
            _lastStillTime = now;
        }
    }

    // Aggiorna background quando scena è stabile
    _updateBackgroundModel(frameBuffer, motionDetected);

    // Copia frame corrente in previous per prossimo ciclo
    memcpy(_previousFrame, frameBuffer, _frameSize);

    _totalFramesProcessed++;

    return motionDetected;
}

void MotionDetector::setSensitivity(uint8_t sensitivity) {
    // Sensitivity: 0 = molto insensibile, 255 = molto sensibile
    // Invertiamo threshold: sensitivity alta -> threshold basso
    _motionThreshold = map(sensitivity, 0, 255, 80, 10);
    _shakeThreshold = map(sensitivity, 0, 255, 200, 100);

    Serial.printf("[MOTION] Sensitivity updated: %u (motion_th: %u, shake_th: %u)\n",
                  sensitivity, _motionThreshold, _shakeThreshold);
}

void MotionDetector::reset() {
    _motionIntensity = 0;
    _changedPixels = 0;
    _lastTotalDelta = 0;
    _shakeDetected = false;
    _motionDirection = DIRECTION_NONE;
    _motionProximity = PROXIMITY_UNKNOWN;
    _proximityTestRequested = false;
    _proximityBrightness = 0;
    _currentGesture = GESTURE_NONE;
    _gestureConfidence = 0;
    _backgroundInitialized = false;
    _backgroundWarmupFrames = 0;
    _centroidValid = false;
    _currentCentroidX = 0.0f;
    _currentCentroidY = 0.0f;
    _motionVectorX = 0.0f;
    _motionVectorY = 0.0f;
    _vectorConfidence = 0;
    _trajectoryIndex = 0;
    _trajectoryFull = false;
    _historyIndex = 0;
    _historyFull = false;
    _lastMotionTime = 0;
    _motionStartTime = 0;
    _lastStillTime = millis();
    _totalFramesProcessed = 0;
    _motionFrameCount = 0;
    _shakeCount = 0;

    memset(_intensityHistory, 0, sizeof(_intensityHistory));
    memset(_zoneIntensities, 0, sizeof(_zoneIntensities));
    memset(_trajectoryBuffer, 0, sizeof(_trajectoryBuffer));

    if (_previousFrame) {
        memset(_previousFrame, 0, _frameSize);
    }
    if (_backgroundFrame) {
        memset(_backgroundFrame, 0, _frameSize);
    }

    Serial.println("[MOTION] State reset");
}

MotionDetector::MotionMetrics MotionDetector::getMetrics() const {
    MotionMetrics metrics;
    metrics.totalFramesProcessed = _totalFramesProcessed;
    metrics.motionFrameCount = _motionFrameCount;
    metrics.shakeCount = _shakeCount;
    metrics.currentIntensity = _motionIntensity;
    metrics.changedPixels = _changedPixels;
    metrics.direction = _motionDirection;
    metrics.gesture = _currentGesture;
    metrics.gestureConfidence = _gestureConfidence;
    metrics.proximity = _motionProximity;
    metrics.proximityBrightness = _proximityBrightness;
    metrics.centroidValid = _centroidValid;
    metrics.centroidX = _currentCentroidX;
    metrics.centroidY = _currentCentroidY;
    metrics.motionVectorX = _motionVectorX;
    metrics.motionVectorY = _motionVectorY;
    metrics.vectorConfidence = _vectorConfidence;

    _getHistoryMinMax(metrics.minIntensityRecent, metrics.maxIntensityRecent);

    // Copia intensità zone
    memcpy(metrics.zoneIntensities, _zoneIntensities, sizeof(_zoneIntensities));

    return metrics;
}

bool MotionDetector::isContinuousMotion(unsigned long durationMs) const {
    if (_lastMotionTime == 0) {
        return false;
    }

    unsigned long now = millis();
    unsigned long motionDuration = now - _motionStartTime;

    // Verifica che movimento sia continuo (ultimo rilevamento recente)
    bool stillActive = (now - _lastMotionTime) < 500;  // Max 500ms gap

    return stillActive && (motionDuration >= durationMs);
}

bool MotionDetector::isStill(unsigned long durationMs) const {
    if (_lastStillTime == 0) {
        return false;
    }

    unsigned long now = millis();
    unsigned long stillDuration = now - _lastStillTime;

    // Verifica che sia ancora fermo (nessun movimento recente)
    bool stillActive = (_lastMotionTime == 0) || (_lastStillTime > _lastMotionTime);

    return stillActive && (stillDuration >= durationMs);
}

void MotionDetector::_calculateZoneIntensities(const uint8_t* currentFrame) {
    // Reset zone intensities
    memset(_zoneIntensities, 0, sizeof(_zoneIntensities));

    uint64_t centroidWeightedX = 0;
    uint64_t centroidWeightedY = 0;
    uint32_t centroidWeight = 0;

    // Calcola dimensioni zone
    uint16_t zoneWidth = _frameWidth / GRID_COLS;
    uint16_t zoneHeight = _frameHeight / GRID_ROWS;

    // Per ogni zona calcola intensità movimento
    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        for (uint8_t col = 0; col < GRID_COLS; col++) {
            uint8_t zoneIndex = row * GRID_COLS + col;

            uint32_t zoneDeltaSum = 0;
            uint32_t zoneChangedPixels = 0;

            // Coordinate zona
            uint16_t startX = col * zoneWidth;
            uint16_t startY = row * zoneHeight;
            uint16_t endX = (col == GRID_COLS - 1) ? _frameWidth : (startX + zoneWidth);
            uint16_t endY = (row == GRID_ROWS - 1) ? _frameHeight : (startY + zoneHeight);

            // Scansiona pixel della zona
            for (uint16_t y = startY; y < endY; y++) {
                for (uint16_t x = startX; x < endX; x++) {
                    size_t pixelIdx = y * _frameWidth + x;

                    int16_t diffPrev = abs((int16_t)currentFrame[pixelIdx] -
                                      (int16_t)_previousFrame[pixelIdx]);
                    int16_t diffBg = 0;
                    if (_backgroundFrame) {
                        diffBg = abs((int16_t)currentFrame[pixelIdx] -
                                     (int16_t)_backgroundFrame[pixelIdx]);
                    }
                    int16_t diff = max(diffPrev, diffBg);

                    if (diff > _motionThreshold) {
                        zoneDeltaSum += diff;
                        zoneChangedPixels++;
                        centroidWeightedX += static_cast<uint64_t>(x) * diff;
                        centroidWeightedY += static_cast<uint64_t>(y) * diff;
                        centroidWeight += diff;
                    }
                }
            }

            // Calcola intensità media della zona
            if (zoneChangedPixels > 0) {
                uint8_t avgDelta = zoneDeltaSum / zoneChangedPixels;
                _zoneIntensities[zoneIndex] = min(avgDelta * 2, 255);
            } else {
                _zoneIntensities[zoneIndex] = 0;
            }
        }
    }

    // Aggiorna centroide e vettore movimento utilizzando i pesi calcolati
    bool hadCentroid = _centroidValid;
    float previousX = _currentCentroidX;
    float previousY = _currentCentroidY;

    if (centroidWeight > MIN_CENTROID_WEIGHT) {
        float centroidX = static_cast<float>(centroidWeightedX) / centroidWeight;
        float centroidY = static_cast<float>(centroidWeightedY) / centroidWeight;

        if (hadCentroid) {
            _currentCentroidX = previousX + (centroidX - previousX) * CENTROID_SMOOTHING;
            _currentCentroidY = previousY + (centroidY - previousY) * CENTROID_SMOOTHING;
        } else {
            _currentCentroidX = centroidX;
            _currentCentroidY = centroidY;
        }
        _centroidValid = true;
    } else {
        _centroidValid = false;
    }

    if (_centroidValid && hadCentroid) {
        _motionVectorX = _currentCentroidX - previousX;
        _motionVectorY = _currentCentroidY - previousY;

        float magnitude = sqrtf((_motionVectorX * _motionVectorX) +
                                (_motionVectorY * _motionVectorY));
        float normFactor = (_frameWidth > _frameHeight) ?
                           static_cast<float>(_frameWidth) :
                           static_cast<float>(_frameHeight);
        float normalizedMag = (normFactor > 0.0f) ? (magnitude / (normFactor / 4.0f)) : 0.0f;
        if (normalizedMag > 1.0f) {
            normalizedMag = 1.0f;
        }
        _vectorConfidence = static_cast<uint8_t>(normalizedMag * 100.0f);
    } else {
        // Decadimento graduale per evitare salti improvvisi
        _motionVectorX *= 0.5f;
        _motionVectorY *= 0.5f;
        if (_vectorConfidence > 5) {
            _vectorConfidence -= 5;
        } else {
            _vectorConfidence = 0;
        }
    }
}

MotionDetector::MotionDirection MotionDetector::_determineMotionDirection() {
    // Se il vettore movimento è affidabile usiamo prioritariamente quello
    if (_vectorConfidence >= 30) {
        MotionDirection vectorDir = _directionFromVector(_motionVectorX, _motionVectorY);
        if (vectorDir != DIRECTION_NONE) {
            return vectorDir;
        }
    }

    // Mappa zone a indici (3x3 grid):
    // 0 1 2
    // 3 4 5
    // 6 7 8

    // Somma intensità per direzioni
    uint16_t topSum = _zoneIntensities[0] + _zoneIntensities[1] + _zoneIntensities[2];
    uint16_t bottomSum = _zoneIntensities[6] + _zoneIntensities[7] + _zoneIntensities[8];
    uint16_t leftSum = _zoneIntensities[0] + _zoneIntensities[3] + _zoneIntensities[6];
    uint16_t rightSum = _zoneIntensities[2] + _zoneIntensities[5] + _zoneIntensities[8];
    uint16_t centerIntensity = _zoneIntensities[4];

    // Soglia minima per considerare movimento in una direzione
    const uint16_t DIRECTION_THRESHOLD = 50;

    // Se centro ha alta intensità, consideriamo movimento centrale
    if (centerIntensity > 100 && topSum < DIRECTION_THRESHOLD &&
        bottomSum < DIRECTION_THRESHOLD && leftSum < DIRECTION_THRESHOLD &&
        rightSum < DIRECTION_THRESHOLD) {
        return DIRECTION_CENTER;
    }

    // Determina direzione predominante
    int16_t verticalBias = bottomSum - topSum;    // Positivo = DOWN, Negativo = UP
    int16_t horizontalBias = rightSum - leftSum;  // Positivo = RIGHT, Negativo = LEFT

    // Soglia per direzioni diagonali
    const int16_t DIAGONAL_THRESHOLD = 30;

    // Nessun movimento significativo
    if (abs(verticalBias) < DIAGONAL_THRESHOLD && abs(horizontalBias) < DIAGONAL_THRESHOLD) {
        return DIRECTION_NONE;
    }

    // Direzioni diagonali (entrambi i bias significativi)
    if (abs(verticalBias) > DIAGONAL_THRESHOLD && abs(horizontalBias) > DIAGONAL_THRESHOLD) {
        if (verticalBias < 0 && horizontalBias < 0) return DIRECTION_UP_LEFT;
        if (verticalBias < 0 && horizontalBias > 0) return DIRECTION_UP_RIGHT;
        if (verticalBias > 0 && horizontalBias < 0) return DIRECTION_DOWN_LEFT;
        if (verticalBias > 0 && horizontalBias > 0) return DIRECTION_DOWN_RIGHT;
    }

    // Direzioni cardinali (un solo bias predominante)
    if (abs(verticalBias) > abs(horizontalBias)) {
        return (verticalBias < 0) ? DIRECTION_UP : DIRECTION_DOWN;
    } else {
        return (horizontalBias < 0) ? DIRECTION_LEFT : DIRECTION_RIGHT;
    }
}

const char* MotionDetector::getMotionDirectionName() const {
    switch (_motionDirection) {
        case DIRECTION_NONE:       return "none";
        case DIRECTION_UP:         return "up";
        case DIRECTION_DOWN:       return "down";
        case DIRECTION_LEFT:       return "left";
        case DIRECTION_RIGHT:      return "right";
        case DIRECTION_UP_LEFT:    return "up_left";
        case DIRECTION_UP_RIGHT:   return "up_right";
        case DIRECTION_DOWN_LEFT:  return "down_left";
        case DIRECTION_DOWN_RIGHT: return "down_right";
        case DIRECTION_CENTER:     return "center";
        default:                   return "unknown";
    }
}

MotionDetector::MotionDirection MotionDetector::_directionFromVector(float vecX, float vecY) const {
    const float MIN_COMPONENT = 0.15f;
    const float DIAGONAL_THRESHOLD = 0.35f;

    float absX = fabsf(vecX);
    float absY = fabsf(vecY);

    if (absX < MIN_COMPONENT && absY < MIN_COMPONENT) {
        return DIRECTION_NONE;
    }

    bool horizontalStrong = absX >= absY;
    bool verticalStrong = absY >= absX;

    if (absX > DIAGONAL_THRESHOLD && absY > DIAGONAL_THRESHOLD) {
        if (vecY < 0 && vecX < 0) return DIRECTION_UP_LEFT;
        if (vecY < 0 && vecX > 0) return DIRECTION_UP_RIGHT;
        if (vecY > 0 && vecX < 0) return DIRECTION_DOWN_LEFT;
        if (vecY > 0 && vecX > 0) return DIRECTION_DOWN_RIGHT;
    }

    if (horizontalStrong) {
        return (vecX < 0) ? DIRECTION_LEFT : DIRECTION_RIGHT;
    }
    if (verticalStrong) {
        return (vecY < 0) ? DIRECTION_UP : DIRECTION_DOWN;
    }

    return DIRECTION_NONE;
}

const char* MotionDetector::getGestureName() const {
    switch (_currentGesture) {
        case GESTURE_NONE:              return "none";
        case GESTURE_SLASH_VERTICAL:    return "slash_vertical";
        case GESTURE_SLASH_HORIZONTAL:  return "slash_horizontal";
        case GESTURE_ROTATION:          return "rotation";
        case GESTURE_THRUST:            return "thrust";
        default:                        return "unknown";
    }
}

// ============================================================================
// TRAJECTORY TRACKING
// ============================================================================

void MotionDetector::_updateTrajectoryBuffer() {
    // Copia heatmap corrente nel trajectory buffer
    memcpy(_trajectoryBuffer[_trajectoryIndex], _zoneIntensities, GRID_SIZE);

    // Avanza indice circolare
    _trajectoryIndex = (_trajectoryIndex + 1) % TRAJECTORY_DEPTH;

    if (!_trajectoryFull && _trajectoryIndex == 0) {
        _trajectoryFull = true;
    }
}

// ============================================================================
// GESTURE RECOGNITION
// ============================================================================

MotionDetector::GestureType MotionDetector::_recognizeGesture() {
    // Gesture recognition richiede trajectory buffer pieno
    if (!_trajectoryFull) {
        _gestureConfidence = 0;
        return GESTURE_NONE;
    }

    // Verifica che ci sia movimento sufficiente
    if (_motionIntensity < 40) {
        _gestureConfidence = 0;
        return GESTURE_NONE;
    }

    uint8_t confidence = 0;
    GestureType detectedGesture = GESTURE_NONE;

    // Tenta rilevamento pattern in ordine di priorità
    // (pattern più specifici prima)

    if (_detectThrust(confidence)) {
        detectedGesture = GESTURE_THRUST;
    } else if (_detectSlashVertical(confidence)) {
        detectedGesture = GESTURE_SLASH_VERTICAL;
    } else if (_detectSlashHorizontal(confidence)) {
        detectedGesture = GESTURE_SLASH_HORIZONTAL;
    } else if (_detectRotation(confidence)) {
        detectedGesture = GESTURE_ROTATION;
    }

    _gestureConfidence = confidence;

    // Log solo se gesture diversa da NONE con confidence sufficiente
    if (detectedGesture != GESTURE_NONE && confidence > 50) {
        Serial.printf("[GESTURE] Detected: %s (confidence: %u%%)\n",
                      getGestureName(), confidence);
    }

    return detectedGesture;
}

// ============================================================================
// PATTERN DETECTION - SLASH VERTICAL
// ============================================================================

bool MotionDetector::_detectSlashVertical(uint8_t& outConfidence) {
    // Pattern: movimento sequenziale dall'alto verso il basso
    // Analizziamo la progressione dell'attività nelle righe nel tempo

    outConfidence = 0;

    // Per ogni riga calcoliamo quando è stata più attiva
    int8_t rowPeakTime[GRID_ROWS];  // Timestamp relativo del picco (-1 = nessun picco)
    memset(rowPeakTime, -1, sizeof(rowPeakTime));

    // Scansiona trajectory per trovare picco di ogni riga
    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        uint16_t maxRowIntensity = 0;
        int8_t peakTime = -1;

        // Scansiona trajectory dal più vecchio al più recente
        for (uint8_t t = 0; t < TRAJECTORY_DEPTH; t++) {
            uint8_t frameIdx = (_trajectoryIndex + t) % TRAJECTORY_DEPTH;

            // Somma intensità di tutta la riga in questo frame
            uint16_t rowIntensity = 0;
            for (uint8_t col = 0; col < GRID_COLS; col++) {
                uint8_t cellIdx = row * GRID_COLS + col;
                rowIntensity += _trajectoryBuffer[frameIdx][cellIdx];
            }

            // Aggiorna picco
            if (rowIntensity > maxRowIntensity) {
                maxRowIntensity = rowIntensity;
                peakTime = t;
            }
        }

        // Registra tempo del picco (se significativo)
        if (maxRowIntensity > 100) {  // Soglia minima
            rowPeakTime[row] = peakTime;
        }
    }

    // Verifica sequenzialità: i picchi devono essere in ordine crescente temporale
    // da riga 0 (top) a riga 8 (bottom)
    uint8_t sequentialRows = 0;
    for (uint8_t row = 0; row < GRID_ROWS - 1; row++) {
        if (rowPeakTime[row] >= 0 && rowPeakTime[row + 1] >= 0) {
            if (rowPeakTime[row] < rowPeakTime[row + 1]) {
                sequentialRows++;
            }
        }
    }

    // Confidence proporzionale a quante righe seguono il pattern
    // Minimo 5 righe consecutive per considerarlo slash verticale
    if (sequentialRows >= 5) {
        outConfidence = map(sequentialRows, 5, GRID_ROWS - 1, 60, 100);
        return true;
    }

    outConfidence = 0;
    return false;
}

// ============================================================================
// PATTERN DETECTION - SLASH HORIZONTAL
// ============================================================================

bool MotionDetector::_detectSlashHorizontal(uint8_t& outConfidence) {
    // Pattern: movimento sequenziale da sinistra verso destra
    // Analizziamo la progressione dell'attività nelle colonne nel tempo

    outConfidence = 0;

    // Per ogni colonna calcoliamo quando è stata più attiva
    int8_t colPeakTime[GRID_COLS];
    memset(colPeakTime, -1, sizeof(colPeakTime));

    // Scansiona trajectory per trovare picco di ogni colonna
    for (uint8_t col = 0; col < GRID_COLS; col++) {
        uint16_t maxColIntensity = 0;
        int8_t peakTime = -1;

        // Scansiona trajectory dal più vecchio al più recente
        for (uint8_t t = 0; t < TRAJECTORY_DEPTH; t++) {
            uint8_t frameIdx = (_trajectoryIndex + t) % TRAJECTORY_DEPTH;

            // Somma intensità di tutta la colonna in questo frame
            uint16_t colIntensity = 0;
            for (uint8_t row = 0; row < GRID_ROWS; row++) {
                uint8_t cellIdx = row * GRID_COLS + col;
                colIntensity += _trajectoryBuffer[frameIdx][cellIdx];
            }

            // Aggiorna picco
            if (colIntensity > maxColIntensity) {
                maxColIntensity = colIntensity;
                peakTime = t;
            }
        }

        // Registra tempo del picco (se significativo)
        if (maxColIntensity > 100) {
            colPeakTime[col] = peakTime;
        }
    }

    // Verifica sequenzialità: i picchi devono essere in ordine crescente temporale
    // da colonna 0 (left) a colonna 8 (right)
    uint8_t sequentialCols = 0;
    for (uint8_t col = 0; col < GRID_COLS - 1; col++) {
        if (colPeakTime[col] >= 0 && colPeakTime[col + 1] >= 0) {
            if (colPeakTime[col] < colPeakTime[col + 1]) {
                sequentialCols++;
            }
        }
    }

    // Minimo 5 colonne consecutive
    if (sequentialCols >= 5) {
        outConfidence = map(sequentialCols, 5, GRID_COLS - 1, 60, 100);
        return true;
    }

    outConfidence = 0;
    return false;
}

// ============================================================================
// PATTERN DETECTION - ROTATION
// ============================================================================

bool MotionDetector::_detectRotation(uint8_t& outConfidence) {
    // Pattern: movimento circolare attorno al centro
    // Definiamo 8 settori attorno al centro e verifichiamo attivazione sequenziale

    outConfidence = 0;

    // Settori attorno al centro (clockwise):
    // 0: top, 1: top-right, 2: right, 3: bottom-right,
    // 4: bottom, 5: bottom-left, 6: left, 7: top-left

    // Mappa celle a settori (zona centrale = 4x4, escludiamo bordo esterno se necessario)
    // Semplifichiamo: dividiamo la griglia in 8 settori radiali

    int8_t sectorPeakTime[8];
    memset(sectorPeakTime, -1, sizeof(sectorPeakTime));

    // Per ogni settore troviamo il picco temporale
    for (uint8_t sector = 0; sector < 8; sector++) {
        uint16_t maxSectorIntensity = 0;
        int8_t peakTime = -1;

        // Scansiona trajectory
        for (uint8_t t = 0; t < TRAJECTORY_DEPTH; t++) {
            uint8_t frameIdx = (_trajectoryIndex + t) % TRAJECTORY_DEPTH;

            // Somma intensità celle del settore
            uint16_t sectorIntensity = 0;

            // Mappa settori a zone della griglia (approssimazione)
            // Centro griglia: (4, 4)
            for (uint8_t row = 0; row < GRID_ROWS; row++) {
                for (uint8_t col = 0; col < GRID_COLS; col++) {
                    // Calcola angolo della cella rispetto al centro
                    int8_t dy = row - 4;
                    int8_t dx = col - 4;

                    // Determina settore (0-7) basato su angolo
                    uint8_t cellSector = 0;
                    if (dx == 0 && dy < 0) cellSector = 0;       // top
                    else if (dx > 0 && dy < 0) cellSector = 1;   // top-right
                    else if (dx > 0 && dy == 0) cellSector = 2;  // right
                    else if (dx > 0 && dy > 0) cellSector = 3;   // bottom-right
                    else if (dx == 0 && dy > 0) cellSector = 4;  // bottom
                    else if (dx < 0 && dy > 0) cellSector = 5;   // bottom-left
                    else if (dx < 0 && dy == 0) cellSector = 6;  // left
                    else if (dx < 0 && dy < 0) cellSector = 7;   // top-left

                    if (cellSector == sector) {
                        uint8_t cellIdx = row * GRID_COLS + col;
                        sectorIntensity += _trajectoryBuffer[frameIdx][cellIdx];
                    }
                }
            }

            if (sectorIntensity > maxSectorIntensity) {
                maxSectorIntensity = sectorIntensity;
                peakTime = t;
            }
        }

        if (maxSectorIntensity > 80) {
            sectorPeakTime[sector] = peakTime;
        }
    }

    // Verifica sequenzialità circolare: settori attivati in ordine crescente
    uint8_t sequentialSectors = 0;
    for (uint8_t i = 0; i < 7; i++) {
        if (sectorPeakTime[i] >= 0 && sectorPeakTime[i + 1] >= 0) {
            if (sectorPeakTime[i] < sectorPeakTime[i + 1]) {
                sequentialSectors++;
            }
        }
    }

    // Minimo 4 settori consecutivi per rotazione
    if (sequentialSectors >= 4) {
        outConfidence = map(sequentialSectors, 4, 7, 60, 100);
        return true;
    }

    outConfidence = 0;
    return false;
}

// ============================================================================
// PATTERN DETECTION - THRUST
// ============================================================================

bool MotionDetector::_detectThrust(uint8_t& outConfidence) {
    // Pattern: movimento rapido e concentrato verso un punto specifico
    // Caratteristiche:
    // - Intensità cresce rapidamente nel tempo
    // - Movimento concentrato in una zona ristretta (poche celle)
    // - Picco finale molto più alto dell'inizio

    outConfidence = 0;

    // Analizza intensità totale nel tempo (tutti i frame)
    uint16_t intensityOverTime[TRAJECTORY_DEPTH];
    for (uint8_t t = 0; t < TRAJECTORY_DEPTH; t++) {
        uint8_t frameIdx = (_trajectoryIndex + t) % TRAJECTORY_DEPTH;
        uint16_t totalIntensity = 0;

        for (uint8_t i = 0; i < GRID_SIZE; i++) {
            totalIntensity += _trajectoryBuffer[frameIdx][i];
        }

        intensityOverTime[t] = totalIntensity;
    }

    // Verifica crescita rapida: ultimi frame molto più intensi dei primi
    uint16_t startAvg = (intensityOverTime[0] + intensityOverTime[1] + intensityOverTime[2]) / 3;
    uint16_t endAvg = (intensityOverTime[TRAJECTORY_DEPTH - 3] +
                       intensityOverTime[TRAJECTORY_DEPTH - 2] +
                       intensityOverTime[TRAJECTORY_DEPTH - 1]) / 3;

    // Thrust: fine almeno 3x più intenso dell'inizio
    if (endAvg < startAvg * 3) {
        outConfidence = 0;
        return false;
    }

    // Verifica concentrazione: trova numero celle attive nell'ultimo frame
    uint8_t latestFrameIdx = (_trajectoryIndex + TRAJECTORY_DEPTH - 1) % TRAJECTORY_DEPTH;
    uint8_t activeCells = 0;
    for (uint8_t i = 0; i < GRID_SIZE; i++) {
        if (_trajectoryBuffer[latestFrameIdx][i] > 50) {
            activeCells++;
        }
    }

    // Thrust deve essere concentrato (max 25 celle attive su 81)
    if (activeCells > 25) {
        outConfidence = 0;
        return false;
    }

    // Confidence basato su rapporto intensità e concentrazione
    uint16_t ratio = endAvg / max(startAvg, (uint16_t)1);
    uint16_t intensityRatio = min((uint16_t)ratio, (uint16_t)10);
    uint8_t concentrationScore = map(activeCells, 1, 25, 100, 60);

    uint16_t confCalc = (intensityRatio * 10 + concentrationScore) / 2;
    outConfidence = min((uint8_t)confCalc, (uint8_t)100);

    return (outConfidence > 50);
}

// ============================================================================
// PROXIMITY DETECTION
// ============================================================================

void MotionDetector::requestProximityTest() {
    _proximityTestRequested = true;
}

const char* MotionDetector::getMotionProximityName() const {
    switch (_motionProximity) {
        case PROXIMITY_UNKNOWN: return "unknown";
        case PROXIMITY_NEAR:    return "near";
        case PROXIMITY_FAR:     return "far";
        default:                return "invalid";
    }
}

MotionDetector::MotionProximity MotionDetector::_analyzeProximity(const uint8_t* flashFrame) {
    // Calcola luminosità media frame con flash
    _proximityBrightness = _calculateAverageBrightness(flashFrame);

    // Calcola anche il numero di pixel "molto luminosi" (hot spots)
    uint32_t brightPixelCount = 0;
    uint32_t maxBrightness = 0;
    const uint8_t HOT_SPOT_THRESHOLD = 200;  // Pixel molto luminosi

    for (size_t i = 0; i < _frameSize; i++) {
        if (flashFrame[i] > HOT_SPOT_THRESHOLD) {
            brightPixelCount++;
        }
        if (flashFrame[i] > maxBrightness) {
            maxBrightness = flashFrame[i];
        }
    }

    float brightPixelPercentage = (brightPixelCount * 100.0f) / _frameSize;

    Serial.printf("[PROXIMITY] Analysis - Avg brightness: %u, Max: %u, Hot pixels: %.2f%%\n",
                  _proximityBrightness, maxBrightness, brightPixelPercentage);

    // Logica decisione:
    // - NEAR: molti pixel molto luminosi (oggetto riflette direttamente il flash)
    // - FAR: luminosità diffusa, pochi hot spots (flash illumina sfondo)

    // Se ci sono molti hot pixels e/o picco massimo molto alto = NEAR
    if (brightPixelPercentage > 5.0f || maxBrightness > 240) {
        return PROXIMITY_NEAR;
    }

    // Se luminosità media alta ma diffusa = FAR
    if (_proximityBrightness > 80) {
        return PROXIMITY_FAR;
    }

    // Se tutto troppo scuro = movimento molto lontano o assente
    return PROXIMITY_FAR;
}

uint8_t MotionDetector::_calculateAverageBrightness(const uint8_t* frame) const {
    if (!frame || _frameSize == 0) {
        return 0;
    }

    // Calcola luminosità media
    uint64_t totalBrightness = 0;
    for (size_t i = 0; i < _frameSize; i++) {
        totalBrightness += frame[i];
    }

    return (uint8_t)(totalBrightness / _frameSize);
}

void MotionDetector::_updateBackgroundModel(const uint8_t* currentFrame, bool motionDetected) {
    if (!_backgroundFrame || !_backgroundInitialized) {
        return;
    }

    if (!motionDetected) {
        memcpy(_backgroundFrame, currentFrame, _frameSize);
        return;
    }

    const uint8_t decayShift = 5;  // Aggiorna lentamente (1/32)
    for (size_t i = 0; i < _frameSize; i++) {
        int16_t diff = static_cast<int16_t>(currentFrame[i]) -
                       static_cast<int16_t>(_backgroundFrame[i]);
        if (diff == 0) {
            continue;
        }

        uint8_t adjustment = (abs(diff) >> decayShift);
        if (adjustment == 0) {
            adjustment = 1;
        }

        if (diff > 0) {
            uint16_t updated = static_cast<uint16_t>(_backgroundFrame[i]) + adjustment;
            _backgroundFrame[i] = (updated > 255) ? 255 : updated;
        } else {
            _backgroundFrame[i] = (_backgroundFrame[i] > adjustment) ?
                                  (_backgroundFrame[i] - adjustment) : 0;
        }
    }
}
