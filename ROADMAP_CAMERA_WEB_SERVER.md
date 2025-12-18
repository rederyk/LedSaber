# 📸 LedSaber Camera Web Server - Roadmap Implementazione

## 🎯 Obiettivo
Implementare un web server leggero ESP32-CAM per streaming MJPEG con overlay real-time dei blocchi optical flow rilevati e direzioni movimento. Attivabile via BLE GATT per training gesture e debug.

---

## 🏗️ Architettura Sistema

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32-CAM Device                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐    ┌──────────────────┐                   │
│  │ BLE GATT     │───▶│ WiFiManager      │                   │
│  │ Service      │    │ (STA Mode only)  │                   │
│  └──────────────┘    └──────────────────┘                   │
│         │                      │                             │
│         │                      ▼                             │
│         │            ┌──────────────────┐                   │
│         │            │ AsyncWebServer   │                   │
│         │            │ Port 80          │                   │
│         │            └──────────────────┘                   │
│         │                      │                             │
│         │            ┌──────────────────────┐               │
│         │            │ /                    │ Dashboard     │
│         │            │ /stream              │ MJPEG Stream  │
│         │            │ /overlay             │ Overlay Data  │
│         │            │ /wifi/status         │ WiFi Info     │
│         │            └──────────────────────┘               │
│         │                      │                             │
│         └────────┬─────────────┘                             │
│                  ▼                                           │
│         ┌──────────────────┐                                │
│         │ CameraManager    │                                │
│         │ + Overlay        │                                │
│         │ Renderer         │                                │
│         └──────────────────┘                                │
│                  │                                           │
│                  ▼                                           │
│         ┌──────────────────┐                                │
│         │ OpticalFlow      │                                │
│         │ Detector         │                                │
│         └──────────────────┘                                │
│                                                               │
└─────────────────────────────────────────────────────────────┘
                       │
                       ▼
              ┌────────────────┐
              │ Browser Client │
              │ (Phone/Laptop) │
              └────────────────┘
```

---

## 📋 Fase 1: Preparazione Dipendenze ⚙️

### 1.1 Aggiungere Librerie PlatformIO

**File**: `platformio.ini`

```ini
lib_deps =
    fastled/FastLED@^3.7.0
    bblanchon/ArduinoJson@^7.0.0
    me-no-dev/ESP Async WebServer@^1.2.3      ; Web server asincrono
    me-no-dev/AsyncTCP@^1.1.1                 ; Dipendenza per ESP32
```

**Note**:
- `ESPAsyncWebServer` è **non bloccante** → ideale per streaming
- Usa `AsyncTCP` invece di TCP sincrono → evita stalli
- Memoria aggiuntiva richiesta: ~15KB RAM + ~50KB Flash

### 1.2 Configurazione Build Flags

**File**: `platformio.ini`

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=0
    -Os
    -DBOARD_HAS_PSRAM
    -DPSRAM_MODE=psram_64m
    -DCONFIG_SPIRAM_SUPPORT=1

    ; WiFi Camera Debug (commentare per disabilitare)
    -DENABLE_CAMERA_WEB_SERVER=1              ; Flag principale
    -DCAMERA_WEB_PORT=80                       ; Porta web server
    -DCAMERA_STREAM_FPS=10                     ; FPS streaming (default: 10)
```

**Deliverable**: ✅ platformio.ini aggiornato con dipendenze e flags

---

## 📋 Fase 2: BLE WiFi Service (Controllo Remoto)

### 2.1 Definire Servizio BLE WiFi

**File**: `src/BLEWiFiService.h`

```cpp
#ifndef BLE_WIFI_SERVICE_H
#define BLE_WIFI_SERVICE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <WiFi.h>

/**
 * @brief Servizio BLE per controllo WiFi da remoto
 *
 * Permette di:
 * - Attivare/disattivare WiFi STA mode
 * - Configurare SSID e password
 * - Ricevere notifiche stato connessione
 * - Ottenere IP address assegnato
 */
class BLEWiFiService {
public:
    BLEWiFiService();

    /**
     * @brief Inizializza servizio BLE WiFi
     * @param pServer Puntatore al BLEServer esistente
     */
    void begin(BLEServer* pServer);

    /**
     * @brief Aggiorna stato connessione WiFi (chiamare in loop)
     */
    void update();

    /**
     * @brief Ottieni stato WiFi
     */
    bool isWiFiEnabled() const { return _wifiEnabled; }
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    String getIPAddress() const;

private:
    // UUIDs (generare con uuidgen)
    static constexpr const char* SERVICE_UUID = "8a7f1234-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_CONTROL_UUID = "8a7f1235-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_STATUS_UUID = "8a7f1236-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_SSID_UUID = "8a7f1237-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_PASSWORD_UUID = "8a7f1238-5678-90ab-cdef-1234567890ac";

    BLECharacteristic* _pCharControl;
    BLECharacteristic* _pCharStatus;
    BLECharacteristic* _pCharSSID;
    BLECharacteristic* _pCharPassword;

    bool _wifiEnabled;
    bool _wasConnected;
    String _ssid;
    String _password;
    unsigned long _lastStatusUpdate;

    void _handleControlCommand(const std::string& value);
    void _notifyStatus();
    void _connectWiFi();
    void _disconnectWiFi();
};

#endif // BLE_WIFI_SERVICE_H
```

### 2.2 Comandi BLE WiFi

**Characteristic**: `CHAR_WIFI_CONTROL_UUID` (Write)

| Comando | Payload | Descrizione |
|---------|---------|-------------|
| `wifi_enable` | `{"cmd":"enable"}` | Attiva WiFi e connette |
| `wifi_disable` | `{"cmd":"disable"}` | Disattiva WiFi |
| `wifi_configure` | `{"ssid":"MyWiFi","pass":"password"}` | Configura credenziali |
| `wifi_status` | `{"cmd":"status"}` | Richiedi status update |

**Characteristic**: `CHAR_WIFI_STATUS_UUID` (Notify)

**Payload JSON**:
```json
{
  "enabled": true,
  "connected": true,
  "ssid": "MyHomeWiFi",
  "ip": "192.168.1.100",
  "rssi": -45,
  "url": "http://192.168.1.100"
}
```

### 2.3 Implementazione BLEWiFiService

**File**: `src/BLEWiFiService.cpp`

```cpp
#include "BLEWiFiService.h"
#include <ArduinoJson.h>

// Callback per CHAR_WIFI_CONTROL
class WiFiControlCallbacks : public BLECharacteristicCallbacks {
public:
    WiFiControlCallbacks(BLEWiFiService* service) : _service(service) {}

    void onWrite(BLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            _service->_handleControlCommand(value);
        }
    }

private:
    BLEWiFiService* _service;
};

BLEWiFiService::BLEWiFiService()
    : _pCharControl(nullptr)
    , _pCharStatus(nullptr)
    , _pCharSSID(nullptr)
    , _pCharPassword(nullptr)
    , _wifiEnabled(false)
    , _wasConnected(false)
    , _lastStatusUpdate(0)
{
}

void BLEWiFiService::begin(BLEServer* pServer) {
    Serial.println("[BLE WiFi] Initializing service...");

    // Crea servizio
    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Characteristic: WiFi Control (Write)
    _pCharControl = pService->createCharacteristic(
        CHAR_WIFI_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pCharControl->setCallbacks(new WiFiControlCallbacks(this));

    // Characteristic: WiFi Status (Notify)
    _pCharStatus = pService->createCharacteristic(
        CHAR_WIFI_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _pCharStatus->addDescriptor(new BLE2902());  // Enable notifications

    // Characteristic: SSID (Read/Write)
    _pCharSSID = pService->createCharacteristic(
        CHAR_WIFI_SSID_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

    // Characteristic: Password (Write Only - sicurezza)
    _pCharPassword = pService->createCharacteristic(
        CHAR_WIFI_PASSWORD_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    // Avvia servizio
    pService->start();

    Serial.println("[BLE WiFi] Service started!");
}

void BLEWiFiService::update() {
    // Verifica cambio stato connessione
    bool connected = isConnected();
    if (connected != _wasConnected) {
        _wasConnected = connected;
        _notifyStatus();
    }

    // Notifica periodica ogni 5s se WiFi attivo
    if (_wifiEnabled && (millis() - _lastStatusUpdate > 5000)) {
        _notifyStatus();
    }
}

void BLEWiFiService::_handleControlCommand(const std::string& value) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value.c_str());

    if (error) {
        Serial.printf("[BLE WiFi] JSON parse error: %s\n", error.c_str());
        return;
    }

    String cmd = doc["cmd"].as<String>();
    Serial.printf("[BLE WiFi] Command received: %s\n", cmd.c_str());

    if (cmd == "enable") {
        _connectWiFi();
    } else if (cmd == "disable") {
        _disconnectWiFi();
    } else if (cmd == "status") {
        _notifyStatus();
    } else if (cmd == "configure") {
        _ssid = doc["ssid"].as<String>();
        _password = doc["pass"].as<String>();
        Serial.printf("[BLE WiFi] Credentials configured: SSID=%s\n", _ssid.c_str());

        // Salva in NVS per persistenza (opzionale)
        _pCharSSID->setValue(_ssid.c_str());

        // Auto-connect se WiFi già abilitato
        if (_wifiEnabled) {
            _connectWiFi();
        }
    }
}

void BLEWiFiService::_connectWiFi() {
    if (_ssid.isEmpty()) {
        Serial.println("[BLE WiFi] ERROR: No SSID configured!");
        _notifyStatus();
        return;
    }

    Serial.printf("[BLE WiFi] Connecting to: %s\n", _ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    _wifiEnabled = true;

    // Timeout 15s per connessione
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < 15000)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[BLE WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[BLE WiFi] Connection failed!");
    }

    _notifyStatus();
}

void BLEWiFiService::_disconnectWiFi() {
    Serial.println("[BLE WiFi] Disconnecting...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _wifiEnabled = false;
    _notifyStatus();
}

void BLEWiFiService::_notifyStatus() {
    JsonDocument doc;

    doc["enabled"] = _wifiEnabled;
    doc["connected"] = isConnected();
    doc["ssid"] = _ssid;

    if (isConnected()) {
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["url"] = "http://" + WiFi.localIP().toString();
    } else {
        doc["ip"] = "";
        doc["rssi"] = 0;
        doc["url"] = "";
    }

    String jsonStr;
    serializeJson(doc, jsonStr);

    _pCharStatus->setValue(jsonStr.c_str());
    _pCharStatus->notify();

    _lastStatusUpdate = millis();

    Serial.printf("[BLE WiFi] Status notified: %s\n", jsonStr.c_str());
}

String BLEWiFiService::getIPAddress() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "";
}
```

**Deliverable**: ✅ Servizio BLE per attivazione WiFi via GATT

---

## 📋 Fase 3: Overlay Renderer (Visualizzazione Blocchi)

### 3.1 Struttura Dati Overlay

**File**: `src/OverlayRenderer.h`

```cpp
#ifndef OVERLAY_RENDERER_H
#define OVERLAY_RENDERER_H

#include <Arduino.h>
#include "OpticalFlowDetector.h"

/**
 * @brief Renderer per overlay optical flow su frame JPEG
 *
 * Disegna su frame JPEG:
 * - Griglia blocchi optical flow
 * - Vettori movimento (frecce)
 * - Confidence (colore)
 * - Direzione globale (testo)
 * - Statistiche (FPS, intensity)
 */
class OverlayRenderer {
public:
    OverlayRenderer();

    /**
     * @brief Renderizza overlay su frame buffer
     * @param frameBuffer Buffer JPEG output
     * @param maxSize Dimensione massima buffer
     * @param detector Puntatore al motion detector
     * @return Dimensione JPEG con overlay
     */
    size_t renderOverlay(
        uint8_t* frameBuffer,
        size_t maxSize,
        const OpticalFlowDetector* detector
    );

    /**
     * @brief Genera JSON con dati overlay (per rendering client-side)
     * @param detector Puntatore al motion detector
     * @return JSON string
     */
    String generateOverlayJSON(const OpticalFlowDetector* detector);

private:
    // Colori RGB565 per overlay
    static constexpr uint16_t COLOR_GRID = 0x07E0;      // Verde
    static constexpr uint16_t COLOR_VECTOR_HIGH = 0xF800; // Rosso
    static constexpr uint16_t COLOR_VECTOR_MED = 0xFD20;  // Arancione
    static constexpr uint16_t COLOR_VECTOR_LOW = 0x07FF;  // Ciano
    static constexpr uint16_t COLOR_TEXT = 0xFFFF;        // Bianco

    void _drawGrid(uint8_t* fb, uint16_t width, uint16_t height);
    void _drawVector(uint8_t* fb, uint16_t x, uint16_t y, int8_t dx, int8_t dy, uint8_t confidence);
    void _drawText(uint8_t* fb, uint16_t x, uint16_t y, const char* text);
};

#endif // OVERLAY_RENDERER_H
```

### 3.2 Implementazione Rendering Client-Side (Preferito)

**Motivo**: Rendering server-side su JPEG è **molto pesante** (richiede decodifica, disegno, ricodifica).

**Soluzione**: Inviare dati overlay come JSON separato e disegnare in JavaScript sul client.

**File**: `src/OverlayRenderer.cpp`

```cpp
#include "OverlayRenderer.h"
#include <ArduinoJson.h>

OverlayRenderer::OverlayRenderer() {
}

String OverlayRenderer::generateOverlayJSON(const OpticalFlowDetector* detector) {
    if (!detector) {
        return "{}";
    }

    JsonDocument doc;

    // Metriche globali
    auto metrics = detector->getMetrics();
    doc["intensity"] = metrics.currentIntensity;
    doc["speed"] = metrics.avgSpeed;
    doc["direction"] = OpticalFlowDetector::directionToString(metrics.dominantDirection);
    doc["active"] = metrics.motionActive;
    doc["activeBlocks"] = metrics.avgActiveBlocks;
    doc["confidence"] = (int)(metrics.avgConfidence * 100);

    // Frame info
    doc["frameWidth"] = 320;
    doc["frameHeight"] = 240;
    doc["blockSize"] = 40;
    doc["gridCols"] = 8;
    doc["gridRows"] = 6;

    // Blocchi con vettori movimento (solo blocchi validi per ridurre JSON)
    JsonArray blocks = doc["blocks"].to<JsonArray>();

    for (uint8_t row = 0; row < 6; row++) {
        for (uint8_t col = 0; col < 8; col++) {
            // Ottieni vettore blocco (dobbiamo aggiungere metodo getter)
            // Per ora usiamo approccio semplificato

            JsonObject block = blocks.add<JsonObject>();
            block["row"] = row;
            block["col"] = col;
            block["x"] = col * 40;
            block["y"] = row * 40;

            // Questi dati li otterremo dal detector
            // (richiede aggiunta di metodo pubblico getBlockVector)
            block["dx"] = 0;  // Placeholder
            block["dy"] = 0;  // Placeholder
            block["confidence"] = 0;  // Placeholder
            block["valid"] = false;  // Placeholder
        }
    }

    String jsonStr;
    serializeJson(doc, jsonStr);
    return jsonStr;
}
```

### 3.3 Aggiungere Metodo Pubblico in OpticalFlowDetector

**File**: `src/OpticalFlowDetector.h` (aggiungere nella sezione PUBLIC API)

```cpp
/**
 * @brief Ottieni vettore movimento di un blocco specifico
 * @param row Riga blocco (0-5)
 * @param col Colonna blocco (0-7)
 * @param outDx Output: componente X vettore
 * @param outDy Output: componente Y vettore
 * @param outConfidence Output: confidence (0-255)
 * @return true se blocco valido
 */
bool getBlockVector(uint8_t row, uint8_t col, int8_t* outDx, int8_t* outDy, uint8_t* outConfidence) const;
```

**File**: `src/OpticalFlowDetector.cpp` (implementazione)

```cpp
bool OpticalFlowDetector::getBlockVector(uint8_t row, uint8_t col, int8_t* outDx, int8_t* outDy, uint8_t* outConfidence) const {
    if (row >= GRID_ROWS || col >= GRID_COLS) {
        return false;
    }

    const BlockMotionVector& vec = _motionVectors[row][col];

    if (outDx) *outDx = vec.dx;
    if (outDy) *outDy = vec.dy;
    if (outConfidence) *outConfidence = vec.confidence;

    return vec.valid;
}
```

**Deliverable**: ✅ Generatore JSON overlay per rendering client-side

---

## 📋 Fase 4: Web Server Asincrono

### 4.1 Struttura CameraWebServer

**File**: `src/CameraWebServer.h`

```cpp
#ifndef CAMERA_WEB_SERVER_H
#define CAMERA_WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include "CameraManager.h"
#include "OpticalFlowDetector.h"
#include "OverlayRenderer.h"

/**
 * @brief Web server asincrono per streaming camera + overlay
 *
 * Endpoints:
 * - GET /              → Dashboard HTML
 * - GET /stream        → MJPEG stream
 * - GET /overlay       → JSON overlay data (SSE)
 * - GET /snapshot      → Singolo frame JPEG
 * - GET /metrics       → JSON metriche detector
 */
class CameraWebServer {
public:
    CameraWebServer(uint16_t port = 80);
    ~CameraWebServer();

    /**
     * @brief Inizializza web server
     * @param camera Puntatore al CameraManager
     * @param detector Puntatore al OpticalFlowDetector
     */
    void begin(CameraManager* camera, OpticalFlowDetector* detector);

    /**
     * @brief Ferma web server
     */
    void end();

    /**
     * @brief Verifica se server è attivo
     */
    bool isRunning() const { return _running; }

private:
    AsyncWebServer* _server;
    CameraManager* _camera;
    OpticalFlowDetector* _detector;
    OverlayRenderer _renderer;
    uint16_t _port;
    bool _running;

    // Request handlers
    void _handleRoot(AsyncWebServerRequest* request);
    void _handleStream(AsyncWebServerRequest* request);
    void _handleOverlay(AsyncWebServerRequest* request);
    void _handleSnapshot(AsyncWebServerRequest* request);
    void _handleMetrics(AsyncWebServerRequest* request);

    // Streaming helper
    static void _streamTask(void* param);
};

#endif // CAMERA_WEB_SERVER_H
```

### 4.2 Implementazione Endpoints

**File**: `src/CameraWebServer.cpp`

```cpp
#include "CameraWebServer.h"
#include <ArduinoJson.h>

// HTML Dashboard (embedded)
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LedSaber Camera Debug</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #1a1a1a;
            color: #fff;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { margin-bottom: 20px; color: #4fc3f7; }

        .video-container {
            position: relative;
            width: 640px;
            height: 480px;
            margin: 0 auto 20px;
            background: #000;
            border-radius: 8px;
            overflow: hidden;
        }

        #stream {
            width: 100%;
            height: 100%;
            object-fit: contain;
        }

        #overlay-canvas {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            pointer-events: none;
        }

        .stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }

        .stat-card {
            background: #2a2a2a;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #4fc3f7;
        }

        .stat-label {
            font-size: 12px;
            color: #aaa;
            margin-bottom: 5px;
        }

        .stat-value {
            font-size: 24px;
            font-weight: bold;
            color: #4fc3f7;
        }

        .direction {
            display: inline-block;
            padding: 5px 15px;
            background: #ff4081;
            border-radius: 20px;
            font-size: 14px;
            margin-top: 5px;
        }

        .active { color: #4caf50; }
        .inactive { color: #f44336; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎥 LedSaber Camera Debug</h1>

        <div class="video-container">
            <img id="stream" src="/stream" alt="Camera Stream">
            <canvas id="overlay-canvas"></canvas>
        </div>

        <div class="stats">
            <div class="stat-card">
                <div class="stat-label">Motion Status</div>
                <div class="stat-value" id="status">-</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Intensity</div>
                <div class="stat-value" id="intensity">0</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Speed (px/frame)</div>
                <div class="stat-value" id="speed">0.0</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Direction</div>
                <div class="stat-value"><span id="direction" class="direction">NONE</span></div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Active Blocks</div>
                <div class="stat-value" id="activeBlocks">0</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Confidence</div>
                <div class="stat-value" id="confidence">0%</div>
            </div>
        </div>
    </div>

    <script>
        const canvas = document.getElementById('overlay-canvas');
        const ctx = canvas.getContext('2d');
        const stream = document.getElementById('stream');

        // Adatta canvas alla dimensione dell'immagine
        stream.onload = () => {
            canvas.width = stream.clientWidth;
            canvas.height = stream.clientHeight;
        };

        // Polling overlay data ogni 100ms
        setInterval(async () => {
            try {
                const response = await fetch('/overlay');
                const data = await response.json();
                updateOverlay(data);
                updateStats(data);
            } catch (error) {
                console.error('Overlay fetch error:', error);
            }
        }, 100);

        function updateStats(data) {
            document.getElementById('status').textContent = data.active ? 'ACTIVE' : 'IDLE';
            document.getElementById('status').className = data.active ? 'active' : 'inactive';
            document.getElementById('intensity').textContent = data.intensity;
            document.getElementById('speed').textContent = data.speed.toFixed(1);
            document.getElementById('direction').textContent = data.direction;
            document.getElementById('activeBlocks').textContent = data.activeBlocks;
            document.getElementById('confidence').textContent = data.confidence + '%';
        }

        function updateOverlay(data) {
            // Pulisci canvas
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            if (!data.blocks || data.blocks.length === 0) return;

            const scaleX = canvas.width / data.frameWidth;
            const scaleY = canvas.height / data.frameHeight;

            // Disegna griglia
            ctx.strokeStyle = 'rgba(0, 255, 0, 0.3)';
            ctx.lineWidth = 1;
            for (let col = 0; col <= data.gridCols; col++) {
                const x = col * data.blockSize * scaleX;
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, canvas.height);
                ctx.stroke();
            }
            for (let row = 0; row <= data.gridRows; row++) {
                const y = row * data.blockSize * scaleY;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(canvas.width, y);
                ctx.stroke();
            }

            // Disegna vettori
            data.blocks.forEach(block => {
                if (!block.valid || (block.dx === 0 && block.dy === 0)) return;

                const centerX = (block.x + data.blockSize / 2) * scaleX;
                const centerY = (block.y + data.blockSize / 2) * scaleY;
                const endX = centerX + block.dx * scaleX * 2;
                const endY = centerY + block.dy * scaleY * 2;

                // Colore basato su confidence
                const confidence = block.confidence / 255;
                if (confidence > 0.7) {
                    ctx.strokeStyle = 'rgba(255, 0, 0, 0.8)';  // Rosso
                } else if (confidence > 0.4) {
                    ctx.strokeStyle = 'rgba(255, 165, 0, 0.8)'; // Arancione
                } else {
                    ctx.strokeStyle = 'rgba(0, 255, 255, 0.6)'; // Ciano
                }

                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(centerX, centerY);
                ctx.lineTo(endX, endY);
                ctx.stroke();

                // Freccia
                drawArrowhead(ctx, centerX, centerY, endX, endY);
            });
        }

        function drawArrowhead(ctx, fromX, fromY, toX, toY) {
            const headlen = 8;
            const angle = Math.atan2(toY - fromY, toX - fromX);

            ctx.beginPath();
            ctx.moveTo(toX, toY);
            ctx.lineTo(
                toX - headlen * Math.cos(angle - Math.PI / 6),
                toY - headlen * Math.sin(angle - Math.PI / 6)
            );
            ctx.moveTo(toX, toY);
            ctx.lineTo(
                toX - headlen * Math.cos(angle + Math.PI / 6),
                toY - headlen * Math.sin(angle + Math.PI / 6)
            );
            ctx.stroke();
        }
    </script>
</body>
</html>
)rawliteral";

CameraWebServer::CameraWebServer(uint16_t port)
    : _server(nullptr)
    , _camera(nullptr)
    , _detector(nullptr)
    , _port(port)
    , _running(false)
{
}

CameraWebServer::~CameraWebServer() {
    end();
}

void CameraWebServer::begin(CameraManager* camera, OpticalFlowDetector* detector) {
    if (_running) {
        Serial.println("[WebServer] Already running");
        return;
    }

    _camera = camera;
    _detector = detector;

    Serial.printf("[WebServer] Starting on port %u...\n", _port);

    _server = new AsyncWebServer(_port);

    // Route: Dashboard
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", DASHBOARD_HTML);
    });

    // Route: MJPEG Stream
    _server->on("/stream", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleStream(request);
    });

    // Route: Overlay JSON
    _server->on("/overlay", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleOverlay(request);
    });

    // Route: Snapshot singolo
    _server->on("/snapshot", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleSnapshot(request);
    });

    // Route: Metriche JSON
    _server->on("/metrics", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleMetrics(request);
    });

    _server->begin();
    _running = true;

    Serial.printf("[WebServer] Started! URL: http://%s\n", WiFi.localIP().toString().c_str());
}

void CameraWebServer::end() {
    if (_server) {
        _server->end();
        delete _server;
        _server = nullptr;
    }
    _running = false;
    Serial.println("[WebServer] Stopped");
}

void CameraWebServer::_handleStream(AsyncWebServerRequest* request) {
    // MJPEG multipart stream
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        "multipart/x-mixed-replace;boundary=frame",
        [this](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            // Cattura frame JPEG
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                return 0; // Fine stream
            }

            // Prepara header multipart
            static const char* BOUNDARY = "\r\n--frame\r\nContent-Type: image/jpeg\r\n\r\n";
            static const size_t BOUNDARY_LEN = strlen(BOUNDARY);

            size_t totalLen = BOUNDARY_LEN + fb->len;

            if (totalLen > maxLen) {
                esp_camera_fb_return(fb);
                return 0; // Buffer troppo piccolo
            }

            // Copia boundary + frame
            memcpy(buffer, BOUNDARY, BOUNDARY_LEN);
            memcpy(buffer + BOUNDARY_LEN, fb->buf, fb->len);

            esp_camera_fb_return(fb);

            return totalLen;
        }
    );

    request->send(response);
}

void CameraWebServer::_handleOverlay(AsyncWebServerRequest* request) {
    String json = _renderer.generateOverlayJSON(_detector);
    request->send(200, "application/json", json);
}

void CameraWebServer::_handleSnapshot(AsyncWebServerRequest* request) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        request->send(503, "text/plain", "Camera capture failed");
        return;
    }

    AsyncWebServerResponse* response = request->beginResponse_P(
        200,
        "image/jpeg",
        fb->buf,
        fb->len
    );

    esp_camera_fb_return(fb);
    request->send(response);
}

void CameraWebServer::_handleMetrics(AsyncWebServerRequest* request) {
    auto metrics = _detector->getMetrics();

    JsonDocument doc;
    doc["totalFrames"] = metrics.totalFramesProcessed;
    doc["motionFrames"] = metrics.motionFrameCount;
    doc["intensity"] = metrics.currentIntensity;
    doc["avgBrightness"] = metrics.avgBrightness;
    doc["flashIntensity"] = metrics.flashIntensity;
    doc["trajectoryLength"] = metrics.trajectoryLength;
    doc["motionActive"] = metrics.motionActive;
    doc["avgComputeTimeMs"] = metrics.avgComputeTimeMs;
    doc["avgConfidence"] = metrics.avgConfidence;
    doc["avgActiveBlocks"] = metrics.avgActiveBlocks;
    doc["direction"] = OpticalFlowDetector::directionToString(metrics.dominantDirection);
    doc["avgSpeed"] = metrics.avgSpeed;

    String jsonStr;
    serializeJson(doc, jsonStr);

    request->send(200, "application/json", jsonStr);
}
```

**Deliverable**: ✅ Web server asincrono con MJPEG streaming + overlay JSON

---

## 📋 Fase 5: Integrazione Main Loop

### 5.1 Modificare main.cpp

**File**: `src/main.cpp`

```cpp
#include "BLEWiFiService.h"
#include "CameraWebServer.h"

// Istanze globali
BLEWiFiService bleWiFiService;
CameraWebServer* webServer = nullptr;

void setup() {
    // ... setup esistente ...

    // Inizializza BLE WiFi Service
    bleWiFiService.begin(bleController.getServer());

    Serial.println("[MAIN] Setup complete!");
}

void loop() {
    // ... loop esistente ...

    // Aggiorna WiFi service
    bleWiFiService.update();

    // Avvia/ferma web server in base a stato WiFi
    if (bleWiFiService.isConnected() && webServer == nullptr) {
        webServer = new CameraWebServer(80);
        webServer->begin(&cameraManager, &motionDetector);
    } else if (!bleWiFiService.isConnected() && webServer != nullptr) {
        delete webServer;
        webServer = nullptr;
    }

    delay(10);
}
```

**Deliverable**: ✅ Integrazione completa in main loop

---

## 📋 Fase 6: Client Python Extension

### 6.1 Aggiungere Comandi WiFi al Controller

**File**: `tools/ledsaber_control.py`

```python
# WiFi Service UUIDs
WIFI_SERVICE_UUID = "8a7f1234-5678-90ab-cdef-1234567890ac"
WIFI_CONTROL_UUID = "8a7f1235-5678-90ab-cdef-1234567890ac"
WIFI_STATUS_UUID = "8a7f1236-5678-90ab-cdef-1234567890ac"

async def wifi_enable(client, ssid: str, password: str):
    """Attiva WiFi e connette a rete"""
    payload = json.dumps({"cmd": "configure", "ssid": ssid, "pass": password})
    await client.write_gatt_char(WIFI_CONTROL_UUID, payload.encode())

    await asyncio.sleep(1)

    payload = json.dumps({"cmd": "enable"})
    await client.write_gatt_char(WIFI_CONTROL_UUID, payload.encode())

    print("✓ WiFi enabled, waiting for connection...")

async def wifi_status(client):
    """Leggi stato WiFi"""
    status = await client.read_gatt_char(WIFI_STATUS_UUID)
    data = json.loads(status.decode())

    print(f"📡 WiFi Status:")
    print(f"  Enabled: {data['enabled']}")
    print(f"  Connected: {data['connected']}")
    print(f"  SSID: {data['ssid']}")
    if data['connected']:
        print(f"  IP: {data['ip']}")
        print(f"  RSSI: {data['rssi']} dBm")
        print(f"  🌐 Dashboard: {data['url']}")

# Aggiungere comandi CLI:
# wifi enable <ssid> <password>
# wifi disable
# wifi status
# wifi open  - apre browser su dashboard
```

**Deliverable**: ✅ Estensione Python con comandi WiFi

---

## 📋 Fase 7: Testing e Ottimizzazione

### 7.1 Test Checklist

- [ ] **BLE WiFi Service**: Attivazione WiFi via GATT
- [ ] **Connessione WiFi**: Timeout 15s, gestione errori
- [ ] **Web Server**: Dashboard accessibile su browser
- [ ] **MJPEG Streaming**: Streaming fluido 10 FPS
- [ ] **Overlay JSON**: Aggiornamento real-time 10Hz
- [ ] **Visualizzazione Blocchi**: Griglia + vettori corretti
- [ ] **Metriche Real-time**: Intensity, speed, direction
- [ ] **Memory Usage**: Heap free > 80KB con WiFi attivo
- [ ] **Stabilità**: 30 minuti streaming senza crash

### 7.2 Ottimizzazioni

**Performance**:
- Ridurre FPS streaming se CPU usage > 70%
- Usare task separato per MJPEG encoding
- Limitare overlay JSON a blocchi validi solo

**Memoria**:
- Deallocare web server quando WiFi disattivato
- Usare PSRAM per frame buffers
- Ridurre dimensione JSON overlay

**Network**:
- Implementare WebSocket per overlay (riduce overhead HTTP)
- Compressione gzip per JSON se browser supporta

**Deliverable**: ✅ Sistema ottimizzato e testato

---

## 🎯 Metriche di Successo

| Metrica | Target | Misurato |
|---------|--------|----------|
| Latency attivazione WiFi | < 20s | - |
| FPS streaming MJPEG | 10 fps | - |
| Overlay update rate | 10 Hz | - |
| Heap free con WiFi | > 80KB | - |
| CPU usage streaming | < 60% | - |
| Stabilità streaming | 30 min | - |
| Dimensione JSON overlay | < 2KB | - |
| Tempo apertura dashboard | < 3s | - |

---

## 🔧 Troubleshooting Comune

### WiFi non si connette
- Verificare SSID/password corretti
- Verificare segnale WiFi sufficiente (RSSI > -70 dBm)
- Provare riavvio ESP32
- Verificare router supporta 2.4GHz (ESP32 non supporta 5GHz)

### Stream MJPEG lento
- Ridurre FPS streaming (default: 10 fps)
- Verificare larghezza banda WiFi
- Ridurre risoluzione camera a QVGA
- Disabilitare overlay se non necessario

### Overlay non sincronizzato
- Verificare polling rate client (default: 100ms)
- Controllare latenza rete
- Verificare dimensione JSON < 2KB

### Crash dopo attivazione WiFi
- Verificare heap free > 100KB prima attivazione
- Ridurre fb_count camera a 1
- Disabilitare features non necessarie

---

## 📚 Risorse

### Hardware
- [ESP32 WiFi Stack](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html)

### Software
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
- [MJPEG Streaming](https://en.wikipedia.org/wiki/Motion_JPEG)

### Examples
- [ESP32-CAM MJPEG Stream Example](https://github.com/espressif/esp32-camera/blob/master/examples/main/take_picture.c)

---

**Versione**: 1.0
**Data**: 2025-12-18
**Autore**: LedSaber Team
**Status**: Ready for Implementation

---

## 📝 Note Implementazione

### Priorità Features
1. **Must Have**: BLE WiFi control + MJPEG streaming
2. **Should Have**: Overlay JSON + dashboard
3. **Nice to Have**: WebSocket per overlay, mDNS discovery

### Alternative Approach
Se memoria insufficiente, considerare:
- Streaming JPEG statico (no MJPEG) con refresh manuale
- Overlay server-side su JPEG (più pesante ma meno traffico)
- Risoluzione ridotta QQVGA (160x120) per debug

### Security Considerations
- WiFi password inviata via BLE (già criptato)
- Nessuna autenticazione web server (OK per uso locale/debug)
- Non esporre web server su internet pubblico

---

## ✅ Quick Start Checklist

Per implementare il web server camera:

1. [ ] Aggiungere dipendenze a `platformio.ini`
2. [ ] Creare `BLEWiFiService.h/cpp`
3. [ ] Creare `OverlayRenderer.h/cpp`
4. [ ] Creare `CameraWebServer.h/cpp`
5. [ ] Aggiungere metodo `getBlockVector()` in `OpticalFlowDetector`
6. [ ] Integrare in `main.cpp`
7. [ ] Estendere `ledsaber_control.py` con comandi WiFi
8. [ ] Testare attivazione WiFi via BLE
9. [ ] Testare dashboard browser
10. [ ] Testare streaming + overlay
11. [ ] Ottimizzare performance
12. [ ] Documentare uso finale

---

**🚀 Ready to implement!**
