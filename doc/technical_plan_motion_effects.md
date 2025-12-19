# Technical Implementation Plan: Motion-Responsive LED Effects

## System Architecture Analysis

### Current Components
- **OpticalFlowDetector** ([src/OpticalFlowDetector.h](../src/OpticalFlowDetector.h)): Calculates 6x8 grid (48 blocks) of motion vectors using SAD algorithm
- **MotionProcessor** ([src/MotionProcessor.cpp](../src/MotionProcessor.cpp)): Converts block vectors to perturbation grid (6x8, values 0-255)
- **LedEffectEngine** ([src/LedEffectEngine.cpp](../src/LedEffectEngine.cpp)): Renders effects with perturbation grid integration

### Current Perturbation System
Located in [MotionProcessor.cpp:233-278](../src/MotionProcessor.cpp#L233-L278):
```cpp
// Perturbation calculation:
magnitude = sqrt(dx² + dy²)  // Motion vector magnitude
normalized = magnitude / 3.0  // Sensitivity scaling
confidenceBoost = confidence * 0.5 + 0.5  // Min 50% confidence
normalized = pow(normalized, 0.7)  // Dramatic curve
value = normalized * 255
```

### LED Strip Mapping
- Folded strip: logical index maps to two physical LEDs
- `setLedPair(logicalIndex, foldPoint, color)` at [LedEffectEngine.cpp:132-142](../src/LedEffectEngine.cpp#L132-L142)
- Grid-to-LED mapping: `col = map(ledIndex, 0, foldPoint-1, 0, 7)`

---

## Requirements Analysis

### 1. Flicker Effect (Kylo Ren Style)
**Current state** ([LedEffectEngine.cpp:270-309](../src/LedEffectEngine.cpp#L270-L309)):
- Already has aggressive perturbation boost
- Motion adds up to 200% of base flicker
- Random spikes on high motion

**Required change**: "Più contrasto con spegni di più i LED"
- Increase blackout intensity in perturbed areas
- Higher contrast between stable/unstable regions

**Implementation**:
- Increase fadeout range: `200 → 220` (darker blacks)
- Boost perturbation multiplier: `200 → 255`
- Add deeper random spikes: `random8(40,100) → random8(60,140)`

---

### 2. Rainbow Blade Effect
**Current state** ([LedEffectEngine.cpp:488-529](../src/LedEffectEngine.cpp#L488-L529)):
- Motion creates chromatic shimmer (hue shift + saturation reduction)
- "Sparkle" effect on high motion (desaturates to white)

**Required change**: "Deve diventare bianco nei punti dove ci passano le dita"
- Already implemented! (line 514-519)
- Threshold might be too high (avgPerturbation > 12)

**Implementation**:
- Lower white threshold: `12 → 8` (more sensitive)
- Increase saturation reduction for white effect
- Boost sparkle probability: `60 → 100`

---

### 3. Pulse Effect
**Current state** ([LedEffectEngine.cpp:376-424](../src/LedEffectEngine.cpp#L376-L424)):
- Motion creates brightness ripples
- Base brightness: 150, pulse peak: 255

**Required changes**:
- "Velocizzare l'impulso" → increase speed
- "Aumentare luminosità e contrasto" → brighter pulse, darker base
- "Spazio più scuro intorno al pulse per dare profondità" → depth gradient

**Implementation**:
- Speed: Modify `pulseSpeed = map(state.speed, 1, 255, 80, 1)` → `map(1, 255, 50, 1)`
- Contrast: `baseBrightness 150 → 120`, keep peak at 255
- Depth: Add darkness halo outside pulseWidth
- Motion ripple: Increase boost from `+80 → +100`

---

### 4. Unstable Effect
**Current state** ([LedEffectEngine.cpp:311-374](../src/LedEffectEngine.cpp#L311-L374)):
- Heat buffer for plasma eruptions
- Motion triggers eruptions at specific columns

**Required changes**:
- "Mappato alla griglia così due dita = due buchi" → Already mapped!
- "Aumentare contrasto al contrario: luminosità INTORNO alle perturbazioni" → Invert logic

**Implementation**:
- **INVERTED LOGIC**: High perturbation → BRIGHT halo around it, dark center
- Change eruption behavior:
  - Center (perturbation point): darker (`brightness 180-255 → 100-150`)
  - Neighbors: brighter (`+80,+150 → +150,+255`)
- Increase contrast range: `180+heat/3 → 120+heat*0.6`

---

### 5. Grid Mapping for All Effects
**Current state**: Already implemented for most effects

**Effects using grid**:
- ✅ solid ([LedEffectEngine.cpp:148-188](../src/LedEffectEngine.cpp#L148-L188))
- ✅ rainbow ([LedEffectEngine.cpp:190-228](../src/LedEffectEngine.cpp#L190-L228))
- ✅ breathe ([LedEffectEngine.cpp:230-268](../src/LedEffectEngine.cpp#L230-L268))
- ✅ flicker ([LedEffectEngine.cpp:270-309](../src/LedEffectEngine.cpp#L270-L309))
- ✅ unstable ([LedEffectEngine.cpp:311-374](../src/LedEffectEngine.cpp#L311-L374))
- ✅ pulse ([LedEffectEngine.cpp:376-424](../src/LedEffectEngine.cpp#L376-L424))
- ✅ dual_pulse ([LedEffectEngine.cpp:426-486](../src/LedEffectEngine.cpp#L426-L486))
- ✅ rainbow_blade ([LedEffectEngine.cpp:488-529](../src/LedEffectEngine.cpp#L488-L529))

**Implementation**: No changes needed - all effects already grid-mapped!

---

### 6. Solid Effect with Noise Attenuation
**Current state** ([LedEffectEngine.cpp:148-188](../src/LedEffectEngine.cpp#L148-L188)):
- Motion creates breathing fade effect
- Simple threshold: `maxPerturbation > 10`

**Required change**: "Aumentare di poco la luminosità nei punti perturbati ma se troppo rumore si attenua invece"
- Low perturbation (10-60): slight brightness increase
- High perturbation (>60): attenuation (opposite)

**Implementation**:
```cpp
if (maxPerturbation > 10 && maxPerturbation < 60) {
    // Gentle brightness boost
    uint8_t boost = scale8(maxPerturbation, 40);
    perturbedColor.fadeToBlackBy(-boost);  // Brighten
} else if (maxPerturbation >= 60) {
    // Attenuation on high noise
    uint8_t fade = scale8(maxPerturbation - 60, 120);
    perturbedColor.fadeToBlackBy(fade);
}
```

---

## Implementation Tasks

### Task 1: Flicker Contrast Enhancement
**File**: [src/LedEffectEngine.cpp:270-309](../src/LedEffectEngine.cpp#L270-L309)
**Changes**:
- Line 289: `scale8(perturbSum, 200)` → `scale8(perturbSum, 255)`
- Line 294: `random8(40, 100)` → `random8(60, 140)`
- Line 299: `min(noise, 200)` → `min(noise, 220)`

---

### Task 2: Rainbow Blade White Sensitivity
**File**: [src/LedEffectEngine.cpp:488-529](../src/LedEffectEngine.cpp#L488-L529)
**Changes**:
- Line 508: `if (avgPerturbation > 12)` → `if (avgPerturbation > 8)`
- Line 514: `saturation = 255 - scale8(avgPerturbation, 120)` → `scale8(avgPerturbation, 160)`
- Line 517: `random8() < 60` → `random8() < 100`

---

### Task 3: Pulse Speed, Contrast, Depth
**File**: [src/LedEffectEngine.cpp:376-424](../src/LedEffectEngine.cpp#L376-L424)
**Changes**:
- Line 378: `map(state.speed, 1, 255, 80, 1)` → `map(state.speed, 1, 255, 50, 1)`
- Line 394-396: Modify brightness logic:
  ```cpp
  uint8_t brightness;
  if (distance < pulseWidth) {
      brightness = map(distance, 0, pulseWidth, 255, 200);  // Peak brighter
  } else if (distance < pulseWidth + 8) {
      // Depth halo: darker zone around pulse
      brightness = map(distance, pulseWidth, pulseWidth + 8, 200, 100);
  } else {
      brightness = 100;  // Darker base
  }
  ```
- Line 412: `rippleBoost = scale8(avgPerturbation, 80)` → `scale8(avgPerturbation, 100)`

---

### Task 4: Unstable Inverted Contrast
**File**: [src/LedEffectEngine.cpp:311-374](../src/LedEffectEngine.cpp#L311-L374)
**Major refactor** (lines 333-350):
- Invert eruption logic: perturbation center gets LESS heat, neighbors get MORE
- Change rendering (lines 361-371): Higher heat = brighter (inverted)

**New logic**:
```cpp
// Motion creates BRIGHT halos (inverted plasma)
if (maxPerturbation > 30) {
    uint16_t pos = map(col, 0, 7, 0, maxIndex - 1);

    // CENTER: reduce heat (dark hole)
    _unstableHeat[pos] = qsub8(_unstableHeat[pos], random8(80, 150));

    // NEIGHBORS: add major heat (bright halo)
    if (pos > 0) {
        _unstableHeat[pos - 1] = qadd8(_unstableHeat[pos - 1], random8(150, 255));
    }
    if (pos < maxIndex - 1) {
        _unstableHeat[pos + 1] = qadd8(_unstableHeat[pos + 1], random8(150, 255));
    }
}

// Render: high heat = BRIGHT
uint8_t brightness = 120 + scale8(_unstableHeat[i], 135);  // 120-255 range
```

---

### Task 5: Solid Dual-Mode Perturbation
**File**: [src/LedEffectEngine.cpp:148-188](../src/LedEffectEngine.cpp#L148-L188)
**Replace** lines 169-177:
```cpp
if (maxPerturbation > 10) {
    CRGB perturbedColor = baseColor;

    if (maxPerturbation < 60) {
        // LOW MOTION: gentle brightness boost
        uint8_t boost = scale8(maxPerturbation, 40);
        perturbedColor.nscale8(255 - boost);  // Inverted fade = brighten
        perturbedColor |= scaleColorByBrightness(baseColor, boost);
    } else {
        // HIGH MOTION (noise): attenuation
        uint8_t fade = scale8(maxPerturbation - 60, 120);
        perturbedColor.fadeToBlackBy(fade);
    }

    perturbedColor = scaleColorByBrightness(perturbedColor, safeBrightness);
    setLedPair(i, state.foldPoint, perturbedColor);
} else {
    // Stable area
    CRGB scaledColor = scaleColorByBrightness(baseColor, safeBrightness);
    setLedPair(i, state.foldPoint, scaledColor);
}
```

---

## Testing Strategy

### Per-Effect Testing
1. **Flicker**: Move hand quickly → verify deeper blacks at perturbation points
2. **Rainbow Blade**: Pass fingers → verify white/desaturated areas appear
3. **Pulse**: Verify faster movement + darker base + bright peak + dark halo
4. **Unstable**: Two fingers → two bright halos with dark centers
5. **Solid**: Slow motion (boost), fast/noisy motion (attenuation)

### Grid Mapping Verification
- Use debug serial output from [MotionProcessor.cpp:268-272](../src/MotionProcessor.cpp#L268-L272)
- Verify perturbation values align with finger positions

### Integration Testing
- Test all effects with different speeds, distances, multi-finger gestures
- Verify no conflicts with gesture detection (IGNITION/RETRACT/CLASH)

---

## Files Affected
1. [src/LedEffectEngine.cpp](../src/LedEffectEngine.cpp) - Main implementation (5 effect functions)
2. [src/LedEffectEngine.h](../src/LedEffectEngine.h) - No header changes needed

---

## Performance Considerations
- All changes are CPU-local (no memory allocation)
- Grid mapping already optimized (8 columns, sampled rows)
- No impact on optical flow computation (30-50ms @ 5fps)

---

## Summary of Changes by Effect

| Effect | Current Behavior | New Behavior | Lines Changed |
|--------|-----------------|--------------|---------------|
| **flicker** | Motion adds flicker | **MORE contrast, deeper blacks** | 289, 294, 299 |
| **rainbow_blade** | Motion creates shimmer | **More sensitive white effect** | 508, 514, 517 |
| **pulse** | Ripple on motion | **Faster, brighter peak, dark halo** | 378, 394-396, 412 |
| **unstable** | Plasma eruptions | **INVERTED: bright halos, dark centers** | 333-350, 361-371 |
| **solid** | Breathing fade | **Dual mode: boost low, fade high** | 169-177 |
| **grid mapping** | Already implemented | ✅ No changes | - |

---

## Next Steps
1. Implement Task 1 (flicker)
2. Implement Task 2 (rainbow_blade)
3. Implement Task 3 (pulse)
4. Implement Task 4 (unstable - major refactor)
5. Implement Task 5 (solid dual-mode)
6. Test each effect individually
7. Integration test with multi-finger gestures
