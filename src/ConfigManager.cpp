#include "ConfigManager.h"

ConfigManager::ConfigManager(LedState* state) {
    ledState = state;
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

    // Valida i valori caricati
    if (ledState->brightness > 255) ledState->brightness = 255;
    if (ledState->r > 255) ledState->r = 255;
    if (ledState->g > 255) ledState->g = 255;
    if (ledState->b > 255) ledState->b = 255;
    if (ledState->speed > 255) ledState->speed = 255;
    if (ledState->statusLedBrightness > 255) ledState->statusLedBrightness = 255;

    Serial.printf("[CONFIG] Loaded: brightness=%d, r=%d, g=%d, b=%d, effect=%s, speed=%d\n",
        ledState->brightness, ledState->r, ledState->g, ledState->b,
        ledState->effect.c_str(), ledState->speed);
    Serial.printf("[CONFIG] Status: enabled=%d, statusLed=%d (brightness=%d)\n",
        ledState->enabled, ledState->statusLedEnabled, ledState->statusLedBrightness);

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
}
