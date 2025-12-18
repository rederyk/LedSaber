#include "OpticalFlowDetector.h"
#include <esp_heap_caps.h>
#include <math.h>

OpticalFlowDetector::OpticalFlowDetector()
    : _initialized(false)
    , _frameWidth(0)
    , _frameHeight(0)
    , _frameSize(0)
    , _previousFrame(nullptr)
    , _searchRange(10)
    , _searchStep(2)
    , _minConfidence(50)
    , _minActiveBlocks(6)
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
    , _lastMotionTime(0)
    , _totalFramesProcessed(0)
    , _motionFrameCount(0)
    , _totalComputeTime(0)
{
    memset(_motionVectors, 0, sizeof(_motionVectors));
    memset(_trajectory, 0, sizeof(_trajectory));
}

OpticalFlowDetector::~OpticalFlowDetector() {
    if (_previousFrame) {
        heap_caps_free(_previousFrame);
        _previousFrame = nullptr;
    }
    _initialized = false;
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

    // Alloca buffer in PSRAM per frame precedente
    _previousFrame = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);
    if (!_previousFrame) {
        Serial.println("[OPTICAL FLOW ERROR] Failed to allocate frame buffer!");
        return false;
    }

    memset(_previousFrame, 0, _frameSize);
    _initialized = true;

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

    if (frameLength != _frameSize) {
        Serial.printf("[OPTICAL FLOW ERROR] Frame size mismatch! Expected %u, got %u\n",
                      _frameSize, frameLength);
        return false;
    }

    unsigned long startTime = millis();
    _totalFramesProcessed++;

    // Calcola luminosità media per auto flash
    _avgBrightness = _calculateAverageBrightness(frameBuffer);
    _updateFlashIntensity();

    // Calcola optical flow
    _computeOpticalFlow(frameBuffer);

    // Filtra outliers
    _filterOutliers();

    // Calcola movimento globale
    _calculateGlobalMotion();

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
    memcpy(_previousFrame, frameBuffer, _frameSize);

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

            // Compute SAD
            uint16_t sad = _computeSAD(
                _previousFrame, currentFrame,
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

uint16_t OpticalFlowDetector::_computeSAD(
    const uint8_t* frame1,
    const uint8_t* frame2,
    uint16_t x1, uint16_t y1,
    uint16_t x2, uint16_t y2,
    uint8_t blockSize
) {
    uint32_t sad = 0;

    for (uint8_t by = 0; by < blockSize; by++) {
        for (uint8_t bx = 0; bx < blockSize; bx++) {
            uint16_t idx1 = (y1 + by) * _frameWidth + (x1 + bx);
            uint16_t idx2 = (y2 + by) * _frameWidth + (x2 + bx);

            int16_t diff = (int16_t)frame1[idx1] - (int16_t)frame2[idx2];
            sad += abs(diff);
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

    // Verifica soglia minima
    _motionActive = (_motionIntensity > 25 && _motionSpeed > 2.0f);
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

    if (totalWeight < 100.0f) {
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

    uint64_t total = 0;
    for (size_t i = 0; i < _frameSize; i++) {
        total += frame[i];
    }

    return (uint8_t)(total / _frameSize);
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

void OpticalFlowDetector::setSensitivity(uint8_t sensitivity) {
    // Sensitivity alta -> confidence threshold basso e meno blocchi richiesti
    _minConfidence = map(sensitivity, 0, 255, 80, 30);
    _minActiveBlocks = map(sensitivity, 0, 255, 10, 4);

    Serial.printf("[OPTICAL FLOW] Sensitivity updated: %u (minConf: %u, minBlocks: %u)\n",
                  sensitivity, _minConfidence, _minActiveBlocks);
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

    return metrics;
}

// ═══════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════

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

    const BlockMotionVector& vec = _motionVectors[row][col];

    if (!vec.valid) {
        return '.';
    }

    int8_t dx = vec.dx;
    int8_t dy = vec.dy;
    int absDx = abs(dx);
    int absDy = abs(dy);

    // Ignore extremely small movements
    if (absDx <= 1 && absDy <= 1) {
        return '.';
    }

    // Determine predominant axis
    if (absDy > absDx * 2) {
        return (dy < 0) ? '^' : 'v';
    }
    if (absDx > absDy * 2) {
        return (dx < 0) ? '<' : '>';
    }

    // Diagonals
    if (dx > 0 && dy < 0) return 'A';   // Up-right
    if (dx < 0 && dy < 0) return 'B';   // Up-left
    if (dx > 0 && dy > 0) return 'C';   // Down-right
    if (dx < 0 && dy > 0) return 'D';   // Down-left

    return '*';
}
