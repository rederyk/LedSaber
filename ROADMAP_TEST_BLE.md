# 📋 ROADMAP TEST BLE - Ambiente Controllato

## 🎯 Obiettivo

Creare un progetto di test isolato per sviluppare e validare tutte le funzionalità BLE prima di integrarle nel firmware principale.

**Workflow:**
1. Test BLE in ambiente controllato (questo documento)
2. Test Camera con BLE (vedi `ROADMAP_TEST_CAMERA_BLE.md`)
3. Integrazione finale nel progetto principale

---

## 📁 Struttura Progetto Test

```
ledDense/
├── test_ble/                    # Progetto test isolato
│   ├── platformio.ini
│   ├── src/
│   │   └── main.cpp
│   ├── include/
│   │   ├── BluetoothLogger.h
│   │   ├── BLELedController.h
│   │   └── BLEOTAController.h
│   └── lib/
│       ├── BluetoothLogger/
│       │   ├── BluetoothLogger.h
│       │   └── BluetoothLogger.cpp
│       ├── BLELedController/
│       │   ├── BLELedController.h
│       │   └── BLELedController.cpp
│       └── BLEOTAController/
│           ├── BLEOTAController.h
│           └── BLEOTAController.cpp
```

---

## 🎯 FASE 1: Setup Progetto Test

### 1.1 Creare cartella test

```bash
cd /home/reder/Documenti/PlatformIO/Projects/ledDense
mkdir -p test_ble/src test_ble/include test_ble/lib test_ble/tools
```

---

### 1.2 Creare `test_ble/platformio.ini`

```ini
[env:esp32-test-ble]
platform = espressif32
board = esp32dev
framework = arduino

monitor_speed = 115200
monitor_filters = esp32_exception_decoder

build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM

lib_deps =
    fastled/FastLED@^3.7.0
    bblanchon/ArduinoJson@^7.0.0

upload_speed = 921600
```

**Note:**
- `esp32dev`: Board generica ESP32 (funziona con ESP32-CAM)
- `CORE_DEBUG_LEVEL=3`: Abilita log debug BLE
- Nessuna dipendenza WiFi

---

## 🎯 FASE 2: Implementazione BluetoothLogger

### 2.1 Creare `test_ble/lib/BluetoothLogger/BluetoothLogger.h`

```cpp
#ifndef BLUETOOTH_LOGGER_H
#define BLUETOOTH_LOGGER_H

#include <BluetoothSerial.h>

class BluetoothLogger {
private:
    BluetoothSerial* bt;
    bool enabled;

public:
    BluetoothLogger(BluetoothSerial* bluetooth);
    void begin(const char* deviceName);
    void log(const String& message);
    void logf(const char* format, ...);
    bool isConnected();
};

#endif
```

---

### 2.2 Creare `test_ble/lib/BluetoothLogger/BluetoothLogger.cpp`

```cpp
#include "BluetoothLogger.h"
#include <cstdarg>

BluetoothLogger::BluetoothLogger(BluetoothSerial* bluetooth) {
    bt = bluetooth;
    enabled = false;
}

void BluetoothLogger::begin(const char* deviceName) {
    if (!bt->begin(deviceName)) {
        Serial.println("[BT ERROR] Failed to start!");
        enabled = false;
    } else {
        Serial.printf("[BT OK] Started: %s\n", deviceName);
        enabled = true;
    }
}

void BluetoothLogger::log(const String& message) {
    unsigned long ms = millis();
    String timestamped = "[" + String(ms) + "ms] " + message;

    Serial.println(timestamped);

    if (enabled && bt->hasClient()) {
        bt->println(timestamped);
    }
}

void BluetoothLogger::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(String(buffer));
}

bool BluetoothLogger::isConnected() {
    return enabled && bt->hasClient();
}
```

---

## 🎯 FASE 3: Implementazione BLELedController

### 3.1 Creare `test_ble/lib/BLELedController/BLELedController.h`

```cpp
#ifndef BLE_LED_CONTROLLER_H
#define BLE_LED_CONTROLLER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#define LED_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_LED_STATE_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_LED_COLOR_UUID     "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab"
#define CHAR_LED_EFFECT_UUID    "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc"
#define CHAR_LED_BRIGHTNESS_UUID "f3g7c6e5-gd32-6c5g-ac6e-3456789012cd"

struct LedState {
    uint8_t r = 255;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t brightness = 255;
    String effect = "solid";
    uint8_t speed = 50;
    bool enabled = true;
};

class BLELedController {
private:
    BLEServer* pServer;
    BLECharacteristic* pCharState;
    BLECharacteristic* pCharColor;
    BLECharacteristic* pCharEffect;
    BLECharacteristic* pCharBrightness;
    bool deviceConnected;
    LedState* ledState;

public:
    BLELedController(LedState* state);
    void begin(const char* deviceName);
    void notifyState();
    bool isConnected();

    friend class ServerCallbacks;
    friend class ColorCallbacks;
    friend class EffectCallbacks;
    friend class BrightnessCallbacks;
};

#endif
```

---

### 3.2 Creare `test_ble/lib/BLELedController/BLELedController.cpp`

**NOTA:** Copia il codice completo dalla ROADMAP_HYBRID_BLE_BT.md (sezione 3.2)

---

## 🎯 FASE 4: Main di Test

### 4.1 Creare `test_ble/src/main.cpp`

```cpp
#include <Arduino.h>
#include <FastLED.h>
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include "BluetoothLogger.h"
#include "BLELedController.h"

// LED Config (test con pochi LED)
#define NUM_LEDS 8
#define DATA_PIN 12
CRGB leds[NUM_LEDS];

// Bluetooth Classic
BluetoothSerial SerialBT;
BluetoothLogger logger(&SerialBT);

// BLE
LedState ledState;
BLELedController bleController(&ledState);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== TEST BLE + BLUETOOTH CLASSIC ===");

    // Inizializza LED strip
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
    fill_solid(leds, NUM_LEDS, CRGB::Green);  // Verde = boot OK
    FastLED.show();
    Serial.println("[LED] FastLED initialized");

    // Avvia Bluetooth Classic
    logger.begin("TestBLE-Log");
    logger.log("*** Bluetooth Classic OK ***");

    // Avvia BLE
    bleController.begin("TestBLE-Control");
    logger.log("*** BLE GATT Server OK ***");

    logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
    logger.log("*** READY FOR TESTING ***");

    // LED blu = sistema pronto
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
}

void loop() {
    static unsigned long lastLedUpdate = 0;
    static unsigned long lastBleNotify = 0;
    static unsigned long lastMemLog = 0;
    static uint8_t hue = 0;

    const unsigned long now = millis();

    // 1. Aggiorna LED in base allo stato BLE
    if (now - lastLedUpdate > 20) {
        if (ledState.enabled) {
            if (ledState.effect == "solid") {
                fill_solid(leds, NUM_LEDS, CRGB(ledState.r, ledState.g, ledState.b));

            } else if (ledState.effect == "rainbow") {
                fill_rainbow(leds, NUM_LEDS, hue, 256 / NUM_LEDS);
                hue += ledState.speed / 10;

            } else if (ledState.effect == "breathe") {
                uint8_t breath = beatsin8(ledState.speed, 0, 255);
                fill_solid(leds, NUM_LEDS, CRGB(ledState.r, ledState.g, ledState.b));
                FastLED.setBrightness(breath);
            }

            FastLED.setBrightness(ledState.brightness);
            FastLED.show();
        } else {
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
        }

        lastLedUpdate = now;
    }

    // 2. Notifica stato BLE ogni 1 secondo
    if (bleController.isConnected() && now - lastBleNotify > 1000) {
        bleController.notifyState();
        lastBleNotify = now;
    }

    // 3. Log memoria ogni 5 secondi
    if (now - lastMemLog > 5000) {
        logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
        logger.logf("BT connected: %s | BLE connected: %s",
            logger.isConnected() ? "YES" : "NO",
            bleController.isConnected() ? "YES" : "NO");
        lastMemLog = now;
    }

    // 4. Comandi debug via Bluetooth Classic
    if (SerialBT.available()) {
        String cmd = SerialBT.readStringUntil('\n');
        cmd.trim();

        if (cmd == "ping") {
            logger.log("pong");

        } else if (cmd == "status") {
            logger.logf("Uptime: %lu ms", millis());
            logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
            logger.logf("LED State: R=%d G=%d B=%d", ledState.r, ledState.g, ledState.b);

        } else if (cmd == "reset") {
            logger.log("Rebooting...");
            delay(500);
            ESP.restart();

        } else if (cmd == "red") {
            ledState.r = 255; ledState.g = 0; ledState.b = 0;
            logger.log("LED set to RED");

        } else if (cmd == "green") {
            ledState.r = 0; ledState.g = 255; ledState.b = 0;
            logger.log("LED set to GREEN");

        } else if (cmd == "blue") {
            ledState.r = 0; ledState.g = 0; ledState.b = 255;
            logger.log("LED set to BLUE");

        } else {
            logger.logf("Unknown: %s", cmd.c_str());
        }
    }

    yield();
}
```

---

## 🎯 FASE 5: Test Procedure

### 5.1 Build e Upload

```bash
cd test_ble
pio run -t upload -t monitor
```

**Output atteso:**
```
=== TEST BLE + BLUETOOTH CLASSIC ===
[LED] FastLED initialized
[BT OK] Started: TestBLE-Log
[0ms] *** Bluetooth Classic OK ***
[BLE OK] BLE GATT Server started!
[120ms] *** BLE GATT Server OK ***
[125ms] Free heap: 280000 bytes
[130ms] *** READY FOR TESTING ***
```

---

### 5.2 Test Bluetooth Classic (Android)

**App:** Serial Bluetooth Terminal

**Steps:**
1. Scansiona → "TestBLE-Log"
2. Connect
3. Invia comandi:
   - `ping` → Ricevi "pong"
   - `status` → Vedi uptime, RAM, stato LED
   - `red` → LED diventano rossi
   - `green` → LED diventano verdi
   - `blue` → LED diventano blu

**Verifica:**
- ✅ Log in tempo reale
- ✅ Comandi ricevuti correttamente
- ✅ Timestamp nei messaggi

---

### 5.3 Test BLE GATT (Android)

**App:** nRF Connect

**Steps:**
1. Scansiona → "TestBLE-Control"
2. Connect
3. Naviga Service `4fafc201...`

**Test 1: Imposta Colore Rosso**
- Scrivi su `CHAR_LED_COLOR_UUID`:
  ```json
  {"r": 255, "g": 0, "b": 0}
  ```
- ✅ LED diventano rossi
- ✅ Log su Serial BT: `[BLE] Color set to RGB(255,0,0)`

**Test 2: Effetto Arcobaleno**
- Scrivi su `CHAR_LED_EFFECT_UUID`:
  ```json
  {"mode": "rainbow", "speed": 80}
  ```
- ✅ LED mostrano effetto arcobaleno
- ✅ Log su Serial BT: `[BLE] Effect set to rainbow (speed: 80)`

**Test 3: Luminosità 50%**
- Scrivi su `CHAR_LED_BRIGHTNESS_UUID`:
  ```json
  {"brightness": 128, "enabled": true}
  ```
- ✅ LED dimmed al 50%

**Test 4: Notifiche Push**
- Tap su "Notify" di `CHAR_LED_STATE_UUID`
- ✅ Ricevi JSON aggiornato ogni 1 secondo:
  ```json
  {"r":255,"g":0,"b":0,"brightness":128,"effect":"rainbow","speed":80,"enabled":true}
  ```

---

### 5.4 Test Simultaneo BT Classic + BLE

**Setup:**
1. Android 1: "Serial Bluetooth Terminal" → "TestBLE-Log"
2. Android 2 (o stesso): "nRF Connect" → "TestBLE-Control"

**Test:**
1. Cambia colore via nRF Connect
2. Vedi conferma log su Serial Bluetooth Terminal
3. Invia comando `status` su Serial BT
4. Verifica stato LED aggiornato

**Verifica:**
- ✅ Due stack Bluetooth funzionano simultaneamente
- ✅ Nessun conflitto RAM
- ✅ Latenza accettabile (<100ms)

---

## 🎯 FASE 6: Test Stress e Performance

### 6.1 Test Memoria

**Comandi:**
```cpp
// Aggiungere nel loop()
if (cmd == "mem") {
    logger.logf("Total heap: %u", ESP.getHeapSize());
    logger.logf("Free heap: %u", ESP.getFreeHeap());
    logger.logf("Min free heap: %u", ESP.getMinFreeHeap());
    logger.logf("Max alloc heap: %u", ESP.getMaxAllocHeap());
}
```

**Target:**
- Free heap: > 200 KB
- Nessun memory leak dopo 1h uptime

---

### 6.2 Test Latenza

**Procedura:**
1. Invia 10 comandi BLE consecutivi (cambio colore)
2. Misura tempo tra invio e ricezione log

**Target:**
- Latenza media: < 100ms
- Max latenza: < 200ms

---

### 6.3 Test Disconnessione/Riconnessione

**Procedura:**
1. Connect BLE
2. Disconnect
3. Riconnect
4. Ripeti 10 volte

**Verifica:**
- ✅ Advertising riprende automaticamente
- ✅ Nessun crash
- ✅ Stato LED preservato

---

## 📊 Checklist Validazione

### Bluetooth Classic
- [ ] Advertising visibile ("TestBLE-Log")
- [ ] Pairing funzionante
- [ ] Log in tempo reale
- [ ] Comandi ricevuti correttamente
- [ ] Timestamp accurati
- [ ] Disconnessione gestita

### BLE GATT
- [ ] Advertising visibile ("TestBLE-Control")
- [ ] Service `4fafc201...` accessibile
- [ ] Tutte le 4 characteristics presenti
- [ ] Write su COLOR funziona
- [ ] Write su EFFECT funziona
- [ ] Write su BRIGHTNESS funziona
- [ ] Notify su STATE funziona
- [ ] JSON parsing corretto

### LED
- [ ] Effetto "solid" funziona
- [ ] Effetto "rainbow" funziona
- [ ] Effetto "breathe" funziona
- [ ] Luminosità regolabile
- [ ] Spegnimento funziona

### Sistema
- [ ] Free heap > 200 KB
- [ ] Nessun crash dopo 1h
- [ ] BT + BLE simultanei OK
- [ ] Latenza < 100ms
- [ ] Reconnect automatico

---

## 🐛 Troubleshooting

### BLE non si avvia
```
[BLE ERROR] BLE init failed
```
**Soluzione:**
- Verifica `sdkconfig`: BLE abilitato
- Rimuovi `#include <WiFi.h>` (conflitto)
- Aumenta stack size: `CONFIG_BT_TASK_STACK_SIZE=4096`

---

### Bluetooth Classic non connette
```
[BT ERROR] Failed to start!
```
**Soluzione:**
- Verifica nome univoco (`TestBLE-Log`)
- Reset Bluetooth su Android
- Unpair e re-pair

---

### JSON parsing fallisce
```
[BLE ERROR] Invalid JSON for color
```
**Soluzione:**
- Verifica formato: `{"r":255,"g":0,"b":0}`
- No spazi extra
- Usa double quotes `"`

---

### Memory leak
```
Free heap diminuisce nel tempo
```
**Soluzione:**
- Verifica `delete` per ogni `new`
- Usa `StaticJsonDocument` invece di `DynamicJsonDocument`
- Limita buffer BLE a 256 byte

---

## 📝 Note Finali

### Successo Test
Se tutti i test passano:
1. ✅ Stack BLE validato
2. ✅ Stack BT Classic validato
3. ✅ Coesistenza confermata
4. ✅ Pronto per FASE 2: Test Camera

**Prossimo step:**
→ Vedi `ROADMAP_TEST_CAMERA_BLE.md`

### Fallimento Test
Se qualche test fallisce:
- Debug isolato (disabilita BLE o BT Classic)
- Verifica log ESP32 (`CORE_DEBUG_LEVEL=5`)
- Testa su hardware diverso (non ESP32-CAM)

---

## 🚀 Comandi Rapidi

```bash
# Build
cd test_ble && pio run

# Upload + Monitor
pio run -t upload -t monitor

# Clean
pio run -t clean

# Monitor seriale
pio device monitor -b 115200
```

---

**Tempo stimato test completo:** ~2-3 ore

**Next:** `ROADMAP_TEST_CAMERA_BLE.md` 🚀
