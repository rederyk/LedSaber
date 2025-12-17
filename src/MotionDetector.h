#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include <Arduino.h>

/**
 * @brief Rileva movimento tramite frame differencing su immagini grayscale
 *
 * Implementa:
 * - Frame differencing (confronto frame corrente vs precedente)
 * - Calcolo intensità movimento (0-255)
 * - Shake detection con history buffer
 * - Gesture recognition base (hand over, quick shake, still)
 *
 * Ottimizzato per ESP32-CAM QVGA (320x240) grayscale
 */
class MotionDetector {
public:
    MotionDetector();
    ~MotionDetector();

    /**
     * @brief Inizializza il motion detector
     * @param frameWidth Larghezza frame (default: 320)
     * @param frameHeight Altezza frame (default: 240)
     * @return true se inizializzazione OK
     */
    bool begin(uint16_t frameWidth = 320, uint16_t frameHeight = 240);

    /**
     * @brief Processa un nuovo frame per motion detection
     * @param frameBuffer Buffer frame grayscale (1 byte/pixel)
     * @param frameLength Lunghezza buffer in bytes
     * @return true se movimento rilevato
     */
    bool processFrame(const uint8_t* frameBuffer, size_t frameLength);

    /**
     * @brief Ottiene intensità movimento ultimo frame (0-255)
     * @return Intensità movimento
     */
    uint8_t getMotionIntensity() const { return _motionIntensity; }

    /**
     * @brief Verifica se è stato rilevato uno shake
     * @return true se shake rilevato
     */
    bool isShakeDetected() const { return _shakeDetected; }

    /**
     * @brief Ottiene numero pixel cambiati ultimo frame
     * @return Pixel count
     */
    uint32_t getChangedPixels() const { return _changedPixels; }

    /**
     * @brief Configura soglia rilevamento movimento
     * @param threshold Soglia differenza pixel (0-255, default: 30)
     */
    void setMotionThreshold(uint8_t threshold) { _motionThreshold = threshold; }

    /**
     * @brief Configura soglia rilevamento shake
     * @param threshold Soglia swing intensità (default: 150)
     */
    void setShakeThreshold(uint8_t threshold) { _shakeThreshold = threshold; }

    /**
     * @brief Configura sensibilità generale (scala threshold)
     * @param sensitivity 0-255 (0=insensibile, 255=molto sensibile)
     */
    void setSensitivity(uint8_t sensitivity);

    /**
     * @brief Reset stato detector
     */
    void reset();

    /**
     * @brief Ottiene metriche detector
     */
    struct MotionMetrics {
        uint32_t totalFramesProcessed;
        uint32_t motionFrameCount;
        uint32_t shakeCount;
        uint8_t currentIntensity;
        uint32_t changedPixels;
        uint8_t minIntensityRecent;
        uint8_t maxIntensityRecent;
    };

    MotionMetrics getMetrics() const;

    /**
     * @brief Verifica se rilevato movimento continuo
     * @param durationMs Durata minima movimento in ms
     * @return true se movimento continuo per almeno durationMs
     */
    bool isContinuousMotion(unsigned long durationMs = 1000) const;

    /**
     * @brief Verifica se fermo (still) per un certo tempo
     * @param durationMs Durata minima still in ms
     * @return true se nessun movimento per almeno durationMs
     */
    bool isStill(unsigned long durationMs = 3000) const;

private:
    bool _initialized;
    uint16_t _frameWidth;
    uint16_t _frameHeight;
    size_t _frameSize;

    // Frame buffers
    uint8_t* _previousFrame;

    // Motion detection parameters
    uint8_t _motionThreshold;      // Soglia differenza pixel (default: 30)
    uint8_t _shakeThreshold;       // Soglia swing per shake (default: 150)

    // State
    uint8_t _motionIntensity;      // Intensità movimento corrente (0-255)
    uint32_t _changedPixels;       // Pixel cambiati ultimo frame
    bool _shakeDetected;           // Flag shake rilevato

    // Shake detection history buffer
    static constexpr uint8_t HISTORY_SIZE = 10;
    uint8_t _intensityHistory[HISTORY_SIZE];
    uint8_t _historyIndex;
    bool _historyFull;

    // Timing
    unsigned long _lastMotionTime;
    unsigned long _motionStartTime;
    unsigned long _lastStillTime;

    // Metrics
    uint32_t _totalFramesProcessed;
    uint32_t _motionFrameCount;
    uint32_t _shakeCount;

    /**
     * @brief Calcola differenza tra frame corrente e precedente
     * @param currentFrame Frame corrente
     * @return Numero pixel cambiati oltre threshold
     */
    uint32_t _calculateFrameDifference(const uint8_t* currentFrame);

    /**
     * @brief Calcola intensità movimento media dai pixel cambiati
     * @param currentFrame Frame corrente
     * @param changedPixels Numero pixel cambiati
     * @return Intensità movimento (0-255)
     */
    uint8_t _calculateMotionIntensity(const uint8_t* currentFrame, uint32_t changedPixels);

    /**
     * @brief Aggiorna history buffer e rileva shake
     * @param intensity Intensità corrente
     */
    void _updateHistoryAndDetectShake(uint8_t intensity);

    /**
     * @brief Ottiene min/max intensità da history buffer
     */
    void _getHistoryMinMax(uint8_t& outMin, uint8_t& outMax) const;
};

#endif // MOTION_DETECTOR_H
