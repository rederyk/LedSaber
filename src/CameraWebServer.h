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
 * - GET /overlay       → JSON overlay data
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
};

#endif // CAMERA_WEB_SERVER_H
