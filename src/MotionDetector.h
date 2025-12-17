#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include <Arduino.h>

/**
 * @brief Rilevatore di movimento semplificato per ESP32-CAM
 *
 * Funzionalità:
 * - Auto flash LED basato sulla luminosità della scena
 * - Tracciamento 2D di oggetti in movimento vicino alla camera
 * - Curva di traiettoria dal punto di entrata al punto di uscita
 * - Stabilizzazione immagine per ridurre falsi positivi
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
     * @brief Ottiene intensità flash LED consigliata (0-255)
     * Basata sulla luminosità media della scena
     * @return 0 = flash spento, 255 = flash al massimo
     */
    uint8_t getRecommendedFlashIntensity() const { return _flashIntensity; }

    /**
     * @brief Verifica se c'è movimento attivo
     */
    bool isMotionActive() const { return _motionActive; }

    /**
     * @brief Ottiene intensità movimento corrente (0-255)
     */
    uint8_t getMotionIntensity() const { return _motionIntensity; }

    /**
     * @brief Struttura che rappresenta un punto 2D nella traiettoria
     */
    struct TrajectoryPoint {
        float x;           // Coordinata X normalizzata (0.0-1.0)
        float y;           // Coordinata Y normalizzata (0.0-1.0)
        uint32_t timestamp; // Timestamp in millisecondi
        uint8_t intensity;  // Intensità del movimento in quel punto
    };

    /**
     * @brief Ottiene la traiettoria corrente dell'oggetto in movimento
     * @param outPoints Array dove salvare i punti (deve essere almeno MAX_TRAJECTORY_POINTS)
     * @return Numero di punti nella traiettoria (0 se nessun movimento)
     */
    uint8_t getTrajectory(TrajectoryPoint* outPoints) const;

    /**
     * @brief Numero massimo di punti nella traiettoria
     */
    static constexpr uint8_t MAX_TRAJECTORY_POINTS = 20;

    /**
     * @brief Reset dello stato del detector
     */
    void reset();

    /**
     * @brief Configura sensibilità rilevamento movimento
     * @param sensitivity 0-255 (0=insensibile, 255=molto sensibile)
     */
    void setSensitivity(uint8_t sensitivity);

    /**
     * @brief Struttura metriche
     */
    struct Metrics {
        uint32_t totalFramesProcessed;
        uint32_t motionFrameCount;
        uint8_t currentIntensity;
        uint8_t avgBrightness;
        uint8_t flashIntensity;
        uint8_t trajectoryLength;
        bool motionActive;
    };

    Metrics getMetrics() const;

private:
    bool _initialized;
    uint16_t _frameWidth;
    uint16_t _frameHeight;
    size_t _frameSize;

    // Frame buffers
    uint8_t* _previousFrame;

    // Auto flash
    uint8_t _flashIntensity;        // Intensità flash consigliata (0-255)
    uint8_t _avgBrightness;         // Luminosità media scena

    // Motion detection
    uint8_t _motionThreshold;       // Soglia differenza pixel
    uint8_t _motionIntensity;       // Intensità movimento corrente
    bool _motionActive;             // Stato movimento confermato
    uint8_t _motionConfidence;      // Confidence counter per stabilizzazione

    // Trajectory tracking
    TrajectoryPoint _trajectory[MAX_TRAJECTORY_POINTS];
    uint8_t _trajectoryLength;

    // Centroid tracking
    float _currentCentroidX;
    float _currentCentroidY;
    bool _centroidValid;

    // Timing
    unsigned long _lastMotionTime;
    unsigned long _motionStartTime;

    // Metrics
    uint32_t _totalFramesProcessed;
    uint32_t _motionFrameCount;

    // Costanti
    static constexpr uint8_t MIN_MOTION_INTENSITY = 25;   // Ridotto da 40 per maggiore sensibilità
    static constexpr uint8_t MOTION_CONFIRM_FRAMES = 2;   // Ridotto da 3 per risposta più veloce
    static constexpr uint8_t STILL_CONFIRM_FRAMES = 5;
    static constexpr float MIN_MOTION_AREA = 0.015f;      // Ridotto da 0.02 (1.5% del frame)

    /**
     * @brief Calcola differenza tra frame corrente e precedente
     * @return Numero pixel cambiati
     */
    uint32_t _calculateFrameDifference(const uint8_t* currentFrame);

    /**
     * @brief Calcola intensità movimento
     */
    uint8_t _calculateMotionIntensity(uint32_t changedPixels, uint32_t totalDelta);

    /**
     * @brief Calcola centroide del movimento
     * @return true se centroide valido
     */
    bool _calculateCentroid(const uint8_t* currentFrame);

    /**
     * @brief Aggiorna traiettoria con nuovo centroide
     */
    void _updateTrajectory();

    /**
     * @brief Calcola luminosità media del frame
     */
    uint8_t _calculateAverageBrightness(const uint8_t* frame);

    /**
     * @brief Aggiorna intensità flash consigliata basata su luminosità
     */
    void _updateFlashIntensity();
};

#endif // MOTION_DETECTOR_H
