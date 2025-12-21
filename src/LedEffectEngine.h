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
     * @brief Set LedState reference for power control
     * @param state Pointer to LedState (required for powerOn/powerOff)
     */
    void setLedStateRef(LedState* state) { _ledStateRef = state; }

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
     * @brief Power ON: Enable blade, trigger ignition animation, set to normal effects
     * This is the primary function for turning on the saber
     */
    void powerOn();

    /**
     * @brief Power OFF: Trigger retraction animation, disable blade, optionally deep sleep
     * @param deepSleep If true, ESP32 will enter deep sleep after animation completes
     * This is the primary function for turning off the saber
     */
    void powerOff(bool deepSleep = false);

    /**
     * @brief Trigger ignition effect (one-shot mode)
     * Used at boot and BLE connection - LEGACY, prefer powerOn()
     */
    void triggerIgnitionOneShot();

    /**
     * @brief Trigger retraction effect (one-shot mode)
     * Used for shutdown/power-off - LEGACY, prefer powerOff()
     */
    void triggerRetractionOneShot();

private:
    CRGB* _leds;
    uint16_t _numLeds;

    Mode _mode;
    uint32_t _modeStartTime;
    bool _suppressGestureOverrides;

    // Power state management
    bool _deepSleepRequested;     // true = enter deep sleep after retraction completes
    LedState* _ledStateRef;       // Reference to LedState for bladeEnabled control

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
    unsigned long _lastDualPulseSimpleUpdate;

    // Secondary pulses spawned from main pulse (plasma discharge effect)
    struct SecondaryPulse {
        uint16_t position;         // Current position on the blade
        uint16_t birthTime;        // When this pulse was spawned (millis() & 0xFFFF for overflow handling)
        uint8_t velocityPhase;     // 0-255: tracks position in velocity curve
        bool active;               // Is this pulse alive?
        uint8_t size;              // Size multiplier: 1=normal, 2=fused, 3=mega-fused
        uint8_t brightness;        // Max brightness: 200=normal, 255=fused
        uint8_t width;             // Width locked at spawn
    };
    SecondaryPulse _secondaryPulses[5];  // Up to 5 secondary pulses
    unsigned long _lastSecondarySpawn;
    uint8_t _clashBrightness;
    unsigned long _lastClashTrigger;
    bool _clashActive;
    uint8_t _rainbowHue;
    uint8_t _unstableHeat[72];  // Buffer per unstable effect
    uint8_t _breathOverride;    // Breathe effect brightness override

    uint8_t _mainPulseWidth;    // Width of the main pulse (locked per cycle)
    unsigned long _bladeOffTimestamp; // Timestamp when blade turned off (for auto-ignition debounce)
    unsigned long _lastUpdate;

    // ChronoHybrid effect state
    float _visualOffset;          // Offset temporale virtuale (in secondi)
    uint32_t _lastMotionTime;     // Ultimo rilevamento movimento

    // Chrono theme enums
    enum class ChronoHourTheme : uint8_t {
        CLASSIC = 0,        // Marker statici uniformi
        NEON_CYBERPUNK = 1, // Colori vibranti con glow
        PLASMA = 2,         // Blend arcobaleno fluido
        DIGITAL_GLITCH = 3, // Scan lines RGB
        INFERNO = 4,        // Magma/Embers (match for Fire Clock)
        STORM = 5           // Dark clouds/Electric (match for Lightning)
    };

    enum class ChronoSecondTheme : uint8_t {
        CLASSIC = 0,       // Cursore lineare solido
        TIME_SPIRAL = 1,   // Spirale colorata rotante
        FIRE_CLOCK = 2,    // Fuoco che cresce dalla base
        LIGHTNING = 3,     // Fulmini pulsanti
        PARTICLE_FLOW = 4, // Particelle che fluiscono
        QUANTUM_WAVE = 5   // Onda sinusoidale
    };

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
    void renderDualPulseSimple(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderRainbowBlade(const LedState& state, const uint8_t perturbationGrid[6][8]);
    void renderRainbowEffect(const LedState& state, const uint8_t perturbationGrid[6][8], const MotionProcessor::ProcessedMotion* motion);
    void renderChronoHybrid(const LedState& state, const uint8_t perturbationGrid[6][8], const MotionProcessor::ProcessedMotion* motion);

    // ═══════════════════════════════════════════════════════════
    // CHRONO THEME RENDERERS (modular)
    // ═══════════════════════════════════════════════════════════

    // Hour marker themes
    void renderChronoHours_Classic(uint16_t foldPoint, CRGB baseColor);
    void renderChronoHours_Neon(uint16_t foldPoint, CRGB baseColor, uint8_t hours);
    void renderChronoHours_Plasma(uint16_t foldPoint, uint8_t hours, uint8_t minutes);
    void renderChronoHours_Digital(uint16_t foldPoint, CRGB baseColor, uint8_t seconds);
    void renderChronoHours_Inferno(uint16_t foldPoint, CRGB baseColor, uint8_t hours);
    void renderChronoHours_Storm(uint16_t foldPoint, CRGB baseColor, uint8_t hours);

    // Second/minute cursor themes
    void renderChronoSeconds_Classic(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor);
    void renderChronoSeconds_TimeSpiral(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor);
    void renderChronoSeconds_FireClock(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor);
    void renderChronoSeconds_Lightning(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor);
    void renderChronoSeconds_Particle(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor);
    void renderChronoSeconds_Quantum(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor);

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
