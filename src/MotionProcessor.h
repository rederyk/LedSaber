#ifndef MOTION_PROCESSOR_H
#define MOTION_PROCESSOR_H

#include <Arduino.h>
#include "OpticalFlowDetector.h"

/**
 * @brief Processes raw motion data into gestures and perturbations
 *
 * Converts optical flow data into:
 * 1. Classified gestures (IGNITION, RETRACT, CLASH, SWING)
 * 2. Localized perturbation grid for LED effects
 */
class MotionProcessor {
public:
    enum class GestureType : uint8_t {
        NONE = 0,
        IGNITION,      // UP veloce e sostenuto
        RETRACT,       // DOWN veloce e sostenuto
        CLASH,         // Spike improvviso di intensity
        SWING,         // LEFT/RIGHT veloce
        THRUST,        // UP rapido + corto (future)
    };

    struct ProcessedMotion {
        GestureType gesture;
        uint8_t gestureConfidence;     // 0-100
        uint8_t motionIntensity;      // 0-255 (raw motion intensity)
        OpticalFlowDetector::Direction direction;
        float speed;                   // px/frame
        uint32_t timestamp;

        // Localized perturbation data (8x6 grid matching optical flow)
        uint8_t perturbationGrid[6][8];  // Per-block perturbation intensity
    };

    struct Config {
        bool gesturesEnabled;
        uint8_t gestureThreshold;      // Min intensity for gesture (default: 20)
        uint16_t gestureDurationMs;    // Min duration for sustained gestures (default: 100ms)
        uint8_t clashDeltaThreshold;   // Min delta for clash (default: 15)
        uint16_t clashWindowMs;        // Time window for clash delta (default: 350ms)
        uint16_t gestureCooldownMs;    // Cooldown after major gesture (default: 800ms)

        bool perturbationEnabled;
        uint8_t perturbationScale;     // Perturbation multiplier 0-255 (default: 255)

        Config() :
            gesturesEnabled(true),
            gestureThreshold(12),   // Più sensibile (5fps + mano vicina alla camera)
            gestureDurationMs(80),  // Più reattivo
            clashDeltaThreshold(60), // Aumentato da 15 a 60 per ridurre falsi positivi
            clashWindowMs(400),     // ~2.2 frame @ 5.6fps per catturare veri clash
            gestureCooldownMs(800), // Meno "bloccante" del vecchio 2s
            perturbationEnabled(true),
            perturbationScale(255) {}  // 100% = effetto massimo
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
    uint32_t _lastMotionTime;
    uint8_t _lastIntensity;
    OpticalFlowDetector::Direction _lastDirection;
    uint32_t _directionStartTime;
    bool _gestureCooldown;
    uint32_t _gestureCooldownEnd;
    uint8_t _lastGestureConfidence;

    /**
     * @brief Detect gesture from motion data
     */
    GestureType _detectGesture(uint8_t intensity,
                               OpticalFlowDetector::Direction direction,
                               float speed,
                               uint32_t timestamp);

    /**
     * @brief Calculate perturbation grid from optical flow blocks
     */
    void _calculatePerturbationGrid(const OpticalFlowDetector& detector,
                                    uint8_t perturbationGrid[6][8]);

    /**
     * @brief Check if direction is sustained
     */
    bool _isSustainedDirection(OpticalFlowDetector::Direction direction,
                               uint32_t timestamp);
};

#endif // MOTION_PROCESSOR_H
