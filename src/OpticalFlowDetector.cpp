#include "OpticalFlowDetector.h"
#include <esp_heap_caps.h>
#include <math.h>

OpticalFlowDetector::OpticalFlowDetector()
    : _initialized(false)
    , _frameWidth(0)
    , _frameHeight(0)
    , _frameSize(0)
    , _previousFrame(nullptr)
    , _edgeFrame(nullptr)
    , _binaryEdgeFrame(nullptr)
    , _labelMap(nullptr)
    , _searchRange(8)        // Ridotto per blocchi più piccoli (30px): 8px range
    , _searchStep(4)         // Step: 4px (4×4=16 posizioni, meno calcoli)
    , _minConfidence(25)     // Ridotto per blocchi più piccoli (meno pixel = SAD più basso)
    , _minActiveBlocks(6)    // Aumentato a 6 per griglia 8x8 (più blocchi disponibili)
    , _quality(160)      // Default: bilanciato (meno rumore)
    , _directionMagnitudeThreshold(2.0f)
    , _minCentroidWeight(100.0f)
    , _motionIntensityThreshold(15)
    , _motionSpeedThreshold(1.2f)
    , _hasPreviousFrame(false)
    , _currentBlobCount(0)
    , _previousBlobCount(0)
    , _nextBlobId(1)
    , _motionActive(false)
    , _motionIntensity(0)
    , _motionDirection(Direction::NONE)
    , _motionSpeed(0.0f)
    , _motionConfidence(0.0f)
    , _activeBlocks(0)
    , _centroidX(0.0f)
    , _centroidY(0.0f)
    , _centroidValid(false)
    , _trajectoryLength(0)
    , _flashIntensity(150)
    , _avgBrightness(0)
    , _frameDiffAvg(0)
    , _lastMotionTime(0)
    , _totalFramesProcessed(0)
    , _motionFrameCount(0)
    , _totalComputeTime(0)
{
    memset(_motionVectors, 0, sizeof(_motionVectors));
    memset(_trajectory, 0, sizeof(_trajectory));
    memset(_currentBlobs, 0, sizeof(_currentBlobs));
    memset(_previousBlobs, 0, sizeof(_previousBlobs));
}

OpticalFlowDetector::~OpticalFlowDetector() {
    if (_previousFrame) {
        heap_caps_free(_previousFrame);
        _previousFrame = nullptr;
    }
    if (_edgeFrame) {
        heap_caps_free(_edgeFrame);
        _edgeFrame = nullptr;
    }
    if (_binaryEdgeFrame) {
        heap_caps_free(_binaryEdgeFrame);
        _binaryEdgeFrame = nullptr;
    }
    if (_labelMap) {
        heap_caps_free(_labelMap);
        _labelMap = nullptr;
    }
    _initialized = false;
}

// Helper statico per calcolare la mappa dei bordi (Gradient Magnitude)
// Questo evidenzia solo i contorni e ignora le aree piatte/uniformi
static void computeEdgeImage(const uint8_t* src, uint8_t* dst, uint16_t width, uint16_t height, int srcFullWidth, int offsetX, int offsetY, int step) {
    // Pulisci il buffer (bordi a 0)
    memset(dst, 0, width * height);
    
    const int threshold = 50; // SOGLIA RUMORE: Aumentata per evitare falsi positivi da rumore sensore
    
    // Calcola gradiente per ogni pixel (esclusi i bordi estremi)
    for (uint16_t y = 0; y < height - 1; y++) {
        int srcY = offsetY + y * step;
        const uint8_t* rowPtr = src + srcY * srcFullWidth + offsetX;
        
        const uint8_t* nextRowPtr = src + (srcY + step) * srcFullWidth + offsetX;
        
        uint8_t* dstPtr = dst + y * width;
        
        for (uint16_t x = 0; x < width - 1; x++) {
            int srcX = x * step;
            
            // Gradiente semplice (Manhattan): |dx| + |dy|
            int dx = abs((int)rowPtr[srcX] - (int)rowPtr[srcX + step]);
            int dy = abs((int)rowPtr[srcX] - (int)nextRowPtr[srcX]);
            int mag = dx + dy;
            
            // Applica soglia e amplifica i bordi reali
            if (mag < threshold) {
                dstPtr[x] = 0;
            } else {
                // Saturazione a 255
                dstPtr[x] = (mag > 255) ? 255 : (uint8_t)mag;
            }
        }
    }
}

bool OpticalFlowDetector::begin(uint16_t frameWidth, uint16_t frameHeight) {
    if (_initialized) {
        Serial.println("[OPTICAL FLOW] Already initialized");
        return true;
    }

    _frameWidth = frameWidth;
    _frameHeight = frameHeight;
    _frameSize = frameWidth * frameHeight;

    Serial.printf("[OPTICAL FLOW] Initializing for %ux%u frames (%u bytes)\n",
                  _frameWidth, _frameHeight, _frameSize);
    Serial.printf("[OPTICAL FLOW] Grid: %dx%d blocks (%d total)\n",
                  GRID_COLS, GRID_ROWS, TOTAL_BLOCKS);
    Serial.printf("[OPTICAL FLOW] Block size: %dx%d pixels\n", BLOCK_SIZE, BLOCK_SIZE);

    // Alloca buffer in PSRAM per frame precedente e mappa bordi
    _previousFrame = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);
    _edgeFrame = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);
    _binaryEdgeFrame = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);
    _labelMap = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);

    if (!_previousFrame || !_edgeFrame || !_binaryEdgeFrame || !_labelMap) {
        Serial.println("[OPTICAL FLOW ERROR] Failed to allocate frame buffers!");
        if (_previousFrame) {
            heap_caps_free(_previousFrame);
            _previousFrame = nullptr;
        }
        if (_edgeFrame) {
            heap_caps_free(_edgeFrame);
            _edgeFrame = nullptr;
        }
        if (_binaryEdgeFrame) {
            heap_caps_free(_binaryEdgeFrame);
            _binaryEdgeFrame = nullptr;
        }
        if (_labelMap) {
            heap_caps_free(_labelMap);
            _labelMap = nullptr;
        }
        return false;
    }

    memset(_previousFrame, 0, _frameSize);
    memset(_edgeFrame, 0, _frameSize);
    memset(_binaryEdgeFrame, 0, _frameSize);
    memset(_labelMap, 0, _frameSize);
    _initialized = true;
    _hasPreviousFrame = false;

    // Applica qualità default (o quella impostata via BLE prima della init)
    setQuality(_quality);

    Serial.println("[OPTICAL FLOW] Initialized successfully!");
    Serial.printf("[OPTICAL FLOW] Search range: ±%d px, step: %d px\n",
                  _searchRange, _searchStep);
    Serial.printf("[OPTICAL FLOW] Min confidence: %d, min active blocks: %d\n",
                  _minConfidence, _minActiveBlocks);

    return true;
}

bool OpticalFlowDetector::processFrame(const uint8_t* frameBuffer, size_t frameLength) {
    if (!_initialized) {
        Serial.println("[OPTICAL FLOW ERROR] Not initialized!");
        return false;
    }

    bool isQVGA = (frameLength == 320 * 240);
    if (frameLength != _frameSize && !isQVGA) {
        Serial.printf("[OPTICAL FLOW ERROR] Frame size mismatch! Expected %u (or QVGA input), got %u\n",
                      _frameSize, frameLength);
        return false;
    }

    unsigned long startTime = millis();
    _totalFramesProcessed++;

    // 1. Usa buffer bordi riutilizzabile
    uint8_t* edgeFrame = _edgeFrame;
    if (!edgeFrame) {
        Serial.println("[OPTICAL FLOW ERROR] Edge buffer not allocated!");
        return false;
    }

    // Configurazione Crop/Scale
    int srcFullWidth = _frameWidth;
    int offsetX = 0;
    int offsetY = 0;
    int step = 1;

    if (isQVGA) {
        srcFullWidth = 320;
        if (_frameWidth == 240 && _frameHeight == 240) {
            // CROP 240x240 CENTRALE da QVGA (320x240)
            // Offset X = (320 - 240) / 2 = 40
            offsetX = 40;
            offsetY = 0;
            step = 1;  // Nessun subsample, già alla risoluzione corretta
        }
    }

    // 2. Calcola i bordi dal frame grezzo
    computeEdgeImage(frameBuffer, edgeFrame, _frameWidth, _frameHeight, srcFullWidth, offsetX, offsetY, step);

    // Primo frame: inizializza previous e non rilevare motion
    if (!_hasPreviousFrame) {
        memcpy(_previousFrame, edgeFrame, _frameSize); // Salva i bordi, non il raw
        _hasPreviousFrame = true;
        _motionActive = false;
        _motionIntensity = 0;
        _motionDirection = Direction::NONE;
        _motionSpeed = 0.0f;
        _motionConfidence = 0.0f;
        _activeBlocks = 0;
        _frameDiffAvg = 0;
        return false;
    }

    // Calcola luminosità media per auto flash
    // NOTA: Usiamo frameBuffer (raw) per la luminosità, è corretto
    _avgBrightness = _calculateAverageBrightness(frameBuffer);
    _updateFlashIntensity();

    // Frame diff (fallback per occlusioni / scene change)
    // NOTA: Usiamo edgeFrame per confrontare bordi correnti vs bordi precedenti
    _frameDiffAvg = _calculateFrameDiffAvg(edgeFrame);

    // BLOB TRACKING PIPELINE
    // 1. Binarizza edge frame
    _binarizeEdges(edgeFrame);

    // 2. Trova blob con connected component labeling
    _detectBlobs();

    // 3. Match blob con frame precedente
    _matchBlobs();

    // 4. Calcola movimento globale dai blob
    _calculateGlobalMotionFromBlobs();

    // Calcola centroide se c'è movimento
    if (_motionActive) {
        _calculateCentroid();
        _updateTrajectory();
        _motionFrameCount++;
        _lastMotionTime = millis();
    } else {
        // Reset traiettoria se fermo per troppo tempo
        if (_trajectoryLength > 0 && (millis() - _lastMotionTime) > 1000) {
            _trajectoryLength = 0;
            _centroidValid = false;
        }
    }

    // Copia frame corrente in previous
    // Salviamo i bordi per il prossimo confronto
    memcpy(_previousFrame, edgeFrame, _frameSize);

    // Aggiorna metriche timing
    unsigned long computeTime = millis() - startTime;
    _totalComputeTime += computeTime;

    return _motionActive;
}

void OpticalFlowDetector::_computeOpticalFlow(const uint8_t* currentFrame) {
    // Calcola motion vector per ogni blocco
    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        for (uint8_t col = 0; col < GRID_COLS; col++) {
            _calculateBlockMotion(row, col, currentFrame);
        }
    }
}

void OpticalFlowDetector::_calculateBlockMotion(uint8_t row, uint8_t col, const uint8_t* currentFrame) {
    uint16_t blockX = col * BLOCK_SIZE;
    uint16_t blockY = row * BLOCK_SIZE;

    int8_t bestDx = 0, bestDy = 0;
    uint16_t minSAD = UINT16_MAX;

    // Search window: ±searchRange px in step di searchStep px
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

            // Compute SAD con early termination
            uint16_t sad = _computeSAD(
                _previousFrame, currentFrame,
                blockX, blockY,
                searchX, searchY,
                BLOCK_SIZE,
                minSAD  // Passa best SAD per early exit
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

uint16_t OpticalFlowDetector::_computeSAD(
    const uint8_t* frame1,
    const uint8_t* frame2,
    uint16_t x1, uint16_t y1,
    uint16_t x2, uint16_t y2,
    uint8_t blockSize,
    uint16_t currentMinSAD
) {
    uint32_t sad = 0;

    for (uint8_t by = 0; by < blockSize; by++) {
        for (uint8_t bx = 0; bx < blockSize; bx++) {
            uint16_t idx1 = (y1 + by) * _frameWidth + (x1 + bx);
            uint16_t idx2 = (y2 + by) * _frameWidth + (x2 + bx);

            int16_t diff = (int16_t)frame1[idx1] - (int16_t)frame2[idx2];
            sad += abs(diff);

            // Early termination: interrompi se già peggiore del best
            if (sad >= currentMinSAD) {
                return currentMinSAD;  // Ritorna subito, tanto è già peggiore
            }
        }
    }

    return (sad > UINT16_MAX) ? UINT16_MAX : (uint16_t)sad;
}

void OpticalFlowDetector::_filterOutliers() {
    // Median filter 3x3 sui vettori
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

            // Threshold: 8px di deviazione
            if (diffDx > 8 || diffDy > 8) {
                center.valid = false;  // Mark come outlier
            }
        }
    }
}

void OpticalFlowDetector::_calculateGlobalMotion() {
    // Aggrega vettori validi
    float sumDx = 0.0f;
    float sumDy = 0.0f;
    uint32_t sumConfidence = 0;
    uint8_t validBlocks = 0;

    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        for (uint8_t col = 0; col < GRID_COLS; col++) {
            BlockMotionVector& vec = _motionVectors[row][col];
            if (vec.valid) {
                sumDx += vec.dx * vec.confidence;
                sumDy += vec.dy * vec.confidence;
                sumConfidence += vec.confidence;
                validBlocks++;
            }
        }
    }

    _activeBlocks = validBlocks;

    // Verifica movimento
    if (validBlocks < _minActiveBlocks || sumConfidence == 0) {
        _motionActive = false;
        _motionIntensity = 0;
        _motionDirection = Direction::NONE;
        _motionSpeed = 0.0f;
        _motionConfidence = 0.0f;
        return;
    }

    // Calcola media pesata
    float avgDx = sumDx / sumConfidence;
    float avgDy = sumDy / sumConfidence;

    // Calcola velocità (magnitude)
    _motionSpeed = sqrtf(avgDx * avgDx + avgDy * avgDy);

    // Calcola direzione
    _motionDirection = _vectorToDirection(avgDx, avgDy);

    // Calcola confidence (0.0-1.0)
    _motionConfidence = (float)sumConfidence / (float)(validBlocks * 255);

    // Calcola intensità (0-255)
    // Basata su velocità e confidence
    float normalizedSpeed = min(_motionSpeed / 20.0f, 1.0f);  // 20px/frame = max
    _motionIntensity = (uint8_t)(normalizedSpeed * _motionConfidence * 255.0f);

    // Soglie globali (stabili, poco rumore)
    // Aumentata soglia intensità da 8 a 15 per tagliare il rumore di fondo
    _motionActive = (_motionIntensity > _motionIntensityThreshold &&
                     _motionSpeed > _motionSpeedThreshold);
}

void OpticalFlowDetector::_calculateCentroid() {
    float weightedX = 0.0f;
    float weightedY = 0.0f;
    float totalWeight = 0.0f;

    // Calcola centroide pesato per confidence
    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        for (uint8_t col = 0; col < GRID_COLS; col++) {
            BlockMotionVector& vec = _motionVectors[row][col];
            if (vec.valid) {
                float blockCenterX = (col * BLOCK_SIZE) + (BLOCK_SIZE / 2.0f);
                float blockCenterY = (row * BLOCK_SIZE) + (BLOCK_SIZE / 2.0f);
                float weight = vec.confidence;

                weightedX += blockCenterX * weight;
                weightedY += blockCenterY * weight;
                totalWeight += weight;
            }
        }
    }

    if (totalWeight < _minCentroidWeight) {
        _centroidValid = false;
        return;
    }

    // Calcola posizione centroide
    float centroidX = weightedX / totalWeight;
    float centroidY = weightedY / totalWeight;

    // Smooth del centroide per ridurre jitter
    const float ALPHA = 0.7f;  // Smoothing factor
    if (_centroidValid) {
        _centroidX = _centroidX * (1.0f - ALPHA) + centroidX * ALPHA;
        _centroidY = _centroidY * (1.0f - ALPHA) + centroidY * ALPHA;
    } else {
        _centroidX = centroidX;
        _centroidY = centroidY;
    }

    _centroidValid = true;
}

void OpticalFlowDetector::_updateTrajectory() {
    if (!_centroidValid) {
        return;
    }

    unsigned long now = millis();

    // Normalizza coordinate (0.0 - 1.0)
    float normX = _centroidX / (float)_frameWidth;
    float normY = _centroidY / (float)_frameHeight;

    // Se traiettoria vuota, aggiungi primo punto
    if (_trajectoryLength == 0) {
        _trajectory[0].x = normX;
        _trajectory[0].y = normY;
        _trajectory[0].timestamp = now;
        _trajectory[0].intensity = _motionIntensity;
        _trajectory[0].speed = _motionSpeed;
        _trajectory[0].direction = _motionDirection;
        _trajectoryLength = 1;
        return;
    }

    // Verifica distanza dall'ultimo punto
    TrajectoryPoint& last = _trajectory[_trajectoryLength - 1];
    float dx = normX - last.x;
    float dy = normY - last.y;
    float distance = sqrtf(dx * dx + dy * dy);

    // Aggiungi punto solo se movimento significativo
    const float MIN_DISTANCE = 0.03f;  // 3% del frame
    if (distance < MIN_DISTANCE) {
        // Aggiorna timestamp e intensità dell'ultimo punto
        last.timestamp = now;
        last.intensity = max(last.intensity, _motionIntensity);
        last.speed = _motionSpeed;
        last.direction = _motionDirection;
        return;
    }

    // Aggiungi nuovo punto se c'è spazio
    if (_trajectoryLength < MAX_TRAJECTORY_POINTS) {
        _trajectory[_trajectoryLength].x = normX;
        _trajectory[_trajectoryLength].y = normY;
        _trajectory[_trajectoryLength].timestamp = now;
        _trajectory[_trajectoryLength].intensity = _motionIntensity;
        _trajectory[_trajectoryLength].speed = _motionSpeed;
        _trajectory[_trajectoryLength].direction = _motionDirection;
        _trajectoryLength++;
    } else {
        // Buffer pieno: shifta a sinistra e aggiungi in coda
        for (uint8_t i = 0; i < MAX_TRAJECTORY_POINTS - 1; i++) {
            _trajectory[i] = _trajectory[i + 1];
        }
        _trajectory[MAX_TRAJECTORY_POINTS - 1].x = normX;
        _trajectory[MAX_TRAJECTORY_POINTS - 1].y = normY;
        _trajectory[MAX_TRAJECTORY_POINTS - 1].timestamp = now;
        _trajectory[MAX_TRAJECTORY_POINTS - 1].intensity = _motionIntensity;
        _trajectory[MAX_TRAJECTORY_POINTS - 1].speed = _motionSpeed;
        _trajectory[MAX_TRAJECTORY_POINTS - 1].direction = _motionDirection;
    }
}

uint8_t OpticalFlowDetector::getTrajectory(TrajectoryPoint* outPoints) const {
    if (!outPoints || _trajectoryLength == 0) {
        return 0;
    }

    memcpy(outPoints, _trajectory, sizeof(TrajectoryPoint) * _trajectoryLength);
    return _trajectoryLength;
}

uint8_t OpticalFlowDetector::_calculateAverageBrightness(const uint8_t* frame) {
    if (!frame || _frameSize == 0) {
        return 0;
    }

    // Sample 1 pixel ogni 4 per ridurre calcoli (76800 → 19200 ops, -75%)
    uint64_t total = 0;
    uint32_t sampleCount = 0;
    for (size_t i = 0; i < _frameSize; i += 4) {
        total += frame[i];
        sampleCount++;
    }

    return (uint8_t)(total / sampleCount);
}

void OpticalFlowDetector::_updateFlashIntensity() {
    // Logica auto flash con smoothing graduale (identica a MotionDetector)
    uint8_t targetIntensity;

    if (_avgBrightness < 30) {
        targetIntensity = 255;
    } else if (_avgBrightness < 100) {
        targetIntensity = map(_avgBrightness, 30, 100, 255, 150);
    } else if (_avgBrightness < 180) {
        targetIntensity = map(_avgBrightness, 100, 180, 150, 80);
    } else {
        targetIntensity = 80;
    }

    // Smoothing
    if (_flashIntensity < targetIntensity) {
        uint16_t newValue = (uint16_t)_flashIntensity + 5;
        _flashIntensity = (newValue > targetIntensity) ? targetIntensity : newValue;
    } else if (_flashIntensity > targetIntensity) {
        int16_t newValue = (int16_t)_flashIntensity - 5;
        _flashIntensity = (newValue < targetIntensity) ? targetIntensity : newValue;
    }
}

void OpticalFlowDetector::setQuality(uint8_t quality) {
    _quality = quality;

    // Qualità alta -> confidence threshold più basso e meno blocchi richiesti.
    // Manteniamo soglie globali di motion stabili per non introdurre rumore.
    _minConfidence = (uint8_t)map(quality, 0, 255, 80, 30);
    _minActiveBlocks = (uint8_t)map(quality, 0, 255, 10, 4);

    Serial.printf("[OPTICAL FLOW] Quality updated: %u (minConf: %u, minBlocks: %u)\n",
                  _quality, _minConfidence, _minActiveBlocks);
}

void OpticalFlowDetector::setMotionIntensityThreshold(uint8_t threshold) {
    _motionIntensityThreshold = (uint8_t)constrain(threshold, 0, 255);
    Serial.printf("[OPTICAL FLOW] Motion intensity threshold set: %u\n", _motionIntensityThreshold);
}

void OpticalFlowDetector::setMotionSpeedThreshold(float threshold) {
    if (threshold < 0.0f) threshold = 0.0f;
    _motionSpeedThreshold = threshold;
    Serial.printf("[OPTICAL FLOW] Motion speed threshold set: %.2f\n", _motionSpeedThreshold);
}

void OpticalFlowDetector::reset() {
    _motionActive = false;
    _motionIntensity = 0;
    _motionDirection = Direction::NONE;
    _motionSpeed = 0.0f;
    _motionConfidence = 0.0f;
    _activeBlocks = 0;
    _trajectoryLength = 0;
    _centroidX = 0.0f;
    _centroidY = 0.0f;
    _centroidValid = false;
    _lastMotionTime = 0;
    _totalFramesProcessed = 0;
    _motionFrameCount = 0;
    _totalComputeTime = 0;
    _flashIntensity = 150;
    _avgBrightness = 0;
    _frameDiffAvg = 0;
    _hasPreviousFrame = false;

    memset(_motionVectors, 0, sizeof(_motionVectors));
    memset(_trajectory, 0, sizeof(_trajectory));

    if (_previousFrame) {
        memset(_previousFrame, 0, _frameSize);
    }

    Serial.println("[OPTICAL FLOW] State reset");
}

OpticalFlowDetector::Metrics OpticalFlowDetector::getMetrics() const {
    Metrics metrics;

    // Campi compatibili
    metrics.totalFramesProcessed = _totalFramesProcessed;
    metrics.motionFrameCount = _motionFrameCount;
    metrics.currentIntensity = _motionIntensity;
    metrics.avgBrightness = _avgBrightness;
    metrics.flashIntensity = _flashIntensity;
    metrics.trajectoryLength = _trajectoryLength;
    metrics.motionActive = _motionActive;

    // Nuove metriche optical flow
    metrics.avgComputeTimeMs = (_totalFramesProcessed > 0) ?
        (_totalComputeTime / _totalFramesProcessed) : 0;
    metrics.avgConfidence = _motionConfidence;
    metrics.avgActiveBlocks = _activeBlocks;
    metrics.dominantDirection = _motionDirection;
    metrics.avgSpeed = _motionSpeed;
    metrics.frameDiff = _frameDiffAvg;

    return metrics;
}

// ═══════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════

OpticalFlowDetector::Direction OpticalFlowDetector::_vectorToDirection(float dx, float dy) {
    // Magnitude threshold
    float magnitude = sqrtf(dx * dx + dy * dy);
    if (magnitude < _directionMagnitudeThreshold) {  // sotto soglia = fermo
        return Direction::NONE;
    }

    // Calcola angolo in gradi
    float angle = atan2f(dy, dx) * 180.0f / PI;

    // Normalizza 0-360
    if (angle < 0) angle += 360.0f;

    // Quantizza in 8 direzioni
    // Sistema di coordinate: Y aumenta verso il basso (standard immagini)
    // DOWN:       67.5 - 112.5  (90°)  - movimento verso il basso
    // UP:        247.5 - 292.5  (270°) - movimento verso l'alto
    // RIGHT:     337.5 -  22.5  (0°/360°) - movimento a destra
    // LEFT:      157.5 - 202.5  (180°) - movimento a sinistra
    // DOWN_RIGHT: 22.5 -  67.5  (45°)
    // DOWN_LEFT: 112.5 - 157.5  (135°)
    // UP_LEFT:   202.5 - 247.5  (225°)
    // UP_RIGHT:  292.5 - 337.5  (315°)

    if (angle >= 67.5f && angle < 112.5f) return Direction::DOWN;
    if (angle >= 247.5f && angle < 292.5f) return Direction::UP;
    if (angle >= 157.5f && angle < 202.5f) return Direction::LEFT;
    if (angle < 22.5f || angle >= 337.5f) return Direction::RIGHT;
    if (angle >= 22.5f && angle < 67.5f) return Direction::DOWN_RIGHT;
    if (angle >= 112.5f && angle < 157.5f) return Direction::DOWN_LEFT;
    if (angle >= 202.5f && angle < 247.5f) return Direction::UP_LEFT;
    if (angle >= 292.5f && angle < 337.5f) return Direction::UP_RIGHT;

    return Direction::NONE;
}

const char* OpticalFlowDetector::directionToString(Direction dir) {
    switch (dir) {
        case Direction::NONE:       return "none";
        case Direction::UP:         return "up";
        case Direction::DOWN:       return "down";
        case Direction::LEFT:       return "left";
        case Direction::RIGHT:      return "right";
        case Direction::UP_LEFT:    return "up_left";
        case Direction::UP_RIGHT:   return "up_right";
        case Direction::DOWN_LEFT:  return "down_left";
        case Direction::DOWN_RIGHT: return "down_right";
        default:                    return "unknown";
    }
}

int8_t OpticalFlowDetector::_median(int8_t* values, uint8_t count) {
    if (count == 0) return 0;

    // Simple bubble sort per piccoli array
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            if (values[j] > values[j + 1]) {
                int8_t temp = values[j];
                values[j] = values[j + 1];
                values[j + 1] = temp;
            }
        }
    }

    // Ritorna mediana
    return values[count / 2];
}

bool OpticalFlowDetector::getBlockVector(uint8_t row, uint8_t col, int8_t* outDx, int8_t* outDy, uint8_t* outConfidence) const {
    if (row >= GRID_ROWS || col >= GRID_COLS) {
        return false;
    }

    const BlockMotionVector& vec = _motionVectors[row][col];

    if (outDx) *outDx = vec.dx;
    if (outDy) *outDy = vec.dy;
    if (outConfidence) *outConfidence = vec.confidence;

    return vec.valid;
}

char OpticalFlowDetector::getBlockDirectionTag(uint8_t row, uint8_t col) const {
    if (row >= GRID_ROWS || col >= GRID_COLS) {
        return '?';
    }

    // Calcola coordinate centro blocco
    float blockCenterX = (col * BLOCK_SIZE) + (BLOCK_SIZE / 2.0f);
    float blockCenterY = (row * BLOCK_SIZE) + (BLOCK_SIZE / 2.0f);

    // Cerca blob che contiene questo blocco (usa centroide più vicino)
    int8_t bestBlobIdx = -1;
    float minDistance = BLOCK_SIZE * 1.5f;  // Max distanza per considerare blocco parte del blob

    for (uint8_t i = 0; i < _currentBlobCount; i++) {
        const Blob& blob = _currentBlobs[i];

        // Verifica se il centro del blocco è dentro il bounding box del blob (con margine)
        if (blockCenterX >= blob.minX && blockCenterX <= blob.maxX &&
            blockCenterY >= blob.minY && blockCenterY <= blob.maxY) {

            // Calcola distanza dal centroide
            float dx = blockCenterX - blob.centroidX;
            float dy = blockCenterY - blob.centroidY;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance < minDistance) {
                minDistance = distance;
                bestBlobIdx = i;
            }
        }
    }

    // Se nessun blob copre questo blocco, mostra vuoto
    if (bestBlobIdx < 0) {
        return '.';
    }

    const Blob& blob = _currentBlobs[bestBlobIdx];

    // Se blob non è stato tracciato (no movimento), mostra marker statico
    if (!blob.matched) {
        return '·';  // Blob rilevato ma fermo
    }

    // Mostra direzione del movimento del blob
    float dx = blob.dx;
    float dy = blob.dy;
    float absDx = abs(dx);
    float absDy = abs(dy);

    // Ignora movimenti molto piccoli
    if (absDx < 2.0f && absDy < 2.0f) {
        return '·';
    }

    // Determina asse predominante
    if (absDy > absDx * 2) {
        return (dy < 0) ? '^' : 'v';
    }
    if (absDx > absDy * 2) {
        return (dx < 0) ? '<' : '>';
    }

    // Diagonali
    if (dx > 0 && dy < 0) return 'A';   // Up-right
    if (dx < 0 && dy < 0) return 'B';   // Up-left
    if (dx > 0 && dy > 0) return 'C';   // Down-right
    if (dx < 0 && dy > 0) return 'D';   // Down-left

    return '*';
}

uint8_t OpticalFlowDetector::_calculateFrameDiffAvg(const uint8_t* currentFrame) const {
    if (!currentFrame || !_previousFrame || _frameSize == 0) {
        return 0;
    }

    // Sample 1 pixel ogni 16: 76800 -> 4800 ops
    uint32_t total = 0;
    uint32_t count = 0;
    for (size_t i = 0; i < _frameSize; i += 16) {
        int16_t diff = (int16_t)currentFrame[i] - (int16_t)_previousFrame[i];
        total += (uint16_t)abs(diff);
        count++;
    }

    if (count == 0) return 0;
    uint32_t avg = total / count;
    return (avg > 255) ? 255 : (uint8_t)avg;
}

// ═══════════════════════════════════════════════════════════
// BLOB DETECTION & TRACKING IMPLEMENTATION
// ═══════════════════════════════════════════════════════════

void OpticalFlowDetector::_binarizeEdges(const uint8_t* edgeFrame) {
    if (!edgeFrame || !_binaryEdgeFrame) return;

    // Threshold adattivo: usa percentile invece di valore fisso
    // per adattarsi alla luminosità della scena
    const uint8_t EDGE_THRESHOLD = 60;  // Soglia per considerare un bordo significativo

    for (uint32_t i = 0; i < _frameSize; i++) {
        _binaryEdgeFrame[i] = (edgeFrame[i] > EDGE_THRESHOLD) ? 255 : 0;
    }
}

void OpticalFlowDetector::_detectBlobs() {
    if (!_binaryEdgeFrame || !_labelMap) return;

    // Reset label map
    memset(_labelMap, 0, _frameSize);
    _currentBlobCount = 0;

    // Trova blob usando flood fill
    uint8_t currentLabel = 1;
    const uint16_t MIN_BLOB_SIZE = 40;   // Min pixel per considerare un blob (ridotto rumore)
    const uint16_t MAX_BLOB_SIZE = (_frameWidth * _frameHeight) / 3;  // Max 33% del frame

    for (uint16_t y = 0; y < _frameHeight; y++) {
        for (uint16_t x = 0; x < _frameWidth; x++) {
            uint32_t idx = y * _frameWidth + x;

            // Se è un pixel di bordo non ancora etichettato
            if (_binaryEdgeFrame[idx] == 255 && _labelMap[idx] == 0) {
                // Esegui flood fill
                uint16_t blobSize = _floodFill(x, y, currentLabel);

                // Verifica dimensione blob
                if (blobSize >= MIN_BLOB_SIZE && blobSize <= MAX_BLOB_SIZE) {
                    // Blob valido
                    currentLabel++;
                    _currentBlobCount++;

                    // Limita numero blob per performance
                    if (_currentBlobCount >= MAX_BLOBS) {
                        goto blob_detection_done;
                    }
                } else {
                    // Blob troppo piccolo o grande: rimuovi label
                    for (uint32_t i = 0; i < _frameSize; i++) {
                        if (_labelMap[i] == currentLabel) {
                            _labelMap[i] = 0;
                        }
                    }
                }
            }
        }
    }

blob_detection_done:
    // Estrai statistiche blob
    _extractBlobStats();
}

uint16_t OpticalFlowDetector::_floodFill(uint16_t startX, uint16_t startY, uint8_t label) {
    // Stack-based flood fill per evitare ricorsione
    // Usa buffer statico per stack (ottimizzato per ESP32)
    static uint16_t stackX[512];
    static uint16_t stackY[512];
    uint16_t stackSize = 0;
    uint16_t pixelCount = 0;

    // Push iniziale
    stackX[stackSize] = startX;
    stackY[stackSize] = startY;
    stackSize++;

    while (stackSize > 0) {
        // Pop
        stackSize--;
        uint16_t x = stackX[stackSize];
        uint16_t y = stackY[stackSize];

        uint32_t idx = y * _frameWidth + x;

        // Verifica bounds e condizioni
        if (x >= _frameWidth || y >= _frameHeight) continue;
        if (_binaryEdgeFrame[idx] != 255) continue;
        if (_labelMap[idx] != 0) continue;

        // Etichetta pixel
        _labelMap[idx] = label;
        pixelCount++;

        // Limita dimensione blob per evitare overflow
        if (pixelCount > 5000) {
            return pixelCount;
        }

        // Push vicini (4-connectivity)
        if (stackSize < 510) {  // Lascia spazio per 4 push
            // Destra
            if (x + 1 < _frameWidth) {
                stackX[stackSize] = x + 1;
                stackY[stackSize] = y;
                stackSize++;
            }
            // Sinistra
            if (x > 0) {
                stackX[stackSize] = x - 1;
                stackY[stackSize] = y;
                stackSize++;
            }
            // Basso
            if (y + 1 < _frameHeight) {
                stackX[stackSize] = x;
                stackY[stackSize] = y + 1;
                stackSize++;
            }
            // Alto
            if (y > 0) {
                stackX[stackSize] = x;
                stackY[stackSize] = y - 1;
                stackSize++;
            }
        }
    }

    return pixelCount;
}

void OpticalFlowDetector::_extractBlobStats() {
    // Reset blob array
    memset(_currentBlobs, 0, sizeof(_currentBlobs));

    // Calcola statistiche per ogni blob
    for (uint8_t blobIdx = 0; blobIdx < _currentBlobCount; blobIdx++) {
        uint8_t label = blobIdx + 1;
        Blob& blob = _currentBlobs[blobIdx];

        blob.id = label;
        blob.pixelCount = 0;
        blob.minX = _frameWidth;
        blob.minY = _frameHeight;
        blob.maxX = 0;
        blob.maxY = 0;
        blob.matched = false;

        uint32_t sumX = 0;
        uint32_t sumY = 0;

        // Scansiona label map
        for (uint16_t y = 0; y < _frameHeight; y++) {
            for (uint16_t x = 0; x < _frameWidth; x++) {
                uint32_t idx = y * _frameWidth + x;
                if (_labelMap[idx] == label) {
                    blob.pixelCount++;
                    sumX += x;
                    sumY += y;

                    if (x < blob.minX) blob.minX = x;
                    if (x > blob.maxX) blob.maxX = x;
                    if (y < blob.minY) blob.minY = y;
                    if (y > blob.maxY) blob.maxY = y;
                }
            }
        }

        // Calcola centroide
        if (blob.pixelCount > 0) {
            blob.centroidX = (float)sumX / blob.pixelCount;
            blob.centroidY = (float)sumY / blob.pixelCount;
        }
    }
}

void OpticalFlowDetector::_matchBlobs() {
    // Match blob correnti con blob precedenti usando nearest neighbor
    const float MAX_MATCH_DISTANCE = 80.0f;  // Max pixel tra centroidi per match

    // Reset match flags
    for (uint8_t i = 0; i < _currentBlobCount; i++) {
        _currentBlobs[i].matched = false;
        _currentBlobs[i].dx = 0.0f;
        _currentBlobs[i].dy = 0.0f;
        _currentBlobs[i].confidence = 0;
    }

    // Match ogni blob corrente con il più vicino precedente
    for (uint8_t currIdx = 0; currIdx < _currentBlobCount; currIdx++) {
        Blob& currBlob = _currentBlobs[currIdx];

        float minDistance = MAX_MATCH_DISTANCE;
        int8_t bestMatchIdx = -1;

        // Cerca miglior match
        for (uint8_t prevIdx = 0; prevIdx < _previousBlobCount; prevIdx++) {
            const Blob& prevBlob = _previousBlobs[prevIdx];

            float dx = currBlob.centroidX - prevBlob.centroidX;
            float dy = currBlob.centroidY - prevBlob.centroidY;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance < minDistance) {
                minDistance = distance;
                bestMatchIdx = prevIdx;
            }
        }

        // Se trovato match valido
        if (bestMatchIdx >= 0) {
            const Blob& prevBlob = _previousBlobs[bestMatchIdx];

            currBlob.matched = true;
            currBlob.dx = currBlob.centroidX - prevBlob.centroidX;
            currBlob.dy = currBlob.centroidY - prevBlob.centroidY;

            // Confidence basata su distanza (più vicino = più confident)
            float normalizedDist = minDistance / MAX_MATCH_DISTANCE;
            currBlob.confidence = (uint8_t)((1.0f - normalizedDist) * 255.0f);
        }
    }

    // Copia blob correnti in precedenti per prossimo frame
    memcpy(_previousBlobs, _currentBlobs, sizeof(_currentBlobs));
    _previousBlobCount = _currentBlobCount;
}

void OpticalFlowDetector::_calculateGlobalMotionFromBlobs() {
    // Calcola movimento globale dai blob tracciati
    float sumDx = 0.0f;
    float sumDy = 0.0f;
    uint32_t sumConfidence = 0;
    uint8_t matchedBlobs = 0;

    for (uint8_t i = 0; i < _currentBlobCount; i++) {
        const Blob& blob = _currentBlobs[i];
        if (blob.matched && blob.confidence > 50) {  // Solo blob ben tracciati
            sumDx += blob.dx * blob.confidence;
            sumDy += blob.dy * blob.confidence;
            sumConfidence += blob.confidence;
            matchedBlobs++;
        }
    }

    // Verifica se c'è movimento
    if (matchedBlobs == 0 || sumConfidence == 0) {
        _motionActive = false;
        _motionIntensity = 0;
        _motionDirection = Direction::NONE;
        _motionSpeed = 0.0f;
        _motionConfidence = 0.0f;
        _activeBlocks = 0;
        return;
    }

    // Calcola media pesata
    float avgDx = sumDx / sumConfidence;
    float avgDy = sumDy / sumConfidence;

    // Calcola velocità (magnitude)
    _motionSpeed = sqrtf(avgDx * avgDx + avgDy * avgDy);

    // Calcola direzione
    _motionDirection = _vectorToDirection(avgDx, avgDy);

    // Calcola confidence (0.0-1.0)
    _motionConfidence = (float)sumConfidence / (float)(matchedBlobs * 255);

    // Calcola intensità (0-255)
    float normalizedSpeed = min(_motionSpeed / 30.0f, 1.0f);  // 30px/frame = max
    _motionIntensity = (uint8_t)(normalizedSpeed * _motionConfidence * 255.0f);

    // Aggiorna activeBlocks per compatibilità
    _activeBlocks = matchedBlobs;

    // Soglie globali
    _motionActive = (_motionIntensity > _motionIntensityThreshold &&
                     _motionSpeed > _motionSpeedThreshold &&
                     matchedBlobs >= 1);  // Almeno 1 blob tracciato
}
