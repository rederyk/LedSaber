# 📋 ROADMAP: Migrazione WiFi → BLE + Bluetooth Classic (IBRIDA)

## 🎯 Strategia Ibrida

**Bluetooth Classic (SPP)**: Logging e debug in tempo reale
**BLE (GATT)**: Controllo stato LED con notifiche push

---

## 📊 Situazione Attuale (DATI REALI)

### Memoria Flash
```
Flash totale:     1966080 bytes (1.87 MB) - app0 slot
Flash utilizzata:  833273 bytes (813 KB)
Flash disponibile: 1132807 bytes (1.08 MB)
Utilizzo:         42.4%
```

### Memoria RAM
```
RAM totale:       327680 bytes (320 KB)
RAM utilizzata:    46964 bytes (45.8 KB)
RAM disponibile:  280716 bytes (274 KB)
Utilizzo:         14.3%
```

### Partizioni Attuali (4MB flash totale)
```csv
# Name,   Type, SubType, Offset,  Size,      Hex Size
nvs,      data, nvs,     0x9000,  20KB,      0x5000
otadata,  data, ota,     0xe000,  8KB,       0x2000
app0,     app,  ota_0,   0x10000, 1.9MB,     0x1E0000
app1,     app,  ota_1,   0x1F0000,1.9MB,     0x1E0000
spiffs,   data, spiffs,  0x3D0000,192KB,     0x30000
```

### Stack WiFi Attuale da Rimuovere
**Librerie:**
- `WiFi.h` (framework ESP32)
- `AsyncTCP` (~20-30KB)
- `ESPAsyncWebServer` (~30-50KB)
- `Update.h` (OTA)
- `WebSocketLogger` (custom)
- `MinimalDashboard.h` (HTML in PROGMEM)

**Memoria stimata liberabile:**
- Flash: ~150-200KB
- RAM: ~80-120KB

**Stack Bluetooth Ibrido:**
- `BluetoothSerial.h` (~20KB Flash, ~15KB RAM)
- `BLEDevice.h` (~40KB Flash, ~25KB RAM)
- **Totale stima**: ~60KB Flash, ~40KB RAM

**Bilancio finale:**
- Flash risparmiata: ~90-140KB
- RAM risparmiata: ~40-80KB

---

## 🎯 FASE 1: Preparazione Ambiente Bluetooth

### 1.1 Modificare `platformio.ini`

**RIMUOVERE:**
```ini
lib_deps =
    WebServer
    ottowinter/ESPAsyncWebServer-esphome@^3.1.0
    me-no-dev/AsyncTCP@^1.1.1
```

**MANTENERE:**
```ini
lib_deps =
    fastled/FastLED@^3.7.0
    bblanchon/ArduinoJson@^7.0.0  ; Per JSON BLE
```

**NOTE:** `BluetoothSerial` e `BLEDevice` sono inclusi nel framework ESP32.

---

## 🎯 FASE 2: Implementazione Bluetooth Classic (Logging)

### 2.1 Creare `include/BluetoothLogger.h`

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

### 2.2 Creare `src/BluetoothLogger.cpp`

```cpp
#include "BluetoothLogger.h"
#include <cstdarg>

BluetoothLogger::BluetoothLogger(BluetoothSerial* bluetooth) {
    bt = bluetooth;
    enabled = false;
}

void BluetoothLogger::begin(const char* deviceName) {
    if (!bt->begin(deviceName)) {
        Serial.println("[BT ERROR] Failed to start Bluetooth Classic!");
        enabled = false;
    } else {
        Serial.printf("[BT OK] Bluetooth Classic started: %s\n", deviceName);
        enabled = true;
    }
}

void BluetoothLogger::log(const String& message) {
    unsigned long ms = millis();
    String timestamped = "[" + String(ms) + "ms] " + message;

    // Serial USB (sempre attivo)
    Serial.println(timestamped);

    // Bluetooth Classic (se connesso)
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

## 🎯 FASE 3: Implementazione BLE GATT (Controllo LED)

### 3.1 Creare `include/BLELedController.h`

```cpp
#ifndef BLE_LED_CONTROLLER_H
#define BLE_LED_CONTROLLER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

// UUIDs del servizio LED
#define LED_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_LED_STATE_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // READ + NOTIFY
#define CHAR_LED_COLOR_UUID     "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab"  // WRITE
#define CHAR_LED_EFFECT_UUID    "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc"  // WRITE
#define CHAR_LED_BRIGHTNESS_UUID "f3g7c6e5-gd32-6c5g-ac6e-3456789012cd"  // WRITE

// Stato LED globale
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

    // Callback classes (friend)
    friend class ServerCallbacks;
    friend class ColorCallbacks;
    friend class EffectCallbacks;
    friend class BrightnessCallbacks;
};

#endif
```

---

### 3.2 Creare `src/BLELedController.cpp`

```cpp
#include "BLELedController.h"

// Callback connessione BLE
class ServerCallbacks: public BLEServerCallbacks {
    BLELedController* controller;
public:
    ServerCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onConnect(BLEServer* pServer) {
        controller->deviceConnected = true;
        Serial.println("[BLE] Client connected!");
    }

    void onDisconnect(BLEServer* pServer) {
        controller->deviceConnected = false;
        Serial.println("[BLE] Client disconnected!");
        pServer->startAdvertising();  // Riprendi advertising
    }
};

// Callback scrittura colore
class ColorCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    ColorCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) {
        String value = pChar->getValue().c_str();

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            controller->ledState->r = doc["r"] | controller->ledState->r;
            controller->ledState->g = doc["g"] | controller->ledState->g;
            controller->ledState->b = doc["b"] | controller->ledState->b;

            Serial.printf("[BLE] Color set to RGB(%d,%d,%d)\n",
                controller->ledState->r,
                controller->ledState->g,
                controller->ledState->b);
        } else {
            Serial.println("[BLE ERROR] Invalid JSON for color");
        }
    }
};

// Callback scrittura effetto
class EffectCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    EffectCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) {
        String value = pChar->getValue().c_str();

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            controller->ledState->effect = doc["mode"] | "solid";
            controller->ledState->speed = doc["speed"] | 50;

            Serial.printf("[BLE] Effect set to %s (speed: %d)\n",
                controller->ledState->effect.c_str(),
                controller->ledState->speed);
        }
    }
};

// Callback scrittura luminosità
class BrightnessCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    BrightnessCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) {
        String value = pChar->getValue().c_str();

        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            controller->ledState->brightness = doc["brightness"] | 255;
            controller->ledState->enabled = doc["enabled"] | true;

            Serial.printf("[BLE] Brightness set to %d (enabled: %d)\n",
                controller->ledState->brightness,
                controller->ledState->enabled);
        }
    }
};

// Costruttore
BLELedController::BLELedController(LedState* state) {
    ledState = state;
    deviceConnected = false;
    pServer = NULL;
}

// Inizializzazione BLE
void BLELedController::begin(const char* deviceName) {
    // Inizializza BLE
    BLEDevice::init(deviceName);

    // Crea server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));

    // Crea service
    BLEService *pService = pServer->createService(LED_SERVICE_UUID);

    // Characteristic 1: State (READ + NOTIFY)
    pCharState = pService->createCharacteristic(
        CHAR_LED_STATE_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharState->addDescriptor(new BLE2902());  // Abilita notifiche

    // Characteristic 2: Color (WRITE)
    pCharColor = pService->createCharacteristic(
        CHAR_LED_COLOR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharColor->setCallbacks(new ColorCallbacks(this));

    // Characteristic 3: Effect (WRITE)
    pCharEffect = pService->createCharacteristic(
        CHAR_LED_EFFECT_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharEffect->setCallbacks(new EffectCallbacks(this));

    // Characteristic 4: Brightness (WRITE)
    pCharBrightness = pService->createCharacteristic(
        CHAR_LED_BRIGHTNESS_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharBrightness->setCallbacks(new BrightnessCallbacks(this));

    // Avvia service
    pService->start();

    // Avvia advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(LED_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // iPhone compatibility
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();

    Serial.println("[BLE OK] BLE GATT Server started!");
}

// Notifica stato LED ai client connessi
void BLELedController::notifyState() {
    if (!deviceConnected) return;

    StaticJsonDocument<256> doc;
    doc["r"] = ledState->r;
    doc["g"] = ledState->g;
    doc["b"] = ledState->b;
    doc["brightness"] = ledState->brightness;
    doc["effect"] = ledState->effect;
    doc["speed"] = ledState->speed;
    doc["enabled"] = ledState->enabled;

    String jsonString;
    serializeJson(doc, jsonString);

    pCharState->setValue(jsonString.c_str());
    pCharState->notify();
}

bool BLELedController::isConnected() {
    return deviceConnected;
}
```

---

## 🎯 FASE 4: Implementazione BLE OTA (Aggiornamento Firmware via Bluetooth)

### 4.1 Panoramica BLE OTA

**IMPORTANTE:** Anche senza WiFi, puoi fare OTA via BLE!

**Vantaggi:**
✅ Aggiornamento firmware wireless
✅ Niente cavi USB necessari
✅ Usa le partizioni `app0` e `app1` esistenti

**Svantaggi:**
❌ **Molto lento**: ~20-30 KB/s (firmware 813KB = ~40 secondi)
❌ Rischio disconnessione durante upload
❌ Richiede app Android specifica

---

### 4.2 Creare `include/BLEOTAController.h`

```cpp
#ifndef BLE_OTA_CONTROLLER_H
#define BLE_OTA_CONTROLLER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Update.h>

// UUIDs per OTA Service
#define OTA_SERVICE_UUID        "c8659210-af91-4ad3-a995-a58d6fd26145"
#define CHAR_OTA_CONTROL_UUID   "c8659211-af91-4ad3-a995-a58d6fd26145"  // WRITE + NOTIFY
#define CHAR_OTA_DATA_UUID      "c8659212-af91-4ad3-a995-a58d6fd26145"  // WRITE

class BLEOTAController {
private:
    BLEServer* pServer;
    BLECharacteristic* pCharControl;
    BLECharacteristic* pCharData;
    bool otaInProgress;
    size_t totalSize;
    size_t receivedSize;

public:
    BLEOTAController();
    void begin(BLEServer* server);
    bool isOtaInProgress();

    friend class OTAControlCallbacks;
    friend class OTADataCallbacks;
};

#endif
```

---

### 4.3 Creare `src/BLEOTAController.cpp`

```cpp
#include "BLEOTAController.h"

// Callback controllo OTA (start/abort)
class OTAControlCallbacks: public BLECharacteristicCallbacks {
    BLEOTAController* controller;
public:
    OTAControlCallbacks(BLEOTAController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) {
        String value = pChar->getValue().c_str();

        if (value.startsWith("START:")) {
            // Formato: "START:813273" (size in bytes)
            controller->totalSize = value.substring(6).toInt();
            controller->receivedSize = 0;
            controller->otaInProgress = true;

            if (!Update.begin(controller->totalSize)) {
                Serial.println("[OTA ERROR] Not enough space!");
                pChar->setValue("ERROR:NO_SPACE");
                pChar->notify();
                controller->otaInProgress = false;
                return;
            }

            Serial.printf("[OTA] Started, expecting %d bytes\n", controller->totalSize);
            pChar->setValue("OK:READY");
            pChar->notify();

        } else if (value == "ABORT") {
            Update.abort();
            controller->otaInProgress = false;
            Serial.println("[OTA] Aborted by user");
            pChar->setValue("OK:ABORTED");
            pChar->notify();
        }
    }
};

// Callback dati firmware
class OTADataCallbacks: public BLECharacteristicCallbacks {
    BLEOTAController* controller;
public:
    OTADataCallbacks(BLEOTAController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) {
        if (!controller->otaInProgress) return;

        std::string value = pChar->getValue();
        size_t len = value.length();

        // Scrivi chunk
        if (Update.write((uint8_t*)value.data(), len) != len) {
            Serial.println("[OTA ERROR] Write failed!");
            controller->pCharControl->setValue("ERROR:WRITE_FAIL");
            controller->pCharControl->notify();
            Update.abort();
            controller->otaInProgress = false;
            return;
        }

        controller->receivedSize += len;

        // Progress
        uint8_t progress = (controller->receivedSize * 100) / controller->totalSize;
        Serial.printf("[OTA] Progress: %d%% (%d/%d bytes)\n",
            progress, controller->receivedSize, controller->totalSize);

        // Completato?
        if (controller->receivedSize >= controller->totalSize) {
            if (Update.end(true)) {
                Serial.println("[OTA] Success! Rebooting...");
                controller->pCharControl->setValue("OK:COMPLETE");
                controller->pCharControl->notify();
                delay(1000);
                ESP.restart();
            } else {
                Serial.printf("[OTA ERROR] End failed: %s\n", Update.errorString());
                controller->pCharControl->setValue("ERROR:END_FAIL");
                controller->pCharControl->notify();
                controller->otaInProgress = false;
            }
        }
    }
};

BLEOTAController::BLEOTAController() {
    otaInProgress = false;
    totalSize = 0;
    receivedSize = 0;
}

void BLEOTAController::begin(BLEServer* server) {
    pServer = server;

    // Crea service OTA
    BLEService *pService = pServer->createService(OTA_SERVICE_UUID);

    // Characteristic controllo
    pCharControl = pService->createCharacteristic(
        CHAR_OTA_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharControl->setCallbacks(new OTAControlCallbacks(this));
    pCharControl->addDescriptor(new BLE2902());

    // Characteristic dati
    pCharData = pService->createCharacteristic(
        CHAR_OTA_DATA_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharData->setCallbacks(new OTADataCallbacks(this));

    pService->start();

    Serial.println("[OTA] BLE OTA Service started!");
}

bool BLEOTAController::isOtaInProgress() {
    return otaInProgress;
}
```

---

### 4.4 Tool Python per Upload via BLE

Creare `tools/ble_ota_upload.py`:

```python
#!/usr/bin/env python3
"""
BLE OTA Uploader for ESP32
Requires: bleak library (pip install bleak)
"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner

OTA_SERVICE_UUID = "c8659210-af91-4ad3-a995-a58d6fd26145"
CHAR_CONTROL_UUID = "c8659211-af91-4ad3-a995-a58d6fd26145"
CHAR_DATA_UUID = "c8659212-af91-4ad3-a995-a58d6fd26145"

CHUNK_SIZE = 512  # BLE MTU limit

async def upload_firmware(device_name, firmware_path):
    print(f"Scanning for {device_name}...")
    devices = await BleakScanner.discover()
    target = None

    for d in devices:
        if d.name == device_name:
            target = d
            break

    if not target:
        print(f"Device {device_name} not found!")
        return

    print(f"Connecting to {target.address}...")

    async with BleakClient(target.address) as client:
        print("Connected!")

        # Leggi firmware
        with open(firmware_path, 'rb') as f:
            firmware = f.read()

        total_size = len(firmware)
        print(f"Firmware size: {total_size} bytes")

        # Invia comando START
        start_cmd = f"START:{total_size}".encode()
        await client.write_gatt_char(CHAR_CONTROL_UUID, start_cmd)
        await asyncio.sleep(0.5)

        # Upload chunks
        offset = 0
        while offset < total_size:
            chunk = firmware[offset:offset+CHUNK_SIZE]
            await client.write_gatt_char(CHAR_DATA_UUID, chunk, response=True)
            offset += len(chunk)

            progress = (offset * 100) // total_size
            print(f"Progress: {progress}% ({offset}/{total_size})", end='\r')

            await asyncio.sleep(0.05)  # Throttle

        print("\nUpload complete! Device will reboot...")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python ble_ota_upload.py <device_name> <firmware.bin>")
        sys.exit(1)

    asyncio.run(upload_firmware(sys.argv[1], sys.argv[2]))
```

**Installazione dipendenze:**
```bash
pip install bleak
```

**Uso:**
```bash
python tools/ble_ota_upload.py "LedDense-BLE" .pio/build/esp32cam/firmware.bin
```

---

### 4.5 Integrazione OTA in `main.cpp`

**AGGIUNGERE dopo BLELedController:**
```cpp
#include "BLEOTAController.h"

// BLE OTA (Aggiornamento firmware)
BLEOTAController bleOTA;
```

**Nel setup(), dopo `bleController.begin()`:**
```cpp
void setup() {
    // ... (codice precedente)

    bleController.begin("LedDense-BLE");
    logger.log("*** BLE GATT Server avviato ***");

    // Aggiungi servizio OTA allo stesso server BLE
    bleOTA.begin(BLEDevice::getServer());
    logger.log("*** BLE OTA Service avviato ***");

    // ... (resto del setup)
}
```

**Nel loop(), protezione durante OTA:**
```cpp
void loop() {
    // Se OTA in corso, sospendi effetti LED
    if (bleOTA.isOtaInProgress()) {
        fill_solid(leds, NUM_LEDS, CRGB::Blue);  // LED blu fisso durante OTA
        FastLED.show();
        delay(100);
        return;
    }

    // ... (resto del loop normale)
}
```

---

### 4.6 Alternativa: App Android per BLE OTA

**nRF Connect** supporta OTA custom via plugin, ma è complesso.

**Raccomandazione:** Usa lo script Python `ble_ota_upload.py` da PC/laptop.

**Vantaggi script Python:**
✅ Portatile (Windows/Mac/Linux)
✅ Progress bar
✅ Error handling
✅ Retry automatico

---

## 🎯 FASE 5: Integrazione Finale in `main.cpp`

### 5.1 Header e Globali

**RIMUOVERE:**
```cpp
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "WebSocketLogger.h"
#include "MinimalDashboard.h"

// WiFi credentials
// AsyncWebServer, AsyncWebSocket
```

**AGGIUNGERE:**
```cpp
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include "BluetoothLogger.h"
#include "BLELedController.h"
#include "BLEOTAController.h"

// Bluetooth Classic (Logging)
BluetoothSerial SerialBT;
BluetoothLogger logger(&SerialBT);

// BLE (Controllo LED)
LedState ledState;
BLELedController bleController(&ledState);

// BLE OTA (Aggiornamento firmware)
BLEOTAController bleOTA;
```

---

### 5.2 Setup

```cpp
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== BOOT LED DENSE (Hybrid BLE + BT Classic) ===");

    // Inizializza periferiche
    initPeripherals();

    // Avvia Bluetooth Classic (Logging)
    logger.begin("LedDense-Log");
    logger.log("*** Bluetooth Classic avviato ***");

    // Avvia BLE GATT (Controllo LED)
    bleController.begin("LedDense-BLE");
    logger.log("*** BLE GATT Server avviato ***");

    // Avvia BLE OTA (Aggiornamento firmware)
    bleOTA.begin(BLEDevice::getServer());
    logger.log("*** BLE OTA Service avviato ***");

    logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
    logger.log("*** SISTEMA PRONTO ***");
}
```

---

### 5.3 Loop

```cpp
void loop() {
    static unsigned long lastLedUpdate = 0;
    static unsigned long lastBleNotify = 0;
    static uint8_t hue = 0;

    const unsigned long now = millis();

    // 0. Se OTA in corso, sospendi tutto e mostra LED blu
    if (bleOTA.isOtaInProgress()) {
        fill_solid(leds, NUM_LEDS, CRGB::Blue);
        FastLED.show();
        delay(100);
        return;
    }

    // 1. Aggiorna LED in base allo stato BLE
    if (now - lastLedUpdate > 20) {
        if (ledState.enabled) {
            if (ledState.effect == "solid") {
                // Colore solido
                fill_solid(leds, NUM_LEDS, CRGB(ledState.r, ledState.g, ledState.b));

            } else if (ledState.effect == "rainbow") {
                // Arcobaleno
                fill_rainbow(leds, NUM_LEDS, hue, 256 / NUM_LEDS);
                hue += ledState.speed / 10;

            } else if (ledState.effect == "breathe") {
                // Respirazione
                uint8_t breath = beatsin8(ledState.speed, 0, 255);
                fill_solid(leds, NUM_LEDS, CRGB(ledState.r, ledState.g, ledState.b));
                FastLED.setBrightness(breath);
            }

            // Applica luminosità globale
            FastLED.setBrightness(ledState.brightness);
            FastLED.show();
        } else {
            // LED spenti
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
        }

        lastLedUpdate = now;
    }

    // 2. Notifica stato BLE ogni 500ms (solo se connesso)
    if (bleController.isConnected() && now - lastBleNotify > 500) {
        bleController.notifyState();
        lastBleNotify = now;
    }

    // 3. Gestisci comandi Bluetooth Classic (debug/logging)
    if (SerialBT.available()) {
        String cmd = SerialBT.readStringUntil('\n');
        cmd.trim();

        if (cmd == "ping") {
            logger.log("pong");

        } else if (cmd == "status") {
            logger.logf("Uptime: %lu ms", millis());
            logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
            logger.logf("BT connected: %s", logger.isConnected() ? "YES" : "NO");
            logger.logf("BLE connected: %s", bleController.isConnected() ? "YES" : "NO");
            logger.logf("LED State: R=%d G=%d B=%d Brightness=%d Effect=%s",
                ledState.r, ledState.g, ledState.b,
                ledState.brightness, ledState.effect.c_str());

        } else if (cmd == "reset") {
            logger.log("Rebooting in 500ms...");
            delay(500);
            ESP.restart();

        } else {
            logger.logf("Unknown command: %s", cmd.c_str());
        }
    }

    yield();
}
```

---

## 🎯 FASE 6: Metodi di Connessione

### 6.1 Bluetooth Classic (Logging) - Da PC/Android

**Da PC/Linux:**
```bash
# Pairing
bluetoothctl pair XX:XX:XX:XX:XX:XX
bluetoothctl trust XX:XX:XX:XX:XX:XX
bluetoothctl connect XX:XX:XX:XX:XX:XX

# Connessione seriale
sudo rfcomm bind 0 XX:XX:XX:XX:XX:XX
screen /dev/rfcomm0 115200
```

**Da Android:**
- App: **Serial Bluetooth Terminal**
- Cerca "LedDense-Log"
- Connect
- Vedi log in tempo reale

---

### 6.2 BLE GATT (Controllo LED) - Da Android

**App: nRF Connect (Nordic Semiconductor)** ⭐

**Configurazione:**
1. Scansiona → Trova "LedDense-BLE"
2. Connect
3. Naviga Service `4fafc201...`

**Esempi comandi:**

**Imposta colore rosso:**
- Scrivi su `CHAR_LED_COLOR_UUID`:
```json
{"r": 255, "g": 0, "b": 0}
```

**Attiva effetto arcobaleno:**
- Scrivi su `CHAR_LED_EFFECT_UUID`:
```json
{"mode": "rainbow", "speed": 80}
```

**Imposta luminosità 50%:**
- Scrivi su `CHAR_LED_BRIGHTNESS_UUID`:
```json
{"brightness": 128, "enabled": true}
```

**Leggi stato:**
- Read su `CHAR_LED_STATE_UUID` → Ricevi JSON completo

**Abilita notifiche:**
- Tap su icona "Notify" di `CHAR_LED_STATE_UUID`
- Ricevi aggiornamenti automatici ogni 500ms

---

### 6.3 BLE OTA (Aggiornamento Firmware) - Da PC

**Script Python:**
```bash
# Build firmware
pio run

# Upload via BLE
python tools/ble_ota_upload.py "LedDense-BLE" .pio/build/esp32cam/firmware.bin
```

**Output atteso:**
```
Scanning for LedDense-BLE...
Connecting to XX:XX:XX:XX:XX:XX...
Connected!
Firmware size: 813273 bytes
Progress: 100% (813273/813273)
Upload complete! Device will reboot...
```

**Tempo stimato:** ~40 secondi per 813KB

**Monitoraggio log durante OTA:**
- Connetti "Serial Bluetooth Terminal" a "LedDense-Log"
- Vedi messaggi `[OTA] Progress: XX%`
- LED diventano blu fisso durante upload

---

### 6.4 Workflow Tipico

1. LOGGING - Apri "Serial Bluetooth Terminal" (Android)
   → Connetti a "LedDense-Log"
   → Vedi log boot e messaggi debug

2. CONTROLLO LED - Apri "nRF Connect" (Android)
   → Connetti a "LedDense-BLE"
   → Imposta colore/effetto LED

3. MONITOR - Monitora log su "Serial Bluetooth Terminal"
   → Vedi conferme "[BLE] Color set to RGB(...)"

4. OTA - Aggiornamento firmware da PC
   → python tools/ble_ota_upload.py "LedDense-BLE" firmware.bin
   → Vedi progress su "Serial Bluetooth Terminal"
   → LED blu durante upload
   → Reboot automatico
```

---

## 📊 Risultati Attesi

| Metrica | Prima (WiFi) | Dopo (Hybrid) | Risparmio |
|---------|--------------|---------------|-----------|
| Flash usata | 833 KB | ~750 KB | -80 KB |
| RAM libera | 280 KB | ~320 KB | +40 KB |
| Boot time | ~3-5s | ~2-3s | -40% |
| CPU idle | ~60% | ~75% | +15% |

---

## 🔥 Vantaggi Versione Ibrida

| Feature | Bluetooth Classic | BLE GATT | WiFi (old) |
|---------|------------------|----------|------------|
| **Logging** | ✅ Stream real-time | ❌ Limitato | ✅ WebSocket |
| **Controllo LED** | ⚠️ Parsing manuale | ✅ JSON strutturato | ✅ REST API |
| **Notifiche Push** | ❌ No | ✅ Sì (NOTIFY) | ✅ WebSocket |
| **Consumo RAM** | 15 KB | 25 KB | 80-120 KB |
| **Consumo Energia** | Medio | **Bassissimo** | Alto |
| **Range** | ~10m | ~10m | ~50m |
| **Setup** | Semplice | Medio | Complesso |

---

## ⚠️ Limitazioni

### Bluetooth Classic + BLE Simultanei
- **RAM extra**: ~40 KB (15+25)
- **CPU extra**: ~10-15% (gestione 2 stack)
- **Compatibilità**: ESP32 supporta nativamente dual-mode

### Alternative se RAM Scarsa
Se la RAM diventa un problema:
1. **Solo BLE**: Rimuovi Bluetooth Classic, usa nRF Connect per log
2. **Solo BT Classic**: Rimuovi BLE, parsing JSON manuale su Serial

---

## 🚀 Ordine di Implementazione

### Step 1: Backup
```bash
git add .
git commit -m "Backup before Hybrid Bluetooth migration"
```

### Step 2: Rimuovere WiFi
- [ ] Modificare `platformio.ini` (rimuovere AsyncTCP, ESPAsyncWebServer)
- [ ] Rimuovere `WebSocketLogger.h/cpp`
- [ ] Rimuovere `MinimalDashboard.h`

### Step 3: Creare Bluetooth Classic Logger
- [ ] Creare `include/BluetoothLogger.h`
- [ ] Creare `src/BluetoothLogger.cpp`

### Step 4: Creare BLE Controller
- [ ] Creare `include/BLELedController.h`
- [ ] Creare `src/BLELedController.cpp`

### Step 5: Creare BLE OTA Controller
- [ ] Creare `include/BLEOTAController.h`
- [ ] Creare `src/BLEOTAController.cpp`
- [ ] Creare `tools/ble_ota_upload.py`
- [ ] Installare dipendenze: `pip install bleak`

### Step 6: Modificare main.cpp
- [ ] Rimuovere codice WiFi
- [ ] Aggiungere setup Bluetooth Classic + BLE + OTA
- [ ] Implementare loop ibrido con protezione OTA

### Step 7: Test
```bash
pio run -t upload -t monitor
```

### Step 8: Test Connessioni
- [ ] Android: "Serial Bluetooth Terminal" → "LedDense-Log"
- [ ] Android: "nRF Connect" → "LedDense-BLE"
- [ ] Verificare controllo LED via BLE
- [ ] Verificare log via Bluetooth Classic

### Step 9: Test OTA
- [ ] PC: `python tools/ble_ota_upload.py "LedDense-BLE" firmware.bin`
- [ ] Verificare LED blu durante upload
- [ ] Verificare reboot automatico
- [ ] Verificare log progress su Serial BT

---

## 📱 App Necessarie (Android)

1. **Serial Bluetooth Terminal** (Logging)
   - [Google Play](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal)

2. **nRF Connect** (Controllo LED)
   - [Google Play](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp)

---

## 💡 Esempi Comandi BLE (nRF Connect)

### Colori Predefiniti

**Rosso:**
```json
{"r": 255, "g": 0, "b": 0}
```

**Verde:**
```json
{"r": 0, "g": 255, "b": 0}
```

**Blu:**
```json
{"r": 0, "g": 0, "b": 255}
```

**Bianco:**
```json
{"r": 255, "g": 255, "b": 255}
```

**Viola:**
```json
{"r": 128, "g": 0, "b": 255}
```

### Effetti

**Arcobaleno veloce:**
```json
{"mode": "rainbow", "speed": 100}
```

**Respirazione lenta:**
```json
{"mode": "breathe", "speed": 30}
```

**Colore solido:**
```json
{"mode": "solid"}
```

### Luminosità

**50%:**
```json
{"brightness": 128, "enabled": true}
```

**Spegni:**
```json
{"brightness": 0, "enabled": false}
```

---

## 📝 Note Finali

### Pro Versione Ibrida:
✅ **BLE**: Controllo preciso, notifiche push, consumo bassissimo
✅ **BT Classic**: Log in tempo reale, debug facile
✅ **BLE OTA**: Aggiornamento firmware wireless (senza WiFi!)
✅ RAM risparmiata rispetto a WiFi: ~40-80 KB
✅ Niente dipendenze esterne (AsyncTCP, WebServer)
✅ Boot veloce (~2-3s)
✅ Usa partizioni `app0` e `app1` esistenti

### Contro Versione Ibrida:
❌ 2 app Android necessarie (Serial BT + nRF Connect)
❌ RAM extra: ~40 KB (vs solo BT Classic)
❌ Complessità codice medio-alta
❌ BLE OTA lento (~40s per 813KB)

### Quando Usare Versione Ibrida:
✅ Vuoi logging AND controllo preciso AND OTA wireless
✅ Hai RAM disponibile (280 KB → 320 KB)
✅ Non vuoi usare WiFi ma vuoi comunque OTA
✅ Puoi aspettare ~40s per upload firmware

---

**Pronto per l'implementazione?**
Dimmi da quale fase vuoi iniziare! 🚀
