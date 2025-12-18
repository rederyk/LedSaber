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
        const uint8_t* frame1,
        const uint8_t* frame2,
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
     * @brief Calcola mediana di un array
     */
    static int8_t _median(int8_t* values, uint8_t count);
};

#endif // OPTICAL_FLOW_DETECTOR_H
