# LED Saber - Implementation Plan for Remaining TODO Items

## Project Context
- **Hardware**: ESP32-CAM with WS2812B LED strip (144 LEDs)
- **Framerate**: 5.6 fps (178ms per frame)
- **LED Architecture**: Folded blade - logical index 0-71 (default foldPoint=72) maps to physical LEDs symmetrically
  - Logical 0 → Physical LED 0 + 143
  - Logical 35 → Physical LED 35 + 108
- **Effect Engine**: `/src/LedEffectEngine.cpp` (695 lines)
- **Motion Detection**: OpticalFlowDetector (8x6 grid) + MotionProcessor (gesture classification)

---

## ✅ COMPLETED

### 1. Clash Event Sensitivity Fix
**File**: `/src/MotionProcessor.h:52-53`

**Changes Made**:
```cpp
clashDeltaThreshold(60),  // Increased from 15 to 60
clashWindowMs(400),       // Set to ~2.2 frames @ 5.6fps
```

**Rationale**:
- Original threshold of 15 was too low, causing false triggers from minor hand movements or lighting changes
- Window of 400ms provides adequate coverage for ~2.2 frames at 5.6fps to detect real impact gestures
- Requires much more significant intensity jump (60 vs 15) to register as clash

---

## 🔧 TASK 1: Fix Pulse Effect - Smooth Gradient Charging

### Current Implementation
**File**: `/src/LedEffectEngine.cpp:381-434` (`renderPulse()`)

**Issues**:
1. **Abrupt start**: Pulse appears instantly without buildup
2. **Low contrast**: No darker surrounding area to emphasize the pulse
3. **Not seamless**: Linear position advancement doesn't account for folded index synchronization
4. **No rhythm calculation**: Speed mapping is simple linear, doesn't sync with LED count

**Current Logic**:
```cpp
_pulsePosition = (_pulsePosition + 1) % state.foldPoint;  // Linear advance
pulseWidth = 15;  // Fixed width
// Brightness: peak=255, halo at 200-100
```

### Requirements (from todo.md)

> "pulse è brusco e non seamless, carica il plasma piano prima di lanciarlo dalla base della lama per farlo meno brusco, e calcola il ritmo in base ai led con folded index per la sincronia e seamless"

Translation:
- Pulse is abrupt and not seamless
- Load the plasma slowly before launching from blade base to make it less abrupt
- Calculate rhythm based on LEDs with folded index for synchronization and seamless loop

### Implementation Strategy

#### A. Add Charging Phase
1. Add new state variable: `_pulseCharging` (bool) and `_chargeProgress` (0-255)
2. Before launching pulse, build up energy at base (LED index 0) over ~300-500ms
3. Visual: Gradual brightness increase at base with color temperature shift (blue→white→intense color)

#### B. Improve Contrast
1. Dim surrounding LEDs when pulse is active
2. Create "darkness halo" around pulse wave for depth
3. Suggested: 60% brightness for non-pulse LEDs, 40% for adjacent areas

#### C. Seamless Loop Synchronization
1. Calculate pulse speed to complete exactly in rhythm with foldPoint
2. Formula: `pulseDuration = (foldPoint / pulseSpeed) * frameInterval`
3. Ensure pulse returns to base exactly when charging completes for seamless loop

#### D. Enhanced Motion Ripples
Current motion ripples already exist (lines 415-433), preserve and enhance them

### Pseudo-code
```cpp
void LedEffectEngine::renderPulse(const BLELedController::LedState& state) {
    static bool _pulseCharging = true;
    static uint8_t _chargeProgress = 0;

    // Phase 1: Charging at base
    if (_pulseCharging) {
        _chargeProgress = qadd8(_chargeProgress, 3);  // Slow buildup

        // Gradient charge effect at base (LED 0-10)
        for (uint16_t i = 0; i < 10; i++) {
            uint8_t brightness = map(_chargeProgress, 0, 255, 0, 255 - (i * 20));
            CRGB color = state.color;
            color.nscale8(brightness);
            setLedPair(i, state.foldPoint, color);
        }

        if (_chargeProgress == 255) {
            _pulseCharging = false;
            _pulsePosition = 0;
        }
        return;
    }

    // Phase 2: Pulse wave traveling
    // Calculate seamless speed based on foldPoint
    uint16_t pulsePeriodMs = map(state.speed, 1, 255, 3000, 500);  // Total cycle time
    uint16_t frameMs = 178;  // 5.6fps
    float advancePerFrame = (float)state.foldPoint / (pulsePeriodMs / frameMs);

    _pulsePosition += advancePerFrame;
    if (_pulsePosition >= state.foldPoint) {
        _pulsePosition = 0;
        _pulseCharging = true;
        _chargeProgress = 0;
    }

    // Render with dark background for contrast
    for (uint16_t i = 0; i < state.foldPoint; i++) {
        CRGB color = state.color;

        // Calculate distance from pulse center
        float dist = abs((float)i - _pulsePosition);

        if (dist < 15) {  // Pulse width
            // Bright pulse wave
            uint8_t brightness = 255 - (dist * 15);
            color.nscale8(brightness);
        } else {
            // Dark background with depth
            color.nscale8(60);  // 60% background brightness
            if (dist < 25) {
                // Darker halo around pulse for depth
                color.nscale8(map(dist, 15, 25, 40, 100));
            }
        }

        // Add motion ripples (preserve existing logic)
        // ...

        setLedPair(i, state.foldPoint, color);
    }
}
```

### Files to Modify
- `/src/LedEffectEngine.cpp:381-434` - Main implementation
- `/src/LedEffectEngine.h` - Add state variables if needed

---

## 🔧 TASK 2: Ignition Single-Run Logic

### Current Implementation
**Files**:
- `/src/LedEffectEngine.cpp:545-571` (`renderIgnition()`)
- `/src/LedEffectEngine.cpp:662-668` (`handleGestureTriggers()` - ignition trigger)

**Issues**:
1. **Loops continuously**: `_ignitionProgress` increments forever while in IGNITION_ACTIVE mode
2. **No completion flag**: No way to detect "ignition finished, don't run again"
3. **Timeout-based**: Mode exits after 2000ms timeout, not after completion

**Current Logic**:
```cpp
void renderIgnition() {
    _ignitionProgress++;  // Increments forever
    if (_ignitionProgress > state.foldPoint) {
        _ignitionProgress = 0;  // LOOPS!
    }
    // ... render logic
}
```

### Requirements (from todo.md)

> "ignition deve essere lanciato per un ciclo solo all'avvio e alla connessione ble"

Translation:
- Ignition must be launched for only one cycle at startup and BLE connection

### Implementation Strategy

#### A. Add Completion Tracking
1. Add flag: `_ignitionCompleted` (bool)
2. When `_ignitionProgress >= state.foldPoint`, set flag and return to IDLE mode immediately
3. Reset flag only on explicit triggers (startup, BLE connect, manual gesture)

#### B. Modify renderIgnition()
```cpp
void LedEffectEngine::renderIgnition(const BLELedController::LedState& state) {
    if (_ignitionCompleted) {
        // Ignition already done, switch back to idle effect
        _mode = Mode::IDLE;
        return;
    }

    // Calculate speed-based delay
    uint32_t delayMs = map(state.speed, 1, 255, 100, 1);
    if (millis() - _ignitionLastUpdate < delayMs) {
        return;
    }
    _ignitionLastUpdate = millis();

    // Increment progress
    _ignitionProgress++;

    // Check completion
    if (_ignitionProgress >= state.foldPoint) {
        _ignitionCompleted = true;  // Mark as done
        _mode = Mode::IDLE;          // Return to base effect
        return;
    }

    // Render progressive blade activation
    for (uint16_t i = 0; i <= _ignitionProgress && i < state.foldPoint; i++) {
        CRGB color = state.color;

        // Glow effect on leading edge
        if (i >= _ignitionProgress - 5) {
            uint8_t fade = map(i, _ignitionProgress - 5, _ignitionProgress, 100, 255);
            color.nscale8(fade);
        }

        setLedPair(i, state.foldPoint, color);
    }
}
```

#### C. Reset Completion Flag on Triggers
Modify `handleGestureTriggers()`:
```cpp
case MotionProcessor::GestureType::IGNITION:
    if (_mode == Mode::IDLE) {  // Only trigger if not already in override
        _mode = Mode::IGNITION_ACTIVE;
        _modeStartTime = millis();
        _ignitionProgress = 0;
        _ignitionCompleted = false;  // RESET FLAG
    }
    break;
```

### Files to Modify
- `/src/LedEffectEngine.h` - Add `bool _ignitionCompleted;`
- `/src/LedEffectEngine.cpp:545-571` - Modify renderIgnition()
- `/src/LedEffectEngine.cpp:662-668` - Reset flag in gesture handler

---

## 🔧 TASK 3: Add Ignition Trigger on BLE Connection

### Current Implementation
**File**: `/src/main.cpp:83-95` (`MainServerCallbacks::onConnect()`)

**Current Code**:
```cpp
void onConnect(BLEServer* pServer) override {
    bleController.setConnected(true);
    // ... MTU negotiation
}
```

**Issue**: No ignition trigger on BLE connection

### Requirements (from todo.md)

> "ignition deve essere lanciato per un ciclo solo all'avvio e alla connessione ble"

### Implementation Strategy

#### A. Add Public Trigger Method in LedEffectEngine
```cpp
// In LedEffectEngine.h
public:
    void triggerIgnition();

// In LedEffectEngine.cpp
void LedEffectEngine::triggerIgnition() {
    if (_mode == Mode::IDLE) {
        _mode = Mode::IGNITION_ACTIVE;
        _modeStartTime = millis();
        _ignitionProgress = 0;
        _ignitionCompleted = false;
    }
}
```

#### B. Call from BLE Connection Callback
```cpp
// In main.cpp
extern LedEffectEngine ledEngine;  // Need to make engine accessible

void onConnect(BLEServer* pServer) override {
    bleController.setConnected(true);
    ledEngine.triggerIgnition();  // TRIGGER IGNITION
    // ... rest of code
}
```

#### C. Trigger at Startup
In `setup()` after LED initialization:
```cpp
void setup() {
    // ... existing initialization

    ledEngine.begin(ledStrip, NUM_LEDS);
    ledEngine.triggerIgnition();  // Auto-ignite on power-up

    // ... rest of setup
}
```

### Files to Modify
- `/src/LedEffectEngine.h` - Add public `triggerIgnition()` declaration
- `/src/LedEffectEngine.cpp` - Implement `triggerIgnition()`
- `/src/main.cpp:84-85` - Call in `onConnect()`
- `/src/main.cpp` (setup function) - Call at startup

---

## 🔧 TASK 4: Redesign Rainbow Effect

### Current Implementation
**Files**:
- `/src/LedEffectEngine.cpp:199-237` - `renderRainbow()` (old, still in use)
- `/src/LedEffectEngine.cpp:498-539` - `renderRainbowBlade()` (new experimental version)

**Current "rainbow" effect**:
```cpp
void renderRainbow() {
    fill_rainbow(_leds, _numLeds, _hue, deltaHue);
    _hue += map(state.speed, 1, 255, 1, 15);  // Classic rainbow cycling
}
```

**Issues**:
1. Old `renderRainbow()` still used when effect name is "rainbow"
2. New `renderRainbowBlade()` only triggered by effect name "rainbow_blade"
3. Requirements ask for deprecating old rainbow

### Requirements (from todo.md)

> "rainbow senza blade è ridondante e deprecato crea rainbow effect, fai la lama bianca e perturbala con tutti i colori in base alla direzione, attento devi far risultare più luminosi i colori della base bianca"

Translation:
- Rainbow without blade is redundant and deprecated, create rainbow effect
- Make the blade white and perturb it with all colors based on direction
- Be careful to make the colors appear more luminous than the white base

### Current renderRainbowBlade() Analysis
**File**: `/src/LedEffectEngine.cpp:498-539`

Good elements:
- White base with chromatic perturbation
- Motion-based hue shifts
- Sparkles on high motion

Issues with current implementation:
- Saturation too high (makes colors muddy against white)
- Doesn't explicitly use direction for color mapping
- Colors may not appear more luminous than white base

### Implementation Strategy

#### A. Replace renderRainbow() with New Logic
Keep the function name `renderRainbow()` for backward compatibility, but implement new logic:

```cpp
void LedEffectEngine::renderRainbow(const BLELedController::LedState& state) {
    // White blade base
    CRGB whiteBase = CRGB(255, 255, 255);

    for (uint16_t i = 0; i < state.foldPoint; i++) {
        CRGB color = whiteBase;

        // Get perturbation for this LED
        uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);
        uint8_t perturbation = _perturbationGrid[3][col];  // Use middle row

        if (perturbation > 8) {  // Only perturb if motion detected
            // Map direction to hue
            uint8_t hue = 0;
            switch (_motionDirection) {
                case OpticalFlowDetector::Direction::UP:
                    hue = 0;    // Red
                    break;
                case OpticalFlowDetector::Direction::DOWN:
                    hue = 160;  // Blue
                    break;
                case OpticalFlowDetector::Direction::LEFT:
                    hue = 96;   // Green
                    break;
                case OpticalFlowDetector::Direction::RIGHT:
                    hue = 224;  // Pink
                    break;
                case OpticalFlowDetector::Direction::UP_LEFT:
                    hue = 64;   // Yellow
                    break;
                case OpticalFlowDetector::Direction::UP_RIGHT:
                    hue = 192;  // Purple
                    break;
                case OpticalFlowDetector::Direction::DOWN_LEFT:
                    hue = 128;  // Cyan
                    break;
                case OpticalFlowDetector::Direction::DOWN_RIGHT:
                    hue = 32;   // Orange
                    break;
                default:
                    hue = _hue;  // Fallback to cycling hue
                    break;
            }

            // Create color overlay - ADDITIVE for luminosity
            CRGB overlay = CHSV(hue, 200, perturbation);  // Medium saturation

            // Add to white base (makes colors more luminous)
            color.r = qadd8(color.r, overlay.r);
            color.g = qadd8(color.g, overlay.g);
            color.b = qadd8(color.b, overlay.b);

            // Boost overall brightness in perturbed areas
            if (perturbation > 100) {
                color.nscale8_video(255);  // Keep bright
            }
        }

        setLedPair(i, state.foldPoint, color);
    }

    // Gentle hue rotation for no-motion state
    _hue += map(state.speed, 1, 255, 1, 5);
}
```

#### B. Deprecate renderRainbowBlade()
Option 1: Redirect to renderRainbow()
```cpp
void LedEffectEngine::renderRainbowBlade(const BLELedController::LedState& state) {
    renderRainbow(state);  // Redirect to new implementation
}
```

Option 2: Remove entirely and update effect name mapping

#### C. Key Design Principles
1. **White base (255,255,255)** - Full brightness white blade
2. **Additive color blending** - Use `qadd8()` to ADD colors to white, making them brighter
3. **Direction-based hue** - Each motion direction maps to distinct color
4. **Medium saturation (200)** - Prevents muddy colors against white
5. **Brightness boost** - Perturbed areas should be BRIGHTER than base white

### Files to Modify
- `/src/LedEffectEngine.cpp:199-237` - Replace renderRainbow() implementation
- `/src/LedEffectEngine.cpp:498-539` - Optionally deprecate renderRainbowBlade()
- `/src/LedEffectEngine.h` - May need to add `_motionDirection` storage

---

## 📋 Additional Context

### Key Architecture Components

**Folded LED Mapping**:
```cpp
void setLedPair(uint16_t logicalIndex, uint16_t foldPoint, CRGB color) {
    uint16_t led1 = logicalIndex;
    uint16_t led2 = (_numLeds - 1) - logicalIndex;
    _leds[led1] = color;
    _leds[led2] = color;
}
```

**Perturbation Grid Access**:
```cpp
uint8_t col = map(i, 0, state.foldPoint - 1, 0, 7);  // LED index → grid column
uint8_t perturbation = _perturbationGrid[row][col];  // 0-255
```

**Mode Timeout System**:
```cpp
// In render() function (lines 639-653)
if (_mode != Mode::IDLE) {
    if (millis() - _modeStartTime >= timeoutMs) {
        _mode = Mode::IDLE;
    }
}
```

### Testing Checklist
- [ ] Pulse effect charges smoothly from base
- [ ] Pulse has high contrast with darker surroundings
- [ ] Pulse loops seamlessly without visual jumps
- [ ] Ignition runs exactly once on power-up
- [ ] Ignition runs exactly once on BLE connection
- [ ] Ignition triggered by gesture still works
- [ ] Rainbow shows white blade with colored perturbations
- [ ] Rainbow colors appear more luminous than white base
- [ ] Clash events only trigger on strong, deliberate impacts
- [ ] All effects work with folded LED architecture

### Build and Upload
```bash
cd /home/reder/Documenti/PlatformIO/Projects/ledSaber
pio run -t upload
pio device monitor
```

---

## Notes
- All timing calculations assume 5.6 fps (178ms per frame)
- Preserve existing motion ripple logic in effects
- Maintain backward compatibility with BLE control protocol
- Test with actual optical flow at runtime (simulator has limitations)
