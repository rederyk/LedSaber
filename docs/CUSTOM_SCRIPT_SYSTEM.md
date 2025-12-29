# Sistema Script Personalizzati - LedSaber

## 📋 Indice
- [Panoramica](#panoramica)
- [Analisi Compilazione](#analisi-compilazione)
- [Architettura Sistema](#architettura-sistema)
- [Linguaggio DSL](#linguaggio-dsl)
- [Bytecode Format](#bytecode-format)
- [API Disponibili](#api-disponibili)
- [Roadmap Implementazione](#roadmap-implementazione)
- [Risorse e Stime](#risorse-e-stime)

---

## 🎯 Panoramica

Sistema per creare ed eseguire effetti LED personalizzati tramite script inviabili via BLE dall'app mobile.

### Obiettivi
- ✅ Permettere agli utenti di creare effetti custom senza ricompilare firmware
- ✅ Accesso sicuro a LED, Motion, Time API
- ✅ Performance: 60 FPS anche con script complessi
- ✅ Memoria: <15KB flash, <500 bytes RAM runtime
- ✅ Sicurezza: sandbox completo, timeout, bounds checking

### Approccio Scelto: **DSL Bytecode**
- **DSL** (Domain-Specific Language): linguaggio custom per LED
- **Bytecode**: compilato in istruzioni binarie compatte
- **Compilazione**: su app Flutter (non ESP32)
- **Esecuzione**: interprete veloce su ESP32

---

## 🔍 Analisi: Dove Compilare?

### Opzione A: Compilazione su ESP32
**PRO:**
- ✅ App più semplice (invia solo testo)
- ✅ Nessuna dipendenza versioni compilatore
- ✅ Debugging più facile (log seriale)

**CONTRO:**
- ❌ Occupa ~8-12KB flash per compilatore
- ❌ Usa ~5-8KB RAM durante compilazione
- ❌ Parsing stringe CPU-intensive
- ❌ Errori di sintassi solo dopo invio BLE
- ❌ Compilazione blocca rendering LED (100-300ms)

### Opzione B: Compilazione su App Flutter ⭐ **SCELTA**
**PRO:**
- ✅ ESP32 più leggero (solo interprete ~5KB)
- ✅ Zero overhead RAM su ESP32
- ✅ Validazione errori **prima** dell'invio BLE
- ✅ Syntax highlighting e autocomplete nell'editor
- ✅ Test bytecode offline senza device connesso
- ✅ Versioning compilatore indipendente da firmware

**CONTRO:**
- ❌ App più complessa
- ❌ Necessità di sincronizzare versioni bytecode
- ❌ Testing più complesso (emulatore bytecode in Dart)

### Decisione Finale: **COMPILAZIONE SU APP**

**Motivazioni:**
1. **Memoria ESP32 limitata** - Camera + Motion usano già molta RAM
2. **UX superiore** - Errori mostrati subito, niente attese BLE
3. **Futuro-proof** - Aggiornare compilatore senza flash firmware
4. **Sicurezza** - Validazione bytecode prima dell'esecuzione

---

## 🏗️ Architettura Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                    APP FLUTTER (Dart)                        │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  Script Editor  │→ │  Compiler    │→ │  Validator    │  │
│  │  (Syntax HL)    │  │  (Text→AST)  │  │  (Type Check) │  │
│  └─────────────────┘  └──────────────┘  └───────────────┘  │
│           │                    │                   │         │
│           ↓                    ↓                   ↓         │
│  ┌─────────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  Preview        │  │  Bytecode    │→ │  BLE Service  │  │
│  │  (Emulator)     │  │  Generator   │  │  (Upload)     │  │
│  └─────────────────┘  └──────────────┘  └───────────────┘  │
└─────────────────────────────────────────────┬───────────────┘
                                              │ BLE
                                              │ (CHAR_CUSTOM_SCRIPT_UUID)
┌─────────────────────────────────────────────┴───────────────┐
│                    ESP32 FIRMWARE (C++)                      │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  BLE Callback   │→ │  LittleFS    │→ │  ConfigMgr    │  │
│  │  (Receive)      │  │  Storage     │  │  (Load/Save)  │  │
│  └─────────────────┘  └──────────────┘  └───────────────┘  │
│           │                    │                   │         │
│           ↓                    ↓                   ↓         │
│  ┌─────────────────────────────────────────────────────┐   │
│  │           LedEffectEngine::renderCustomScript()     │   │
│  └─────────────────────────────────────────────────────┘   │
│                             │                               │
│                             ↓                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              ScriptEngine (Interpreter)             │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  │   │
│  │  │  Stack   │  │  Vars    │  │  Opcode Executor │  │   │
│  │  │ [32 int] │  │ [16 int] │  │  (Switch-case)   │  │   │
│  │  └──────────┘  └──────────┘  └──────────────────┘  │   │
│  │                                                      │   │
│  │  API Bindings:                                       │   │
│  │  ┌────┐  ┌────────┐  ┌──────┐  ┌──────┐           │   │
│  │  │LED │  │ Motion │  │ Time │  │ Math │           │   │
│  │  └────┘  └────────┘  └──────┘  └──────┘           │   │
│  └─────────────────────────────────────────────────────┘   │
│                             │                               │
│                             ↓                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │           FastLED (leds[] array write)              │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 📝 Linguaggio DSL

### Sintassi Esempio

```dsl
SCRIPT "plasma_motion_reactive"
  // Variabili locali
  speed = 30
  base_brightness = 180

  // Loop sui LED
  FOR i = 0 TO foldPoint
    // Calcola hue con onda temporale
    hue = (i * 4 + time.millis() / speed) % 256

    // Brightness base
    brightness = base_brightness

    // Reattività al movimento
    IF motion.detected THEN
      // Aumenta brightness in base all'intensità
      boost = motion.intensity / 3
      brightness = brightness + boost

      // Cambia colore in base alla direzione
      IF motion.direction == DIR_UP THEN
        hue = hue + 30
      ELSE IF motion.direction == DIR_DOWN THEN
        hue = hue - 30
      END
    END

    // Clash gesture override
    IF motion.gesture == GESTURE_CLASH THEN
      // Flash bianco
      led.setRGB(i, 255, 255, 255)
    ELSE
      // Colore normale
      led.setHSV(i, hue, 255, brightness)
    END
  END
END
```

### Tipi di Dati
- **int16_t**: Tutti i numeri sono interi signed 16-bit (-32768 a 32767)
- **bool**: Risultato di comparazioni (true = 1, false = 0)

### Operatori
- **Aritmetici**: `+`, `-`, `*`, `/`, `%` (modulo)
- **Comparazione**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logici**: `AND`, `OR`, `NOT`

### Costanti Predefinite
```dsl
// Direzioni motion (0-7)
DIR_NONE = 0
DIR_UP = 1
DIR_UP_RIGHT = 2
DIR_RIGHT = 3
DIR_DOWN_RIGHT = 4
DIR_DOWN = 5
DIR_DOWN_LEFT = 6
DIR_LEFT = 7
DIR_UP_LEFT = 8

// Gesture types
GESTURE_NONE = 0
GESTURE_CLASH = 1
GESTURE_SWING = 2
GESTURE_STAB = 3
GESTURE_SPIN = 4
```

### Controllo di Flusso
```dsl
// Condizionale
IF condition THEN
  // code
ELSE IF condition THEN
  // code
ELSE
  // code
END

// Loop
FOR variable = start TO end
  // code
END

// Loop con step
FOR variable = start TO end STEP increment
  // code
END
```

---

## 🔢 Bytecode Format

### Opcodes (1 byte ciascuno)

#### Stack Operations (0x01-0x0F)
```cpp
OP_LOAD_CONST   = 0x01  // Push constant: LOAD_CONST <value>
OP_LOAD_VAR     = 0x02  // Push variable: LOAD_VAR <var_index>
OP_STORE_VAR    = 0x03  // Pop to variable: STORE_VAR <var_index>
OP_DUP          = 0x04  // Duplicate top of stack
OP_POP          = 0x05  // Discard top of stack
```

#### Math Operations (0x10-0x1F)
```cpp
OP_ADD          = 0x10  // Pop 2, push sum
OP_SUB          = 0x11  // Pop 2, push difference
OP_MUL          = 0x12  // Pop 2, push product
OP_DIV          = 0x13  // Pop 2, push quotient
OP_MOD          = 0x14  // Pop 2, push remainder
OP_NEG          = 0x15  // Negate top of stack
```

#### Comparison (0x20-0x2F)
```cpp
OP_CMP_EQ       = 0x20  // Pop 2, push (a == b)
OP_CMP_NE       = 0x21  // Pop 2, push (a != b)
OP_CMP_LT       = 0x22  // Pop 2, push (a < b)
OP_CMP_LE       = 0x23  // Pop 2, push (a <= b)
OP_CMP_GT       = 0x24  // Pop 2, push (a > b)
OP_CMP_GE       = 0x25  // Pop 2, push (a >= b)
```

#### Logical (0x28-0x2F)
```cpp
OP_AND          = 0x28  // Pop 2, push (a AND b)
OP_OR           = 0x29  // Pop 2, push (a OR b)
OP_NOT          = 0x2A  // Pop 1, push (NOT a)
```

#### Control Flow (0x30-0x3F)
```cpp
OP_JUMP         = 0x30  // Unconditional jump: JUMP <offset>
OP_JUMP_IF_TRUE = 0x31  // Pop, jump if != 0: JUMP_IF_TRUE <offset>
OP_JUMP_IF_FALSE= 0x32  // Pop, jump if == 0: JUMP_IF_FALSE <offset>
OP_LABEL        = 0x33  // No-op marker for jumps
```

#### LED API (0x40-0x4F)
```cpp
OP_LED_SET_RGB  = 0x40  // led.setRGB(index, r, g, b)
                        // Stack: [index, r, g, b] → []

OP_LED_SET_HSV  = 0x41  // led.setHSV(index, h, s, v)
                        // Stack: [index, h, s, v] → []

OP_LED_SET_PAIR = 0x42  // led.setPair(logicalIndex, r, g, b)
                        // Stack: [logicalIndex, r, g, b] → []
                        // Rispetta foldPoint per strip piegato

OP_LED_FILL     = 0x43  // led.fill(start, count, r, g, b)
                        // Stack: [start, count, r, g, b] → []

OP_LED_CLEAR    = 0x44  // led.clear()
                        // Stack: [] → []
```

#### Motion API (0x50-0x5F)
```cpp
OP_MOTION_DETECTED  = 0x50  // Push motion.detected (0 or 1)
OP_MOTION_INTENSITY = 0x51  // Push motion.intensity (0-255)
OP_MOTION_DIRECTION = 0x52  // Push motion.direction (0-8)
OP_MOTION_SPEED     = 0x53  // Push motion.speed (0-255)
OP_MOTION_GESTURE   = 0x54  // Push motion.gesture (0-4)
```

#### Time API (0x60-0x6F)
```cpp
OP_TIME_MILLIS  = 0x60  // Push time.millis() (truncated to 16-bit)
OP_TIME_HOURS   = 0x61  // Push current hour (0-23)
OP_TIME_MINUTES = 0x62  // Push current minute (0-59)
OP_TIME_SECONDS = 0x63  // Push current second (0-59)
```

**NOTA API TIME**: L'API time è collegata all'effetto ChronoHybrid esistente.
- Usa `ledState.epochBase` e `ledState.millisAtSync` per calcolare tempo corrente
- L'app sincronizza il tempo tramite `CHAR_TIME_SYNC_UUID` (già implementato)
- Se il tempo non è sincronizzato (epochBase == 0), ritorna 0 per hours/minutes/seconds

#### Math API (0x70-0x7F)
```cpp
OP_MATH_SIN8    = 0x70  // Push sin8(angle) - FastLED sin8()
                        // Stack: [angle_0_255] → [result_0_255]

OP_MATH_COS8    = 0x71  // Push cos8(angle) - FastLED cos8()
                        // Stack: [angle_0_255] → [result_0_255]

OP_MATH_RANDOM8 = 0x72  // Push random8(max)
                        // Stack: [max] → [random_0_to_max]

OP_MATH_MAP     = 0x73  // Push map(value, in_min, in_max, out_min, out_max)
                        // Stack: [value, in_min, in_max, out_min, out_max] → [result]

OP_MATH_BLEND   = 0x74  // Push blend(a, b, amount)
                        // Stack: [a, b, amount_0_255] → [result]

OP_MATH_SCALE8  = 0x75  // Push scale8(value, scale)
                        // Stack: [value, scale] → [value * scale / 256]
```

#### System (0xF0-0xFF)
```cpp
OP_NOP          = 0xF0  // No operation
OP_HALT         = 0xFF  // End execution
```

### Esempio Bytecode Compilato

Script:
```dsl
FOR i = 0 TO 72
  hue = (i * 4) % 256
  led.setHSV(i, hue, 255, 200)
END
```

Bytecode (hex):
```
01 00       // LOAD_CONST 0 (i = 0)
03 00       // STORE_VAR 0 (store i)

33 01       // LABEL 1 (loop_start)
02 00       // LOAD_VAR 0 (load i)
01 48       // LOAD_CONST 72
24          // CMP_GT (i > 72?)
31 0F       // JUMP_IF_TRUE +15 (to end)

02 00       // LOAD_VAR 0 (load i)
01 04       // LOAD_CONST 4
12          // MUL
01 00 01    // LOAD_CONST 256
14          // MOD
03 01       // STORE_VAR 1 (store hue)

02 00       // LOAD_VAR 0 (i)
02 01       // LOAD_VAR 1 (hue)
01 FF       // LOAD_CONST 255 (saturation)
01 C8       // LOAD_CONST 200 (brightness)
41          // OP_LED_SET_HSV

02 00       // LOAD_VAR 0 (i)
01 01       // LOAD_CONST 1
10          // ADD
03 00       // STORE_VAR 0 (i++)
30 01       // JUMP LABEL 1

FF          // HALT
```

**Dimensione**: 37 bytes per un loop completo con calcolo e rendering LED

---

## 🔌 API Disponibili

### LED API

#### `led.setRGB(index, r, g, b)`
Imposta colore LED singolo (RGB)
```dsl
led.setRGB(10, 255, 0, 100)  // LED 10 → rosa
```

#### `led.setHSV(index, h, s, v)`
Imposta colore LED singolo (HSV)
```dsl
led.setHSV(10, 128, 255, 200)  // LED 10 → cyan brillante
```

#### `led.setPair(logicalIndex, r, g, b)`
Imposta coppia di LED simmetrica (rispetta foldPoint)
```dsl
// Con foldPoint=72:
led.setPair(5, 255, 0, 0)   // Imposta LED[5] e LED[138] a rosso
```

#### `led.fill(start, count, r, g, b)`
Riempie range di LED
```dsl
led.fill(0, 20, 0, 0, 255)  // Primi 20 LED → blu
```

#### `led.clear()`
Spegne tutti i LED
```dsl
led.clear()  // Tutti LED → nero
```

---

### Motion API

#### `motion.detected` (bool, read-only)
Motion attivo (true/false)
```dsl
IF motion.detected THEN
  // Movimento rilevato
END
```

#### `motion.intensity` (int, 0-255, read-only)
Intensità movimento
```dsl
brightness = 100 + (motion.intensity / 2)
```

#### `motion.direction` (int, 0-8, read-only)
Direzione movimento (usa costanti `DIR_*`)
```dsl
IF motion.direction == DIR_UP THEN
  hue = 0  // Rosso per movimento verso l'alto
ELSE IF motion.direction == DIR_DOWN THEN
  hue = 128  // Cyan per movimento verso il basso
END
```

Valori:
- `DIR_NONE = 0`
- `DIR_UP = 1`
- `DIR_UP_RIGHT = 2`
- `DIR_RIGHT = 3`
- `DIR_DOWN_RIGHT = 4`
- `DIR_DOWN = 5`
- `DIR_DOWN_LEFT = 6`
- `DIR_LEFT = 7`
- `DIR_UP_LEFT = 8`

#### `motion.speed` (int, 0-255, read-only)
Velocità movimento
```dsl
pulse_speed = 100 - (motion.speed / 3)
```

#### `motion.gesture` (int, 0-4, read-only)
Tipo di gesture rilevata (usa costanti `GESTURE_*`)
```dsl
IF motion.gesture == GESTURE_CLASH THEN
  // Flash bianco per clash
  led.fill(0, 72, 255, 255, 255)
END
```

Valori:
- `GESTURE_NONE = 0`
- `GESTURE_CLASH = 1`
- `GESTURE_SWING = 2`
- `GESTURE_STAB = 3`
- `GESTURE_SPIN = 4`

---

### Time API

**IMPORTANTE**: L'API Time è collegata al sistema di sincronizzazione dell'effetto ChronoHybrid.

#### Come Funziona
1. L'app sincronizza il tempo tramite BLE caratteristica `CHAR_TIME_SYNC_UUID`
2. Invia JSON: `{"epoch": 1234567890}` (Unix timestamp)
3. ESP32 salva `ledState.epochBase` e `ledState.millisAtSync`
4. Script calcola tempo corrente: `currentEpoch = epochBase + (millis() - millisAtSync) / 1000`

#### `time.millis()` (int, read-only)
Millisecondi da boot (troncato a 16-bit)
```dsl
// Animazione temporale
offset = time.millis() / 50
hue = (i * 4 + offset) % 256
```

**NOTA**: Overflow ogni ~65 secondi (16-bit). Usare modulo per animazioni cicliche.

#### `time.hours()` (int, 0-23, read-only)
Ora corrente (formato 24h)
```dsl
// Cambia colore in base all'ora
hour = time.hours()
IF hour >= 6 AND hour < 12 THEN
  base_hue = 32   // Giallo mattino
ELSE IF hour >= 12 AND hour < 18 THEN
  base_hue = 128  // Cyan pomeriggio
ELSE
  base_hue = 200  // Viola notte
END
```

**NOTA**: Ritorna 0 se il tempo non è sincronizzato (`epochBase == 0`)

#### `time.minutes()` (int, 0-59, read-only)
Minuti correnti
```dsl
// Indicatore minuti sulla blade
minute_pos = (time.minutes() * 72) / 60  // Map 0-59 → 0-72
led.setRGB(minute_pos, 255, 255, 255)
```

#### `time.seconds()` (int, 0-59, read-only)
Secondi correnti
```dsl
// Cursore secondi pulsante
second = time.seconds()
pulse = math.sin8((second * 4) % 256)
brightness = 100 + pulse
```

#### Sincronizzazione Time da App
```dart
// Flutter app - invia tempo corrente
Future<void> syncTime() async {
  final now = DateTime.now();
  final epoch = now.millisecondsSinceEpoch ~/ 1000;

  final json = jsonEncode({"epoch": epoch});
  await timeSyncCharacteristic.write(utf8.encode(json));
}
```

---

### Math API

#### `math.sin8(angle)` (int, read-only)
Seno veloce 8-bit (FastLED `sin8()`)
```dsl
// Onda sinusoidale
brightness = 128 + math.sin8((i * 10 + time.millis() / 20) % 256)
```

Input: `angle` 0-255 (0-255 = 0°-360°)
Output: 0-255

#### `math.cos8(angle)` (int, read-only)
Coseno veloce 8-bit (FastLED `cos8()`)
```dsl
// Movimento circolare
x = math.cos8(angle)
y = math.sin8(angle)
```

#### `math.random8(max)` (int, read-only)
Numero casuale 0 a max (FastLED `random8()`)
```dsl
// Flicker casuale
noise = math.random8(50)
brightness = 200 - noise
```

#### `math.map(value, in_min, in_max, out_min, out_max)` (int, read-only)
Mappa valore da un range a un altro
```dsl
// Map motion intensity (0-255) → brightness boost (0-100)
boost = math.map(motion.intensity, 0, 255, 0, 100)
brightness = base_brightness + boost
```

#### `math.blend(a, b, amount)` (int, read-only)
Blend lineare tra due valori
```dsl
// Blend rosso → blu in base a posizione
color_a = 0    // Rosso
color_b = 160  // Blu
hue = math.blend(color_a, color_b, (i * 255) / 72)
```

`amount`: 0-255 (0 = tutto `a`, 255 = tutto `b`)

#### `math.scale8(value, scale)` (int, read-only)
Scala valore per fattore 8-bit (FastLED `scale8()`)
```dsl
// Dimm brightness del 50%
dimmed = math.scale8(255, 128)  // 255 * 128 / 256 = 127
```

`scale`: 0-255 (0 = 0%, 255 = 100%)

---

## 📋 Roadmap Implementazione

### **MILESTONE 1: Foundation (3-4 giorni)**

#### **Giorno 1: Design & Prototyping**
- [ ] Finalizzare sintassi DSL
- [ ] Progettare set completo opcodes (40-45 istruzioni)
- [ ] Definire API disponibili per script
- [ ] Creare documento specifica bytecode format
- [ ] Scrivere 5-6 esempi script dimostrativi

**Deliverable:**
- `docs/SCRIPT_SPEC.md` - Specifica linguaggio
- `docs/BYTECODE_FORMAT.md` - Formato bytecode
- `examples/scripts/` - Esempi script testuali

---

#### **Giorno 2-3: Interprete ESP32**
**File da creare:**
```
src/ScriptEngine.h
src/ScriptEngine.cpp
include/ScriptOpcodes.h
```

**Tasks:**
- [ ] Implementare struct `ScriptEngine` con stack/vars
- [ ] Implementare loop esecuzione bytecode
- [ ] Implementare opcodes base (LOAD, STORE, MATH, JUMP)
- [ ] Implementare API LED (setRGB, setHSV, setPair, fill, clear)
- [ ] Implementare API Motion (detected, intensity, direction, speed, gesture)
- [ ] Implementare API Time (millis, hours, minutes, seconds)
  - [ ] Collegare a `ledState.epochBase` e `ledState.millisAtSync`
  - [ ] Calcolare tempo corrente da sync
  - [ ] Gestire caso non-sincronizzato (epochBase == 0)
- [ ] Implementare API Math (sin8, cos8, random8, map, blend, scale8)
- [ ] Sistema sicurezza (timeout 50ms, bounds checking)
- [ ] Testing unitario interprete

**Codice chiave:**
```cpp
// src/ScriptEngine.h
class ScriptEngine {
public:
  bool execute(const uint8_t* bytecode, size_t length,
               CRGB* leds, uint16_t numLeds,
               const LedState& state,
               const MotionProcessor::ProcessedMotion* motion);

  void setTimeProvider(uint32_t epochBase, uint32_t millisAtSync);

private:
  static constexpr uint8_t MAX_STACK = 32;
  static constexpr uint8_t MAX_VARS = 16;
  static constexpr uint32_t MAX_EXEC_TIME_MS = 50;

  int16_t stack[MAX_STACK];
  int16_t vars[MAX_VARS];
  uint8_t sp;  // Stack pointer

  // Time sync data (from ChronoHybrid)
  uint32_t _epochBase;
  uint32_t _millisAtSync;

  // API implementation methods
  void executeOpLedSetRGB();
  void executeOpLedSetHSV();
  void executeOpLedSetPair();
  void executeOpMotionIntensity();
  void executeOpTimeHours();
  void executeOpTimeMinutes();
  void executeOpTimeSeconds();
  void executeOpMathSin8();
  // ... altri metodi
};
```

**Time API Implementation:**
```cpp
void ScriptEngine::executeOpTimeHours() {
  if (sp >= MAX_STACK) return; // Overflow check

  if (_epochBase == 0) {
    // Tempo non sincronizzato
    stack[sp++] = 0;
    return;
  }

  // Calcola epoch corrente
  uint32_t elapsedSeconds = (millis() - _millisAtSync) / 1000;
  uint32_t currentEpoch = _epochBase + elapsedSeconds;

  // Estrai ore (0-23)
  time_t rawTime = (time_t)currentEpoch;
  struct tm* timeInfo = gmtime(&rawTime);
  stack[sp++] = timeInfo->tm_hour;
}
```

---

#### **Giorno 4: Integrazione BLE & Storage**
**File da modificare:**
```
include/BLELedController.h
src/BLELedController.cpp
include/ConfigManager.h
src/ConfigManager.cpp
src/LedEffectEngine.h
src/LedEffectEngine.cpp
```

**Tasks:**
- [ ] Aggiungere `CHAR_CUSTOM_SCRIPT_UUID` a BLE
- [ ] Implementare callback ricezione script
- [ ] Storage script in LittleFS (4 slot: `/scripts/slot0.bin` ... `slot3.bin`)
- [ ] Metadati script (nome 32 char, versione, CRC32)
- [ ] API BLE per list/load/delete/activate script
- [ ] Integrazione in LedEffectEngine (`renderCustomScript()`)
- [ ] Aggiungere effetto "custom_0" ... "custom_3" alla lista effetti

**Codice chiave:**
```cpp
// include/BLELedController.h
#define CHAR_CUSTOM_SCRIPT_UUID "e9f0a1bb-7d09-3c4d-de1f-901bcdef345"

struct CustomScriptMetadata {
  char name[32];          // Nome script
  uint16_t bytecodeLength;
  uint8_t version;        // Versione bytecode format
  uint32_t crc32;         // Checksum
};

// src/LedEffectEngine.cpp
void LedEffectEngine::renderCustomScript(const LedState& state,
                                          const MotionProcessor::ProcessedMotion* motion) {
  if (!_customScriptBytecode || _customScriptLength == 0) return;

  ScriptEngine engine;

  // Passa time sync data (da ChronoHybrid)
  engine.setTimeProvider(state.epochBase, state.millisAtSync);

  // Esegui script
  bool success = engine.execute(
    _customScriptBytecode, _customScriptLength,
    _leds, _numLeds, state, motion
  );

  if (!success) {
    Serial.println("[SCRIPT] Execution failed, reverting to solid");
    fill_solid(_leds, _numLeds, CRGB(state.r, state.g, state.b));
  }

  // FastLED show gestito dal caller
}
```

**BLE Protocol:**
```json
// Upload script (WRITE to CHAR_CUSTOM_SCRIPT_UUID)
{
  "slot": 0,              // 0-3
  "name": "plasma_wave",
  "bytecode": [0x01, 0x00, 0x03, ...],  // Array bytecode
  "crc32": 3735928559
}

// List scripts (READ from CHAR_CUSTOM_SCRIPT_UUID)
{
  "scripts": [
    {"slot": 0, "name": "plasma_wave", "size": 128, "active": true},
    {"slot": 1, "name": "fire_sim", "size": 256, "active": false},
    {"slot": 2, "name": "", "size": 0, "active": false},
    {"slot": 3, "name": "", "size": 0, "active": false}
  ]
}

// Activate script (WRITE to CHAR_LED_EFFECT_UUID)
{
  "mode": "custom_0"  // Attiva script slot 0
}
```

---

### **MILESTONE 2: Compilatore App (4-5 giorni)**

#### **Giorno 5-6: Compilatore Dart**
**File da creare:**
```
AppMobile/flutter_led_saber/lib/script/
  - script_compiler.dart       (Main compiler)
  - script_tokenizer.dart      (Lexer: text → tokens)
  - script_parser.dart         (Parser: tokens → AST)
  - script_ast.dart            (AST node definitions)
  - bytecode_generator.dart    (AST → bytecode)
  - opcodes.dart               (Opcode constants)
  - script_validator.dart      (Type checking, bounds)
  - compile_error.dart         (Error types)
```

**Tasks:**
- [ ] Implementare tokenizer (keywords, numbers, identifiers, operators)
- [ ] Implementare parser (build AST from tokens)
- [ ] Implementare bytecode generator (AST → opcodes)
- [ ] Sistema validazione (type checking, bounds, API calls)
- [ ] Gestione errori con line/column info
- [ ] Testing con esempi script

**Codice chiave:**
```dart
// script_compiler.dart
class ScriptCompiler {
  static const int BYTECODE_VERSION = 1;

  CompileResult compile(String sourceCode) {
    try {
      // 1. Tokenize
      final tokens = ScriptTokenizer().tokenize(sourceCode);

      // 2. Parse
      final ast = ScriptParser().parse(tokens);

      // 3. Validate
      final errors = ScriptValidator().validate(ast);
      if (errors.isNotEmpty) {
        return CompileResult.error(errors);
      }

      // 4. Generate bytecode
      final bytecode = BytecodeGenerator().generate(ast);

      // 5. Add version header + HALT at end
      final fullBytecode = Uint8List.fromList([
        BYTECODE_VERSION,
        ...bytecode,
        OpCode.OP_HALT
      ]);

      return CompileResult.success(fullBytecode);
    } catch (e, stackTrace) {
      return CompileResult.error([
        CompileError(
          message: e.toString(),
          line: 0,
          column: 0,
          stackTrace: stackTrace.toString()
        )
      ]);
    }
  }
}

class CompileResult {
  final bool success;
  final Uint8List? bytecode;
  final List<CompileError>? errors;

  CompileResult.success(this.bytecode)
    : success = true, errors = null;
  CompileResult.error(this.errors)
    : success = false, bytecode = null;
}

class CompileError {
  final String message;
  final int line;
  final int column;
  final String? stackTrace;

  CompileError({
    required this.message,
    required this.line,
    required this.column,
    this.stackTrace
  });

  @override
  String toString() => 'Line $line:$column - $message';
}
```

**Tokenizer:**
```dart
enum TokenType {
  // Keywords
  SCRIPT, END, FOR, TO, STEP, IF, THEN, ELSE, AND, OR, NOT,

  // Literals
  NUMBER, IDENTIFIER,

  // Operators
  ASSIGN, PLUS, MINUS, STAR, SLASH, PERCENT,
  EQ, NE, LT, LE, GT, GE,

  // Delimiters
  LPAREN, RPAREN, COMMA, DOT,

  // Special
  NEWLINE, EOF
}

class Token {
  final TokenType type;
  final String lexeme;
  final int line;
  final int column;
  final dynamic value;  // For NUMBER tokens

  Token(this.type, this.lexeme, this.line, this.column, [this.value]);
}
```

---

#### **Giorno 7: Testing Compilatore + Emulatore**
**File da creare:**
```
test/script/
  - compiler_test.dart
  - bytecode_emulator.dart    (Emula interprete ESP32 in Dart)
  - integration_test.dart

AppMobile/flutter_led_saber/lib/script/
  - script_emulator.dart      (Per preview in app)
```

**Tasks:**
- [ ] Test unitari tokenizer
- [ ] Test unitari parser
- [ ] Test unitari bytecode generator
- [ ] Emulatore bytecode in Dart (simula ScriptEngine C++)
- [ ] Test integrazione end-to-end (script → bytecode → esecuzione simulata)
- [ ] Test edge cases (overflow, division by zero, infinite loops)
- [ ] Benchmarking dimensione bytecode

**Emulatore Bytecode:**
```dart
class BytecodeEmulator {
  List<int> stack = [];
  List<int> vars = List.filled(16, 0);
  List<Color> virtualLeds = List.filled(72, Color(0xFF000000));

  EmulatorResult execute(Uint8List bytecode, {
    int? motionIntensity,
    int? motionDirection,
    int? currentHour,
  }) {
    int pc = 0;
    int iterations = 0;
    const maxIterations = 10000;

    while (pc < bytecode.length && iterations < maxIterations) {
      iterations++;

      final opcode = bytecode[pc++];

      switch (opcode) {
        case OpCode.OP_LOAD_CONST:
          stack.add(bytecode[pc++]);
          break;

        case OpCode.OP_LED_SET_HSV:
          final v = stack.removeLast();
          final s = stack.removeLast();
          final h = stack.removeLast();
          final index = stack.removeLast();

          if (index >= 0 && index < virtualLeds.length) {
            virtualLeds[index] = HSVColor.fromAHSV(1.0, h.toDouble(), s/255, v/255).toColor();
          }
          break;

        case OpCode.OP_HALT:
          return EmulatorResult.success(virtualLeds, iterations);

        // ... altri opcodes
      }
    }

    if (iterations >= maxIterations) {
      return EmulatorResult.error("Infinite loop detected");
    }

    return EmulatorResult.success(virtualLeds, iterations);
  }
}
```

---

### **MILESTONE 3: UI App (3-4 giorni)**

#### **Giorno 8-9: Editor Script**
**File da creare:**
```
AppMobile/flutter_led_saber/lib/screens/
  - script_editor_screen.dart
  - script_library_screen.dart

AppMobile/flutter_led_saber/lib/widgets/script/
  - code_editor_widget.dart
  - syntax_highlighter.dart
  - error_display_widget.dart
  - api_autocomplete_widget.dart
  - bytecode_viewer_widget.dart
  - led_preview_widget.dart
```

**Tasks:**
- [ ] UI editor codice con syntax highlighting
- [ ] Autocomplete API (led., motion., time., math.)
- [ ] Display errori in tempo reale con line highlighting
- [ ] Preview bytecode compilato (hex dump + opcode names)
- [ ] Test script in emulatore (simulazione 72 LED virtuali)
- [ ] Salva/carica script locali (shared_preferences)
- [ ] Upload script a ESP32 (gestione 4 slot)
- [ ] Activate/deactivate script

**UI Features:**
```
┌────────────────────────────────────────────────┐
│ ← Back    Plasma Wave          Save  Upload ▶ │
├────────────────────────────────────────────────┤
│ 1  SCRIPT "plasma_wave"              ┌────────┐
│ 2    speed = 30                      │ APIs   │
│ 3    FOR i = 0 TO 72                 ├────────┤
│ 4      hue = (i * 4 + time.millis()) │led.    │
│ 5      led.setHSV(i, hue, 255, 200)  │motion. │
│ 6    END                              │time.   │
│ 7  END                                │math.   │
│                                       └────────┘
├────────────────────────────────────────────────┤
│ ✓ Compiled successfully (128 bytes)            │
│                                                 │
│ ┌──────────────────────────────────┐           │
│ │   LED Preview (Emulator)         │           │
│ │ [████████████████████████████]   │           │
│ │                                  │           │
│ │ ▶ Play  ⏸ Pause  🔄 Reset       │           │
│ └──────────────────────────────────┘           │
└────────────────────────────────────────────────┘
```

**Syntax Highlighting:**
```dart
class ScriptSyntaxHighlighter extends SyntaxHighlighter {
  @override
  TextSpan format(String source) {
    final tokens = ScriptTokenizer().tokenize(source);
    final spans = <TextSpan>[];

    for (final token in tokens) {
      Color color;
      FontWeight weight = FontWeight.normal;

      switch (token.type) {
        case TokenType.SCRIPT:
        case TokenType.FOR:
        case TokenType.IF:
        case TokenType.END:
          color = Colors.blue;
          weight = FontWeight.bold;
          break;
        case TokenType.NUMBER:
          color = Colors.green;
          break;
        case TokenType.IDENTIFIER:
          color = token.lexeme.startsWith('led.') ||
                 token.lexeme.startsWith('motion.') ||
                 token.lexeme.startsWith('time.') ||
                 token.lexeme.startsWith('math.')
              ? Colors.purple
              : Colors.white;
          break;
        default:
          color = Colors.white;
      }

      spans.add(TextSpan(
        text: token.lexeme,
        style: TextStyle(color: color, fontWeight: weight)
      ));
    }

    return TextSpan(children: spans);
  }
}
```

---

#### **Giorno 10: Libreria Script Predefiniti**
**File da creare:**
```
AppMobile/flutter_led_saber/assets/scripts/
  - plasma_wave.dsl
  - motion_reactive.dsl
  - rainbow_chase.dsl
  - fire_simulation.dsl
  - lightning_storm.dsl
  - breathing_glow.dsl
  - clash_flash.dsl
  - time_indicator.dsl
  - gradient_blend.dsl
  - particle_system.dsl

AppMobile/flutter_led_saber/lib/screens/
  - script_gallery_screen.dart

AppMobile/flutter_led_saber/lib/models/
  - predefined_script.dart
```

**Tasks:**
- [ ] Creare 10-12 script esempio pre-compilati
- [ ] UI galleria script (card con nome, preview GIF/animazione, descrizione)
- [ ] One-tap install script su device
- [ ] Share script (export/import JSON con base64 bytecode)
- [ ] QR code per condivisione rapida

**Script Esempio:**

**1. plasma_wave.dsl**
```dsl
SCRIPT "Plasma Wave"
  speed = 30
  FOR i = 0 TO foldPoint
    hue = (i * 4 + time.millis() / speed) % 256
    led.setHSV(i, hue, 255, 200)
  END
END
```

**2. motion_reactive.dsl**
```dsl
SCRIPT "Motion Reactive"
  base_hue = 160
  FOR i = 0 TO foldPoint
    brightness = 150

    IF motion.detected THEN
      brightness = brightness + (motion.intensity / 2)

      // Shift hue in base a direzione
      IF motion.direction == DIR_UP THEN
        hue = base_hue + 40
      ELSE IF motion.direction == DIR_DOWN THEN
        hue = base_hue - 40
      ELSE
        hue = base_hue
      END
    ELSE
      hue = base_hue
    END

    led.setHSV(i, hue, 255, brightness)
  END
END
```

**3. time_indicator.dsl** (Usa API Time collegata a ChronoHybrid)
```dsl
SCRIPT "Time Indicator"
  // Base glow
  FOR i = 0 TO foldPoint
    led.setHSV(i, 200, 255, 50)
  END

  // Marker ore (ogni 6 LED = 1 ora)
  hour = time.hours()
  hour_pos = (hour * foldPoint) / 24
  led.setRGB(hour_pos, 255, 0, 0)

  // Cursore minuti
  minute = time.minutes()
  minute_pos = (minute * foldPoint) / 60
  pulse = math.sin8((time.seconds() * 4) % 256)
  brightness = 100 + pulse
  led.setRGB(minute_pos, 0, 255, brightness)
END
```

**4. clash_flash.dsl**
```dsl
SCRIPT "Clash Flash"
  IF motion.gesture == GESTURE_CLASH THEN
    // White flash
    led.fill(0, foldPoint, 255, 255, 255)
  ELSE
    // Colore base
    FOR i = 0 TO foldPoint
      led.setRGB(i, 0, 100, 255)
    END
  END
END
```

---

### **MILESTONE 4: Testing & Release (2-3 giorni)**

#### **Giorno 11: Integration Testing**
**Tasks:**
- [ ] Test full pipeline (Editor → Compile → BLE → ESP32 → LED fisici)
- [ ] Test tutti gli script esempio su hardware reale
- [ ] Test performance (FPS con script attivo + camera + motion)
- [ ] Test stabilità (run 30min con script complessi)
- [ ] Test memoria (heap ESP32 con script attivi)
- [ ] Test sicurezza (script malevoli):
  - [ ] Infinite loops (timeout check)
  - [ ] Buffer overflow (bounds check LED index)
  - [ ] Stack overflow (stack depth limit)
  - [ ] Division by zero (trap in DIV/MOD opcodes)

**Test Cases:**
```cpp
// test_scripts/security_tests/
- infinite_loop.dsl        // FOR i = 0 TO 1000000 (deve timeout)
- buffer_overflow.dsl      // led.setRGB(999, ...) (deve ignorare)
- stack_overflow.dsl       // Nested operations 100 livelli (deve fallire)
- division_by_zero.dsl     // x = 10 / 0 (deve gestire)
```

---

#### **Giorno 12: Documentazione & Polish**
**Tasks:**
- [ ] Tutorial in-app (primo script guidato step-by-step)
- [ ] Documentazione API completa (markdown + HTML)
- [ ] Video demo (YouTube: 3-5min tutorial)
- [ ] README aggiornato con sezione "Custom Scripts"
- [ ] Changelog dettagliato
- [ ] Commenti codice (Doxygen style)

**Deliverable:**
- `docs/SCRIPT_TUTORIAL.md` - Tutorial principianti
- `docs/API_REFERENCE.md` - Reference completa
- `docs/SCRIPT_EXAMPLES.md` - 20+ snippet
- `CHANGELOG.md` - Versione con script system

**Tutorial In-App:**
```
Step 1: Welcome
  → "Create your first custom LED effect!"

Step 2: Simple Rainbow
  → Template pre-filled:
     FOR i = 0 TO 72
       led.setHSV(i, (i * 4) % 256, 255, 200)
     END
  → "Press Compile to test"

Step 3: Add Motion
  → Add IF motion.detected block
  → Preview in emulator

Step 4: Upload
  → Connect to device
  → Upload to slot 0
  → Activate effect

Step 5: Success!
  → Congrats screen
  → Link to gallery for more examples
```

---

#### **Giorno 13: Release**
- [ ] Build firmware finale (versione con script engine)
- [ ] Build app release (Android APK + iOS)
- [ ] Testing beta con 5-10 utenti
- [ ] Raccolta feedback e fix bug critici
- [ ] Release pubblica su GitHub/Play Store

**Release Checklist:**
- [ ] Firmware version bump (es. v2.0.0-script)
- [ ] App version bump (es. v2.0.0)
- [ ] Tag Git: `v2.0.0-custom-scripts`
- [ ] Release notes completi
- [ ] Video demo caricato
- [ ] Post social media (se applicabile)

---

## 📊 Risorse e Stime

### **Tempo Totale:** 13-15 giorni lavorativi (~3 settimane)

### **Breakdown per Componente:**
| Componente | Giorni | Complessità | Priorità |
|------------|--------|-------------|----------|
| Interprete ESP32 | 3 | 🔴 Alta | 🔥 Critica |
| Compilatore Dart | 3 | 🔴 Alta | 🔥 Critica |
| UI Editor App | 3 | 🟡 Media | 🟢 Alta |
| Testing & Debug | 2 | 🟡 Media | 🟢 Alta |
| Documentazione | 1 | 🟢 Bassa | 🟡 Media |
| Script Esempio | 1 | 🟢 Bassa | 🟡 Media |

### **Memoria Flash ESP32:**
- **Interprete**: ~5-8 KB
- **Opcodes definitions**: ~2 KB
- **Storage 4 script**: ~4 KB (1KB/slot max)
- **BLE characteristics**: ~1 KB
- **TOTALE**: ~12-15 KB

**Spazio disponibile**: Firmware attuale ~1.2MB, partizione 1.3MB → **100KB liberi** ✅

### **Memoria RAM ESP32:**
- **Stack interprete**: 64 bytes (32 × int16_t)
- **Variabili**: 32 bytes (16 × int16_t)
- **Bytecode buffer**: 1024 bytes (max script size)
- **Metadata**: 64 bytes
- **TOTALE runtime**: ~1184 bytes

**Heap disponibile**: ~100KB senza camera, ~40KB con camera → **OK** ✅

### **Performance Target:**
- **FPS**: 60 FPS (16ms per frame) → Script execution < 10ms
- **Script size**: Max 1KB bytecode per slot
- **Compilation**: < 500ms su app
- **Upload BLE**: < 5 secondi per 1KB script

---

## 🚀 Quick Start - Prototipo MVP (2 giorni)

Se vuoi un **MVP veloce** per testare il concetto:

### **Giorno 1: Interprete Minimale ESP32**
- [ ] Solo 10 opcodes essenziali:
  - LOAD_CONST, LOAD_VAR, STORE_VAR
  - ADD, MUL, MOD
  - LED_SET_HSV
  - JUMP, LABEL, HALT
- [ ] Solo API LED (niente motion/time)
- [ ] Test con bytecode hardcoded in `main.cpp`

**Risultato**: Script eseguiti su ESP32 senza BLE

### **Giorno 2: Editor Minimo + Upload**
- [ ] Editor testo plain (TextField Flutter, no highlighting)
- [ ] Compilatore manuale (parsing regex semplice)
- [ ] Upload BLE funzionante (1 slot)
- [ ] 2 script esempio (rainbow, solid)

**Risultato**: End-to-end funzionante in 48h!

**Deliverable**: Demo video che mostra:
1. Scrittura script in app
2. Compilazione
3. Upload via BLE
4. Esecuzione su LED reali

---

## 🎯 Next Steps

Per procedere, scegli una delle seguenti opzioni:

1. **🏃 MVP 2 giorni** - Prototipo veloce per validare concetto
2. **🏗️ Full Implementation** - Seguire roadmap completa (13 giorni)
3. **📝 Spec Dettagliata** - Approfondire design prima di codificare
4. **🔧 Solo Interprete** - Iniziare solo da lato ESP32
5. **📱 Solo App** - Iniziare solo da compilatore/editor

---

## 📚 Riferimenti

### Librerie e Strumenti
- **FastLED**: API LED (sin8, cos8, CHSV, etc.)
- **ArduinoJson**: Già usato per BLE (riuso per metadata)
- **LittleFS**: Storage script su flash
- **Flutter**: UI app mobile
- **flutter_blue_plus**: BLE communication

### Documenti Correlati
- `README.md` - Panoramica progetto
- `docs/BLE_PROTOCOL.md` - Protocollo BLE esistente
- `src/LedEffectEngine.cpp` - Effetti LED attuali
- `AppMobile/flutter_led_saber/` - App Flutter esistente

### Ispirazione
- **FastLED XY effects** - Mapping 2D LED
- **WLED** - JSON config effects
- **PixelBlaze** - Bytecode LED engine (commerciale)
- **MicroPython** - Lightweight Python interpreter

---

**Documento creato**: 2025-01-XX
**Versione**: 1.0
**Autore**: Claude Code Assistant
**Progetto**: LedSaber Custom Script System
