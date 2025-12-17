#include "MotionDetector.h"
#include <esp_heap_caps.h>

MotionDetector::MotionDetector()
    : _initialized(false)
    , _frameWidth(0)
    , _frameHeight(0)
    , _frameSize(0)
    , _previousFrame(nullptr)
    , _motionThreshold(30)
    , _shakeThreshold(150)
    , _motionIntensity(0)
    , _changedPixels(0)
    , _shakeDetected(false)
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
    if (!_previousFrame) {
        Serial.println("[MOTION ERROR] Failed to allocate previous frame buffer!");
        return false;
    }

    // Inizializza a nero
    memset(_previousFrame, 0, _frameSize);

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

    // Confronta ogni pixel con frame precedente
    for (size_t i = 0; i < _frameSize; i++) {
        int16_t diff = abs((int16_t)currentFrame[i] - (int16_t)_previousFrame[i]);
        if (diff > _motionThreshold) {
            changedPixels++;
        }
    }

    return changedPixels;
}

uint8_t MotionDetector::_calculateMotionIntensity(const uint8_t* currentFrame, uint32_t changedPixels) {
    if (changedPixels == 0) {
        return 0;
    }

    // Calcola delta medio solo sui pixel cambiati
    uint32_t totalDelta = 0;
    uint32_t sampledPixels = 0;

    // Campiona solo pixel cambiati per efficienza
    for (size_t i = 0; i < _frameSize && sampledPixels < changedPixels; i++) {
        int16_t diff = abs((int16_t)currentFrame[i] - (int16_t)_previousFrame[i]);
        if (diff > _motionThreshold) {
            totalDelta += diff;
            sampledPixels++;
        }
    }

    // Media delta
    uint8_t avgDelta = (sampledPixels > 0) ? (totalDelta / sampledPixels) : 0;

    // Normalizza a 0-255 (assumendo delta max ~100)
    uint8_t intensity = min(avgDelta * 2, 255);

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

    // Calcola differenza frame
    _changedPixels = _calculateFrameDifference(frameBuffer);

    // Calcola intensità movimento
    _motionIntensity = _calculateMotionIntensity(frameBuffer, _changedPixels);

    // Aggiorna history e rileva shake
    _updateHistoryAndDetectShake(_motionIntensity);

    // Aggiorna timing
    bool motionDetected = (_motionIntensity > 0);

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
    _shakeDetected = false;
    _historyIndex = 0;
    _historyFull = false;
    _lastMotionTime = 0;
    _motionStartTime = 0;
    _lastStillTime = millis();
    _totalFramesProcessed = 0;
    _motionFrameCount = 0;
    _shakeCount = 0;

    memset(_intensityHistory, 0, sizeof(_intensityHistory));

    if (_previousFrame) {
        memset(_previousFrame, 0, _frameSize);
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

    _getHistoryMinMax(metrics.minIntensityRecent, metrics.maxIntensityRecent);

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
