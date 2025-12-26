#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "BLELedController.h"
#include "OpticalFlowDetector.h"
#include "MotionProcessor.h"

class ConfigManager {
private:
    static constexpr const char* CONFIG_FILE = "/config.json";
    LedState* ledState;
    OpticalFlowDetector* motionDetector = nullptr;
    MotionProcessor* motionProcessor = nullptr;

    // Valori di default (stessi di main.cpp)
    struct DefaultConfig {
        uint8_t brightness = 30;
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        String effect = "solid";
        uint8_t speed = 50;
        bool enabled = true;
        uint8_t statusLedBrightness = 32;
        bool statusLedEnabled = true;
        uint8_t foldPoint = 72;  // Punto di piegatura LED (default = metà)
        bool autoIgnitionOnBoot = true;
        uint32_t autoIgnitionDelayMs = 2000;
        bool motionOnBoot = false;
        String gestureClashEffect = "clash";
        uint16_t gestureClashDurationMs = 500;
        // Motion defaults
        uint8_t motionQuality = 160;
        uint8_t motionIntensityMin = 6;
        float motionSpeedMin = 0.4f;
        uint8_t gestureIgnitionMin = 15;
        uint8_t gestureRetractMin = 15;
        uint8_t gestureClashMin = 15;
        String effectMapUp = "";
        String effectMapDown = "";
        String effectMapLeft = "";
        String effectMapRight = "";
    };
    DefaultConfig defaults;

    bool mountFilesystem();
    void createDefaultConfig();

public:
    explicit ConfigManager(LedState* state);

    void setMotionComponents(OpticalFlowDetector* detector, MotionProcessor* processor);
    bool begin();           // Inizializza LittleFS e carica config
    bool loadConfig();      // Carica config da JSON
    bool saveConfig();      // Salva SOLO valori diversi dai default
    void resetToDefaults(); // Ripristina default ed elimina config.json
    void printDebugInfo();  // Stampa stato filesystem e config
};

#endif
