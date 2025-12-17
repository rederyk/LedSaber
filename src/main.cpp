#include <Arduino.h>
#include <FastLED.h>
#include <BLEDevice.h>
#include <esp_gap_ble_api.h>

#include "BLELedController.h"
#include "OTAManager.h"
#include "ConfigManager.h"
#include "CameraManager.h"
#include "BLECameraService.h"
#include "MotionDetector.h"
#include "BLEMotionService.h"

// GPIO
static constexpr uint8_t STATUS_LED_PIN = 4;   // LED integrato per stato connessione
static constexpr uint8_t STATUS_LED_PWM_CHANNEL = 0;
static constexpr uint16_t STATUS_LED_PWM_FREQ = 5000;
static constexpr uint8_t STATUS_LED_PWM_RES = 8;
static constexpr uint8_t LED_STRIP_PIN = 13;   // Striscia WS2812B
static constexpr uint16_t NUM_LEDS = 144;
static constexpr uint8_t DEFAULT_BRIGHTNESS = 30;
static constexpr uint8_t DEFAULT_STATUS_LED_BRIGHTNESS = 32;

// Limite di sicurezza per alimentatore 2A
// Calcolo: 5V * 2A = 10W disponibili
// 144 LED * 60mA (max) = 8.64A teorici a luminosità 255
// Luminosità massima sicura: (2A / 8.64A) * 255 ≈ 59
// Luminosità massima modulo 5v5A: (4A / 8.64A) * 255 ≈ 112
static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 112;

CRGB leds[NUM_LEDS];

extern LedState ledState;

static void applyStatusLedOutput(bool ledOn, bool force = false, int fallbackBrightness = -1) {
    uint8_t brightness = ledState.statusLedBrightness;
    if (fallbackBrightness >= 0 && brightness == 0) {
        brightness = static_cast<uint8_t>(fallbackBrightness);
    }
    brightness = min<uint8_t>(brightness, 255);

    if (!force && (!ledState.statusLedEnabled || brightness == 0)) {
        ledcWrite(STATUS_LED_PWM_CHANNEL, 0);
        return;
    }

    uint8_t duty = (ledOn && brightness > 0) ? brightness : 0;
    ledcWrite(STATUS_LED_PWM_CHANNEL, duty);
}

// BLE GATT
LedState ledState;
BLELedController bleController(&ledState);
OTAManager otaManager;
ConfigManager configManager(&ledState);

// Camera Manager
CameraManager cameraManager;
BLECameraService bleCameraService(&cameraManager);

// Motion Detector
MotionDetector motionDetector;
BLEMotionService bleMotionService(&motionDetector);

// ============================================================================
// CALLBACKS GLOBALI DEL SERVER BLE
// ============================================================================

class MainServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        bleController.setConnected(true);
        Serial.println("[BLE] Client connected!");
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
// MAPPING LED FISICI PER STRISCIA PIEGATA E ACCOPPIATA
// ============================================================================

/**
 * Scala un colore applicando la brightness globale PRIMA di scriverlo nel buffer.
 * Questo preserva il contrasto relativo negli effetti con fading/dimming interno.
 *
 * @param color Colore da scalare
 * @param brightness Brightness globale (0-255)
 * @return Colore scalato
 */
static inline CRGB scaleColorByBrightness(CRGB color, uint8_t brightness) {
    return CRGB(
        scale8(color.r, brightness),
        scale8(color.g, brightness),
        scale8(color.b, brightness)
    );
}

/**
 * Imposta il colore di una COPPIA di LED accoppiati sulla striscia piegata.
 *
 * IMPORTANTE: La striscia LED è PIEGATA e i LED sono ACCOPPIATI con i LED rivolti
 * verso l'esterno su entrambi i lati. Questo significa che ogni posizione logica
 * corrisponde a DUE LED fisici che devono essere accesi insieme.
 *
 * ESEMPIO con NUM_LEDS=144 e foldPoint=72:
 * - Posizione logica 0   -> accende LED fisici 0 E 143 (base della spada, entrambi i lati)
 * - Posizione logica 1   -> accende LED fisici 1 E 142
 * - Posizione logica 35  -> accende LED fisici 35 E 108
 * - Posizione logica 71  -> accende LED fisici 71 E 72  (punta della spada)
 *
 * @param logicalIndex Posizione logica dalla base (0) alla punta (foldPoint-1)
 * @param foldPoint Punto di piegatura della striscia (tipicamente NUM_LEDS/2)
 * @param color Colore da impostare per entrambi i LED
 */
static void setLedPair(uint16_t logicalIndex, uint16_t foldPoint, CRGB color) {
    // Limita logicalIndex al range valido (0 a foldPoint-1)
    if (logicalIndex >= foldPoint) {
        return;
    }

    // LED del primo lato (mappatura diretta)
    uint16_t led1 = logicalIndex;

    // LED del secondo lato (mappatura invertita)
    // logicalIndex=0 -> led2=143 (NUM_LEDS-1)
    // logicalIndex=1 -> led2=142 (NUM_LEDS-2)
    // logicalIndex=71 -> led2=72 (foldPoint)
    uint16_t led2 = (NUM_LEDS - 1) - logicalIndex;

    // Imposta entrambi i LED
    leds[led1] = color;
    leds[led2] = color;
}

// ============================================================================
// FUNZIONI DI STATO E GRAFICA
// ============================================================================

static void initPeripherals() {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    ledcSetup(STATUS_LED_PWM_CHANNEL, STATUS_LED_PWM_FREQ, STATUS_LED_PWM_RES);
    ledcAttachPin(STATUS_LED_PIN, STATUS_LED_PWM_CHANNEL);
    applyStatusLedOutput(false, true);

    FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
}

static void updateStatusLed_OTAMode() {
    // Blink rosso veloce durante OTA (100ms) - ATTENZIONE: NON TOCCARE!
    static unsigned long lastBlink = 0;
    static bool ledOn = false;
    unsigned long now = millis();

    if (now - lastBlink > 100) {
        ledOn = !ledOn;
        applyStatusLedOutput(ledOn, true, DEFAULT_STATUS_LED_BRIGHTNESS);
        lastBlink = now;
    }
}

static void updateStatusLed(bool bleConnected, bool btConnected, bool cameraFlashMode, uint8_t flashIntensity) {
    static unsigned long lastBlink = 0;
    static bool ledOn = false;
    unsigned long now = millis();

    // MODALITÀ CAMERA FLASH: ignora tutto e usa solo l'intensità flash
    if (cameraFlashMode) {
        // Scrivi direttamente l'intensità flash senza controlli
        ledcWrite(STATUS_LED_PWM_CHANNEL, flashIntensity);
        return;
    }

    // Se il LED è disabilitato via BLE, lo spegniamo
    if (!ledState.statusLedEnabled || ledState.statusLedBrightness == 0) {
        applyStatusLedOutput(false);
        ledOn = false;
        return;
    }

    // LED fisso acceso se c'è almeno una connessione
    if (bleConnected || btConnected) {
        applyStatusLedOutput(true);
        ledOn = true;
        return;
    }

    // Blink lento quando nessuna connessione è presente
    if (now - lastBlink > 500) {
        ledOn = !ledOn;
        applyStatusLedOutput(ledOn);
        lastBlink = now;
    }
}

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

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== LEDSABER (BLE GATT + OTA) ===");

    initPeripherals();

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
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // iPhone compatibility
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->start();

    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("*** SISTEMA PRONTIssimo ***");
}

void loop() {
    static unsigned long lastBleNotify = 0;
    static unsigned long lastLoopDebug = 0;
    static unsigned long lastConfigSave = 0;
    static unsigned long lastCameraUpdate = 0;
    static unsigned long lastMotionUpdate = 0;
    static bool motionDetectorInitialized = false;
    const unsigned long now = millis();

    const bool bleConnected = bleController.isConnected();

    // Debug loop ogni 10 secondi (disabilitato durante OTA per non rallentare)
    if (!otaManager.isOTAInProgress() && now - lastLoopDebug > 10000) {
        Serial.printf("[LOOP] Running, OTA state: %d, heap: %u\n",
            (int)otaManager.getState(), ESP.getFreeHeap());
        lastLoopDebug = now;
    }

    // Aggiorna OTA Manager (controlla timeout)
    otaManager.update();

    // Se OTA in corso: blocca LED strip e mostra status OTA
    if (otaManager.isOTAInProgress()) {
        updateStatusLed_OTAMode();  // Blink veloce per indicare OTA
        // LED strip spento durante OTA - NON chiamare FastLED.show() per non rallentare!
        // FastLED.show() blocca per diversi ms e rallenta il trasferimento BLE
    } else {
        // Funzionamento normale

        // Determina se il LED deve essere in modalità flash camera
        bool cameraFlashMode = bleCameraService.isCameraActive();
        uint8_t flashIntensity = motionDetectorInitialized ? motionDetector.getRecommendedFlashIntensity() : 0;

        updateStatusLed(bleConnected, false, cameraFlashMode, flashIntensity);
        renderLedStrip();

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

        // Cattura continua camera se attiva
        if (bleCameraService.isCameraActive()) {
            uint8_t* frameBuffer = nullptr;
            size_t frameLength = 0;

            // Auto flash basato su luminosità
            static bool wasMotionDetected = false;

            if (cameraManager.captureFrame(&frameBuffer, &frameLength)) {
                // Frame catturato con successo

                // Inizializza motion detector al primo frame (dimensioni note)
                if (!motionDetectorInitialized && frameLength > 0) {
                    // QVGA = 320x240 = 76800 bytes
                    if (motionDetector.begin(320, 240)) {
                        motionDetectorInitialized = true;
                        Serial.println("[MAIN] Motion detector initialized");
                    }
                }

                // Processa frame per motion detection se abilitato
                if (motionDetectorInitialized && bleMotionService.isMotionEnabled()) {
                    bool motionDetected = motionDetector.processFrame(frameBuffer, frameLength);

                    wasMotionDetected = motionDetected;

                    // Aggiorna BLE motion service con eventi
                    bleMotionService.update(motionDetected, false);
                }

                cameraManager.releaseFrame();
            }
            // Non aggiungiamo delay - lasciamo che la camera catturi al max FPS
        }

        // Aggiorna metriche motion ogni 1 secondo
        if (motionDetectorInitialized && now - lastMotionUpdate > 1000) {
            bleMotionService.notifyStatus();
            lastMotionUpdate = now;
        }
    }

    yield();
}
