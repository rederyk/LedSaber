#include "CameraManager.h"

// Pinout ESP32-CAM (AI-Thinker)
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

CameraManager::CameraManager()
    : _initialized(false)
    , _flashPin(4)
    , _flashEnabled(false)
    , _flashBrightness(0)
    , _currentFrameBuffer(nullptr)
    , _lastFrameTime(0)
    , _frameCount(0)
    , _fpsStartTime(0)
{
    memset(&_metrics, 0, sizeof(_metrics));
}

CameraManager::~CameraManager() {
    if (_currentFrameBuffer) {
        esp_camera_fb_return(_currentFrameBuffer);
        _currentFrameBuffer = nullptr;
    }

    if (_initialized) {
        esp_camera_deinit();
        _initialized = false;
    }
}

void CameraManager::_configurePinout(camera_config_t& config) {
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
}

bool CameraManager::begin(uint8_t flashPin) {
    if (_initialized) {
        Serial.println("[CAMERA] Already initialized!");
        return true;
    }

    _flashPin = flashPin;

    // NOTA: Non configurare il pin qui, è già configurato come PWM in main.cpp
    // per evitare conflitti con il LED di stato (stesso pin, stesso canale PWM 0)

    Serial.println("[CAMERA] Initializing ESP32-CAM...");

    // Configurazione camera ottimizzata per motion detection
    camera_config_t config;
    memset(&config, 0, sizeof(config));

    // Pinout
    _configurePinout(config);

    // Configurazione
    config.xclk_freq_hz = 24000000;  // 24MHz clock (OV2640 max, era 20MHz)
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    // Formato immagine: VGA grayscale (verrà croppato a 480x480 dal motion detector)
    config.pixel_format = PIXFORMAT_GRAYSCALE;  // 1 byte/pixel
    config.frame_size = FRAMESIZE_VGA;          // 640x480 (High Res, limited FPS)
    config.jpeg_quality = 12;                    // Non usato per grayscale
    config.fb_count = 2;                         // Double buffering
    config.fb_location = CAMERA_FB_IN_PSRAM;     // Usa PSRAM per buffer
    config.grab_mode = CAMERA_GRAB_LATEST;       // Prendi sempre frame più recente

    // Inizializza camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAMERA ERROR] Init failed: 0x%x\n", err);
        return false;
    }

    // Ottimizzazioni sensore
    sensor_t* s = esp_camera_sensor_get();
    if (s != nullptr) {
        // Disabilita funzioni non necessarie per motion detection
        s->set_brightness(s, 0);     // Brightness: -2 to 2
        s->set_contrast(s, 0);       // Contrast: -2 to 2
        s->set_saturation(s, 0);     // Non applicabile a grayscale
        s->set_whitebal(s, 1);       // White balance auto
        s->set_awb_gain(s, 1);       // Auto white balance gain
        s->set_wb_mode(s, 0);        // White balance mode auto
        s->set_exposure_ctrl(s, 1);  // Auto exposure ENABLED for cleaner image
        s->set_aec2(s, 1);           // AEC DSP ENABLED
        s->set_ae_level(s, 0);       // AE level: -2 to 2
        s->set_aec_value(s, 400);    // AEC value fisso: 0 to 1200 (era 300, aumentato per compensare)
        s->set_gain_ctrl(s, 1);      // Auto gain
        s->set_agc_gain(s, 0);       // AGC gain: 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0);  // Gain ceiling
        s->set_bpc(s, 0);            // Black pixel correction
        s->set_wpc(s, 1);            // White pixel correction
        s->set_raw_gma(s, 1);        // Gamma correction
        s->set_lenc(s, 1);           // Lens correction
        s->set_hmirror(s, 0);        // Horizontal mirror
        s->set_vflip(s, 0);          // Vertical flip
        s->set_dcw(s, 0);            // DCW (Downsize EN)
        s->set_colorbar(s, 0);       // Color bar test pattern
    }

    _initialized = true;
    _fpsStartTime = millis();

    Serial.println("[CAMERA] ✓ Initialized successfully!");
    Serial.printf("[CAMERA] Format: VGA (640x480) Grayscale, %d FB\n", config.fb_count);
    Serial.printf("[CAMERA] PSRAM available: %u bytes\n", ESP.getPsramSize());
    Serial.printf("[CAMERA] PSRAM free: %u bytes\n", ESP.getFreePsram());

    return true;
}

bool CameraManager::captureFrame(uint8_t** outBuffer, size_t* outLength) {
    if (!_initialized) {
        Serial.println("[CAMERA ERROR] Not initialized!");
        return false;
    }

    // Rilascia frame precedente se presente
    if (_currentFrameBuffer) {
        esp_camera_fb_return(_currentFrameBuffer);
        _currentFrameBuffer = nullptr;
    }

    unsigned long startCapture = millis();

    // Cattura frame
    _currentFrameBuffer = esp_camera_fb_get();
    if (!_currentFrameBuffer) {
        Serial.println("[CAMERA ERROR] Frame capture failed!");
        _metrics.failedCaptures++;
        return false;
    }

    // Aggiorna metriche
    _metrics.totalFramesCaptured++;
    _metrics.lastFrameSize = _currentFrameBuffer->len;
    _metrics.lastCaptureTime = millis() - startCapture;

    // Calcola FPS
    _frameCount++;
    unsigned long now = millis();
    if (now - _fpsStartTime >= 1000) {
        _metrics.currentFps = (float)_frameCount / ((now - _fpsStartTime) / 1000.0f);
        _frameCount = 0;
        _fpsStartTime = now;
    }

    // Restituisce buffer
    *outBuffer = _currentFrameBuffer->buf;
    *outLength = _currentFrameBuffer->len;

    return true;
}

void CameraManager::releaseFrame() {
    if (_currentFrameBuffer) {
        esp_camera_fb_return(_currentFrameBuffer);
        _currentFrameBuffer = nullptr;
    }
}

void CameraManager::setFlash(bool enabled, uint8_t brightness) {
    _flashEnabled = enabled;
    _flashBrightness = enabled ? brightness : 0;

    Serial.printf("[CAMERA] Flash %s (brightness=%u)\n",
                  enabled ? "ENABLED" : "DISABLED", _flashBrightness);
}

void CameraManager::resetMetrics() {
    memset(&_metrics, 0, sizeof(_metrics));
    _frameCount = 0;
    _fpsStartTime = millis();
}
