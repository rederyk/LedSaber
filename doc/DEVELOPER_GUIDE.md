# 🛠️ LedSaber - Developer & AI Agent Guide

Guida completa per sviluppatori umani e agenti AI che lavorano sul progetto LedSaber.

---

## 📚 Indice

1. [Panoramica Architettura](#panoramica-architettura)
2. [Stack Tecnologico](#stack-tecnologico)
3. [Struttura del Progetto](#struttura-del-progetto)
4. [Workflow di Sviluppo](#workflow-di-sviluppo)
5. [Sistema Chrono Themes](#sistema-chrono-themes)
6. [Protocollo BLE](#protocollo-ble)
7. [Dashboard Python](#dashboard-python)
8. [Testing](#testing)
9. [Deployment](#deployment)
10. [Best Practices](#best-practices)
11. [Troubleshooting](#troubleshooting)

---

## 🏗️ Panoramica Architettura

### Componenti Principali

```
┌─────────────────────────────────────────────────────────┐
│                    LEDSABER SYSTEM                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────┐      BLE       ┌──────────────┐     │
│  │   ESP32-CAM  │ ◄────────────► │   Python     │     │
│  │   Firmware   │                 │   Dashboard  │     │
│  └──────────────┘                 └──────────────┘     │
│         │                                               │
│         ├─► LED Strip (WS2812B)                        │
│         ├─► Camera (OV2640)                            │
│         ├─► IMU (MPU6050)                              │
│         └─► Status LED                                 │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Layer Stack

1. **Hardware Layer**
   - ESP32-CAM (240MHz dual-core)
   - WS2812B LED strip (144 LED, folded)
   - OV2640 camera (optical flow)
   - MPU6050 IMU (accelerometer + gyro)

2. **Firmware Layer** (C++/Arduino)
   - LED rendering engine
   - Motion processing (optical flow + IMU)
   - BLE server (GATT)
   - Configuration management

3. **Communication Layer**
   - BLE GATT protocol
   - JSON payloads
   - State notifications
   - Time synchronization

4. **Client Layer** (Python)
   - BLE client (Bleak)
   - TUI dashboard (Textual)
   - State management
   - Real-time monitoring

---

## 🔧 Stack Tecnologico

### Firmware (ESP32)

- **Framework**: Arduino for ESP32 (v3.20017)
- **Platform**: PlatformIO (espressif32 v6.12.0)
- **Libraries**:
  - FastLED 3.10.3 (LED control)
  - ArduinoJson 7.4.2 (JSON parsing)
  - ESP32 BLE Arduino 2.0.0 (Bluetooth)
  - ESP32 Camera (OV2640 driver)

### Dashboard (Python)

- **Python**: 3.10+
- **Libraries**:
  - `bleak` (BLE client)
  - `textual` (TUI framework)
  - `rich` (terminal formatting)

### Tools

- **IDE**: VSCode + PlatformIO extension
- **Serial Monitor**: `platformio device monitor`
- **Git**: Version control
- **SSH/SCP**: Remote deployment (optional)

---

## 📁 Struttura del Progetto

```
ledSaber/
├── src/                          # Firmware C++
│   ├── main.cpp                  # Entry point, setup & loop
│   ├── BLELedController.cpp      # BLE GATT server LED
│   ├── BLEMotionService.cpp      # BLE motion service
│   ├── BLECameraService.cpp      # BLE camera service
│   ├── LedEffectEngine.cpp       # LED rendering (CORE)
│   ├── MotionProcessor.cpp       # Gesture detection
│   ├── OpticalFlowDetector.cpp   # Camera-based motion
│   ├── CameraManager.cpp         # OV2640 driver
│   ├── ConfigManager.cpp         # Flash storage
│   └── StatusLedManager.cpp      # Onboard LED
│
├── include/                      # Headers
│   ├── BLELedController.h        # LedState struct (CRITICAL)
│   └── *.h                       # Other headers
│
├── lib/                          # Custom libraries (if any)
│
├── platformio.ini                # Build configuration
│
├── *.py                          # Python scripts
│   ├── saber_dashboard.py        # Main TUI dashboard
│   └── ledsaber_control.py       # BLE client library
│
├── doc/                          # Documentation
│   ├── CHRONO_THEMES.md          # User guide temi
│   ├── CHRONO_CUSTOM_THEMES_GUIDE.md  # Developer guide temi
│   ├── DEVELOPER_GUIDE.md        # This file
│   └── *.md                      # Other docs
│
└── README.md                     # Project overview
```

### File Critici da Conoscere

| File | Scopo | Quando Modificare |
|------|-------|-------------------|
| `LedEffectEngine.cpp` | Rendering LED, temi Chrono | Nuovi effetti, temi, animazioni |
| `BLELedController.h` | Struct `LedState`, definizioni UUID | Nuovi parametri BLE, state fields |
| `BLELedController.cpp` | Callbacks BLE, parsing JSON | Nuovi comandi BLE |
| `saber_dashboard.py` | TUI dashboard completa | UI changes, nuovi widget |
| `ledsaber_control.py` | Client BLE Python | Nuove funzioni API |

---

## 🔄 Workflow di Sviluppo

### 1. Setup Ambiente

```bash
# Clone repository
git clone <repo-url>
cd ledSaber

# Install PlatformIO (se non installato)
pip install platformio

# Install Python dependencies
pip install bleak textual rich

# Verifica setup
platformio run  # Compila firmware
python3 saber_dashboard.py  # Test dashboard (senza device)
```

### 2. Branch Strategy

```bash
# Feature branch
git checkout -b feature/new-chrono-theme

# Bug fix
git checkout -b fix/ble-connection-timeout

# Experimental
git checkout -b experiment/optical-flow-v2
```

### 3. Develop Cycle

**A. Modifiche Firmware (C++)**

```bash
# 1. Edit code
vim src/LedEffectEngine.cpp

# 2. Compile
platformio run

# 3. Upload to device
platformio run --target upload

# 4. Monitor serial (debug)
platformio device monitor
```

**B. Modifiche Dashboard (Python)**

```bash
# 1. Edit code
vim saber_dashboard.py

# 2. Test syntax
python3 -m py_compile saber_dashboard.py

# 3. Run dashboard
python3 saber_dashboard.py

# 4. Connect to device and test
```

### 4. Test Protocol

1. **Unit Test** (firmware): LED patterns senza motion
2. **Integration Test**: BLE commands da dashboard
3. **Motion Test**: Swing, clash, shake detection
4. **Performance Test**: FPS camera, LED frame rate
5. **Battery Test**: Power consumption monitoring

### 5. Commit & Push

```bash
# Stage changes
git add src/LedEffectEngine.cpp saber_dashboard.py

# Commit with meaningful message
git commit -m "feat(chrono): add Galaxy theme for hour markers

- Implemented renderChronoHours_Galaxy with pulsating stars
- Added galaxy preset to dashboard ChronoThemesCard
- Updated CHRONO_THEMES.md documentation"

# Push
git push origin feature/new-chrono-theme
```

---

## 🎨 Sistema Chrono Themes

### Architettura Modulare

Il sistema Chrono è diviso in **2 layer indipendenti**:

```cpp
renderChronoHybrid() {
    // Layer 1: Hour markers (background)
    switch (state.chronoHourTheme) {
        case 0: renderChronoHours_Classic(...); break;
        case 1: renderChronoHours_Neon(...); break;
        // ...
    }

    // Layer 2: Second/minute cursors (foreground)
    switch (state.chronoSecondTheme) {
        case 0: renderChronoSeconds_Classic(...); break;
        case 1: renderChronoSeconds_TimeSpiral(...); break;
        // ...
    }
}
```

### Aggiungere un Nuovo Tema: Checklist

- [ ] **Step 1**: Aggiungere enum in `LedEffectEngine.h`
  ```cpp
  enum class ChronoHourTheme : uint8_t {
      // ... existing ...
      NEW_THEME = 4  // ← Add here
  };
  ```

- [ ] **Step 2**: Dichiarare funzione in `LedEffectEngine.h`
  ```cpp
  void renderChronoHours_NewTheme(uint16_t foldPoint, CRGB baseColor, uint8_t hours);
  ```

- [ ] **Step 3**: Implementare in `LedEffectEngine.cpp`
  ```cpp
  void LedEffectEngine::renderChronoHours_NewTheme(...) {
      // Implementation
  }
  ```

- [ ] **Step 4**: Aggiungere case in switch (`renderChronoHybrid`)
  ```cpp
  case 4: renderChronoHours_NewTheme(...); break;
  ```

- [ ] **Step 5**: Aggiornare dashboard Python
  ```python
  HOUR_THEMES = ["Classic", "Neon", "Plasma", "Digital", "NewTheme"]
  hour_icons = ["◎", "◉", "◈", "◆", "✨"]
  ```

- [ ] **Step 6**: Testare con `chrono 4 0` command

- [ ] **Step 7**: Documentare in `CHRONO_THEMES.md`

### Parametri Comuni a Tutti i Temi

```cpp
void renderChronoXXX(
    uint16_t foldPoint,      // Numero LED logici (metà strip)
    uint8_t hours,           // 0-11 (formato 12h)
    uint8_t minutes,         // 0-59
    uint8_t seconds,         // 0-59
    float visualOffset,      // Motion offset (±30s max)
    CRGB baseColor           // Colore primario (state.r, g, b)
)
```

### Helper Functions Disponibili

```cpp
// Set LED pair (folded strip)
setLedPair(uint16_t logicalIndex, uint16_t foldPoint, CRGB color);

// FastLED math
sin8(uint8_t x) → uint8_t      // Fast sine
cos8(uint8_t x) → uint8_t      // Fast cosine
beat8(uint8_t bpm) → uint8_t   // Sawtooth wave
qadd8(uint8_t a, uint8_t b)    // Saturating add

// Color
CHSV(hue, sat, val) → CRGB
rgb2hsv_approximate(CRGB) → CHSV
blend(CRGB c1, CRGB c2, uint8_t amount)
```

---

## 📡 Protocollo BLE

### UUIDs Servizi

```cpp
// Service LED
#define LED_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// Characteristics LED
#define CHAR_LED_STATE_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // READ + NOTIFY
#define CHAR_LED_COLOR_UUID      "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab"  // WRITE
#define CHAR_LED_EFFECT_UUID     "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc"  // WRITE
#define CHAR_LED_BRIGHTNESS_UUID "f3e7c6e5-0d32-4c5a-ac6e-3456789012cd"  // WRITE
#define CHAR_FOLD_POINT_UUID     "a5b0f9a7-3c65-8e9f-cf0c-6789abcdef01"  // WRITE + READ
#define CHAR_TIME_SYNC_UUID      "d6e1a0b8-4a76-9f0c-dc1a-789abcdef012"  // WRITE
```

### Formato JSON Comandi

**Set Color:**
```json
{
  "r": 255,
  "g": 0,
  "b": 0
}
```

**Set Effect:**
```json
{
  "mode": "chrono_hybrid",
  "speed": 150,
  "chronoHourTheme": 2,
  "chronoSecondTheme": 3
}
```

**Set Brightness:**
```json
{
  "brightness": 128,
  "enabled": true
}
```

**Time Sync:**
```json
{
  "epoch": 1703001234
}
```

### State Notification Format

```json
{
  "r": 255,
  "g": 0,
  "b": 0,
  "brightness": 255,
  "effect": "chrono_hybrid",
  "speed": 150,
  "enabled": true,
  "foldPoint": 72
}
```

### Aggiungere Nuova Characteristic

**Firmware (BLELedController.cpp):**

```cpp
// 1. Define UUID in header
#define CHAR_NEW_FEATURE_UUID "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"

// 2. Add characteristic pointer
BLECharacteristic* pCharNewFeature;

// 3. Create in begin()
pCharNewFeature = pService->createCharacteristic(
    CHAR_NEW_FEATURE_UUID,
    BLECharacteristic::PROPERTY_WRITE
);
pCharNewFeature->setCallbacks(new NewFeatureCallbacks(this));

// 4. Implement callback
class NewFeatureCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();
        JsonDocument doc;
        deserializeJson(doc, value);

        // Process data
        controller->ledState->newParameter = doc["value"];
    }
};
```

**Python Client (ledsaber_control.py):**

```python
CHAR_NEW_FEATURE_UUID = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"

async def set_new_feature(self, value: int):
    """Set new feature value"""
    if not self.client or not self.client.is_connected:
        print(f"{Colors.RED}✗ Not connected{Colors.RESET}")
        return

    data = json.dumps({"value": value})
    await self._write(CHAR_NEW_FEATURE_UUID, data.encode('utf-8'), label="new_feature")
```

---

## 🖥️ Dashboard Python

### Architettura Textual

```python
SaberDashboard (App)
├── HeaderWidget               # Titolo + BLE status
├── VerticalScroll (main_body)
│   └── Container (stats_grid)
│       ├── Vertical (led_column)
│       │   ├── LEDPanelWidget
│       │   └── Container (kpi_row)
│       │       ├── BLERSSICard
│       │       └── ActiveFXCard
│       ├── Vertical (camera_column)
│       │   ├── CameraPanelWidget
│       │   └── CameraFramesCard
│       ├── MotionSection (motion_summary)
│       │   ├── MotionStatusCard
│       │   ├── MotionIntensityCard
│       │   ├── MotionDirectionCard
│       │   ├── ChronoThemesCard        ← Widget temi
│       │   └── DeviceInfoCard
│       ├── OpticalFlowGridWidget
│       └── Vertical (console_column)
│           ├── CommandInputWidget
│           └── ConsoleWidget
└── Footer
```

### Aggiungere Nuovo Widget

**1. Definire Widget Class:**

```python
class MyNewCard(Static):
    """Description of new widget"""

    my_value: reactive[int] = reactive(0)

    def render(self):
        content = Text(f"Value: {self.my_value}", style="cyan")

        return Panel(
            Align.center(content, vertical="middle"),
            title="[bold cyan]MY WIDGET[/]",
            border_style="cyan",
            box=box.ROUNDED,
            height=5
        )
```

**2. Aggiungere a Layout:**

```python
# In SaberDashboard.compose()
yield MyNewCard(id="my_new_card")
```

**3. Aggiungere Reference:**

```python
# In SaberDashboard.__init__()
self.my_card: Optional[MyNewCard] = None

# In on_mount()
self.my_card = self.query_one("#my_new_card", MyNewCard)
```

**4. Update da Callback:**

```python
def _on_something_update(self, data: Dict):
    if self.my_card:
        self.my_card.my_value = data.get('value', 0)
```

### CSS Styling

```python
CSS = """
#my_new_card {
    margin: 1;
    padding: 1;
    border: round cyan;
}

#my_new_card:focus {
    border: round green;
}
"""
```

### Keybindings

```python
BINDINGS = [
    Binding("f8", "my_action", "My Action", show=True),
]

def action_my_action(self) -> None:
    """F8: Execute my action"""
    self._log("Action executed!", "green")
    # ... logic ...
```

---

## 🧪 Testing

### Unit Tests (Firmware)

Creare file `test/test_led_engine.cpp`:

```cpp
#include <unity.h>
#include "LedEffectEngine.h"

CRGB leds[144];
LedEffectEngine engine(leds, 144);

void test_chrono_hours_classic() {
    CRGB baseColor = CRGB(255, 0, 0);
    engine.renderChronoHours_Classic(72, baseColor);

    // Verify LED 0 is set (first hour marker)
    TEST_ASSERT_NOT_EQUAL(CRGB::Black, leds[0]);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_chrono_hours_classic);
    UNITY_END();
}

void loop() {}
```

Run tests:
```bash
platformio test
```

### Integration Tests (Python)

```python
# test_dashboard.py
import asyncio
from ledsaber_control import LedSaberClient

async def test_chrono_themes():
    client = LedSaberClient()

    # Scan
    devices = await client.scan()
    assert len(devices) > 0, "No devices found"

    # Connect
    success = await client.connect(devices[0].address)
    assert success, "Connection failed"

    # Set chrono theme
    await client.set_effect_raw({
        "mode": "chrono_hybrid",
        "chronoHourTheme": 2,
        "chronoSecondTheme": 3
    })

    # Verify
    state = await client.get_state()
    assert state.get('effect') == 'chrono_hybrid'

    print("✓ Chrono themes test passed")

if __name__ == "__main__":
    asyncio.run(test_chrono_themes())
```

### Performance Benchmarks

**LED Render Rate:**
```cpp
unsigned long start = micros();
renderChronoHybrid(state, nullptr, nullptr);
unsigned long duration = micros() - start;
Serial.printf("Render time: %lu µs (%.1f FPS)\n", duration, 1000000.0 / duration);
```

Target: >60 FPS (< 16666 µs per frame)

---

## 📦 Deployment

### Build Release Firmware

```bash
# Build optimized release
platformio run --environment esp32cam

# Upload to device
platformio run --target upload

# Monitor output
platformio device monitor --baud 115200
```

### Firmware OTA Update (Future)

```cpp
// In main.cpp
#include "OTAManager.h"

void setup() {
    OTAManager::begin("ledsaber", "password");
}

void loop() {
    OTAManager::handle();
}
```

### Package Python Dashboard

```bash
# Create standalone executable (PyInstaller)
pip install pyinstaller

pyinstaller --onefile \
    --name saber_dashboard \
    --add-data "*.py:." \
    saber_dashboard.py

# Output: dist/saber_dashboard
```

---

## ✅ Best Practices

### Firmware (C++)

1. **Memory Management**
   - Use stack allocation when possible
   - Avoid `new`/`malloc` in render loops
   - Static buffers for known sizes

   ```cpp
   // ✅ GOOD
   static uint8_t buffer[256];

   // ❌ BAD
   uint8_t* buffer = new uint8_t[256];  // Memory leak risk
   ```

2. **Performance**
   - Cache `millis()` calls
   - Use FastLED math (`sin8`, not `sin`)
   - Minimize `Serial.print` in loops

   ```cpp
   // ✅ GOOD
   unsigned long now = millis();
   uint8_t phase = (now / 50) % 256;

   // ❌ BAD
   uint8_t phase = (millis() / 50) % 256;  // Multiple calls
   ```

3. **Code Style**
   - Use `camelCase` for variables
   - Use `PascalCase` for classes
   - Prefix private members with `_`

4. **Documentation**
   - Comment complex algorithms
   - Use Doxygen-style comments
   - Update CHANGELOG.md

### Python

1. **Async/Await**
   - Always use `async def` for BLE operations
   - Don't block event loop

   ```python
   # ✅ GOOD
   async def connect_device(self):
       await self.client.connect(address)

   # ❌ BAD
   def connect_device(self):
       asyncio.run(self.client.connect(address))  # Blocks!
   ```

2. **Type Hints**
   ```python
   async def set_color(self, r: int, g: int, b: int) -> bool:
       """Set LED color"""
       pass
   ```

3. **Error Handling**
   ```python
   try:
       await self.client.set_effect("chrono_hybrid", 150)
   except Exception as e:
       self._log(f"Failed to set effect: {e}", "red")
   ```

### Git

1. **Commit Messages** (Conventional Commits)
   ```
   feat(chrono): add meteor shower theme
   fix(ble): resolve connection timeout issue
   docs(api): update BLE protocol specification
   refactor(led): optimize render loop performance
   ```

2. **Branch Naming**
   ```
   feature/optical-flow-v2
   fix/dashboard-crash-on-disconnect
   docs/developer-guide
   ```

---

## 🐛 Troubleshooting

### Firmware Non Compila

**Errore**: `undefined reference to renderChronoHours_NewTheme`

**Soluzione**: Verifica dichiarazione in `.h` e implementazione in `.cpp`

```bash
# Check for typos
grep -r "renderChronoHours_NewTheme" src/ include/
```

---

**Errore**: `error: 'class LedState' has no member named 'chronoHourTheme'`

**Soluzione**: Aggiungi campo in `BLELedController.h`:

```cpp
struct LedState {
    // ... existing fields ...
    uint8_t chronoHourTheme = 0;
};
```

### BLE Non Si Connette

**Problema**: Dashboard non trova device

**Debug Steps**:

1. Verifica Bluetooth attivo:
   ```bash
   bluetoothctl
   > power on
   > scan on
   ```

2. Check ESP32 serial output:
   ```bash
   platformio device monitor
   # Look for "BLE Server started"
   ```

3. Clear BLE cache (Linux):
   ```bash
   bluetoothctl
   > remove <MAC_ADDRESS>
   ```

### Dashboard Crash

**Errore**: `AttributeError: 'NoneType' object has no attribute 'chrono_hour_theme'`

**Soluzione**: Widget non inizializzato. Verifica `on_mount()`:

```python
def on_mount(self):
    self.chrono_themes_card = self.query_one("#chrono_themes_card", ChronoThemesCard)
```

### LED Non Si Accendono

**Problema**: Firmware caricato, ma LED neri

**Debug**:

1. Check alimentazione (5V, min 3A per 144 LED)
2. Verifica pin LED in `main.cpp`:
   ```cpp
   #define LED_PIN 12  // ← Correct pin?
   ```
3. Test con effetto `solid`:
   ```python
   await client.set_effect("solid", 0)
   await client.set_color(255, 255, 255)
   ```

---

## 📚 Risorse Utili

### Documentazione Esterna

- [FastLED Wiki](https://github.com/FastLED/FastLED/wiki)
- [ESP32 BLE Docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gatt_defs.html)
- [Textual Docs](https://textual.textualize.io/)
- [Bleak Docs](https://bleak.readthedocs.io/)
- [PlatformIO Docs](https://docs.platformio.org/)

### Esempi nel Progetto

- **Chrono Themes**: Vedi `src/LedEffectEngine.cpp` linee 2014+
- **BLE Callbacks**: Vedi `src/BLELedController.cpp`
- **Dashboard Widgets**: Vedi `saber_dashboard.py` classi `*Card`
- **Optical Flow**: Vedi `src/OpticalFlowDetector.cpp`

### Community

- GitHub Issues: Report bugs e feature request
- Pull Requests: Contributi benvenuti!

---

## 🤖 Note per AI Agents

### Quando Modifichi il Codice

1. **Leggi sempre prima di scrivere**: Usa `Read` tool per capire il contesto
2. **Compila dopo ogni modifica**: `platformio run` per verificare
3. **Testa su hardware se possibile**: Non fidarti solo della compilazione
4. **Aggiorna documentazione**: Modifica `*.md` se cambi API/comandi
5. **Commit atomici**: Una feature = un commit

### File da NON Modificare Senza Conferma

- `platformio.ini`: Build configuration (cambia solo se sai cosa fai)
- `main.cpp`: Entry point critico
- `*.json`: Config files (backup prima!)

### Pattern Comuni da Seguire

**Aggiungere effetto LED:**
1. Implementa `renderNewEffect()` in `LedEffectEngine.cpp`
2. Aggiungi case in `render()` switch
3. Testa con comando `effect new_effect 150`

**Aggiungere comando BLE:**
1. Definisci UUID in `BLELedController.h`
2. Crea characteristic + callback in `.cpp`
3. Implementa funzione Python in `ledsaber_control.py`
4. Aggiungi comando in `saber_dashboard.py`

**Aggiungere widget dashboard:**
1. Crea classe `NewCard(Static)` in `saber_dashboard.py`
2. Aggiungi in `compose()` e `on_mount()`
3. Update da callback appropriato
4. Aggiungi CSS se necessario

### Debug Checklist

Prima di dire "ho finito":

- [ ] Codice compila senza errori
- [ ] Nessun warning critico
- [ ] Testato su device reale (se possibile)
- [ ] Documentazione aggiornata
- [ ] Commit message descrittivo
- [ ] No file temporanei committati (`.pyc`, `.o`, etc.)

---

## 📝 Changelog Template

Quando aggiungi feature, aggiorna `CHANGELOG.md`:

```markdown
## [Unreleased]

### Added
- New "Galaxy" theme for chrono hours with pulsating stars
- Support for custom color palettes via BLE

### Changed
- Improved optical flow performance (30 → 35 FPS)
- Dashboard widget layout responsive to terminal width

### Fixed
- BLE connection timeout on Linux with BlueZ
- Memory leak in particle effect renderer

### Removed
- Deprecated `set_effect_legacy()` function
```

---

## 🚀 Quick Start per AI Agents

```bash
# 1. Understand project
cat README.md
cat doc/DEVELOPER_GUIDE.md

# 2. Explore codebase
ls -la src/
grep -r "chrono" src/

# 3. Read critical files
cat include/BLELedController.h
cat src/LedEffectEngine.cpp | head -100

# 4. Test compile
platformio run

# 5. Make changes
vim src/LedEffectEngine.cpp

# 6. Verify
platformio run
python3 -m py_compile saber_dashboard.py

# 7. Commit
git add -p
git commit -m "feat: descriptive message"
```

---

**Happy Coding! 🎉**

Per domande o supporto, apri una issue su GitHub con tag `question` o `help wanted`.
