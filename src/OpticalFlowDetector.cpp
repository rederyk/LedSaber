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
    , _searchRange(6)        // Ridotto a 6px per stabilizzare FPS (worst-case minore)
    , _searchStep(3)         // Step 3px (bilanciamento precisione/velocità)
    , _algorithm(Algorithm::OPTICAL_FLOW_SAD) // Default standard
    , _minConfidence(25)     // Ridotto per blocchi più piccoli (meno pixel = SAD più basso)
    , _minActiveBlocks(6)    // Aumentato a 6 per griglia 8x8 (più blocchi disponibili)
    , _quality(160)      // Default: bilanciato (meno rumore)
    , _directionMagnitudeThreshold(2.0f)  // Ridotto a 2.0 per maggiore sensibilità
    , _minCentroidWeight(100.0f)
    , _motionIntensityThreshold(6)        // Ridotto a 6 per rilevare movimenti fluidi (logs: ~7-10)
    , _motionSpeedThreshold(0.4f)         // Ridotto a 0.4 per rilevare inizio movimento (logs: ~0.7)
    , _hasPreviousFrame(false)
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
    , _flashIntensity(200)  // Flash default attivo (era 0)
    , _avgBrightness(0)
    , _smoothedBrightness(0)
    , _brightnessFilterInitialized(false)
    , _frameDiffAvg(0)
    , _lastMotionTime(0)
    , _totalFramesProcessed(0)
    , _motionFrameCount(0)
    , _totalComputeTime(0)
    , _consecutiveMotionFrames(0)
    , _consecutiveStillFrames(0)
    , _lastFrameTimestamp(0)
    , _currentFrameDt(100)
{
    memset(_motionVectors, 0, sizeof(_motionVectors));
    memset(_trajectory, 0, sizeof(_trajectory));
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
    _initialized = false;
}

// Helper statico per calcolare la mappa dei bordi (Gradient Magnitude)
// Questo evidenzia solo i contorni e ignora le aree piatte/uniformi
static void computeEdgeImage(const uint8_t* src, uint8_t* dst, uint16_t width, uint16_t height, int srcFullWidth, int offsetX, int offsetY, int step) {
    // Pulisci il buffer (bordi a 0)
    memset(dst, 0, width * height);
    
    const int threshold = 80; // SOGLIA RUMORE: Aumentata significativamente per filtrare rumore sensore
    
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
    if (!_previousFrame || !_edgeFrame) {
        Serial.println("[OPTICAL FLOW ERROR] Failed to allocate frame buffers!");
        if (_previousFrame) {
            heap_caps_free(_previousFrame);
            _previousFrame = nullptr;
        }
        if (_edgeFrame) {
            heap_caps_free(_edgeFrame);
            _edgeFrame = nullptr;
        }
        return false;
    }

    memset(_previousFrame, 0, _frameSize);
    memset(_edgeFrame, 0, _frameSize);
    _initialized = true;
    _hasPreviousFrame = false;

    // Applica qualità default (o quella impostata via BLE prima della init)
    setQuality(_quality);

    Serial.println("[OPTICAL FLOW] Initialized successfully!");
    Serial.printf("[OPTICAL FLOW] Search range: ±%d px, step: %d px\n",
                  _searchRange, _searchStep);
    Serial.printf("[OPTICAL FLOW] Min confidence: %d, min active blocks: %d\n",
                  _minConfidence, _minActiveBlocks);

    _lastFrameTimestamp = millis();
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

    // Calcola Delta Time (dt) per normalizzare la velocità
    unsigned long now = millis();
    _currentFrameDt = now - _lastFrameTimestamp;
    _lastFrameTimestamp = now;
    // Clamp per evitare divisioni per zero o valori assurdi al primo frame
    if (_currentFrameDt == 0) _currentFrameDt = 1;
    if (_currentFrameDt > 200) _currentFrameDt = 200; // Max 200ms (min 5 FPS)

    // --- ALGORITMO LITE: CENTROID TRACKING ---
    if (_algorithm == Algorithm::CENTROID_TRACKING) {
        // In questa modalità usiamo direttamente il frame raw per la differenza
        // Non calcoliamo i bordi (risparmio CPU)
        
        if (!_hasPreviousFrame) {
            memcpy(_previousFrame, frameBuffer, _frameSize);
            _hasPreviousFrame = true;
            return false;
        }

        // Calcola luminosità media (utile per flash)
        _avgBrightness = _calculateAverageBrightness(frameBuffer);
        _updateFlashIntensity();

        // Calcola movimento tramite centroide
        _computeCentroidMotion(frameBuffer);

        // Filtra e aggiorna stato globale (simile a optical flow ma semplificato)
        // Nota: _computeCentroidMotion ha già popolato _motionVectors con un vettore globale
        _calculateGlobalMotion(); 

        // Aggiorna traiettoria
        if (_motionActive) {
            _calculateCentroid(); // Usa i vettori popolati uniformemente
            _updateTrajectory();
            _motionFrameCount++;
            _lastMotionTime = millis();
        }

        // Salva frame corrente per il prossimo ciclo
        memcpy(_previousFrame, frameBuffer, _frameSize);
        
        _totalComputeTime += (millis() - startTime);
        return _motionActive;
    }
    // -----------------------------------------

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
    
    // Calcola optical flow
    // NOTA: Usiamo edgeFrame. _computeOpticalFlow confronterà edgeFrame con _previousFrame (che contiene i bordi precedenti)
    _computeOpticalFlow(edgeFrame);

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
    // Salviamo i bordi per il prossimo confronto
    memcpy(_previousFrame, edgeFrame, _frameSize);

    // Aggiorna metriche timing
    unsigned long computeTime = millis() - startTime;
    _totalComputeTime += computeTime;

    return _motionActive;
}

void OpticalFlowDetector::_computeCentroidMotion(const uint8_t* currentFrame) {
    long sumX = 0;
    long sumY = 0;
    long totalMass = 0;
    
    // Campionamento sparso per velocità (1 pixel ogni 4x4 = 16 pixel)
    // Sufficiente per rilevare oggetti vicini/grandi
    const int step = 4; 
    const int threshold = 25; // Soglia minima differenza pixel

    for (int y = 0; y < _frameHeight; y += step) {
        for (int x = 0; x < _frameWidth; x += step) {
            int idx = y * _frameWidth + x;
            int diff = abs((int)currentFrame[idx] - (int)_previousFrame[idx]);

            if (diff > threshold) {
                sumX += x * diff;
                sumY += y * diff;
                totalMass += diff;
            }
        }
    }

    // Reset griglia vettori
    memset(_motionVectors, 0, sizeof(_motionVectors));
    _frameDiffAvg = (uint8_t)min((long)255, totalMass / (long)(_frameWidth * _frameHeight / (step*step)));

    // Se c'è abbastanza "massa" di movimento
    if (totalMass > 5000) {
        float currentCx = (float)sumX / totalMass;
        float currentCy = (float)sumY / totalMass;

        if (_centroidValid) {
            // Calcola delta rispetto al centroide precedente
            float dx = currentCx - _centroidX;
            float dy = currentCy - _centroidY;

            // Amplifica un po' il segnale
            dx *= 2.0f;
            dy *= 2.0f;

            // Popola la griglia con questo vettore globale (movimento uniforme)
            // Questo permette al resto del sistema (gesture, effetti) di funzionare senza modifiche
            for (uint8_t row = 0; row < GRID_ROWS; row++) {
                for (uint8_t col = 0; col < GRID_COLS; col++) {
                    _motionVectors[row][col].dx = (int8_t)constrain(dx, -127, 127);
                    _motionVectors[row][col].dy = (int8_t)constrain(dy, -127, 127);
                    _motionVectors[row][col].confidence = 200; // Alta fiducia sintetica
                    _motionVectors[row][col].valid = true;
                }
            }
        }

        // Aggiorna centroide precedente per il prossimo frame
        _centroidX = currentCx;
        _centroidY = currentCy;
        _centroidValid = true;
    } else {
        _centroidValid = false; // Movimento perso o fermo
    }
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

    // 1. Calcola PRIMA il costo di stare fermi (0,0)
    // Questo elimina il bias verso sinistra/alto nelle zone uniformi
    int8_t bestDx = 0, bestDy = 0;
    uint16_t minSAD = _computeSAD(
        _previousFrame, currentFrame,
        blockX, blockY,
        blockX, blockY,
        BLOCK_SIZE,
        UINT16_MAX
    );

    // PER-BLOCK NOISE GATE: Ignora blocchi che non sono cambiati significativamente
    if (minSAD < BLOCK_NOISE_THRESHOLD) {
        BlockMotionVector& vec = _motionVectors[row][col];
        vec.dx = 0;
        vec.dy = 0;
        vec.sad = minSAD;
        vec.confidence = 0;
        vec.valid = false; // Ignora blocco statico
        return;
    }

    // Search window: ±searchRange px in step di searchStep px
    for (int8_t dy = -_searchRange; dy <= _searchRange; dy += _searchStep) {
        for (int8_t dx = -_searchRange; dx <= _searchRange; dx += _searchStep) {
            // Salta (0,0) perché già calcolato
            if (dx == 0 && dy == 0) continue;

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

            // Aggiorna SOLO se troviamo un match STRETTAMENTE migliore
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

    // OTTIMIZZAZIONE: Campionamento ogni 2 pixel (step 2)
    // Riduce il carico CPU del 75% mantenendo precisione sufficiente per motion detection
    for (uint8_t by = 0; by < blockSize; by += 2) {
        for (uint8_t bx = 0; bx < blockSize; bx += 2) {
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

    // Contatori per rilevare pattern uniformi (bias sistematico)
    uint8_t downRightBlocks = 0;  // Blocchi con vettore DOWN-RIGHT (tipico rolling shutter)
    uint8_t totalNonZeroBlocks = 0;

    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        for (uint8_t col = 0; col < GRID_COLS; col++) {
            BlockMotionVector& vec = _motionVectors[row][col];
            if (vec.valid) {
                sumDx += vec.dx * vec.confidence;
                sumDy += vec.dy * vec.confidence;
                sumConfidence += vec.confidence;
                validBlocks++;

                // Conta blocchi con movimento significativo
                if (abs(vec.dx) > 1 || abs(vec.dy) > 1) {
                    totalNonZeroBlocks++;
                    // Rileva pattern DOWN-RIGHT (dx>0, dy>0)
                    if (vec.dx > 0 && vec.dy > 0) {
                        downRightBlocks++;
                    }
                }
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
    float rawSpeed = sqrtf(avgDx * avgDx + avgDy * avgDy);

    // NORMALIZZAZIONE VELOCITÀ: Riferimento 10 FPS (100ms)
    // Se dt=100ms -> factor=1.0. Se dt=50ms (20fps) -> factor=2.0.
    _motionSpeed = rawSpeed * (100.0f / (float)_currentFrameDt);

    // Calcola direzione
    _motionDirection = _vectorToDirection(avgDx, avgDy);

    // Calcola confidence (0.0-1.0)
    _motionConfidence = (float)sumConfidence / (float)(validBlocks * 255);

    // Calcola intensità (0-255)
    // Basata su velocità e confidence
    float normalizedSpeed = min(_motionSpeed / 20.0f, 1.0f);  // 20px/frame = max
    _motionIntensity = (uint8_t)(normalizedSpeed * _motionConfidence * 255.0f);

    // Soglie globali con filtro temporale per stabilità
    bool rawMotionDetected = (_motionIntensity > _motionIntensityThreshold &&
                              _motionSpeed > _motionSpeedThreshold);

    // Default: usa rilevamento immediato se il filtro temporale è disattivato
    _motionActive = rawMotionDetected;

  //  // Filtro temporale: richiedi 2 frame consecutivi per attivare motion
  //  // e 2 frame consecutivi di quiete per disattivarlo
  //  if (rawMotionDetected) {
  //      _consecutiveMotionFrames++;
  //      _consecutiveStillFrames = 0;
//
  //      // Attiva motion solo dopo 2 frame consecutivi
  //      if (_consecutiveMotionFrames >= 2) {
  //          _motionActive = true;
  //      }
  //  } else {
  //      _consecutiveStillFrames++;
  //      _consecutiveMotionFrames = 0;
//
  //      // Disattiva motion solo dopo 2 frame consecutivi di quiete
  //      if (_consecutiveStillFrames >= 2) {
  //          _motionActive = false;
  //      }
  //  }
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

    // Sample 1 pixel ogni 16 per ridurre calcoli (76800 → 4800 ops, -93%)
    uint64_t total = 0;
    uint32_t sampleCount = 0;
    for (size_t i = 0; i < _frameSize; i += 16) {
        total += frame[i];
        sampleCount++;
    }

    return (uint8_t)(total / sampleCount);
}

void OpticalFlowDetector::_updateFlashIntensity() {
    // Filtra la luminosita per evitare flicker del flash con piccoli cambi.
    if (!_brightnessFilterInitialized) {
        _smoothedBrightness = _avgBrightness;
        _brightnessFilterInitialized = true;
    } else {
        _smoothedBrightness = (uint8_t)((_smoothedBrightness * 3 + _avgBrightness) / 4);
    }

    // Logica con isteresi: evita continui ON/OFF quando la mano si avvicina.
    const uint8_t lowOn = 60;
    const uint8_t lowOff = 85;
    const uint8_t highOn = 150;
    const uint8_t highOff = 120;

    if (_flashIntensity >= 150) {
        // Modalita flash alto, scendi solo quando c'e abbastanza luce.
        if (_smoothedBrightness > lowOff) {
            _flashIntensity = 100;
        }
    } else if (_flashIntensity == 0) {
        // Flash spento, riaccendi solo se diventa buio.
        if (_smoothedBrightness < highOff) {
            _flashIntensity = 100;
        }
    } else {
        // Modalita media: sali o scendi con soglie separate.
        if (_smoothedBrightness < lowOn) {
            _flashIntensity = 200;
        } else if (_smoothedBrightness > highOn) {
            _flashIntensity = 0;
        }
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
    _smoothedBrightness = 0;
    _brightnessFilterInitialized = false;
    _frameDiffAvg = 0;
    _hasPreviousFrame = false;
    _consecutiveMotionFrames = 0;
    _consecutiveStillFrames = 0;

    memset(_motionVectors, 0, sizeof(_motionVectors));
    memset(_trajectory, 0, sizeof(_trajectory));

    if (_previousFrame) {
        memset(_previousFrame, 0, _frameSize);
    }

    Serial.println("[OPTICAL FLOW] State reset");
}

void OpticalFlowDetector::end() {
    if (_previousFrame) {
        heap_caps_free(_previousFrame);
        _previousFrame = nullptr;
    }
    if (_edgeFrame) {
        heap_caps_free(_edgeFrame);
        _edgeFrame = nullptr;
    }
    _initialized = false;
    Serial.println("[OPTICAL FLOW] De-initialized (buffers freed)");
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

    // Settori allargati a 60 gradi per le cardinali (UP/DOWN/LEFT/RIGHT)
    // per favorire le direzioni principali rispetto alle diagonali.
    if (angle >= 60.0f && angle < 120.0f) return Direction::DOWN;
    if (angle >= 240.0f && angle < 300.0f) return Direction::UP;
    if (angle >= 150.0f && angle < 210.0f) return Direction::LEFT;
    if (angle < 30.0f || angle >= 330.0f) return Direction::RIGHT;
    
    if (angle >= 30.0f && angle < 60.0f) return Direction::DOWN_RIGHT;
    if (angle >= 120.0f && angle < 150.0f) return Direction::DOWN_LEFT;
    if (angle >= 210.0f && angle < 240.0f) return Direction::UP_LEFT;
    if (angle >= 300.0f && angle < 330.0f) return Direction::UP_RIGHT;

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
