/**
 * @file StatusLedManager.cpp
 * @brief Implementation of centralized LED manager for pin 4
 */

#include "StatusLedManager.h"
#include <driver/ledc.h>

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

StatusLedManager& StatusLedManager::getInstance() {
    static StatusLedManager instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void StatusLedManager::begin(uint8_t pin, uint8_t channel, uint16_t freq, uint8_t resolution) {
    _pin = pin;
    _channel = channel;
    _freq = freq;
    _resolution = resolution;

    // Configure GPIO pin
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);

    // Configure PWM channel
    ledcSetup(_channel, _freq, _resolution);
    ledcAttachPin(_pin, _channel);

    // Initialize to OFF
    writePWM(0);

    _initialized = true;

    Serial.printf("[LED MGR] Initialized: pin=%u channel=%u freq=%u resolution=%u\n",
                  _pin, _channel, _freq, _resolution);
}

// ============================================================================
// MODE MANAGEMENT
// ============================================================================

void StatusLedManager::setMode(Mode mode) {
    if (_currentMode == mode) {
        return; // Already in this mode
    }

    int currentPriority = static_cast<int>(_currentMode);
    int requestedPriority = static_cast<int>(mode);

    // Enforce priority hierarchy (except when releasing to STATUS_LED)
    if (mode != Mode::STATUS_LED && requestedPriority < currentPriority) {
        Serial.printf("[LED MGR] Mode change blocked (%d -> %d)\n",
                      currentPriority, requestedPriority);
        return;
    }

    Mode previousMode = _currentMode;
    _currentMode = mode;

    // Reset state when changing modes
    switch (mode) {
        case Mode::STATUS_LED:
            _statusLedOn = false;
            _lastStatusBlink = 0;
            // Don't write PWM here - let updateStatusLed() handle it
            break;

        case Mode::CAMERA_FLASH:
            _cameraFlashIntensity = 0;
            // Don't write PWM here - let setCameraFlash() handle it
            break;

        case Mode::OTA_BLINK:
            _otaLedOn = false;
            _lastOtaBlink = 0;
            // Don't write PWM here - let updateOtaBlink() handle it
            break;
    }

    Serial.printf("[LED MGR] Mode changed: %d -> %d\n",
                  static_cast<int>(previousMode),
                  static_cast<int>(mode));
}

// ============================================================================
// STATUS LED MODE (Priority 0)
// ============================================================================

void StatusLedManager::updateStatusLed(bool connected, bool enabled, uint8_t brightness) {
    // Only work in STATUS_LED mode
    if (_currentMode != Mode::STATUS_LED) {
        return;
    }

    // If disabled or zero brightness, turn off
    if (!enabled || brightness == 0) {
        writePWM(0);
        _statusLedOn = false;
        return;
    }

    unsigned long now = millis();

    // Connected: solid LED
    if (connected) {
        if (!_statusLedOn) {
            writePWM(brightness);
            _statusLedOn = true;
        }
        return;
    }

    // Disconnected: slow blink (500ms interval)
    if (now - _lastStatusBlink >= 500) {
        _statusLedOn = !_statusLedOn;
        writePWM(_statusLedOn ? brightness : 0);
        _lastStatusBlink = now;
    }
}

void StatusLedManager::setStatusLedDirect(bool on, uint8_t brightness) {
    // Only work in STATUS_LED mode
    if (_currentMode != Mode::STATUS_LED) {
        return;
    }

    _statusLedOn = on;
    writePWM(on ? brightness : 0);
}

// ============================================================================
// CAMERA FLASH MODE (Priority 1)
// ============================================================================

void StatusLedManager::setCameraFlash(uint8_t intensity) {
    // Only work in CAMERA_FLASH mode
    if (_currentMode != Mode::CAMERA_FLASH) {
        return;
    }

    // Optimization: only write if value changed
    if (intensity != _cameraFlashIntensity) {
        _cameraFlashIntensity = intensity;
        writePWM(intensity);
    }
}

void StatusLedManager::requestCameraFlash(FlashSource source, uint8_t intensity) {
    bool changed = false;

    if (source == FlashSource::MANUAL) {
        if (!_manualFlashActive || _manualFlashIntensity != intensity) {
            _manualFlashActive = true;
            _manualFlashIntensity = intensity;
            changed = true;
        }
    } else {
        if (!_autoFlashActive || _autoFlashIntensity != intensity) {
            _autoFlashActive = true;
            _autoFlashIntensity = intensity;
            changed = true;
        }
    }

    if (changed) {
        _applyCameraFlashState();
    }
}

void StatusLedManager::releaseCameraFlash(FlashSource source) {
    bool changed = false;

    if (source == FlashSource::MANUAL) {
        if (_manualFlashActive) {
            _manualFlashActive = false;
            changed = true;
        }
    } else {
        if (_autoFlashActive) {
            _autoFlashActive = false;
            changed = true;
        }
    }

    if (changed) {
        _applyCameraFlashState();
    }
}

void StatusLedManager::refreshCameraFlashState() {
    _applyCameraFlashState();
}

void StatusLedManager::_applyCameraFlashState() {
    if (_currentMode == Mode::OTA_BLINK) {
        return; // OTA has priority over any flash request
    }

    if (_manualFlashActive) {
        setMode(Mode::CAMERA_FLASH);
        setCameraFlash(_manualFlashIntensity);
    } else if (_autoFlashActive) {
        setMode(Mode::CAMERA_FLASH);
        setCameraFlash(_autoFlashIntensity);
    } else if (_currentMode == Mode::CAMERA_FLASH) {
        setMode(Mode::STATUS_LED);
    }
}

// ============================================================================
// OTA BLINK MODE (Priority 2 - Highest)
// ============================================================================

void StatusLedManager::updateOtaBlink() {
    // Only work in OTA_BLINK mode
    if (_currentMode != Mode::OTA_BLINK) {
        return;
    }

    unsigned long now = millis();

    // Fast blink: toggle every 100ms
    if (now - _lastOtaBlink >= OTA_BLINK_INTERVAL) {
        _otaLedOn = !_otaLedOn;
        writePWM(_otaLedOn ? OTA_BLINK_BRIGHTNESS : 0);
        _lastOtaBlink = now;
    }
}

// ============================================================================
// LOW-LEVEL PWM CONTROL
// ============================================================================

void StatusLedManager::writePWM(uint8_t value) {
    if (!_initialized) {
        Serial.println("[LED MGR] ERROR: Not initialized, call begin() first!");
        return;
    }

    // Optimization: skip redundant writes
    if (value == _lastPwmValue) {
        return;
    }

    ledcWrite(_channel, value);
    _lastPwmValue = value;

    // Debug logging (can be disabled for production)
    // Serial.printf("[LED MGR] PWM write: mode=%d value=%u\n",
    //               static_cast<int>(_currentMode), value);
}
