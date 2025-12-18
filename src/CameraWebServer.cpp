#include "CameraWebServer.h"
#include <ArduinoJson.h>
#include "esp_camera.h"

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
    Serial.printf("[WebServer] Camera pointer: %p\n", camera);
    Serial.printf("[WebServer] Detector pointer: %p\n", detector);

    if (!camera) {
        Serial.println("[WebServer] ERROR: Camera pointer is NULL!");
        return;
    }

    if (!camera->isInitialized()) {
        Serial.println("[WebServer] ERROR: Camera is not initialized!");
        return;
    }

    if (!detector) {
        Serial.println("[WebServer] ERROR: Detector pointer is NULL!");
        return;
    }

    _server = new AsyncWebServer(_port);

    if (!_server) {
        Serial.println("[WebServer] ERROR: Failed to create AsyncWebServer!");
        return;
    }

    Serial.println("[WebServer] AsyncWebServer created successfully");

    // Route: Dashboard
    Serial.println("[WebServer] Registering route: /");
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] Request received: /");
        request->send_P(200, "text/html", DASHBOARD_HTML);
    });

    // Route: MJPEG Stream
    Serial.println("[WebServer] Registering route: /stream");
    _server->on("/stream", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] Request received: /stream");
        _handleStream(request);
    });

    // Route: Overlay JSON
    Serial.println("[WebServer] Registering route: /overlay");
    _server->on("/overlay", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] Request received: /overlay");
        _handleOverlay(request);
    });

    // Route: Snapshot singolo
    Serial.println("[WebServer] Registering route: /snapshot");
    _server->on("/snapshot", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] Request received: /snapshot");
        _handleSnapshot(request);
    });

    // Route: Metriche JSON
    Serial.println("[WebServer] Registering route: /metrics");
    _server->on("/metrics", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] Request received: /metrics");
        _handleMetrics(request);
    });

    Serial.println("[WebServer] All routes registered, calling begin()...");
    _server->begin();
    _running = true;

    Serial.printf("[WebServer] Started! URL: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("[WebServer] Server is now listening for connections");
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
