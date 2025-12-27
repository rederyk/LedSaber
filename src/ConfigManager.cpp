#include "ConfigManager.h"

 

ConfigManager::ConfigManager(LedState* state) {
    ledState = state;
}

void ConfigManager::setMotionComponents(OpticalFlowDetector* detector, MotionProcessor* processor) {
    motionDetector = detector;
    motionProcessor = processor;
}

bool ConfigManager::begin() {
    Serial.println("[CONFIG] Mounting LittleFS...");

    // Prova prima senza formattazione (preserva file pre-caricati)
    if (!LittleFS.begin(false)) {
        Serial.println("[CONFIG] Mount failed, trying with format...");

        // Se fallisce, prova con formattazione
        if (!LittleFS.begin(true)) {
            Serial.println("[CRITICAL] LittleFS mount failed even with format - using RAM defaults");
            createDefaultConfig();
            return false;
        }

        Serial.println("[CONFIG] Filesystem formatted and mounted");
    } else {
        Serial.println("[CONFIG] Filesystem mounted successfully");
    }

    Serial.printf("[CONFIG] Filesystem: %lu/%lu bytes (%lu used)\n",
        LittleFS.totalBytes(), LittleFS.totalBytes(), LittleFS.usedBytes());

    return loadConfig();
}

bool ConfigManager::loadConfig() {
    Serial.println("[CONFIG] Loading /config.json...");

    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("[CONFIG] File not found - first boot, using defaults");
        createDefaultConfig();
        return true;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) {
        Serial.println("[CONFIG ERROR] Failed to open config file for reading");
        createDefaultConfig();
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[CONFIG ERROR] JSON parsing failed: %s\n", error.c_str());
        Serial.println("[CONFIG] Removing corrupted file and using defaults");
        LittleFS.remove(CONFIG_FILE);
        createDefaultConfig();
        return false;
    }

    // Carica valori dal JSON, usa default se non presenti
    ledState->brightness = doc["brightness"] | defaults.brightness;
    ledState->r = doc["r"] | defaults.r;
    ledState->g = doc["g"] | defaults.g;
    ledState->b = doc["b"] | defaults.b;
    ledState->effect = doc["effect"] | defaults.effect;
    ledState->speed = doc["speed"] | defaults.speed;
    ledState->enabled = doc["enabled"] | defaults.enabled;
    ledState->statusLedEnabled = doc["statusLedEnabled"] | defaults.statusLedEnabled;
    ledState->statusLedBrightness = doc["statusLedBrightness"] | defaults.statusLedBrightness;
    ledState->foldPoint = doc["foldPoint"] | defaults.foldPoint;
    ledState->autoIgnitionOnBoot = doc["autoIgnitionOnBoot"] | defaults.autoIgnitionOnBoot;
    ledState->autoIgnitionDelayMs = doc["autoIgnitionDelayMs"] | defaults.autoIgnitionDelayMs;
    ledState->motionOnBoot = doc["motionOnBoot"] | defaults.motionOnBoot;
    ledState->gestureClashEffect = doc["gestureClashEffect"] | defaults.gestureClashEffect;
    ledState->gestureClashDurationMs = doc["gestureClashDurationMs"] | defaults.gestureClashDurationMs;

    // Carica parametri Motion se i componenti sono stati collegati
    if (motionDetector) {
        uint8_t quality = doc["motionQuality"] | defaults.motionQuality;
        uint8_t intensity = doc["motionIntensityMin"] | defaults.motionIntensityMin;
        float speed = doc["motionSpeedMin"] | defaults.motionSpeedMin;
        
        motionDetector->setQuality(quality);
        motionDetector->setMotionIntensityThreshold(intensity);
        motionDetector->setMotionSpeedThreshold(speed);
    }

    if (motionProcessor) {
        MotionProcessor::Config cfg = motionProcessor->getConfig();
        cfg.gesturesEnabled = doc["gesturesEnabled"] | defaults.gesturesEnabled;
        cfg.ignitionIntensityThreshold = doc["gestureIgnitionMin"] | defaults.gestureIgnitionMin;
        cfg.retractIntensityThreshold = doc["gestureRetractMin"] | defaults.gestureRetractMin;
        cfg.clashIntensityThreshold = doc["gestureClashMin"] | defaults.gestureClashMin;
        cfg.effectOnUp = doc["effectMapUp"] | defaults.effectMapUp;
        cfg.effectOnDown = doc["effectMapDown"] | defaults.effectMapDown;
        cfg.effectOnLeft = doc["effectMapLeft"] | defaults.effectMapLeft;
        cfg.effectOnRight = doc["effectMapRight"] | defaults.effectMapRight;
        
        motionProcessor->setConfig(cfg);
    }

    // Valida i valori caricati
    if (ledState->brightness > 255) ledState->brightness = 255;
    if (ledState->r > 255) ledState->r = 255;
    if (ledState->g > 255) ledState->g = 255;
    if (ledState->b > 255) ledState->b = 255;
    if (ledState->speed > 255) ledState->speed = 255;
    if (ledState->statusLedBrightness > 255) ledState->statusLedBrightness = 255;

    // Validazione auto ignition delay (0..60000ms)
    if (ledState->autoIgnitionDelayMs > 60000) {
        ledState->autoIgnitionDelayMs = defaults.autoIgnitionDelayMs;
    }

    // Validazione foldPoint: deve essere tra 1 e 143 (NUM_LEDS-1)
    if (ledState->foldPoint == 0 || ledState->foldPoint >= 144) {
        ledState->foldPoint = 72;  // Fallback a metà di 144
    }

    // Validazione gesture clash duration
    if (ledState->gestureClashDurationMs < 50 || ledState->gestureClashDurationMs > 5000) {
        ledState->gestureClashDurationMs = defaults.gestureClashDurationMs;
    }

    Serial.printf("[CONFIG] Loaded: brightness=%d, r=%d, g=%d, b=%d, effect=%s, speed=%d\n",
        ledState->brightness, ledState->r, ledState->g, ledState->b,
        ledState->effect.c_str(), ledState->speed);
    Serial.printf("[CONFIG] Status: enabled=%d, statusLed=%d (brightness=%d)\n",
        ledState->enabled, ledState->statusLedEnabled, ledState->statusLedBrightness);
    
    if (motionDetector) {
        Serial.printf("[CONFIG] Motion: quality=%d, intMin=%d, speedMin=%.2f\n",
            motionDetector->getQuality(), 
            motionDetector->getMotionIntensityThreshold(), 
            motionDetector->getMotionSpeedThreshold());
    }

    return true;
}

bool ConfigManager::saveConfig() {
    JsonDocument doc;
    int modifiedCount = 0;

    // Salva SOLO i valori diversi dai default
    if (ledState->brightness != defaults.brightness) {
        doc["brightness"] = ledState->brightness;
        modifiedCount++;
    }
    if (ledState->r != defaults.r || ledState->g != defaults.g || ledState->b != defaults.b) {
        doc["r"] = ledState->r;
        doc["g"] = ledState->g;
        doc["b"] = ledState->b;
        modifiedCount++;
    }
    if (ledState->effect != defaults.effect) {
        doc["effect"] = ledState->effect;
        modifiedCount++;
    }
    if (ledState->speed != defaults.speed) {
        doc["speed"] = ledState->speed;
        modifiedCount++;
    }
    if (ledState->enabled != defaults.enabled) {
        doc["enabled"] = ledState->enabled;
        modifiedCount++;
    }
    if (ledState->statusLedEnabled != defaults.statusLedEnabled) {
        doc["statusLedEnabled"] = ledState->statusLedEnabled;
        modifiedCount++;
    }
    if (ledState->statusLedBrightness != defaults.statusLedBrightness) {
        doc["statusLedBrightness"] = ledState->statusLedBrightness;
        modifiedCount++;
    }
    if (ledState->foldPoint != defaults.foldPoint) {
        doc["foldPoint"] = ledState->foldPoint;
        modifiedCount++;
    }
    if (ledState->autoIgnitionOnBoot != defaults.autoIgnitionOnBoot) {
        doc["autoIgnitionOnBoot"] = ledState->autoIgnitionOnBoot;
        modifiedCount++;
    }
    if (ledState->autoIgnitionDelayMs != defaults.autoIgnitionDelayMs) {
        doc["autoIgnitionDelayMs"] = ledState->autoIgnitionDelayMs;
        modifiedCount++;
    }
    if (ledState->motionOnBoot != defaults.motionOnBoot) {
        doc["motionOnBoot"] = ledState->motionOnBoot;
        modifiedCount++;
    }
    if (ledState->gestureClashEffect != defaults.gestureClashEffect) {
        doc["gestureClashEffect"] = ledState->gestureClashEffect;
        modifiedCount++;
    }
    if (ledState->gestureClashDurationMs != defaults.gestureClashDurationMs) {
        doc["gestureClashDurationMs"] = ledState->gestureClashDurationMs;
        modifiedCount++;
    }

    // Salva parametri Motion
    if (motionDetector) {
        if (motionDetector->getQuality() != defaults.motionQuality) {
            doc["motionQuality"] = motionDetector->getQuality();
            modifiedCount++;
        }
        if (motionDetector->getMotionIntensityThreshold() != defaults.motionIntensityMin) {
            doc["motionIntensityMin"] = motionDetector->getMotionIntensityThreshold();
            modifiedCount++;
        }
        if (motionDetector->getMotionSpeedThreshold() != defaults.motionSpeedMin) {
            doc["motionSpeedMin"] = motionDetector->getMotionSpeedThreshold();
            modifiedCount++;
        }
    }

    if (motionProcessor) {
        MotionProcessor::Config cfg = motionProcessor->getConfig();
        if (cfg.gesturesEnabled != defaults.gesturesEnabled) {
            doc["gesturesEnabled"] = cfg.gesturesEnabled;
            modifiedCount++;
        }
        if (cfg.ignitionIntensityThreshold != defaults.gestureIgnitionMin) {
            doc["gestureIgnitionMin"] = cfg.ignitionIntensityThreshold;
            modifiedCount++;
        }
        if (cfg.retractIntensityThreshold != defaults.gestureRetractMin) {
            doc["gestureRetractMin"] = cfg.retractIntensityThreshold;
            modifiedCount++;
        }
        if (cfg.clashIntensityThreshold != defaults.gestureClashMin) {
            doc["gestureClashMin"] = cfg.clashIntensityThreshold;
            modifiedCount++;
        }
        if (cfg.effectOnUp != defaults.effectMapUp) {
            doc["effectMapUp"] = cfg.effectOnUp;
            modifiedCount++;
        }
        if (cfg.effectOnDown != defaults.effectMapDown) {
            doc["effectMapDown"] = cfg.effectOnDown;
            modifiedCount++;
        }
        if (cfg.effectOnLeft != defaults.effectMapLeft) {
            doc["effectMapLeft"] = cfg.effectOnLeft;
            modifiedCount++;
        }
        if (cfg.effectOnRight != defaults.effectMapRight) {
            doc["effectMapRight"] = cfg.effectOnRight;
            modifiedCount++;
        }
    }

    // Se tutti i valori sono uguali ai default, elimina il file config
    if (modifiedCount == 0) {
        if (LittleFS.exists(CONFIG_FILE)) {
            Serial.println("[CONFIG] All values match defaults - removing config file");
            LittleFS.remove(CONFIG_FILE);
        }
        return true;
    }

    // Salva il file JSON
    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) {
        Serial.println("[CONFIG ERROR] Failed to open config file for writing");
        return false;
    }

    size_t bytesWritten = serializeJson(doc, file);
    file.close();

    if (bytesWritten == 0) {
        Serial.println("[CONFIG ERROR] Failed to write JSON data");
        return false;
    }

    Serial.printf("[CONFIG] Saved %d modified values (%lu bytes)\n", modifiedCount, bytesWritten);
    Serial.printf("[CONFIG] Filesystem: %lu/%lu bytes used\n",
        LittleFS.usedBytes(), LittleFS.totalBytes());

    return true;
}

void ConfigManager::resetToDefaults() {
    Serial.println("[CONFIG] Resetting to defaults...");

    ledState->brightness = defaults.brightness;
    ledState->r = defaults.r;
    ledState->g = defaults.g;
    ledState->b = defaults.b;
    ledState->effect = defaults.effect;
    ledState->speed = defaults.speed;
    ledState->enabled = defaults.enabled;
    ledState->statusLedEnabled = defaults.statusLedEnabled;
    ledState->statusLedBrightness = defaults.statusLedBrightness;
    ledState->foldPoint = defaults.foldPoint;
    ledState->autoIgnitionOnBoot = defaults.autoIgnitionOnBoot;
    ledState->autoIgnitionDelayMs = defaults.autoIgnitionDelayMs;
    ledState->motionOnBoot = defaults.motionOnBoot;
    ledState->gestureClashEffect = defaults.gestureClashEffect;
    ledState->gestureClashDurationMs = defaults.gestureClashDurationMs;

    if (motionDetector) {
        motionDetector->setQuality(defaults.motionQuality);
        motionDetector->setMotionIntensityThreshold(defaults.motionIntensityMin);
        motionDetector->setMotionSpeedThreshold(defaults.motionSpeedMin);
    }
    if (motionProcessor) {
        MotionProcessor::Config cfg;
        cfg.gesturesEnabled = defaults.gesturesEnabled;
        cfg.ignitionIntensityThreshold = defaults.gestureIgnitionMin;
        cfg.retractIntensityThreshold = defaults.gestureRetractMin;
        cfg.clashIntensityThreshold = defaults.gestureClashMin;
        cfg.effectOnUp = defaults.effectMapUp;
        cfg.effectOnDown = defaults.effectMapDown;
        cfg.effectOnLeft = defaults.effectMapLeft;
        cfg.effectOnRight = defaults.effectMapRight;
        motionProcessor->setConfig(cfg);
    }

    if (LittleFS.exists(CONFIG_FILE)) {
        LittleFS.remove(CONFIG_FILE);
        Serial.println("[CONFIG] Config file removed");
    }

    Serial.println("[CONFIG] Reset complete - all defaults restored");
}

void ConfigManager::printDebugInfo() {
    Serial.println("\n=== CONFIG DEBUG INFO ===");
    Serial.printf("Filesystem: %lu/%lu bytes (%lu used)\n",
        LittleFS.totalBytes(), LittleFS.totalBytes(), LittleFS.usedBytes());

    Serial.println("\nFiles in root:");
    File root = LittleFS.open("/");
    if (root) {
        File file = root.openNextFile();
        while (file) {
            Serial.printf("  - %s (%lu bytes)\n", file.name(), file.size());
            file = root.openNextFile();
        }
        root.close();
    }

    if (LittleFS.exists(CONFIG_FILE)) {
        Serial.println("\nConfig file content:");
        File file = LittleFS.open(CONFIG_FILE, "r");
        if (file) {
            while (file.available()) {
                Serial.write(file.read());
            }
            Serial.println();
            file.close();
        }
    } else {
        Serial.println("\nNo config file (using all defaults)");
    }

    Serial.println("=========================\n");
}

void ConfigManager::createDefaultConfig() {
    ledState->brightness = defaults.brightness;
    ledState->r = defaults.r;
    ledState->g = defaults.g;
    ledState->b = defaults.b;
    ledState->effect = defaults.effect;
    ledState->speed = defaults.speed;
    ledState->enabled = defaults.enabled;
    ledState->statusLedEnabled = defaults.statusLedEnabled;
    ledState->statusLedBrightness = defaults.statusLedBrightness;
    ledState->foldPoint = defaults.foldPoint;
    ledState->autoIgnitionOnBoot = defaults.autoIgnitionOnBoot;
    ledState->autoIgnitionDelayMs = defaults.autoIgnitionDelayMs;
    ledState->motionOnBoot = defaults.motionOnBoot;
    ledState->gestureClashEffect = defaults.gestureClashEffect;
    ledState->gestureClashDurationMs = defaults.gestureClashDurationMs;

    if (motionDetector) {
        motionDetector->setQuality(defaults.motionQuality);
        motionDetector->setMotionIntensityThreshold(defaults.motionIntensityMin);
        motionDetector->setMotionSpeedThreshold(defaults.motionSpeedMin);
    }
    if (motionProcessor) {
        MotionProcessor::Config cfg;
        cfg.gesturesEnabled = defaults.gesturesEnabled;
        cfg.ignitionIntensityThreshold = defaults.gestureIgnitionMin;
        cfg.retractIntensityThreshold = defaults.gestureRetractMin;
        cfg.clashIntensityThreshold = defaults.gestureClashMin;
        cfg.effectOnUp = defaults.effectMapUp;
        cfg.effectOnDown = defaults.effectMapDown;
        cfg.effectOnLeft = defaults.effectMapLeft;
        cfg.effectOnRight = defaults.effectMapRight;
        motionProcessor->setConfig(cfg);
    }
}
