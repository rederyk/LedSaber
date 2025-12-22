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

        // Localized perturbation data (6x6 grid matching optical flow)
        uint8_t perturbationGrid[OpticalFlowDetector::GRID_ROWS][OpticalFlowDetector::GRID_COLS];
    };

    struct Config {
        bool gesturesEnabled;
        uint8_t gestureThreshold;      // Min intensity for gesture (default: 20)
        uint16_t gestureDurationMs;    // Min duration for sustained gestures (default: 100ms)
        uint8_t clashDeltaThreshold;   // Min delta for clash (default: 15)
        uint16_t clashWindowMs;        // Time window for clash delta (default: 350ms)
        uint16_t gestureCooldownMs;    // Cooldown after major gesture (default: 800ms)
        uint16_t clashCooldownMs;      // Specific cooldown for clash spam control

        // 4-axis gesture detection (no diagonals)
        // Interpreta i vettori per-blocco e produce una "direzione dominante" robusta nel tempo.
        uint8_t axisMinBlocks;         // Min blocchi coerenti su un asse
        uint8_t axisMinTotalWeight;    // Min "energia" totale per considerare swipe (0-255, scala interna)
        uint8_t axisMinDominance;      // Dominanza best vs second (x/10, es: 16 = 1.6x)
        uint8_t axisEmaAlpha;          // 0-255 (255 = no smoothing)
        uint8_t axisOnFraction;        // % 0-100: quota asse per aggancio
        uint8_t axisOffFraction;       // % 0-100: quota asse per rilascio (isteresi)
        uint16_t swingDurationMs;      // Durata minima per LEFT/RIGHT (più corta)

        bool perturbationEnabled;
        uint8_t perturbationScale;     // Perturbation multiplier 0-255 (default: 255)

        Config() :
            gesturesEnabled(true),
            gestureThreshold(11),   // Più sensibile (5fps + mano vicina alla camera)
            gestureDurationMs(80),  // Più reattivo
            clashDeltaThreshold(60), // Aumentato da 15 a 60 per ridurre falsi positivi
            clashWindowMs(600),     // ~2.2 frame @ 5.6fps per catturare veri clash
            gestureCooldownMs(800), // Meno "bloccante" del vecchio 2s
            clashCooldownMs(5000),   // Clash dedicato per evitare raffiche
            axisMinBlocks(2),
            axisMinTotalWeight(18),
            axisMinDominance(16),
            axisEmaAlpha(110),
            axisOnFraction(62),
            axisOffFraction(50),
            swingDurationMs(55),
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

    enum class Axis : uint8_t { NONE = 0, UP, DOWN, LEFT, RIGHT };
    enum class Rotation : uint8_t { ROT_0 = 0, ROT_90_CW = 1, ROT_180 = 2, ROT_90_CCW = 3 };

    // Gesture detection state
    uint32_t _lastMotionTime;
    uint8_t _lastIntensity;
    OpticalFlowDetector::Direction _lastDirection;
    uint32_t _directionStartTime;
    bool _gestureCooldown;
    uint32_t _gestureCooldownEnd;
    uint32_t _clashCooldownEnd;
    uint8_t _lastGestureConfidence;

    // 4-axis temporal state
    Rotation _deducedRotation;
    bool _deducedRotationValid;
    Axis _trackedAxis;
    uint32_t _axisStartTime;
    uint16_t _axisEmaWeightUp;
    uint16_t _axisEmaWeightDown;
    uint16_t _axisEmaWeightLeft;
    uint16_t _axisEmaWeightRight;
    uint16_t _axisEmaWeightTotal;

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
