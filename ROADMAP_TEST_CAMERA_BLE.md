# 📋 ROADMAP TEST CAMERA + BLE - Ambiente Controllato

## 🎯 Obiettivo

**Prerequisiti:**
- ✅ Test BLE completati con successo (vedi `ROADMAP_TEST_BLE.md`)
- ✅ Stack BLE validato e funzionante

**Workflow:**
1. Test BLE (completato) ← `ROADMAP_TEST_BLE.md`
2. **Test Camera + BLE (questo documento)**
3. Integrazione finale nel progetto principale

---

## 📁 Struttura Progetto Test

```
ledDense/
├── test_camera_ble/             # Progetto test camera isolato
│   ├── platformio.ini
│   ├── src/
│   │   └── main.cpp
│   ├── include/
│   │   ├── BluetoothLogger.h
│   │   └── BLECameraController.h
│   └── lib/
│       ├── BluetoothLogger/     # Riusa da test_ble
│       │   ├── BluetoothLogger.h
│       │   └── BluetoothLogger.cpp
│       └── BLECameraController/
│           ├── BLECameraController.h
│           └── BLECameraController.cpp
```

---

## 🎯 FASE 1: Setup Progetto Test

### 1.1 Creare cartella test

```bash
cd /home/reder/Documenti/PlatformIO/Projects/ledDense
mkdir -p test_camera_ble/src test_camera_ble/include test_camera_ble/lib
```

---

### 1.2 Creare `test_camera_ble/platformio.ini`

```ini
[env:esp32cam-test]
platform = espressif32
board = esp32cam
framework = arduino

monitor_speed = 115200
monitor_filters = esp32_exception_decoder

build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DBOARD_HAS_PSRAM
    -DCAMERA_MODEL_AI_THINKER

lib_deps =
    bblanchon/ArduinoJson@^7.0.0

upload_speed = 921600
```

**Note:**
- `esp32cam`: Board specifica ESP32-CAM (AI-Thinker)
- `CAMERA_MODEL_AI_THINKER`: Define per pin camera
- `BOARD_HAS_PSRAM`: Essenziale per buffer immagine

---

## 🎯 FASE 2: Implementazione BLECameraController

### 2.1 Creare `test_camera_ble/lib/BLECameraController/BLECameraController.h`

```cpp
#ifndef BLE_CAMERA_CONTROLLER_H
#define BLE_CAMERA_CONTROLLER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "esp_camera.h"

// UUIDs Camera Service
#define CAMERA_SERVICE_UUID         "a8b3c4d5-e6f7-4a5b-8c9d-0e1f2a3b4c5d"
#define CHAR_CAMERA_CONTROL_UUID    "a8b3c4d6-e6f7-4a5b-8c9d-0e1f2a3b4c5d"  // WRITE + NOTIFY
#define CHAR_CAMERA_DATA_UUID       "a8b3c4d7-e6f7-4a5b-8c9d-0e1f2a3b4c5d"  // READ + NOTIFY
#define CHAR_CAMERA_CONFIG_UUID     "a8b3c4d8-e6f7-4a5b-8c9d-0e1f2a3b4c5d"  // WRITE + READ

// Configurazione camera
struct CameraConfig {
    framesize_t frameSize = FRAMESIZE_VGA;  // 640x480
    int quality = 10;                        // 10 = buona qualità
    int brightness = 0;                      // -2 to 2
    int contrast = 0;                        // -2 to 2
};

class BLECameraController {
private:
    BLEServer* pServer;
    BLECharacteristic* pCharControl;
    BLECharacteristic* pCharData;
    BLECharacteristic* pCharConfig;
    bool deviceConnected;
    CameraConfig* config;
    bool captureInProgress;

    bool initCamera();
    bool capturePhoto(camera_fb_t** fb);
    void sendPhotoChunked(camera_fb_t* fb);

public:
    BLECameraController(CameraConfig* cfg);
    void begin(const char* deviceName);
    bool isConnected();
    bool isCaptureInProgress();

    friend class CameraServerCallbacks;
    friend class CameraControlCallbacks;
    friend class CameraConfigCallbacks;
};

#endif
```

---

### 2.2 Creare `test_camera_ble/lib/BLECameraController/BLECameraController.cpp`

```cpp
#include "BLECameraController.h"

// Pin ESP32-CAM (AI-Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Callback connessione
class CameraServerCallbacks: public BLEServerCallbacks {
    BLECameraController* controller;
public:
    CameraServerCallbacks(BLECameraController* ctrl) : controller(ctrl) {}

    void onConnect(BLEServer* pServer) {
        controller->deviceConnected = true;
        Serial.println("[CAM BLE] Client connected!");
    }

    void onDisconnect(BLEServer* pServer) {
        controller->deviceConnected = false;
        Serial.println("[CAM BLE] Client disconnected!");
        pServer->startAdvertising();
    }
};

// Callback comandi camera
class CameraControlCallbacks: public BLECharacteristicCallbacks {
    BLECameraController* controller;
public:
    CameraControlCallbacks(BLECameraController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) {
        String value = pChar->getValue().c_str();

        if (value == "CAPTURE") {
            if (controller->captureInProgress) {
                Serial.println("[CAM] Capture already in progress!");
                pChar->setValue("ERROR:BUSY");
                pChar->notify();
                return;
            }

            Serial.println("[CAM] Starting capture...");
            pChar->setValue("OK:CAPTURING");
            pChar->notify();

            camera_fb_t* fb = NULL;
            if (controller->capturePhoto(&fb)) {
                Serial.printf("[CAM] Photo captured: %d bytes\n", fb->len);

                // Invia foto via BLE
                controller->sendPhotoChunked(fb);

                // Libera buffer
                esp_camera_fb_return(fb);

                pChar->setValue("OK:COMPLETE");
                pChar->notify();
            } else {
                Serial.println("[CAM ERROR] Capture failed!");
                pChar->setValue("ERROR:CAPTURE_FAIL");
                pChar->notify();
            }

        } else if (value == "STATUS") {
            StaticJsonDocument<128> doc;
            doc["connected"] = controller->deviceConnected;
            doc["busy"] = controller->captureInProgress;
            doc["frameSize"] = controller->config->frameSize;
            doc["quality"] = controller->config->quality;

            String json;
            serializeJson(doc, json);
            pChar->setValue(json.c_str());
            pChar->notify();
        }
    }
};

// Callback configurazione camera
class CameraConfigCallbacks: public BLECharacteristicCallbacks {
    BLECameraController* controller;
public:
    CameraConfigCallbacks(BLECameraController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) {
        String value = pChar->getValue().c_str();

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            // Aggiorna configurazione
            if (doc.containsKey("frameSize")) {
                controller->config->frameSize = (framesize_t)doc["frameSize"].as<int>();
            }
            if (doc.containsKey("quality")) {
                controller->config->quality = doc["quality"];
            }
            if (doc.containsKey("brightness")) {
                controller->config->brightness = doc["brightness"];
            }

            Serial.printf("[CAM] Config updated: frameSize=%d, quality=%d\n",
                controller->config->frameSize, controller->config->quality);

            // Reinizializza camera con nuova config
            esp_camera_deinit();
            delay(100);
            controller->initCamera();
        }
    }
};

// Costruttore
BLECameraController::BLECameraController(CameraConfig* cfg) {
    config = cfg;
    deviceConnected = false;
    captureInProgress = false;
}

// Inizializzazione camera
bool BLECameraController::initCamera() {
    camera_config_t camera_config;
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = Y2_GPIO_NUM;
    camera_config.pin_d1 = Y3_GPIO_NUM;
    camera_config.pin_d2 = Y4_GPIO_NUM;
    camera_config.pin_d3 = Y5_GPIO_NUM;
    camera_config.pin_d4 = Y6_GPIO_NUM;
    camera_config.pin_d5 = Y7_GPIO_NUM;
    camera_config.pin_d6 = Y8_GPIO_NUM;
    camera_config.pin_d7 = Y9_GPIO_NUM;
    camera_config.pin_xclk = XCLK_GPIO_NUM;
    camera_config.pin_pclk = PCLK_GPIO_NUM;
    camera_config.pin_vsync = VSYNC_GPIO_NUM;
    camera_config.pin_href = HREF_GPIO_NUM;
    camera_config.pin_sscb_sda = SIOD_GPIO_NUM;
    camera_config.pin_sscb_scl = SIOC_GPIO_NUM;
    camera_config.pin_pwdn = PWDN_GPIO_NUM;
    camera_config.pin_reset = RESET_GPIO_NUM;
    camera_config.xclk_freq_hz = 20000000;
    camera_config.pixel_format = PIXFORMAT_JPEG;

    // Configurazione qualità
    camera_config.frame_size = config->frameSize;
    camera_config.jpeg_quality = config->quality;
    camera_config.fb_count = 1;

    // Inizializza
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("[CAM ERROR] Init failed: 0x%x\n", err);
        return false;
    }

    // Applica impostazioni
    sensor_t* s = esp_camera_sensor_get();
    s->set_brightness(s, config->brightness);
    s->set_contrast(s, config->contrast);

    Serial.println("[CAM OK] Camera initialized!");
    return true;
}

// Cattura foto
bool BLECameraController::capturePhoto(camera_fb_t** fb) {
    captureInProgress = true;

    *fb = esp_camera_fb_get();
    if (!*fb) {
        Serial.println("[CAM ERROR] Capture failed!");
        captureInProgress = false;
        return false;
    }

    captureInProgress = false;
    return true;
}

// Invio foto a chunk via BLE
void BLECameraController::sendPhotoChunked(camera_fb_t* fb) {
    const size_t CHUNK_SIZE = 512;  // BLE MTU limit
    size_t totalSize = fb->len;
    size_t offset = 0;

    Serial.printf("[CAM] Sending %d bytes in chunks of %d...\n", totalSize, CHUNK_SIZE);

    // Invia header con dimensione totale
    StaticJsonDocument<128> header;
    header["total"] = totalSize;
    header["chunks"] = (totalSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    String headerStr;
    serializeJson(header, headerStr);
    pCharControl->setValue(headerStr.c_str());
    pCharControl->notify();
    delay(50);

    // Invia chunk
    while (offset < totalSize) {
        size_t chunkSize = min(CHUNK_SIZE, totalSize - offset);

        pCharData->setValue(fb->buf + offset, chunkSize);
        pCharData->notify();

        offset += chunkSize;

        uint8_t progress = (offset * 100) / totalSize;
        Serial.printf("[CAM] Progress: %d%% (%d/%d)\r", progress, offset, totalSize);

        delay(20);  // Throttle per evitare overflow BLE
    }

    Serial.println("\n[CAM] Photo sent!");
}

// Inizializzazione BLE
void BLECameraController::begin(const char* deviceName) {
    // Inizializza camera prima
    if (!initCamera()) {
        Serial.println("[CAM ERROR] Camera init failed!");
        return;
    }

    // Inizializza BLE
    BLEDevice::init(deviceName);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new CameraServerCallbacks(this));

    BLEService* pService = pServer->createService(CAMERA_SERVICE_UUID);

    // Characteristic controllo
    pCharControl = pService->createCharacteristic(
        CHAR_CAMERA_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharControl->setCallbacks(new CameraControlCallbacks(this));
    pCharControl->addDescriptor(new BLE2902());

    // Characteristic dati foto
    pCharData = pService->createCharacteristic(
        CHAR_CAMERA_DATA_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharData->addDescriptor(new BLE2902());

    // Characteristic configurazione
    pCharConfig = pService->createCharacteristic(
        CHAR_CAMERA_CONFIG_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_READ
    );
    pCharConfig->setCallbacks(new CameraConfigCallbacks(this));

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(CAMERA_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("[CAM BLE] BLE Camera Service started!");
}

bool BLECameraController::isConnected() {
    return deviceConnected;
}

bool BLECameraController::isCaptureInProgress() {
    return captureInProgress;
}
```

---

## 🎯 FASE 3: Main di Test

### 3.1 Creare `test_camera_ble/src/main.cpp`

```cpp
#include <Arduino.h>
#include <BluetoothSerial.h>
#include "BluetoothLogger.h"
#include "BLECameraController.h"

// Bluetooth Classic
BluetoothSerial SerialBT;
BluetoothLogger logger(&SerialBT);

// Camera BLE
CameraConfig camConfig;
BLECameraController camController(&camConfig);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== TEST CAMERA + BLE ===");

    // Disabilita brownout detector (ESP32-CAM consuma molto)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Avvia Bluetooth Classic (Logging)
    logger.begin("TestCAM-Log");
    logger.log("*** Bluetooth Classic OK ***");

    // Avvia BLE Camera
    camController.begin("TestCAM-BLE");
    logger.log("*** BLE Camera Service OK ***");

    logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
    logger.log("*** READY FOR CAPTURE ***");
}

void loop() {
    static unsigned long lastMemLog = 0;
    const unsigned long now = millis();

    // Log memoria ogni 5 secondi
    if (now - lastMemLog > 5000) {
        logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
        logger.logf("BT: %s | BLE: %s | Capture: %s",
            logger.isConnected() ? "OK" : "NO",
            camController.isConnected() ? "OK" : "NO",
            camController.isCaptureInProgress() ? "BUSY" : "IDLE");
        lastMemLog = now;
    }

    // Comandi debug
    if (SerialBT.available()) {
        String cmd = SerialBT.readStringUntil('\n');
        cmd.trim();

        if (cmd == "ping") {
            logger.log("pong");

        } else if (cmd == "status") {
            logger.logf("Uptime: %lu ms", millis());
            logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
            logger.logf("Min free heap: %u bytes", ESP.getMinFreeHeap());

        } else if (cmd == "reset") {
            logger.log("Rebooting...");
            delay(500);
            ESP.restart();

        } else {
            logger.logf("Unknown: %s", cmd.c_str());
        }
    }

    yield();
}
```

---

## 🎯 FASE 4: Test Procedure

### 4.1 Build e Upload

```bash
cd test_camera_ble
pio run -t upload -t monitor
```

**Output atteso:**
```
=== TEST CAMERA + BLE ===
[CAM OK] Camera initialized!
[BT OK] Started: TestCAM-Log
[0ms] *** Bluetooth Classic OK ***
[CAM BLE] BLE Camera Service started!
[350ms] *** BLE Camera Service OK ***
[355ms] Free heap: 150000 bytes
[360ms] *** READY FOR CAPTURE ***
```

**Note:** Free heap ~150KB (camera usa PSRAM per buffer immagine)

---

### 4.2 Test Bluetooth Classic (Logging)

**App:** Serial Bluetooth Terminal

**Steps:**
1. Connect a "TestCAM-Log"
2. Invia comandi:
   - `ping` → "pong"
   - `status` → Vedi RAM, uptime

**Verifica:**
- ✅ Log in tempo reale
- ✅ Free heap stabile (~150KB)

---

### 4.3 Test BLE Camera - Cattura Foto

**App:** nRF Connect

**Steps:**
1. Scansiona → "TestCAM-BLE"
2. Connect
3. Naviga Service `a8b3c4d5...`

**Test 1: Cattura Foto VGA (640x480)**

1. Abilita notifiche su `CHAR_CAMERA_CONTROL_UUID`
2. Abilita notifiche su `CHAR_CAMERA_DATA_UUID`
3. Scrivi su `CHAR_CAMERA_CONTROL_UUID`:
   ```
   CAPTURE
   ```

**Output atteso su nRF Connect:**
- `CONTROL` riceve: `OK:CAPTURING`
- `CONTROL` riceve header JSON:
  ```json
  {"total": 25600, "chunks": 50}
  ```
- `DATA` riceve 50 notifiche con chunk da 512 byte
- `CONTROL` riceve: `OK:COMPLETE`

**Verifica:**
- ✅ Header ricevuto correttamente
- ✅ Tutti i chunk ricevuti
- ✅ Nessun errore BLE
- ✅ Log su Serial BT:
  ```
  [CAM] Starting capture...
  [CAM] Photo captured: 25600 bytes
  [CAM] Sending 25600 bytes in chunks of 512...
  [CAM] Progress: 100% (25600/25600)
  [CAM] Photo sent!
  ```

---

### 4.4 Test BLE Camera - Cambio Configurazione

**Test 2: Cambia Risoluzione a QVGA (320x240)**

Scrivi su `CHAR_CAMERA_CONFIG_UUID`:
```json
{"frameSize": 5, "quality": 12}
```

**Note:** `frameSize` values:
- 5 = QVGA (320x240)
- 6 = CIF (400x296)
- 8 = VGA (640x480)
- 10 = SVGA (800x600)

**Verifica:**
- ✅ Log: `[CAM] Config updated: frameSize=5, quality=12`
- ✅ Camera reinizializzata
- ✅ Prossima foto sarà più piccola (~10KB)

---

### 4.5 Test Sequenza Foto Multiple

**Procedura:**
1. Cattura foto 1 → Attendi `OK:COMPLETE`
2. Cattura foto 2 → Attendi `OK:COMPLETE`
3. Cattura foto 3 → Attendi `OK:COMPLETE`

**Verifica:**
- ✅ Nessun crash
- ✅ Ogni foto ricevuta completamente
- ✅ Free heap stabile (no memory leak)

---

## 🎯 FASE 5: Tool Python per Ricezione Foto

### 5.1 Creare `test_camera_ble/tools/ble_receive_photo.py`

```python
#!/usr/bin/env python3
"""
BLE Camera Photo Receiver
Riceve foto da ESP32-CAM via BLE e salva su disco
"""

import asyncio
import sys
import json
from datetime import datetime
from bleak import BleakClient, BleakScanner

CAMERA_SERVICE_UUID = "a8b3c4d5-e6f7-4a5b-8c9d-0e1f2a3b4c5d"
CHAR_CONTROL_UUID = "a8b3c4d6-e6f7-4a5b-8c9d-0e1f2a3b4c5d"
CHAR_DATA_UUID = "a8b3c4d7-e6f7-4a5b-8c9d-0e1f2a3b4c5d"

photo_buffer = bytearray()
total_size = 0
expected_chunks = 0

def control_callback(sender, data):
    global total_size, expected_chunks
    msg = data.decode('utf-8')
    print(f"[CONTROL] {msg}")

    # Parse header JSON
    if msg.startswith('{'):
        try:
            header = json.loads(msg)
            total_size = header['total']
            expected_chunks = header['chunks']
            print(f"[INFO] Expecting {total_size} bytes in {expected_chunks} chunks")
        except:
            pass

def data_callback(sender, data):
    global photo_buffer
    photo_buffer.extend(data)
    progress = (len(photo_buffer) * 100) // total_size if total_size > 0 else 0
    print(f"[DATA] Received chunk ({len(photo_buffer)}/{total_size} bytes - {progress}%)", end='\r')

async def capture_photo(device_name):
    global photo_buffer, total_size

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

        # Abilita notifiche
        await client.start_notify(CHAR_CONTROL_UUID, control_callback)
        await client.start_notify(CHAR_DATA_UUID, data_callback)

        print("Notifications enabled. Sending CAPTURE command...")

        # Reset buffer
        photo_buffer = bytearray()
        total_size = 0

        # Invia comando CAPTURE
        await client.write_gatt_char(CHAR_CONTROL_UUID, b"CAPTURE")

        # Attendi ricezione completa (timeout 30s)
        timeout = 30
        start = asyncio.get_event_loop().time()

        while len(photo_buffer) < total_size or total_size == 0:
            await asyncio.sleep(0.1)
            if asyncio.get_event_loop().time() - start > timeout:
                print("\n[ERROR] Timeout waiting for photo!")
                return

        print(f"\n[OK] Photo received: {len(photo_buffer)} bytes")

        # Salva foto
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"photo_{timestamp}.jpg"

        with open(filename, 'wb') as f:
            f.write(photo_buffer)

        print(f"[OK] Photo saved: {filename}")

        # Stop notifiche
        await client.stop_notify(CHAR_CONTROL_UUID)
        await client.stop_notify(CHAR_DATA_UUID)

if __name__ == "__main__":
    device_name = sys.argv[1] if len(sys.argv) > 1 else "TestCAM-BLE"
    asyncio.run(capture_photo(device_name))
```

**Installazione:**
```bash
pip install bleak
```

**Uso:**
```bash
python tools/ble_receive_photo.py "TestCAM-BLE"
```

**Output:**
```
Scanning for TestCAM-BLE...
Connecting to XX:XX:XX:XX:XX:XX...
Connected!
Notifications enabled. Sending CAPTURE command...
[CONTROL] OK:CAPTURING
[INFO] Expecting 25600 bytes in 50 chunks
[DATA] Received chunk (25600/25600 bytes - 100%)
[OK] Photo received: 25600 bytes
[OK] Photo saved: photo_20250116_143022.jpg
```

---

## 🎯 FASE 6: Test Performance

### 6.1 Test Dimensioni Foto

| Risoluzione | frameSize | Dimensione Tipica | Chunk | Tempo |
|-------------|-----------|-------------------|-------|-------|
| QVGA (320x240) | 5 | ~10 KB | 20 | ~5s |
| CIF (400x296) | 6 | ~15 KB | 30 | ~8s |
| VGA (640x480) | 8 | ~25 KB | 50 | ~12s |
| SVGA (800x600) | 10 | ~40 KB | 80 | ~20s |

**Target:**
- VGA (640x480) in < 15 secondi
- Nessun chunk perso

---

### 6.2 Test Memoria

**Comando:**
```cpp
// Nel loop, aggiungi:
if (cmd == "mem") {
    logger.logf("Total PSRAM: %u", ESP.getPsramSize());
    logger.logf("Free PSRAM: %u", ESP.getFreePsram());
    logger.logf("Free heap: %u", ESP.getFreeHeap());
    logger.logf("Min free heap: %u", ESP.getMinFreeHeap());
}
```

**Target:**
- PSRAM disponibile: > 2 MB
- Heap disponibile: > 100 KB
- No memory leak dopo 10 foto

---

## 📊 Checklist Validazione

### Camera
- [ ] Inizializzazione camera OK
- [ ] Cattura foto VGA funziona
- [ ] Cattura foto QVGA funziona
- [ ] Cambio configurazione funziona
- [ ] 10 foto consecutive senza crash

### BLE Transfer
- [ ] Header JSON ricevuto correttamente
- [ ] Tutti i chunk ricevuti
- [ ] Nessun chunk duplicato
- [ ] Nessun chunk perso
- [ ] Foto ricostruita identica

### Performance
- [ ] VGA (640x480) in < 15s
- [ ] QVGA (320x240) in < 8s
- [ ] Throughput > 2 KB/s
- [ ] Latenza chunk < 50ms

### Stabilità
- [ ] 10 foto senza memory leak
- [ ] Free heap stabile
- [ ] Nessun crash
- [ ] Reconnect funzionante

---

## 🐛 Troubleshooting

### Camera init failed: 0x105
```
[CAM ERROR] Init failed: 0x105
```
**Soluzione:**
- Verifica pin ESP32-CAM (AI-Thinker)
- Disabilita brownout: `WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0)`
- Verifica alimentazione 5V/2A

---

### Out of memory durante capture
```
[CAM ERROR] Capture failed!
```
**Soluzione:**
- Riduci `frameSize` (usa QVGA)
- Aumenta `quality` (12-15)
- Verifica PSRAM abilitato: `ESP.getPsramSize()`

---

### Chunk persi durante transfer
```
[DATA] Received 48/50 chunks
```
**Soluzione:**
- Aumenta delay tra chunk: `delay(50)`
- Riduci `CHUNK_SIZE` a 256 byte
- Verifica signal strength BLE

---

### Foto corrotta/sfocata
```
Photo saved but corrupted
```
**Soluzione:**
- Aumenta `xclk_freq_hz` a 20MHz
- Riduci `jpeg_quality` (8-10 = migliore)
- Verifica illuminazione adeguata

---

## 📝 Note Finali

### Successo Test
Se tutti i test passano:
1. ✅ Camera funzionante
2. ✅ BLE transfer validato
3. ✅ Performance accettabile
4. ✅ **Pronto per integrazione finale**

**Prossimo step:**
→ Integra `BLECameraController` nel progetto principale

### Fallimento Test
- Verifica hardware ESP32-CAM (pin, alimentazione)
- Test camera standalone (senza BLE)
- Riduci risoluzione foto
- Verifica PSRAM funzionante

---

## 🚀 Comandi Rapidi

```bash
# Build + Upload
cd test_camera_ble && pio run -t upload -t monitor

# Cattura foto da PC
python tools/ble_receive_photo.py "TestCAM-BLE"

# Monitor log
pio device monitor -b 115200

# Clean
pio run -t clean
```

---

**Tempo stimato test completo:** ~3-4 ore

**Prerequisiti completati:**
- ✅ Test BLE (`ROADMAP_TEST_BLE.md`)
- ✅ Test Camera + BLE (questo documento)

**Next:** Integrazione nel progetto principale! 🚀
