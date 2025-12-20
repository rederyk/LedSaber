#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "BLELedController.h"

class ConfigManager {
private:
    static constexpr const char* CONFIG_FILE = "/config.json";
    LedState* ledState;

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
        bool autoIgnitionOnBoot = false;
        uint32_t autoIgnitionDelayMs = 5000;
    };
    DefaultConfig defaults;

    bool mountFilesystem();
    void createDefaultConfig();

public:
    explicit ConfigManager(LedState* state);

    bool begin();           // Inizializza LittleFS e carica config
    bool loadConfig();      // Carica config da JSON
    bool saveConfig();      // Salva SOLO valori diversi dai default
    void resetToDefaults(); // Ripristina default ed elimina config.json
    void printDebugInfo();  // Stampa stato filesystem e config
};

#endif
