#include "MotionDetector.h"
#include <esp_heap_caps.h>
#include <math.h>

MotionDetector::MotionDetector()
    : _initialized(false)
    , _frameWidth(0)
    , _frameHeight(0)
    , _frameSize(0)
    , _previousFrame(nullptr)
    , _flashIntensity(150)  // Intensità iniziale media per avvio immediato
    , _avgBrightness(0)
    , _motionThreshold(50)
    , _motionIntensity(0)
    , _motionActive(false)
    , _motionConfidence(0)
    , _trajectoryLength(0)
    , _currentCentroidX(0.0f)
    , _currentCentroidY(0.0f)
    , _centroidValid(false)
    , _lastMotionTime(0)
    , _motionStartTime(0)
    , _totalFramesProcessed(0)
    , _motionFrameCount(0)
{
    memset(_trajectory, 0, sizeof(_trajectory));
}

MotionDetector::~MotionDetector() {
    if (_previousFrame) {
        heap_caps_free(_previousFrame);
        _previousFrame = nullptr;
    }
    _initialized = false;
}

bool MotionDetector::begin(uint16_t frameWidth, uint16_t frameHeight) {
    if (_initialized) {
        Serial.println("[MOTION] Already initialized");
        return true;
    }

    _frameWidth = frameWidth;
    _frameHeight = frameHeight;
    _frameSize = frameWidth * frameHeight;

    Serial.printf("[MOTION] Initializing for %ux%u frames (%u bytes)\n",
                  _frameWidth, _frameHeight, _frameSize);

    // Alloca buffer in PSRAM per frame precedente
    _previousFrame = (uint8_t*)heap_caps_malloc(_frameSize, MALLOC_CAP_SPIRAM);
    if (!_previousFrame) {
        Serial.println("[MOTION ERROR] Failed to allocate frame buffer!");
        return false;
    }

    memset(_previousFrame, 0, _frameSize);
    _initialized = true;

    Serial.println("[MOTION] Initialized successfully!");
    Serial.printf("[MOTION] Motion threshold: %u\n", _motionThreshold);
    Serial.printf("[MOTION] Min motion intensity: %u\n", MIN_MOTION_INTENSITY);

    return true;
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
    _totalFramesProcessed++;

    // Calcola luminosità media per auto flash
    _avgBrightness = _calculateAverageBrightness(frameBuffer);
    _updateFlashIntensity();

    // Calcola differenza frame
    uint32_t totalDelta = 0;
    uint32_t changedPixels = 0;

    for (size_t i = 0; i < _frameSize; i++) {
        int16_t diff = abs((int16_t)frameBuffer[i] - (int16_t)_previousFrame[i]);
        if (diff > _motionThreshold) {
            changedPixels++;
            totalDelta += diff;
        }
    }

    // Calcola intensità movimento
    _motionIntensity = _calculateMotionIntensity(changedPixels, totalDelta);

    // Verifica area minima movimento
    float motionArea = (float)changedPixels / (float)_frameSize;
    bool hasMinArea = motionArea >= MIN_MOTION_AREA;

    // Rileva movimento se sopra soglia e area minima
    bool motionCandidate = (_motionIntensity > MIN_MOTION_INTENSITY) && hasMinArea;

    // Stabilizzazione con confidence counter
    if (motionCandidate) {
        if (_motionConfidence < MOTION_CONFIRM_FRAMES) {
            _motionConfidence++;
        }
    } else {
        if (_motionConfidence > 0) {
            _motionConfidence--;
        }
    }

    // Conferma movimento solo dopo N frame consecutivi
    bool wasActive = _motionActive;
    _motionActive = (_motionConfidence >= MOTION_CONFIRM_FRAMES);

    // Se movimento confermato
    if (_motionActive) {
        // Calcola centroide del movimento
        if (_calculateCentroid(frameBuffer)) {
            _updateTrajectory();
        }

        if (!wasActive) {
            _motionStartTime = now;
            Serial.printf("[MOTION] Motion STARTED (intensity: %u, area: %.2f%%)\n",
                          _motionIntensity, motionArea * 100.0f);
        }

        _lastMotionTime = now;
        _motionFrameCount++;

    } else {
        // Movimento terminato
        if (wasActive) {
            Serial.printf("[MOTION] Motion ENDED (trajectory: %u points)\n", _trajectoryLength);
        }

        // Reset traiettoria se fermo per troppo tempo
        if (_trajectoryLength > 0 && (now - _lastMotionTime) > 1000) {
            _trajectoryLength = 0;
            _centroidValid = false;
        }
    }

    // Copia frame corrente in previous
    memcpy(_previousFrame, frameBuffer, _frameSize);

    return _motionActive;
}

uint32_t MotionDetector::_calculateFrameDifference(const uint8_t* currentFrame) {
    uint32_t changedPixels = 0;

    for (size_t i = 0; i < _frameSize; i++) {
        int16_t diff = abs((int16_t)currentFrame[i] - (int16_t)_previousFrame[i]);
        if (diff > _motionThreshold) {
            changedPixels++;
        }
    }

    return changedPixels;
}

uint8_t MotionDetector::_calculateMotionIntensity(uint32_t changedPixels, uint32_t totalDelta) {
    if (changedPixels == 0 || _frameSize == 0) {
        return 0;
    }

    // Media delta sui pixel cambiati
    uint32_t avgDelta = totalDelta / changedPixels;
    if (avgDelta > 255) {
        avgDelta = 255;
    }

    // Percentuale pixel cambiati
    float coverage = (float)changedPixels / (float)_frameSize;
    uint8_t coverageScore = (uint8_t)(coverage * 255.0f);
    if (coverageScore > 255) {
        coverageScore = 255;
    }

    // Intensità come media tra ampiezza e copertura
    uint8_t intensity = (uint8_t)((avgDelta + coverageScore) / 2);

    return intensity;
}

bool MotionDetector::_calculateCentroid(const uint8_t* currentFrame) {
    uint64_t weightedX = 0;
    uint64_t weightedY = 0;
    uint32_t totalWeight = 0;

    // Calcola centroide pesato del movimento
    for (uint16_t y = 0; y < _frameHeight; y++) {
        for (uint16_t x = 0; x < _frameWidth; x++) {
            size_t idx = y * _frameWidth + x;

            int16_t diff = abs((int16_t)currentFrame[idx] - (int16_t)_previousFrame[idx]);

            if (diff > _motionThreshold) {
                uint16_t weight = diff;
                weightedX += (uint64_t)x * weight;
                weightedY += (uint64_t)y * weight;
                totalWeight += weight;
            }
        }
    }

    // Verifica peso minimo per centroide valido
    if (totalWeight < 500) {
        _centroidValid = false;
        return false;
    }

    // Calcola posizione centroide
    float centroidX = (float)weightedX / (float)totalWeight;
    float centroidY = (float)weightedY / (float)totalWeight;

    // Smooth del centroide per ridurre jitter
    const float SMOOTHING = 0.3f;
    if (_centroidValid) {
        _currentCentroidX = _currentCentroidX + (centroidX - _currentCentroidX) * SMOOTHING;
        _currentCentroidY = _currentCentroidY + (centroidY - _currentCentroidY) * SMOOTHING;
    } else {
        _currentCentroidX = centroidX;
        _currentCentroidY = centroidY;
    }

    _centroidValid = true;
    return true;
}

void MotionDetector::_updateTrajectory() {
    if (!_centroidValid) {
        return;
    }

    unsigned long now = millis();

    // Normalizza coordinate (0.0 - 1.0)
    float normX = _currentCentroidX / (float)_frameWidth;
    float normY = _currentCentroidY / (float)_frameHeight;

    // Se traiettoria vuota, aggiungi primo punto
    if (_trajectoryLength == 0) {
        _trajectory[0].x = normX;
        _trajectory[0].y = normY;
        _trajectory[0].timestamp = now;
        _trajectory[0].intensity = _motionIntensity;
        _trajectoryLength = 1;
        return;
    }

    // Verifica distanza dall'ultimo punto
    TrajectoryPoint& last = _trajectory[_trajectoryLength - 1];
    float dx = normX - last.x;
    float dy = normY - last.y;
    float distance = sqrtf(dx * dx + dy * dy);

    // Aggiungi punto solo se movimento significativo
    const float MIN_DISTANCE = 0.05f;  // 5% del frame
    if (distance < MIN_DISTANCE) {
        // Aggiorna timestamp e intensità dell'ultimo punto
        last.timestamp = now;
        last.intensity = max(last.intensity, _motionIntensity);
        return;
    }

    // Aggiungi nuovo punto se c'è spazio
    if (_trajectoryLength < MAX_TRAJECTORY_POINTS) {
        _trajectory[_trajectoryLength].x = normX;
        _trajectory[_trajectoryLength].y = normY;
        _trajectory[_trajectoryLength].timestamp = now;
        _trajectory[_trajectoryLength].intensity = _motionIntensity;
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
    }
}

uint8_t MotionDetector::getTrajectory(TrajectoryPoint* outPoints) const {
    if (!outPoints || _trajectoryLength == 0) {
        return 0;
    }

    memcpy(outPoints, _trajectory, sizeof(TrajectoryPoint) * _trajectoryLength);
    return _trajectoryLength;
}

uint8_t MotionDetector::_calculateAverageBrightness(const uint8_t* frame) {
    if (!frame || _frameSize == 0) {
        return 0;
    }

    uint64_t total = 0;
    for (size_t i = 0; i < _frameSize; i++) {
        total += frame[i];
    }

    return (uint8_t)(total / _frameSize);
}

void MotionDetector::_updateFlashIntensity() {
    // Logica auto flash con smoothing graduale:
    // - Scena molto scura (< 30): flash al massimo (255)
    // - Scena scura (30-100): flash medio-alto (150-255)
    // - Scena normale (100-180): flash minimo garantito (80-150)
    // - Scena luminosa (> 180): flash minimo (80)
    //
    // Il flash non si spegne MAI per garantire illuminazione costante

    uint8_t targetIntensity;

    if (_avgBrightness < 30) {
        targetIntensity = 255;  // Flash massimo per scena molto scura
    } else if (_avgBrightness < 100) {
        // Scala lineare 30-100 -> 255-150
        targetIntensity = map(_avgBrightness, 30, 100, 255, 150);
    } else if (_avgBrightness < 180) {
        // Scala lineare 100-180 -> 150-80
        targetIntensity = map(_avgBrightness, 100, 180, 150, 80);
    } else {
        targetIntensity = 80;  // Flash minimo garantito
    }

    // Smoothing: cambia gradualmente verso target (evita flicker)
    if (_flashIntensity < targetIntensity) {
        uint16_t newValue = (uint16_t)_flashIntensity + 5;
        _flashIntensity = (newValue > targetIntensity) ? targetIntensity : newValue;
    } else if (_flashIntensity > targetIntensity) {
        int16_t newValue = (int16_t)_flashIntensity - 5;
        _flashIntensity = (newValue < targetIntensity) ? targetIntensity : newValue;
    }
}

void MotionDetector::setSensitivity(uint8_t sensitivity) {
    // Sensitivity: 0 = insensibile, 255 = molto sensibile
    // Invertiamo threshold: sensitivity alta -> threshold basso
    _motionThreshold = map(sensitivity, 0, 255, 100, 20);

    Serial.printf("[MOTION] Sensitivity updated: %u (threshold: %u)\n",
                  sensitivity, _motionThreshold);
}

void MotionDetector::reset() {
    _motionIntensity = 0;
    _motionActive = false;
    _motionConfidence = 0;
    _trajectoryLength = 0;
    _currentCentroidX = 0.0f;
    _currentCentroidY = 0.0f;
    _centroidValid = false;
    _lastMotionTime = 0;
    _motionStartTime = 0;
    _totalFramesProcessed = 0;
    _motionFrameCount = 0;
    _flashIntensity = 0;
    _avgBrightness = 0;

    memset(_trajectory, 0, sizeof(_trajectory));

    if (_previousFrame) {
        memset(_previousFrame, 0, _frameSize);
    }

    Serial.println("[MOTION] State reset");
}

MotionDetector::Metrics MotionDetector::getMetrics() const {
    Metrics metrics;
    metrics.totalFramesProcessed = _totalFramesProcessed;
    metrics.motionFrameCount = _motionFrameCount;
    metrics.currentIntensity = _motionIntensity;
    metrics.avgBrightness = _avgBrightness;
    metrics.flashIntensity = _flashIntensity;
    metrics.trajectoryLength = _trajectoryLength;
    metrics.motionActive = _motionActive;

    return metrics;
}
