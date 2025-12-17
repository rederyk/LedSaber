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
     * @brief Gesture riconoscibili tramite pattern 2D
     */
    enum GestureType {
        GESTURE_NONE = 0,
        GESTURE_SLASH_VERTICAL,      // Movimento alto→basso sequenziale
        GESTURE_SLASH_HORIZONTAL,    // Movimento sinistra→destra sequenziale
        GESTURE_ROTATION,            // Movimento circolare attorno al centro
        GESTURE_THRUST               // Movimento rapido concentrato verso centro/bordi
    };

    /**
     * @brief Direzioni movimento rilevabili (deprecated - sostituito da gesture)
     */
    enum MotionDirection {
        DIRECTION_NONE = 0,
        DIRECTION_UP,
        DIRECTION_DOWN,
        DIRECTION_LEFT,
        DIRECTION_RIGHT,
        DIRECTION_UP_LEFT,
        DIRECTION_UP_RIGHT,
        DIRECTION_DOWN_LEFT,
        DIRECTION_DOWN_RIGHT,
        DIRECTION_CENTER
    };

    /**
     * @brief Prossimità movimento (vicino vs lontano)
     */
    enum MotionProximity {
        PROXIMITY_UNKNOWN = 0,  // Non ancora testato
        PROXIMITY_NEAR,         // Movimento vicino (mano davanti)
        PROXIMITY_FAR           // Movimento lontano (sfondo)
    };

    /**
     * @brief Ottiene gesture rilevata corrente
     * @return Tipo di gesture
     */
    GestureType getGesture() const { return _currentGesture; }

    /**
     * @brief Ottiene nome gesture rilevata
     * @return Nome gesture
     */
    const char* getGestureName() const;

    /**
     * @brief Ottiene confidence score della gesture (0-100)
     * @return Confidence percentage
     */
    uint8_t getGestureConfidence() const { return _gestureConfidence; }

    /**
     * @brief Ottiene direzione movimento predominante (deprecated)
     * @return Direzione movimento
     */
    MotionDirection getMotionDirection() const { return _motionDirection; }

    /**
     * @brief Ottiene nome direzione movimento (deprecated)
     * @return Nome direzione
     */
    const char* getMotionDirectionName() const;

    /**
     * @brief Ottiene stato del vettore movimento calcolato tramite centroidi
     */
    bool hasValidCentroid() const { return _centroidValid; }
    float getCentroidX() const { return _currentCentroidX; }
    float getCentroidY() const { return _currentCentroidY; }
    float getMotionVectorX() const { return _motionVectorX; }
    float getMotionVectorY() const { return _motionVectorY; }
    uint8_t getVectorConfidence() const { return _vectorConfidence; }

    /**
     * @brief Ottiene prossimità movimento (vicino/lontano)
     * @return Prossimità movimento
     */
    MotionProximity getMotionProximity() const { return _motionProximity; }

    /**
     * @brief Ottiene nome prossimità movimento
     * @return Nome prossimità ("unknown", "near", "far")
     */
    const char* getMotionProximityName() const;

    /**
     * @brief Richiede test prossimità con flash nel prossimo frame
     * Deve essere chiamato PRIMA di processFrame() per catturare frame con flash
     */
    void requestProximityTest();

    /**
     * @brief Verifica se è richiesto test prossimità
     * @return true se il prossimo frame deve avere flash acceso
     */
    bool isProximityTestRequested() const { return _proximityTestRequested; }

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
        uint8_t intensityFloor;
        MotionDirection direction;
        uint8_t zoneIntensities[81];  // Intensità per ogni zona (9x9)
        GestureType gesture;
        uint8_t gestureConfidence;
        MotionProximity proximity;
        uint8_t proximityBrightness;  // Luminosità media frame con flash
        bool centroidValid;
        float centroidX;
        float centroidY;
        float motionVectorX;
        float motionVectorY;
        uint8_t vectorConfidence;
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
    uint8_t* _backgroundFrame;

    // Motion detection parameters
    uint8_t _motionThreshold;      // Soglia differenza pixel (default: 30)
    uint8_t _shakeThreshold;       // Soglia swing per shake (default: 150)

    // State
    uint8_t _motionIntensity;      // Intensità movimento corrente (0-255)
    uint32_t _changedPixels;       // Pixel cambiati ultimo frame
    uint32_t _lastTotalDelta;      // Somma differenze per intensità
    bool _motionActive;            // Stato confermato movimento
    bool _shakeDetected;           // Flag shake rilevato
    MotionDirection _motionDirection;  // Direzione movimento predominante (deprecated)
    MotionProximity _motionProximity;  // Prossimità movimento (near/far)
    bool _proximityTestRequested;      // Flag richiesta test con flash
    uint8_t _proximityBrightness;      // Luminosità media frame con flash
    bool _backgroundInitialized;
    uint16_t _backgroundWarmupFrames;

    // Gesture recognition
    GestureType _currentGesture;       // Gesture rilevata corrente
    uint8_t _gestureConfidence;        // Confidence score gesture (0-100)

    // Motion centroid e vettore
    bool _centroidValid;
    float _currentCentroidX;
    float _currentCentroidY;
    float _motionVectorX;
    float _motionVectorY;
    uint8_t _vectorConfidence;
    uint8_t _motionConfidence;
    uint8_t _stillConfidence;
    uint8_t _lightingCooldown;
    uint8_t _lastFrameBrightness;
    bool _brightnessInitialized;

    // Zone detection (9x9 grid)
    static constexpr uint8_t GRID_ROWS = 9;
    static constexpr uint8_t GRID_COLS = 9;
    static constexpr uint8_t GRID_SIZE = GRID_ROWS * GRID_COLS;  // 81 celle
    uint8_t _zoneIntensities[GRID_SIZE];  // Intensità per zona (frame corrente)

    // Trajectory tracking (ultimi N frames di heatmap)
    static constexpr uint8_t TRAJECTORY_DEPTH = 15;  // Ultimi 15 frames
    uint8_t _trajectoryBuffer[TRAJECTORY_DEPTH][GRID_SIZE];  // Storia heatmap
    uint8_t _trajectoryIndex;          // Indice circolare nel buffer
    bool _trajectoryFull;              // Buffer pieno almeno una volta

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
    static constexpr uint8_t BACKGROUND_WARMUP_FRAMES = 8;
    static constexpr uint16_t MIN_CENTROID_WEIGHT = 1500;
    static constexpr float CENTROID_SMOOTHING = 0.35f;
    static constexpr uint8_t MOTION_CONFIRM_FRAMES = 3;
    static constexpr uint8_t STILL_CONFIRM_FRAMES = 4;
    static constexpr uint8_t LIGHTING_SUPPRESSION_FRAMES = 5;
    static constexpr uint8_t BASE_MIN_INTENSITY = 30;
    static constexpr uint8_t NOISE_MARGIN = 12;
    static constexpr uint8_t MAX_INTENSITY_FLOOR = 150;
    static constexpr uint8_t NOISE_SPIKE_LIMIT = 4;
    static constexpr uint16_t NOISE_SPIKE_WINDOW_MS = 1500;

    /**
     * @brief Calcola differenza tra frame corrente e precedente
     * @param currentFrame Frame corrente
     * @return Numero pixel cambiati oltre threshold
     */
    uint32_t _calculateFrameDifference(const uint8_t* currentFrame);

    /**
     * @brief Calcola intensità movimento media dai pixel cambiati
     * @param changedPixels Numero pixel cambiati
     * @return Intensità movimento (0-255)
     */
    uint8_t _calculateMotionIntensity(uint32_t changedPixels);

    /**
     * @brief Calcola intensità movimento per zone (3x3 grid)
     * @param currentFrame Frame corrente
     */
    void _calculateZoneIntensities(const uint8_t* currentFrame);

    /**
     * @brief Determina direzione movimento predominante dalle zone (deprecated)
     * @return Direzione movimento
     */
    MotionDirection _determineMotionDirection();

    /**
     * @brief Aggiorna trajectory buffer con heatmap corrente
     */
    void _updateTrajectoryBuffer();

    /**
     * @brief Riconosce pattern gesture dalla trajectory
     * @return Tipo di gesture rilevata
     */
    GestureType _recognizeGesture();

    /**
     * @brief Rileva slash verticale (alto→basso)
     * @param outConfidence Output confidence score
     * @return true se pattern rilevato
     */
    bool _detectSlashVertical(uint8_t& outConfidence);

    /**
     * @brief Rileva slash orizzontale (sinistra→destra)
     * @param outConfidence Output confidence score
     * @return true se pattern rilevato
     */
    bool _detectSlashHorizontal(uint8_t& outConfidence);

    /**
     * @brief Rileva rotazione circolare
     * @param outConfidence Output confidence score
     * @return true se pattern rilevato
     */
    bool _detectRotation(uint8_t& outConfidence);

    /**
     * @brief Rileva thrust/stoccata (movimento rapido verso punto)
     * @param outConfidence Output confidence score
     * @return true se pattern rilevato
     */
    bool _detectThrust(uint8_t& outConfidence);

    /**
     * @brief Aggiorna history buffer e rileva shake
     * @param intensity Intensità corrente
     */
    void _updateHistoryAndDetectShake(uint8_t intensity);

    /**
     * @brief Ottiene min/max intensità da history buffer
     */
    void _getHistoryMinMax(uint8_t& outMin, uint8_t& outMax) const;

    /**
     * @brief Analizza frame con flash per determinare prossimità
     * @param flashFrame Frame catturato con flash acceso
     * @return Prossimità rilevata (NEAR o FAR)
     */
    MotionProximity _analyzeProximity(const uint8_t* flashFrame);

    /**
     * @brief Calcola luminosità media di un frame
     * @param frame Frame da analizzare
     * @return Luminosità media (0-255)
     */
    uint8_t _calculateAverageBrightness(const uint8_t* frame) const;

    /**
     * @brief Aggiorna modello di background adattivo
     * @param currentFrame Frame corrente
     * @param motionDetected true se è presente movimento nel frame
     */
    void _updateBackgroundModel(const uint8_t* currentFrame, bool motionDetected);

    /**
     * @brief Determina la direzione del vettore movimento stimato
     */
    MotionDirection _directionFromVector(float vecX, float vecY) const;

    /**
     * @brief Aggiorna il modello di rumore e il floor dinamico dell'intensità
     * @param intensity Intensità corrente misurata
     * @param rawMotionCandidate true se sopra la soglia minima di base
     */
    void _updateNoiseModel(uint8_t intensity, bool rawMotionCandidate);

    /**
     * @brief Valida coerenza spaziale del movimento rilevato
     * @return true se il movimento è spazialmente coerente (non rumore sparso)
     */
    bool _validateSpatialCoherence();

    // Noise auto-regulation state
    uint8_t _ambientNoiseEstimate;
    uint8_t _dynamicIntensityFloor;
    bool _noiseModelInitialized;
    uint8_t _noiseSpikeCounter;
    unsigned long _lastNoiseSpikeTime;
    unsigned long _lastNoiseSampleTime;
};

#endif // MOTION_DETECTOR_H
