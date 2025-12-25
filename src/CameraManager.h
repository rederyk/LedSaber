#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <Arduino.h>
#include <esp_camera.h>

/**
 * @brief Gestisce la camera ESP32-CAM OV2640 per motion detection
 *
 * Implementa:
 * - Init camera in modalità QVGA grayscale
 * - Cattura frame senza blocco
 * - Gestione buffer circolari in PSRAM
 * - Monitoraggio metriche (fps, memoria)
 */
class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    /**
     * @brief Inizializza la camera con configurazione ottimizzata
     * @param flashPin Pin per flash LED (default: 4)
     * @return true se inizializzazione OK
     */
    bool begin(uint8_t flashPin = 4);

    /**
     * @brief Cattura un frame dalla camera
     * @param outBuffer Puntatore al buffer frame
     * @param outLength Lunghezza dati frame
     * @return true se cattura OK
     */
    bool captureFrame(uint8_t** outBuffer, size_t* outLength);

    /**
     * @brief Rilascia il frame buffer (OBBLIGATORIO dopo captureFrame!)
     */
    void releaseFrame();

    /**
     * @brief Attiva/disattiva flash LED
     * @param enabled true per accendere flash
     * @param brightness Intensità flash (0-255)
     */
    void setFlash(bool enabled, uint8_t brightness = 255);

    /**
     * @brief Ultima intensità richiesta per il flash (manuale)
     */
    uint8_t getFlashBrightness() const { return _flashBrightness; }

    /**
     * @brief Verifica se il flash è attualmente acceso
     * @return true se flash abilitato
     */
    bool isFlashEnabled() const { return _flashEnabled; }

    /**
     * @brief Stato inizializzazione camera
     */
    bool isInitialized() const { return _initialized; }

    /**
     * @brief De-inizializza la camera e rilascia le risorse
     */
    void deinit();

    /**
     * @brief Ottiene metriche camera
     */
    struct CameraMetrics {
        uint32_t totalFramesCaptured;
        uint32_t failedCaptures;
        uint32_t lastFrameSize;
        uint32_t lastCaptureTime;
        float currentFps;
    };

    CameraMetrics getMetrics() const { return _metrics; }

    /**
     * @brief Reset statistiche
     */
    void resetMetrics();

private:
    bool _initialized;
    uint8_t _flashPin;
    bool _flashEnabled;
    uint8_t _flashBrightness;
    camera_fb_t* _currentFrameBuffer;

    CameraMetrics _metrics;

    // FPS tracking
    unsigned long _lastFrameTime;
    uint32_t _frameCount;
    unsigned long _fpsStartTime;

    /**
     * @brief Configura pinout ESP32-CAM (AI-Thinker)
     */
    void _configurePinout(camera_config_t& config);
};

#endif // CAMERA_MANAGER_H
