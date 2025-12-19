#ifndef LED_EFFECT_ENGINE_H
#define LED_EFFECT_ENGINE_H

#include <Arduino.h>
#include <FastLED.h>
#include "BLELedController.h"
#include "MotionProcessor.h"

/**
 * @brief LED Effect Rendering Engine with Motion Integration
 *
 * Handles all LED rendering logic including:
 * - Base effects (solid, rainbow, breathe, etc.)
 * - Gesture-triggered effects (ignition, retract, clash)
 * - Motion perturbations (localized flicker from optical flow)
 */
class LedEffectEngine {
public:
    enum class Mode : uint8_t {
        IDLE,              // Effetto base (solid, breathe, etc)
        IGNITION_ACTIVE,   // Override: ignition in corso
        RETRACT_ACTIVE,    // Override: retraction in corso
        CLASH_ACTIVE,      // Override: clash flash
    };

    /**
     * @brief Constructor
     * @param leds Pointer to FastLED array
     * @param numLeds Total number of physical LEDs
     */
    LedEffectEngine(CRGB* leds, uint16_t numLeds);

    /**
     * @brief Main render function with motion integration
     * @param state Current LED state
     * @param motion Processed motion data (gestures + perturbations)
     */
    void render(const LedState& state, const MotionProcessor::ProcessedMotion* motion);

    /**
     * @brief Get current rendering mode
     */
    Mode getMode() const { return _mode; }

    /**
     * @brief Force reset to IDLE mode
     */
    void resetMode() { _mode = Mode::IDLE; }

    /**
     * @brief Trigger ignition effect (one-shot mode)
     * Used at boot and BLE connection
     */
    void triggerIgnitionOneShot();

    /**
     * @brief Trigger retraction effect (one-shot mode)
     * Used for shutdown/power-off
     */
    void triggerRetractionOneShot();

private:
    CRGB* _leds;
    uint16_t _numLeds;

    Mode _mode;
    uint32_t _modeStartTime;

    // Animation state variables
    uint8_t _hue;
    uint16_t _ignitionProgress;
    unsigned long _lastIgnitionUpdate;
    bool _ignitionOneShot;        // true = ignition runs once
    bool _ignitionCompleted;      // true = cycle completed
    uint16_t _retractionProgress;
    unsigned long _lastRetractionUpdate;
    bool _retractionOneShot;      // true = retraction runs once
    bool _retractionCompleted;    // true = cycle completed
    uint16_t _pulsePosition;
    unsigned long _lastPulseUpdate;
    uint8_t _pulseCharge;          // 0-255 charge level at base
    bool _pulseCharging;           // true = charging, false = traveling
    uint16_t _pulse1Pos;
    uint16_t _pulse2Pos;
    unsigned long _lastDualPulseUpdate;

    // Secondary pulses spawned from main pulse (plasma discharge effect)
    struct SecondaryPulse {
        uint16_t position;         // Current position on the blade
        uint16_t birthTime;        // When this pulse was spawned (millis() & 0xFFFF for overflow handling)
        uint8_t velocityPhase;     // 0-255: tracks position in velocity curve
        bool active;               // Is this pulse alive?
        uint8_t size;              // Size multiplier: 1=normal, 2=fused, 3=mega-fused
        uint8_t brightness;        // Max brightness: 200=normal, 255=fused
    };
    SecondaryPulse _secondaryPulses[5];  // Up to 5 secondary pulses
    unsigned long _lastSecondarySpawn;
    uint8_t _clashBrightness;
    unsigned long _lastClashTrigger;
    bool _clashActive;
    uint8_t _rainbowHue;
    uint8_t _unstableHeat[72];  // Buffer per unstable effect
    uint8_t _breathOverride;    // Breathe effect brightness override

    unsigned long _lastUpdate;

    // ═══════════════════════════════════════════════════════════
    // UTILITY FUNCTIONS
    // ═══════════════════════════════════════════════════════════

    /**
     * @brief Scale color by brightness (preserves contrast)
     */
    static inline CRGB scaleColorByBrightness(CRGB color, uint8_t brightness);

    /**
     * @brief Set LED pair (for folded strip)
     */
    void setLedPair(uint16_t logicalIndex, uint16_t foldPoint, CRGB color);

    /**
     * @brief Get hue from motion direction (for rainbow_effect)
     */
    static uint8_t getHueFromDirection(OpticalFlowDetector::Direction dir);

    // ═══════════════════════════════════════════════════════════
    // BASE EFFECT RENDERERS
    // ═══════════════════════════════════════════════════════════

    void renderSolid(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderRainbow(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderBreathe(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderFlicker(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderUnstable(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderPulse(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderDualPulse(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderRainbowBlade(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderRainbowEffect(const LedState& state, const uint8_t perturbationGrid[6][8], const MotionProcessor::ProcessedMotion* motion);

    // ═══════════════════════════════════════════════════════════
    // GESTURE-TRIGGERED EFFECTS
    // ═══════════════════════════════════════════════════════════

    void renderIgnition(const LedState& state);
    void renderRetraction(const LedState& state);
    void renderClash(const LedState& state);

    // ═══════════════════════════════════════════════════════════
    // MODE MANAGEMENT
    // ═══════════════════════════════════════════════════════════

    bool checkModeTimeout(uint32_t now);
    void handleGestureTriggers(MotionProcessor::GestureType gesture, uint32_t now);
};

#endif // LED_EFFECT_ENGINE_H
