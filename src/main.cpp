#include <Arduino.h>
#include <FastLED.h>
#include <BLEDevice.h>
#include <esp_gap_ble_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "BLELedController.h"
#include "OTAManager.h"
#include "ConfigManager.h"
#include "CameraManager.h"
#include "BLECameraService.h"
#include "OpticalFlowDetector.h"
#include "BLEMotionService.h"
#include "StatusLedManager.h"
#include "MotionProcessor.h"
#include "LedEffectEngine.h"

// GPIO
static constexpr uint8_t STATUS_LED_PIN = 4;   // LED integrato per stato connessione
// Usa un canale LEDC diverso da 0: il canale 0 è riservato alla camera per il clock XCLK
static constexpr uint8_t STATUS_LED_PWM_CHANNEL = 6;
static constexpr uint16_t STATUS_LED_PWM_FREQ = 5000;
static constexpr uint8_t STATUS_LED_PWM_RES = 8;
static constexpr uint8_t LED_STRIP_PIN = 13;   // Striscia WS2812B
static constexpr uint16_t NUM_LEDS = 144;
static constexpr uint8_t DEFAULT_BRIGHTNESS = 30;
static constexpr uint8_t DEFAULT_STATUS_LED_BRIGHTNESS = 32;

// Limite di sicurezza per alimentatore 2A
// Calcolo: 5V * 2A = 10W disponibili
// 144 LED * 60mA (max) = 8.64A teorici a luminosità 255
// Con FastLED.setMaxPowerInVoltsAndMilliamps(5, 4500) la luminosità massima sicura è gestita dinamicamente da FastLED.
// Impostiamo a 255 per permettere a FastLED di usare l'intera gamma e limitare solo se necessario.
static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 255;

// Configurazione Power Management FastLED
static constexpr uint8_t LED_STRIP_VOLTAGE = 5;      // Voltaggio striscia LED (es. 5V)
static constexpr uint16_t MAX_POWER_MILLIAMPS = 4500; // Limite corrente in mA (es. 4500mA = 4.5A)

CRGB leds[NUM_LEDS];

extern LedState ledState;


// BLE GATT
LedState ledState;
BLELedController bleController(&ledState);
OTAManager otaManager;
ConfigManager configManager(&ledState);

// Camera Manager
CameraManager cameraManager;
BLECameraService bleCameraService(&cameraManager);

// Motion Processor & LED Effect Engine
MotionProcessor motionProcessor;
LedEffectEngine effectEngine(leds, NUM_LEDS);

// Optical Flow Detector
OpticalFlowDetector motionDetector;
BLEMotionService bleMotionService(&motionDetector, &motionProcessor);

struct MotionTaskResult {
    bool valid = false;
    bool motionDetected = false;
    uint8_t flashIntensity = 150;
    uint8_t motionIntensity = 0;
    OpticalFlowDetector::Direction direction = OpticalFlowDetector::Direction::NONE;
    uint32_t timestamp = 0;
    MotionProcessor::ProcessedMotion processedMotion{};
};

static QueueHandle_t gMotionResultQueue = nullptr;
static MotionTaskResult gCachedMotionResult;
static TaskHandle_t gCameraTaskHandle = nullptr;
static volatile bool gCameraTaskShouldRun = false;
static bool gCameraTaskStreaming = false;

static bool gAutoIgnitionScheduled = false;
static uint32_t gAutoIgnitionAtMs = 0;

static void CameraCaptureTask(void* pvParameters);

// ============================================================================
// CALLBACKS GLOBALI DEL SERVER BLE
// ============================================================================

class MainServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        bleController.setConnected(true);
        Serial.println("[BLE] Client connected!");
        // Note: Blade remains in its current state (on/off)
        // User must explicitly send 'ignition' command to turn on
    }

    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
        onConnect(pServer); // Chiama la versione base

        // Richiedi parametri di connessione più aggressivi per throughput OTA
        esp_ble_conn_update_params_t connParams = {};
        memcpy(connParams.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        connParams.min_int = 0x06; // 7.5ms
        connParams.max_int = 0x0C; // 15ms
        connParams.latency = 0;
        connParams.timeout = 400; // 4s
        esp_err_t err = esp_ble_gap_update_conn_params(&connParams);
        Serial.printf("[BLE] Conn params update req: %s\n", esp_err_to_name(err));

        // Richiedi Data Length Extension (251 payload)
        err = esp_ble_gap_set_pkt_data_len(param->connect.remote_bda, 251);
        Serial.printf("[BLE] Data len update req: %s\n", esp_err_to_name(err));
    }

    void onDisconnect(BLEServer* pServer) override {
        bleController.setConnected(false);
        Serial.println("[BLE] Client disconnected! Restarting advertising...");
        pServer->startAdvertising();
    }

    void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override {
        (void)pServer;
        Serial.printf("[BLE] MTU changed: %u\n", param->mtu.mtu);
    }
};

// ============================================================================
// LEGACY CODE - NOW HANDLED BY LedEffectEngine
// ============================================================================
// Note: scaleColorByBrightness() and setLedPair() moved to LedEffectEngine class
// renderLedStrip() replaced by effectEngine.render()

// ============================================================================
// FUNZIONI DI STATO E GRAFICA
// ============================================================================

static void initPeripherals() {
    auto& ledManager = StatusLedManager::getInstance();
    ledManager.begin(STATUS_LED_PIN, STATUS_LED_PWM_CHANNEL, STATUS_LED_PWM_FREQ, STATUS_LED_PWM_RES);
    ledManager.setMode(StatusLedManager::Mode::STATUS_LED);
    ledManager.setStatusLedDirect(false, ledState.statusLedBrightness > 0 ? ledState.statusLedBrightness : DEFAULT_STATUS_LED_BRIGHTNESS);

    FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
}


/*
// ============================================================================
// LEGACY renderLedStrip() - NOW REPLACED BY LedEffectEngine
// ============================================================================
static void renderLedStrip() {
    static unsigned long lastUpdate = 0;
    static uint8_t hue = 0;
    const unsigned long now = millis();

    if (now - lastUpdate < 20) {
        return;
    }

    if (ledState.enabled) {
        if (ledState.effect == "solid") {
            fill_solid(leds, NUM_LEDS, CRGB(ledState.r, ledState.g, ledState.b));
        } else if (ledState.effect == "rainbow") {
            // Speed da 1-255: mappiamo a step incrementali più ampi
            uint8_t step = map(ledState.speed, 1, 255, 1, 15);
            if (step == 0) step = 1;
            fill_rainbow(leds, NUM_LEDS, hue, 256 / NUM_LEDS);
            hue += step;
        } else if (ledState.effect == "breathe") {
            uint8_t breath = beatsin8(ledState.speed, 0, 255);
            fill_solid(leds, NUM_LEDS, CRGB(ledState.r, ledState.g, ledState.b));
            uint8_t safeBrightness = min(ledState.brightness, MAX_SAFE_BRIGHTNESS);
            FastLED.setBrightness(scale8(breath, safeBrightness));
            FastLED.show();
            FastLED.setBrightness(safeBrightness);
            lastUpdate = now;
            return;
        } else if (ledState.effect == "ignition") {
            // EFFETTO: Accensione progressiva dalla base alla punta
            static uint16_t ignitionProgress = 0;
            static unsigned long lastIgnitionUpdate = 0;

            // Velocità di accensione controllata da 'speed' (1-255)
            // speed basso = lento, speed alto = veloce
            uint16_t ignitionSpeed = map(ledState.speed, 1, 255, 100, 1);

            // Aggiorna progressione ogni N millisecondi
            if (now - lastIgnitionUpdate > ignitionSpeed) {
                if (ignitionProgress < ledState.foldPoint) {
                    ignitionProgress++;
                } else {
                    ignitionProgress = 0;  // Riavvia animazione
                }
                lastIgnitionUpdate = now;
            }

            // Spegni tutti i LED
            fill_solid(leds, NUM_LEDS, CRGB::Black);

            // Accendi progressivamente dalla base (0) alla punta (foldPoint-1)
            CRGB color = CRGB(ledState.r, ledState.g, ledState.b);
            for (uint16_t i = 0; i < ignitionProgress; i++) {
                // Effetto "glow" alla punta: ultimi 5 LED più luminosi
                if (i >= ignitionProgress - 5 && i < ignitionProgress) {
                    uint8_t fade = map(i, ignitionProgress - 5, ignitionProgress - 1, 100, 255);
                    CRGB fadedColor = color;
                    fadedColor.fadeToBlackBy(255 - fade);
                    setLedPair(i, ledState.foldPoint, fadedColor);
                } else {
                    setLedPair(i, ledState.foldPoint, color);
                }
            }

        } else if (ledState.effect == "retraction") {
            // EFFETTO: Spegnimento progressivo dalla punta alla base
            static uint16_t retractionProgress = 0;
            static unsigned long lastRetractionUpdate = 0;

            uint16_t retractionSpeed = map(ledState.speed, 1, 255, 100, 1);

            // Inizializza retractionProgress alla prima esecuzione
            if (retractionProgress == 0) {
                retractionProgress = ledState.foldPoint;
            }

            if (now - lastRetractionUpdate > retractionSpeed) {
                if (retractionProgress > 0) {
                    retractionProgress--;
                } else {
                    retractionProgress = ledState.foldPoint;  // Riavvia
                }
                lastRetractionUpdate = now;
            }

            fill_solid(leds, NUM_LEDS, CRGB::Black);
            CRGB color = CRGB(ledState.r, ledState.g, ledState.b);

            for (uint16_t i = 0; i < retractionProgress; i++) {
                // Effetto fade alla "punta" che si ritrae
                if (i >= retractionProgress - 5 && i < retractionProgress) {
                    uint8_t fade = map(i, retractionProgress - 5, retractionProgress - 1, 100, 255);
                    CRGB fadedColor = color;
                    fadedColor.fadeToBlackBy(255 - fade);
                    setLedPair(i, ledState.foldPoint, fadedColor);
                } else {
                    setLedPair(i, ledState.foldPoint, color);
                }
            }
        } else if (ledState.effect == "flicker") {
            // EFFETTO: Instabilità del plasma (tipo Kylo Ren)
            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
            uint8_t safeBrightness = min(ledState.brightness, MAX_SAFE_BRIGHTNESS);

            // 'speed' controlla l'intensità delle fluttuazioni (1-255)
            // speed basso = flicker leggero, speed alto = molto instabile
            uint8_t flickerIntensity = ledState.speed;

            for (uint16_t i = 0; i < ledState.foldPoint; i++) {
                // Rumore casuale per ogni coppia di LED
                uint8_t noise = random8(flickerIntensity);
                uint8_t brightness = 255 - (noise / 2);  // Non scendere troppo

                CRGB flickeredColor = baseColor;
                flickeredColor.fadeToBlackBy(255 - brightness);

                // Applica brightness globale DOPO il fading per preservare il contrasto
                flickeredColor = scaleColorByBrightness(flickeredColor, safeBrightness);
                setLedPair(i, ledState.foldPoint, flickeredColor);
            }

            // Disabilita brightness globale FastLED perché l'abbiamo già applicata manualmente
            FastLED.setBrightness(255);
            FastLED.show();
            // Ripristina brightness per altri effetti
            FastLED.setBrightness(safeBrightness);
            lastUpdate = now;
            return;

        } else if (ledState.effect == "unstable") {
            // EFFETTO: Combinazione flicker + pulses casuali (Kylo Ren avanzato)
            // Nota: usiamo buffer di dimensione foldPoint per risparmiare RAM
            static uint8_t unstableHeat[72];  // Buffer per metà striscia (max foldPoint tipico)
            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
            uint8_t safeBrightness = min(ledState.brightness, MAX_SAFE_BRIGHTNESS);

            // Limita dimensione buffer a foldPoint effettivo
            uint16_t maxIndex = min((uint16_t)ledState.foldPoint, (uint16_t)72);

            // Decay del "calore"
            for (uint16_t i = 0; i < maxIndex; i++) {
                unstableHeat[i] = qsub8(unstableHeat[i], random8(5, 15));
            }

            // Aggiungi nuovi "spark" casuali
            if (random8() < ledState.speed / 2) {
                uint16_t pos = random16(maxIndex);
                unstableHeat[pos] = qadd8(unstableHeat[pos], random8(100, 200));
            }

            // Rendering
            for (uint16_t i = 0; i < maxIndex; i++) {
                // Base colore + heat map
                uint8_t brightness = scale8(255, 200 + (unstableHeat[i] / 4));
                CRGB unstableColor = baseColor;
                unstableColor.fadeToBlackBy(255 - brightness);

                // Applica brightness globale DOPO il fading per preservare il contrasto
                unstableColor = scaleColorByBrightness(unstableColor, safeBrightness);
                setLedPair(i, ledState.foldPoint, unstableColor);
            }

            // Disabilita brightness globale FastLED perché l'abbiamo già applicata manualmente
            FastLED.setBrightness(255);
            FastLED.show();
            // Ripristina brightness per altri effetti
            FastLED.setBrightness(safeBrightness);
            lastUpdate = now;
            return;

        } else if (ledState.effect == "pulse") {
            // EFFETTO: Onde di energia che percorrono la lama
            static uint16_t pulsePosition = 0;
            static unsigned long lastPulseUpdate = 0;

            // Velocità del pulse controllata da 'speed'
            uint16_t pulseSpeed = map(ledState.speed, 1, 255, 80, 1);

            if (now - lastPulseUpdate > pulseSpeed) {
                pulsePosition = (pulsePosition + 1) % ledState.foldPoint;
                lastPulseUpdate = now;
            }

            // Riempimento base
            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
            uint8_t safeBrightness = min(ledState.brightness, MAX_SAFE_BRIGHTNESS);

            // Crea "onda" di luminosità
            const uint8_t pulseWidth = 15;  // Larghezza dell'onda

            for (uint16_t i = 0; i < ledState.foldPoint; i++) {
                // Calcola distanza dal centro del pulse
                int16_t distance = abs((int16_t)i - (int16_t)pulsePosition);

                if (distance < pulseWidth) {
                    // Dentro il pulse: aumenta luminosità
                    uint8_t brightness = map(distance, 0, pulseWidth, 255, 150);
                    CRGB pulseColor = baseColor;
                    pulseColor.fadeToBlackBy(255 - brightness);

                    // Applica brightness globale DOPO il fading per preservare il contrasto
                    pulseColor = scaleColorByBrightness(pulseColor, safeBrightness);
                    setLedPair(i, ledState.foldPoint, pulseColor);
                } else {
                    // Fuori dal pulse: luminosità base
                    CRGB dimColor = baseColor;
                    dimColor.fadeToBlackBy(255 - 150);

                    // Applica brightness globale DOPO il fading per preservare il contrasto
                    dimColor = scaleColorByBrightness(dimColor, safeBrightness);
                    setLedPair(i, ledState.foldPoint, dimColor);
                }
            }

            // Disabilita brightness globale FastLED perché l'abbiamo già applicata manualmente
            FastLED.setBrightness(255);
            FastLED.show();
            // Ripristina brightness per altri effetti
            FastLED.setBrightness(safeBrightness);
            lastUpdate = now;
            return;

        } else if (ledState.effect == "dual_pulse") {
            // EFFETTO: Due pulse che si muovono in direzioni opposte
            static uint16_t pulse1Pos = 0;
            static uint16_t pulse2Pos = 0;
            static unsigned long lastDualPulseUpdate = 0;

            // Inizializza pulse2Pos alla prima esecuzione
            if (pulse2Pos == 0 && pulse1Pos == 0) {
                pulse2Pos = ledState.foldPoint / 2;
            }

            uint16_t pulseSpeed = map(ledState.speed, 1, 255, 60, 1);

            if (now - lastDualPulseUpdate > pulseSpeed) {
                pulse1Pos = (pulse1Pos + 1) % ledState.foldPoint;
                pulse2Pos = (pulse2Pos > 0) ? (pulse2Pos - 1) : (ledState.foldPoint - 1);
                lastDualPulseUpdate = now;
            }

            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
            uint8_t safeBrightness = min(ledState.brightness, MAX_SAFE_BRIGHTNESS);

            const uint8_t pulseWidth = 10;

            for (uint16_t i = 0; i < ledState.foldPoint; i++) {
                int16_t dist1 = abs((int16_t)i - (int16_t)pulse1Pos);
                int16_t dist2 = abs((int16_t)i - (int16_t)pulse2Pos);

                uint8_t brightness = 150;
                if (dist1 < pulseWidth) {
                    brightness = max(brightness, (uint8_t)map(dist1, 0, pulseWidth, 255, 150));
                }
                if (dist2 < pulseWidth) {
                    brightness = max(brightness, (uint8_t)map(dist2, 0, pulseWidth, 255, 150));
                }

                CRGB dualPulseColor = baseColor;
                dualPulseColor.fadeToBlackBy(255 - brightness);

                // Applica brightness globale DOPO il fading per preservare il contrasto
                dualPulseColor = scaleColorByBrightness(dualPulseColor, safeBrightness);
                setLedPair(i, ledState.foldPoint, dualPulseColor);
            }

            // Disabilita brightness globale FastLED perché l'abbiamo già applicata manualmente
            FastLED.setBrightness(255);
            FastLED.show();
            // Ripristina brightness per altri effetti
            FastLED.setBrightness(safeBrightness);
            lastUpdate = now;
            return;

        } else if (ledState.effect == "clash") {
            // EFFETTO: Flash bianco che si dissipa (simula impatto)
            // Nota: ideale da triggerare con accelerometro in futuro
            static uint8_t clashBrightness = 0;
            static unsigned long lastClashTrigger = 0;
            static bool clashActive = false;

            // Trigger automatico ogni 3 secondi per demo (rimuovere con accelerometro)
            if (!clashActive && now - lastClashTrigger > 3000) {
                clashActive = true;
                clashBrightness = 255;
                lastClashTrigger = now;
            }

            if (clashActive) {
                // Dissipazione rapida
                clashBrightness = qsub8(clashBrightness, 15);

                if (clashBrightness == 0) {
                    clashActive = false;
                }
            }

            // Flash bianco che sovrappone il colore base
            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
            CRGB flashColor = CRGB(255, 255, 255);
            uint8_t safeBrightness = min(ledState.brightness, MAX_SAFE_BRIGHTNESS);

            for (uint16_t i = 0; i < ledState.foldPoint; i++) {
                CRGB clashColor = blend(baseColor, flashColor, clashBrightness);

                // Applica brightness globale per mantenere la luminosità corretta
                clashColor = scaleColorByBrightness(clashColor, safeBrightness);
                setLedPair(i, ledState.foldPoint, clashColor);
            }

            // Disabilita brightness globale FastLED perché l'abbiamo già applicata manualmente
            FastLED.setBrightness(255);
            FastLED.show();
            // Ripristina brightness per altri effetti
            FastLED.setBrightness(safeBrightness);
            lastUpdate = now;
            return;

        } else if (ledState.effect == "rainbow_blade") {
            // EFFETTO: Arcobaleno che percorre la lama linearmente (rispetta piegatura)
            static uint8_t rainbowHue = 0;

            // Speed da 1-255: mappiamo a step incrementali più ampi
            uint8_t hueStep = map(ledState.speed, 1, 255, 1, 15);
            if (hueStep == 0) hueStep = 1;

            for (uint16_t i = 0; i < ledState.foldPoint; i++) {
                // Calcola hue basato sulla posizione logica (non fisica!)
                uint8_t hue = rainbowHue + (i * 256 / ledState.foldPoint);
                CRGB rainbowColor = CHSV(hue, 255, 255);
                setLedPair(i, ledState.foldPoint, rainbowColor);
            }

            rainbowHue += hueStep;
        }

        FastLED.setBrightness(min(ledState.brightness, MAX_SAFE_BRIGHTNESS));
        FastLED.show();
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
    }

    lastUpdate = now;
}
*/
// END LEGACY renderLedStrip()

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== LEDSABER (BLE GATT + OTA) ===");

    initPeripherals();

    FastLED.setMaxPowerInVoltsAndMilliamps(LED_STRIP_VOLTAGE, MAX_POWER_MILLIAMPS);

    // 1. Carica configurazione da LittleFS PRIMA di inizializzare BLE
    if (!configManager.begin()) {
        Serial.println("[CONFIG] Warning: using default values");
    }
    configManager.printDebugInfo();

    // 2. Inizializza il dispositivo BLE e crea il Server
    BLEDevice::init("LedSaber-BLE");
    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MainServerCallbacks());

    // 3. Inizializza il servizio LED, agganciandolo al server principale
    bleController.begin(pServer);
    bleController.setEffectEngine(&effectEngine);  // Link EffectEngine for device control
    effectEngine.setLedStateRef(&ledState);  // Set LedState reference for power control
    Serial.println("*** BLE LED Service avviato ***");

    // 4. Inizializza il servizio OTA, agganciandolo allo stesso server
    otaManager.begin(pServer);
    Serial.println("*** OTA Service avviato ***");

    // 5. Inizializza il servizio Camera, agganciandolo allo stesso server
    bleCameraService.begin(pServer);
    Serial.println("*** Camera Service avviato ***");

    // 6. Inizializza il servizio Motion, agganciandolo allo stesso server
    bleMotionService.begin(pServer);
    Serial.println("*** Motion Service avviato ***");

    // 7. Configura e avvia l'advertising DOPO aver inizializzato tutti i servizi
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(LED_SERVICE_UUID);
    pAdvertising->addServiceUUID(OTA_SERVICE_UUID);
    pAdvertising->addServiceUUID(CAMERA_SERVICE_UUID);
    pAdvertising->addServiceUUID(MOTION_SERVICE_UUID);
    // Nota: Non aggiungiamo WiFi service UUID all'advertising per risparmiare spazio
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // iPhone compatibility
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();

    gMotionResultQueue = xQueueCreate(3, sizeof(MotionTaskResult));  // Aumentato da 1 a 3
    if (!gMotionResultQueue) {
        Serial.println("[MAIN] ✗ Failed to create motion result queue");
    } else {
        Serial.println("[MAIN] ✓ Motion result queue ready (size=3)");
    }

    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        CameraCaptureTask,
        "CameraCaptureTask",
        10240,  // Aumentato da 8192 a 10240 bytes per sicurezza
        nullptr,
        5,
        &gCameraTaskHandle,
        0
    );
    if (taskCreated != pdPASS) {
        Serial.println("[MAIN] ✗ Failed to create CameraCaptureTask");
        gCameraTaskHandle = nullptr;
    } else {
        Serial.println("[MAIN] ✓ CameraCaptureTask created on core 0");
    }

    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("*** THE FORCE IS IN YOU ***");

    // Check wake-up reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("[BOOT] Woke up from deep sleep via GPIO (BOOT button)");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[BOOT] Woke up from deep sleep via timer");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            Serial.println("[BOOT] Normal boot (not from deep sleep)");
            break;
    }

    if (ledState.autoIgnitionOnBoot) {
        const uint32_t delayMs = ledState.autoIgnitionDelayMs;
        gAutoIgnitionScheduled = true;
        gAutoIgnitionAtMs = millis() + delayMs;
        Serial.printf("[BOOT] Auto-ignition enabled: scheduling ignition in %lu ms\n", delayMs);
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        // Legacy behavior: auto-ignite immediately after waking from deep sleep
        Serial.println("[BOOT] Auto-ignition disabled: igniting immediately after deep sleep wake");
        effectEngine.powerOn();
    } else {
        Serial.println("[BOOT] Auto-ignition disabled: blade stays OFF at normal boot");
    }
}

void loop() {
    static unsigned long lastBleNotify = 0;
    static unsigned long lastLoopDebug = 0;
    static unsigned long lastConfigSave = 0;
    static unsigned long lastCameraUpdate = 0;
    static unsigned long lastCameraTaskInitWarning = 0;
    static unsigned long lastMotionStatusNotify = 0;
    const unsigned long now = millis();

    const bool bleConnected = bleController.isConnected();
    const bool cameraActive = bleCameraService.isCameraActive();

    if (gAutoIgnitionScheduled && (int32_t)(now - gAutoIgnitionAtMs) >= 0) {
        gAutoIgnitionScheduled = false;
        if (!ledState.bladeEnabled) {
            Serial.println("[BOOT] Auto-ignition trigger");
            effectEngine.powerOn();
        } else {
            Serial.println("[BOOT] Auto-ignition skipped (blade already enabled)");
        }
    }

    // Gestisce stato del task camera/motion
    if (cameraActive && !gCameraTaskStreaming) {
        if (cameraManager.isInitialized() && gCameraTaskHandle != nullptr) {
            gCameraTaskShouldRun = true;
            gCameraTaskStreaming = true;
            xTaskNotifyGive(gCameraTaskHandle);
            Serial.println("[MAIN] Camera streaming task started");
        } else if (now - lastCameraTaskInitWarning > 5000) {
            Serial.println("[MAIN] Camera start requested but camera not initialized or task missing");
            lastCameraTaskInitWarning = now;
        }
    } else if (!cameraActive && gCameraTaskStreaming) {
        gCameraTaskShouldRun = false;
        gCameraTaskStreaming = false;
        Serial.println("[MAIN] Camera streaming task stopping");
    }

    if (gMotionResultQueue) {
        MotionTaskResult result;
        if (xQueueReceive(gMotionResultQueue, &result, 0) == pdTRUE) {
            gCachedMotionResult = result;
        }
    }

    // Process motion data for gesture detection and perturbations
    const MotionProcessor::ProcessedMotion* processedMotion = nullptr;

    if (gCachedMotionResult.valid && bleMotionService.isMotionEnabled()) {
        processedMotion = &gCachedMotionResult.processedMotion;

        // Update BLE motion service
        bleMotionService.update(gCachedMotionResult.motionDetected, false, processedMotion);
        if (now - lastMotionStatusNotify > 300) {
            bleMotionService.notifyStatus();
            lastMotionStatusNotify = now;
        }
    }

    // Debug loop ogni 10 secondi (disabilitato durante OTA per non rallentare)
    if (!otaManager.isOTAInProgress() && now - lastLoopDebug > 10000) {
        Serial.printf("[LOOP] Running, OTA state: %d, heap: %u\n",
            (int)otaManager.getState(), ESP.getFreeHeap());
        lastLoopDebug = now;
    }

    // Aggiorna OTA Manager (controlla timeout)
    otaManager.update();

    StatusLedManager& ledManager = StatusLedManager::getInstance();
   // todo usare colore progressivo viola blu verde in base al progresso della barra
    // Se OTA in corso: blocca LED strip e mostra status OTA
    if (otaManager.isOTAInProgress()) {
        if (!ledManager.isMode(StatusLedManager::Mode::OTA_BLINK)) {
            ledManager.setMode(StatusLedManager::Mode::OTA_BLINK);
        }
        ledManager.updateOtaBlink();  // Blink veloce per indicare OTA

        // Visualizza progresso OTA sulla striscia LED
        // Aggiorna solo se progresso è cambiato (riduce overhead FastLED.show())
        static uint8_t lastOtaProgress = 0;
        uint8_t currentProgress = otaManager.getProgress();

        if (currentProgress != lastOtaProgress) {
            lastOtaProgress = currentProgress;

            // Calcola numero LED logici da accendere (rispetta fold point)
            uint16_t foldPoint = ledState.foldPoint;
            uint16_t logicalLedsToFill = (foldPoint * currentProgress) / 100;

            // Colore in base allo stato OTA
            CRGB color;
            OTAState state = otaManager.getState();

            if (state == OTAState::WAITING) {
                color = CRGB(128, 0, 128);  // Viola - in attesa del primo chunk
            } else if (state == OTAState::RECEIVING) {
                color = CRGB(0, 0, 255);    // Blu - trasferimento in corso
            } else if (state == OTAState::VERIFYING || state == OTAState::READY) {
                color = CRGB(0, 255, 0);    // Verde - completato/verifica
            } else if (state == OTAState::ERROR) {
                color = CRGB(255, 0, 0);    // Rosso - errore
            } else {
                color = CRGB(128, 0, 128);  // Viola - default (IDLE/RECOVERY)
            }

            // Riempimento progressivo simmetrico dalla base (rispetta fold point)
            fill_solid(leds, NUM_LEDS, CRGB::Black);

            for (uint16_t i = 0; i < logicalLedsToFill; i++) {
                // Accende LED simmetrici (base verso punta su entrambi i lati)
                uint16_t led1 = i;
                uint16_t led2 = (NUM_LEDS - 1) - i;
                leds[led1] = color;
                leds[led2] = color;
            }

            // Show solo quando cambia % (max 100 chiamate invece di migliaia)
            FastLED.setBrightness(60);  // Luminosità media per visibilità
            FastLED.show();

            Serial.printf("[OTA LED] Progress: %u%% | Logical LEDs: %u/%u | Physical LEDs: %u | State: %d\n",
                currentProgress, logicalLedsToFill, foldPoint, logicalLedsToFill * 2, (int)state);
        }
    } else {
        // Funzionamento normale

        // Se eravamo in OTA, torna a modalità status LED di default
        if (ledManager.isMode(StatusLedManager::Mode::OTA_BLINK)) {
            ledManager.setMode(StatusLedManager::Mode::STATUS_LED);
            ledManager.refreshCameraFlashState();
        }

        const bool manualFlashActive = cameraManager.isFlashEnabled() && cameraManager.getFlashBrightness() > 0;

        // LED FLASH CAMERA: priorità assoluta
        if (manualFlashActive) {
            ledManager.requestCameraFlash(StatusLedManager::FlashSource::MANUAL, cameraManager.getFlashBrightness());
        } else {
            ledManager.releaseCameraFlash(StatusLedManager::FlashSource::MANUAL);
        }

        if (cameraActive) {
            // Modalità FLASH: scrivi direttamente l'intensità del flash
            // NOTA: Il flash viene aggiornato automaticamente dal task camera/motion
            uint8_t flashIntensity = gCachedMotionResult.valid ? gCachedMotionResult.flashIntensity : 150;

            ledManager.requestCameraFlash(StatusLedManager::FlashSource::AUTO, flashIntensity);
        } else {
            ledManager.releaseCameraFlash(StatusLedManager::FlashSource::AUTO);
        }

        if (!ledManager.isCameraFlashActive()) {
            // Modalità NOTIFICA: LED normale solo quando camera spenta
            ledManager.updateStatusLed(bleConnected, ledState.statusLedEnabled, ledState.statusLedBrightness);
        }

        // Render LED strip with motion integration
        effectEngine.render(ledState, processedMotion);

        // Notifica stato BLE ogni 500ms se c'è una connessione
        if (bleConnected && now - lastBleNotify > 500) {
            bleController.notifyState();
            lastBleNotify = now;
        }

        // Salvataggio ritardato della configurazione (ogni 5 secondi se config dirty)
        if (bleController.isConfigDirty() && now - lastConfigSave > 5000) {
            Serial.println("[CONFIG] Config marked dirty, saving...");
            if (configManager.saveConfig()) {
                bleController.setConfigDirty(false);
                lastConfigSave = now;
            } else {
                Serial.println("[CONFIG ERROR] Failed to save config, will retry");
            }
        }

        // Aggiorna metriche camera ogni 1 secondo
        if (now - lastCameraUpdate > 1000) {
            bleCameraService.updateMetrics();
            bleCameraService.notifyStatus();
            lastCameraUpdate = now;
        }

    }

    yield();
}

static OpticalFlowDetector::Direction rotateDirection90CW(OpticalFlowDetector::Direction dir) {
    switch (dir) {
        case OpticalFlowDetector::Direction::UP:         return OpticalFlowDetector::Direction::RIGHT;
        case OpticalFlowDetector::Direction::UP_RIGHT:   return OpticalFlowDetector::Direction::DOWN_RIGHT;
        case OpticalFlowDetector::Direction::RIGHT:      return OpticalFlowDetector::Direction::DOWN;
        case OpticalFlowDetector::Direction::DOWN_RIGHT: return OpticalFlowDetector::Direction::DOWN_LEFT;
        case OpticalFlowDetector::Direction::DOWN:       return OpticalFlowDetector::Direction::LEFT;
        case OpticalFlowDetector::Direction::DOWN_LEFT:  return OpticalFlowDetector::Direction::UP_LEFT;
        case OpticalFlowDetector::Direction::LEFT:       return OpticalFlowDetector::Direction::UP;
        case OpticalFlowDetector::Direction::UP_LEFT:    return OpticalFlowDetector::Direction::UP_RIGHT;
        default: return dir;
    }
}

static void CameraCaptureTask(void* pvParameters) {
    (void)pvParameters;

    bool motionInitialized = false;

    for (;;) {
        // Attende un segnale d'avvio
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (gCameraTaskShouldRun) {
            if (!cameraManager.isInitialized() || !bleCameraService.isCameraActive()) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            uint8_t* frameBuffer = nullptr;
            size_t frameLength = 0;
            if (!cameraManager.captureFrame(&frameBuffer, &frameLength)) {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }

            if (!motionInitialized && frameLength > 0) {
                if (motionDetector.begin(320, 240)) {
                    motionInitialized = true;
                    Serial.println("[CAM TASK] Motion detector initialized");
                } else {
                    Serial.println("[CAM TASK] Motion detector init failed");
                }
            }

            bool motionDetected = false;
            if (motionInitialized) {
                motionDetected = motionDetector.processFrame(frameBuffer, frameLength);
            }

            cameraManager.releaseFrame();

            if (motionInitialized) {
                MotionTaskResult result;
                result.valid = true;
                result.motionDetected = motionDetected;
                result.flashIntensity = motionDetector.getRecommendedFlashIntensity();
                result.motionIntensity = motionDetector.getMotionIntensity();
                result.direction = rotateDirection90CW(motionDetector.getMotionDirection());
                result.timestamp = millis();
                result.processedMotion = motionProcessor.process(
                    result.motionIntensity,
                    result.direction,
                    motionDetector.getMotionSpeed(),
                    result.timestamp,
                    motionDetector
                );

                if (gMotionResultQueue) {
                    // Usa xQueueSend con timeout 0 per non bloccare (drop se piena)
                    xQueueSend(gMotionResultQueue, &result, 0);
                }
            }

            if (!gCameraTaskShouldRun) {
                break;
            }

            // Minimo delay per evitare watchdog timeout e permettere altre task
            taskYIELD(); // Più veloce di vTaskDelay(1), previene comunque il WDT
        }
    }
}
