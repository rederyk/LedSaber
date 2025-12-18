/**
 * @file StatusLedManager.h
 * @brief Centralized manager for STATUS_LED_PIN (pin 4) and PWM channel 0
 *
 * This class resolves conflicts between multiple systems trying to control
 * the LED on pin 4:
 * - Status LED notifications (BLE connection state)
 * - Camera flash (auto-adjusting based on frame brightness)
 * - OTA mode blinking (update in progress indicator)
 *
 * Priority hierarchy:
 * 1. OTA_BLINK (highest) - safety critical, always visible
 * 2. CAMERA_FLASH - camera needs flash control during capture
 * 3. STATUS_LED (lowest) - normal status notifications
 *
 * Thread safety: All methods should be called from the main loop thread.
 * The singleton pattern ensures a single access point for all PWM writes.
 */

#ifndef STATUS_LED_MANAGER_H
#define STATUS_LED_MANAGER_H

#include <Arduino.h>

class StatusLedManager {
public:
    /**
     * @brief Operating modes with implicit priority (higher = more important)
     */
    enum class Mode {
        STATUS_LED = 0,    // Normal status notifications (lowest priority)
        CAMERA_FLASH = 1,  // Camera flash control (medium priority)
        OTA_BLINK = 2      // OTA update indicator (highest priority)
    };

    /**
     * @brief Sources that can request flash control
     */
    enum class FlashSource {
        AUTO,   // Auto intensity calculated by motion detector
        MANUAL  // Manual override requested via BLE
    };

    /**
     * @brief Get singleton instance
     */
    static StatusLedManager& getInstance();

    /**
     * @brief Initialize PWM channel for LED control
     * @param pin GPIO pin number (default: 4)
     * @param channel PWM channel number (default: 0)
     * @param freq PWM frequency in Hz (default: 5000)
     * @param resolution PWM resolution in bits (default: 8)
     */
    void begin(uint8_t pin = 4, uint8_t channel = 0, uint16_t freq = 5000, uint8_t resolution = 8);

    /**
     * @brief Set current operating mode
     * @param mode New mode to activate
     *
     * Mode changes are only allowed if:
     * - New mode has higher or equal priority than current mode
     * - You explicitly reset to STATUS_LED mode first
     *
     * Example: To switch from CAMERA_FLASH to STATUS_LED:
     *   manager.setMode(Mode::STATUS_LED);
     */
    void setMode(Mode mode);

    /**
     * @brief Get current operating mode
     */
    Mode getMode() const { return _currentMode; }

    /**
     * @brief Check if a specific mode is currently active
     */
    bool isMode(Mode mode) const { return _currentMode == mode; }

    // ========================================================================
    // STATUS LED MODE API (only works when mode = STATUS_LED)
    // ========================================================================

    /**
     * @brief Update status LED state (solid when connected, blink when disconnected)
     * @param connected True if device is connected (BLE/BT)
     * @param enabled True if status LED is enabled via config
     * @param brightness Status LED brightness (0-255)
     *
     * This method handles both solid and blinking behavior:
     * - Connected: solid LED
     * - Disconnected: slow blink (500ms period)
     *
     * Only works when mode = STATUS_LED, otherwise ignored.
     */
    void updateStatusLed(bool connected, bool enabled, uint8_t brightness);

    /**
     * @brief Force status LED to specific state (bypass enable check)
     * @param on True to turn LED on, false to turn off
     * @param brightness Brightness level (0-255)
     *
     * Used for special cases like initialization.
     * Only works when mode = STATUS_LED, otherwise ignored.
     */
    void setStatusLedDirect(bool on, uint8_t brightness);

    // ========================================================================
    // CAMERA FLASH MODE API (only works when mode = CAMERA_FLASH)
    // ========================================================================

    /**
     * @brief Set camera flash intensity
     * @param intensity Flash brightness (0-255)
     *
     * This is typically called with auto-calculated intensity based on
     * frame brightness from the motion detector.
     *
     * Only works when mode = CAMERA_FLASH, otherwise ignored.
     */
    void setCameraFlash(uint8_t intensity);

    /**
     * @brief Get last set camera flash intensity
     */
    uint8_t getCameraFlashIntensity() const { return _cameraFlashIntensity; }

    /**
     * @brief Request camera flash control from a given source
     */
    void requestCameraFlash(FlashSource source, uint8_t intensity);

    /**
     * @brief Release camera flash control for a given source
     */
    void releaseCameraFlash(FlashSource source);

    /**
     * @brief True when any flash source is currently active
     */
    bool isCameraFlashActive() const { return _manualFlashActive || _autoFlashActive; }

    /**
     * @brief Force re-evaluation of flash state (e.g., after OTA ends)
     */
    void refreshCameraFlashState();

    // ========================================================================
    // OTA BLINK MODE API (only works when mode = OTA_BLINK)
    // ========================================================================

    /**
     * @brief Update OTA blink state (fast blink during OTA)
     *
     * Call this repeatedly in the main loop during OTA.
     * Automatically toggles LED state every 100ms for fast blink.
     *
     * Only works when mode = OTA_BLINK, otherwise ignored.
     */
    void updateOtaBlink();

    // ========================================================================
    // LOW-LEVEL API (use with caution)
    // ========================================================================

    /**
     * @brief Write PWM value directly to LED channel
     * @param value PWM duty cycle (0-255)
     *
     * This is the only method that actually writes to hardware.
     * All other methods eventually call this.
     *
     * Public for special cases, but prefer mode-specific methods.
     */
    void writePWM(uint8_t value);

private:
    StatusLedManager() = default;
    ~StatusLedManager() = default;

    // Prevent copying
    StatusLedManager(const StatusLedManager&) = delete;
    StatusLedManager& operator=(const StatusLedManager&) = delete;

    // Hardware configuration
    uint8_t _pin = 4;
    uint8_t _channel = 0;
    uint16_t _freq = 5000;
    uint8_t _resolution = 8;
    bool _initialized = false;

    // Current mode and priority
    Mode _currentMode = Mode::STATUS_LED;

    // Status LED state (Mode::STATUS_LED)
    unsigned long _lastStatusBlink = 0;
    bool _statusLedOn = false;

    // Camera flash state (Mode::CAMERA_FLASH)
    uint8_t _cameraFlashIntensity = 0;
    bool _manualFlashActive = false;
    bool _autoFlashActive = false;
    uint8_t _manualFlashIntensity = 0;
    uint8_t _autoFlashIntensity = 0;

    // OTA blink state (Mode::OTA_BLINK)
    unsigned long _lastOtaBlink = 0;
    bool _otaLedOn = false;
    static constexpr uint8_t OTA_BLINK_BRIGHTNESS = 32;
    static constexpr unsigned long OTA_BLINK_INTERVAL = 100; // ms

    // Last PWM value written (for optimization)
    uint8_t _lastPwmValue = 0;

    // Internal helpers
    void _applyCameraFlashState();
};

#endif // STATUS_LED_MANAGER_H
