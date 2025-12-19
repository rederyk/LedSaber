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

        bool perturbationEnabled;
        uint8_t perturbationScale;     // Perturbation multiplier 0-255 (default: 128)

        Config() :
            gesturesEnabled(true),
            gestureThreshold(20),   // Abbassato da 180 a 20 per movimenti meno intensi
            gestureDurationMs(100), // Abbassato da 150ms a 100ms per reattività
            clashDeltaThreshold(15), // Abbassato da 100 a 15 per clash più sensibili
            perturbationEnabled(true),
            perturbationScale(128) {}
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
