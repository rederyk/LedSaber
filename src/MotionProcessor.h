#ifndef MOTION_PROCESSOR_H
#define MOTION_PROCESSOR_H

#include <Arduino.h>
#include "OpticalFlowDetector.h"

/**
 * @brief Processes raw motion data into gestures and perturbations
 *
 * Converts optical flow data into:
 * 1. Classified gestures (IGNITION, RETRACT, CLASH)
 * 2. Localized perturbation grid for LED effects
 */
class MotionProcessor {
public:
    enum class GestureType : uint8_t {
        NONE = 0,
        IGNITION,      // Gestita a livello LED quando lama spenta
        RETRACT,       // DOWN sostenuto
        CLASH,         // LEFT/RIGHT sostenuto
    };

    struct ProcessedMotion {
        GestureType gesture;
        uint8_t gestureConfidence;     // 0-100
        uint8_t motionIntensity;      // 0-255 (raw motion intensity)
        OpticalFlowDetector::Direction direction;
        float speed;                   // px/frame
        uint32_t timestamp;
        char effectRequest[32];       // Requested effect id (empty = none)

        // Localized perturbation data (6x6 grid matching optical flow)
        uint8_t perturbationGrid[OpticalFlowDetector::GRID_ROWS][OpticalFlowDetector::GRID_COLS];
    };

    struct Config {
        bool gesturesEnabled;
        uint8_t gestureThreshold;      // Min intensity base (legacy/swing)
        uint8_t ignitionIntensityThreshold; // Min intensity for IGNITION
        uint8_t retractIntensityThreshold;  // Min intensity for RETRACT
        uint8_t clashIntensityThreshold;    // Min intensity for CLASH
        uint16_t gestureDurationMs;    // Min duration for sustained gestures (default: 100ms)
        uint8_t clashDeltaThreshold;   // Min delta for clash (default: 15)
        uint16_t clashWindowMs;        // Time window for clash delta (default: 350ms)
        uint16_t gestureCooldownMs;    // Cooldown after major gesture (default: 800ms)
        uint16_t clashCooldownMs;      // Specific cooldown for clash spam control

        bool perturbationEnabled;
        uint8_t perturbationScale;     // Perturbation multiplier 0-255 (default: 255)
        bool debugLogsEnabled;         // Enable motion debug logs (default: false)

        // Speed thresholds (px/frame) - Alternative trigger for fast gestures
        float ignitionSpeedThreshold;
        float retractSpeedThreshold;
        float clashSpeedThreshold;
        // Direction -> effect mapping (4-way, 90 degrees)
        String effectOnUp;
        String effectOnDown;
        String effectOnLeft;
        String effectOnRight;

        Config() :
            gesturesEnabled(true),
            gestureThreshold(6),   // Ridotto a 6 per rilevare gesture più leggere
            ignitionIntensityThreshold(12),  // Ridotto a 6 per facilitare ignition
            retractIntensityThreshold(12),   // Ridotto a 6 (logs mostrano picchi di 9-10)
            clashIntensityThreshold(12),     // Aumentato da 14 a 30 per evitare rumore
            gestureDurationMs(150),  // Aumentato da 100 a 150ms per gesture più stabili
            clashDeltaThreshold(60), // Aumentato da 15 a 60 per ridurre falsi positivi
            clashWindowMs(600),     // ~2.2 frame @ 5.6fps per catturare veri clash
            gestureCooldownMs(1200), // Aumentato da 800 a 1200ms per ridurre spam
            clashCooldownMs(5000),   // Clash dedicato per evitare raffiche
            perturbationEnabled(true),
            perturbationScale(255),
            debugLogsEnabled(false),
            ignitionSpeedThreshold(0.4f),  // Ridotto a 0.5 per facilitare ignition
            retractSpeedThreshold(0.4f),   // Ridotto a 0.4 per facilitare retract
            clashSpeedThreshold(2.0f),
            effectOnUp(""),
            effectOnDown(""),
            effectOnLeft(""),
            effectOnRight("") {}
    };

    MotionProcessor();

    /**
     * @brief Process raw motion data into gestures and perturbations
     * @param rawMotion Motion data from camera task
     * @param detector Optical flow detector for block vectors
     * @return Processed motion with gesture and perturbation grid
     */
    ProcessedMotion process(uint8_t motionIntensity,
                           OpticalFlowDetector::Direction direction,
                           float speed,
                           uint32_t timestamp,
                           const OpticalFlowDetector& detector);

    /**
     * @brief Update configuration
     */
    void setConfig(const Config& config) { _config = config; }

    /**
     * @brief Get current configuration
     */
    const Config& getConfig() const { return _config; }

    /**
     * @brief Reset gesture state (useful after mode changes)
     */
    void reset();

    /**
     * @brief Convert gesture to string (for debug/logging)
     */
    static const char* gestureToString(GestureType gesture);

private:
    Config _config;

    // Gesture detection state
    OpticalFlowDetector::Direction _lastDirection;
    uint32_t _directionStartTime;
    uint32_t _lastFrameTime;
    bool _gestureCooldown;
    uint32_t _gestureCooldownEnd;
    uint32_t _clashCooldownEnd;
    uint8_t _lastGestureConfidence;
    char _lastEffectRequest[32];

    /**
     * @brief Detect gesture from motion data
     */
    GestureType _detectGesture(uint8_t intensity,
                               OpticalFlowDetector::Direction direction,
                               float speed,
                               uint32_t timestamp,
                               const OpticalFlowDetector& detector);

    /**
     * @brief Calculate perturbation grid from optical flow blocks
     */
    void _calculatePerturbationGrid(const OpticalFlowDetector& detector,
                                    uint8_t perturbationGrid[OpticalFlowDetector::GRID_ROWS][OpticalFlowDetector::GRID_COLS]);

    /**
     * @brief Check if direction is sustained
     */
    bool _isSustainedDirection(OpticalFlowDetector::Direction direction,
                               uint32_t timestamp,
                               uint16_t minDurationMs);
};

#endif // MOTION_PROCESSOR_H
