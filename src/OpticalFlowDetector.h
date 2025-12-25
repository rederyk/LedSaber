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
    // Configurazione griglia (condivisa con renderer/servizi BLE)
    // Aumentata a 8x8 per maggiore risoluzione gesture detection
    static constexpr uint8_t BLOCK_SIZE = 30;  // 240/8 = 30px per blocco
    static constexpr uint8_t GRID_COLS = 8;
    static constexpr uint8_t GRID_ROWS = 8;
    static constexpr uint8_t TOTAL_BLOCKS = GRID_COLS * GRID_ROWS;  // 64 blocchi

    // Algoritmo di rilevamento
    enum class Algorithm : uint8_t {
        OPTICAL_FLOW_SAD,   // Alta precisione, più pesante (Default)
        CENTROID_TRACKING   // Molto leggero, ideale per oggetti vicini/gesture veloci
    };

    OpticalFlowDetector();
    ~OpticalFlowDetector();

    // ═══════════════════════════════════════════════════════════
    // PUBLIC API (compatibile con MotionDetector)
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Inizializza detector
     * @param frameWidth Larghezza frame (default: 240)
     * @param frameHeight Altezza frame (default: 240)
     * @return true se successo
     */
    bool begin(uint16_t frameWidth = 240, uint16_t frameHeight = 240);

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
     * @brief Configura qualità motion (mappa minConfidence/minActiveBlocks)
     * @param quality 0-255 (0=rigido, 255=molto permissivo)
     */
    void setQuality(uint8_t quality);

    /**
     * @brief Legge qualità corrente (0-255)
     */
    uint8_t getQuality() const { return _quality; }

    /**
     * @brief Soglie motion globale (per motionActive)
     */
    void setMotionIntensityThreshold(uint8_t threshold);
    uint8_t getMotionIntensityThreshold() const { return _motionIntensityThreshold; }

    void setMotionSpeedThreshold(float threshold);
    float getMotionSpeedThreshold() const { return _motionSpeedThreshold; }

    /**
     * @brief Reset stato detector
     */
    void reset();

    /**
     * @brief De-inizializza e libera la memoria (PSRAM)
     */
    void end();

    /**
     * @brief Imposta l'algoritmo di rilevamento
     * @param algo OPTICAL_FLOW_SAD o CENTROID_TRACKING
     */
    void setAlgorithm(Algorithm algo) { _algorithm = algo; }

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

    /**
     * @brief Ottieni vettore movimento di un blocco specifico
     * @param row Riga blocco (0-5)
     * @param col Colonna blocco (0-5)
     * @param outDx Output: componente X vettore
     * @param outDy Output: componente Y vettore
     * @param outConfidence Output: confidence (0-255)
     * @return true se blocco valido
     */
    bool getBlockVector(uint8_t row, uint8_t col, int8_t* outDx, int8_t* outDy, uint8_t* outConfidence) const;

    /**
     * @brief Restituisce tag ASCII compatto per descrivere il blocco
     * @param row Riga (0-GRID_ROWS)
     * @param col Colonna (0-GRID_COLS)
     * @return '.' se inattivo, altrimenti simbolo direzione (^-v-<->ABCD)
     */
    char getBlockDirectionTag(uint8_t row, uint8_t col) const;

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

        // Fallback frame-diff (0-255): avg abs diff (sampled)
        uint8_t frameDiff;
    };

    Metrics getMetrics() const;

    /**
     * @brief Converte direzione in stringa (per debug/BLE)
     */
    static const char* directionToString(Direction dir);

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

    // Search parameters
    int8_t _searchRange;        // ±pixels (default: 10)
    uint8_t _searchStep;        // Sample step (default: 2)
    uint8_t _minConfidence;     // Threshold (default: 50)
    uint8_t _minActiveBlocks;   // Min blocks (default: 6)

    Algorithm _algorithm;       // Algoritmo corrente
    // Sensitivity and thresholds
    uint8_t _quality;
    float _directionMagnitudeThreshold;
    float _minCentroidWeight;
    uint8_t _motionIntensityThreshold;
    float _motionSpeedThreshold;

    // ═══════════════════════════════════════════════════════════
    // STATE
    // ═══════════════════════════════════════════════════════════

    // Frame buffers
    uint8_t* _previousFrame;    // PSRAM allocated
    uint8_t* _edgeFrame;        // Reused edge buffer (PSRAM)
    bool _hasPreviousFrame;

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
    uint8_t _frameDiffAvg;

    // Timing & metrics
    unsigned long _lastMotionTime;
    uint32_t _totalFramesProcessed;
    uint32_t _motionFrameCount;
    uint32_t _totalComputeTime;

    // Filtro temporale per ridurre rumore
    uint8_t _consecutiveMotionFrames;
    uint8_t _consecutiveStillFrames;

    unsigned long _lastFrameTimestamp;
    unsigned long _currentFrameDt;

    // ═══════════════════════════════════════════════════════════
    // CORE ALGORITHM
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Calcola optical flow completo
     */
    void _computeOpticalFlow(const uint8_t* currentFrame);

    /**
     * @brief Calcola movimento basato sul centroide della differenza (Lite Mode)
     */
    void _computeCentroidMotion(const uint8_t* currentFrame);

    /**
     * @brief Calcola motion vector per singolo blocco
     */
    void _calculateBlockMotion(uint8_t row, uint8_t col, const uint8_t* currentFrame);

    /**
     * @brief Calcola SAD tra due blocchi con early termination
     * @param currentMinSAD Best SAD attuale, per early exit
     * @return Sum of Absolute Differences
     */
    uint16_t _computeSAD(
        const uint8_t* frame1,
        const uint8_t* frame2,
        uint16_t x1, uint16_t y1,
        uint16_t x2, uint16_t y2,
        uint8_t blockSize,
        uint16_t currentMinSAD = UINT16_MAX
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

    /**
     * @brief Calcola frame diff medio (sampled) tra previous e current
     */
    uint8_t _calculateFrameDiffAvg(const uint8_t* currentFrame) const;

    // ═══════════════════════════════════════════════════════════
    // UTILITY
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Converte vettore (dx, dy) in direzione
     */
    Direction _vectorToDirection(float dx, float dy);

    /**
     * @brief Calcola mediana di un array
     */
    static int8_t _median(int8_t* values, uint8_t count);
};

#endif // OPTICAL_FLOW_DETECTOR_H
