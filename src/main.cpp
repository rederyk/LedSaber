#include <Arduino.h>
#include <FastLED.h>
#include <BLEDevice.h>
#include <esp_gap_ble_api.h>

#include "BLELedController.h"
#include "OTAManager.h"
#include "ConfigManager.h"

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
static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 60;

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
// MAPPING LED FISICI PER STRISCIA PIEGATA
// ============================================================================

/**
 * Mappa un indice logico (dalla base alla punta della spada) all'indice fisico
 * sulla striscia LED piegata.
 *
 * ESEMPIO con NUM_LEDS=144 e foldPoint=72:
 * - Indici logici 0-71   -> LED fisici 0-71    (prima metà: base verso piega)
 * - Indici logici 72-143 -> LED fisici 143-72  (seconda metà: dalla piega alla punta)
 *
 * @param logicalIndex Indice logico (0 = base spada, NUM_LEDS-1 = punta)
 * @param foldPoint Punto di piegatura della striscia
 * @return Indice fisico del LED sulla striscia
 */
static uint16_t mapLedIndex(uint16_t logicalIndex, uint16_t foldPoint) {
    if (logicalIndex < foldPoint) {
        // Prima metà: mappatura diretta (0->0, 1->1, ..., foldPoint-1->foldPoint-1)
        return logicalIndex;
    } else {
        // Seconda metà: mappatura invertita
        // logicalIndex=72 -> physicalIndex=143 (ultimo LED fisico)
        // logicalIndex=143 -> physicalIndex=72 (LED al punto di piega)
        uint16_t offset = logicalIndex - foldPoint;
        return (NUM_LEDS - 1) - offset;
    }
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

static void updateStatusLed(bool bleConnected, bool btConnected) {
    static unsigned long lastBlink = 0;
    static bool ledOn = false;
    unsigned long now = millis();

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
            uint8_t step = ledState.speed / 10;
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
            uint16_t ignitionSpeed = map(ledState.speed, 1, 255, 50, 5);

            // Aggiorna progressione ogni N millisecondi
            if (now - lastIgnitionUpdate > ignitionSpeed) {
                if (ignitionProgress < NUM_LEDS) {
                    ignitionProgress++;
                } else {
                    ignitionProgress = 0;  // Riavvia animazione
                }
                lastIgnitionUpdate = now;
            }

            // Spegni tutti i LED
            fill_solid(leds, NUM_LEDS, CRGB::Black);

            // Accendi progressivamente dalla base (0) alla punta (NUM_LEDS-1)
            CRGB color = CRGB(ledState.r, ledState.g, ledState.b);
            for (uint16_t i = 0; i < ignitionProgress; i++) {
                uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);

                // Effetto "glow" alla punta: ultimi 5 LED più luminosi
                if (i >= ignitionProgress - 5 && i < ignitionProgress) {
                    uint8_t fade = map(i, ignitionProgress - 5, ignitionProgress - 1, 100, 255);
                    leds[physicalIndex] = color;
                    leds[physicalIndex].fadeToBlackBy(255 - fade);
                } else {
                    leds[physicalIndex] = color;
                }
            }

        } else if (ledState.effect == "retraction") {
            // EFFETTO: Spegnimento progressivo dalla punta alla base
            static uint16_t retractionProgress = NUM_LEDS;
            static unsigned long lastRetractionUpdate = 0;

            uint16_t retractionSpeed = map(ledState.speed, 1, 255, 50, 5);

            if (now - lastRetractionUpdate > retractionSpeed) {
                if (retractionProgress > 0) {
                    retractionProgress--;
                } else {
                    retractionProgress = NUM_LEDS;  // Riavvia
                }
                lastRetractionUpdate = now;
            }

            fill_solid(leds, NUM_LEDS, CRGB::Black);
            CRGB color = CRGB(ledState.r, ledState.g, ledState.b);

            for (uint16_t i = 0; i < retractionProgress; i++) {
                uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);

                // Effetto fade alla "punta" che si ritrae
                if (i >= retractionProgress - 5 && i < retractionProgress) {
                    uint8_t fade = map(i, retractionProgress - 5, retractionProgress - 1, 100, 255);
                    leds[physicalIndex] = color;
                    leds[physicalIndex].fadeToBlackBy(255 - fade);
                } else {
                    leds[physicalIndex] = color;
                }
            }
        } else if (ledState.effect == "flicker") {
            // EFFETTO: Instabilità del plasma (tipo Kylo Ren)
            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);

            // 'speed' controlla l'intensità delle fluttuazioni (1-255)
            // speed basso = flicker leggero, speed alto = molto instabile
            uint8_t flickerIntensity = ledState.speed;

            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);

                // Rumore casuale per ogni LED
                uint8_t noise = random8(flickerIntensity);
                uint8_t brightness = 255 - (noise / 2);  // Non scendere troppo

                leds[physicalIndex] = baseColor;
                leds[physicalIndex].fadeToBlackBy(255 - brightness);
            }

        } else if (ledState.effect == "unstable") {
            // EFFETTO: Combinazione flicker + pulses casuali (Kylo Ren avanzato)
            static uint8_t unstableHeat[NUM_LEDS];
            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);

            // Decay del "calore"
            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                unstableHeat[i] = qsub8(unstableHeat[i], random8(5, 15));
            }

            // Aggiungi nuovi "spark" casuali
            if (random8() < ledState.speed / 2) {
                uint16_t pos = random16(NUM_LEDS);
                unstableHeat[pos] = qadd8(unstableHeat[pos], random8(100, 200));
            }

            // Rendering
            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);

                // Base colore + heat map
                uint8_t brightness = scale8(255, 200 + (unstableHeat[i] / 4));
                leds[physicalIndex] = baseColor;
                leds[physicalIndex].fadeToBlackBy(255 - brightness);
            }
        } else if (ledState.effect == "pulse") {
            // EFFETTO: Onde di energia che percorrono la lama
            static uint16_t pulsePosition = 0;
            static unsigned long lastPulseUpdate = 0;

            // Velocità del pulse controllata da 'speed'
            uint16_t pulseSpeed = map(ledState.speed, 1, 255, 100, 5);

            if (now - lastPulseUpdate > pulseSpeed) {
                pulsePosition = (pulsePosition + 1) % NUM_LEDS;
                lastPulseUpdate = now;
            }

            // Riempimento base
            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
            fill_solid(leds, NUM_LEDS, baseColor);

            // Crea "onda" di luminosità
            const uint8_t pulseWidth = 15;  // Larghezza dell'onda

            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);

                // Calcola distanza dal centro del pulse
                int16_t distance = abs((int16_t)i - (int16_t)pulsePosition);

                if (distance < pulseWidth) {
                    // Dentro il pulse: aumenta luminosità
                    uint8_t brightness = map(distance, 0, pulseWidth, 255, 150);
                    leds[physicalIndex] = baseColor;
                    leds[physicalIndex].fadeToBlackBy(255 - brightness);
                }
            }

        } else if (ledState.effect == "dual_pulse") {
            // EFFETTO: Due pulse che si muovono in direzioni opposte
            static uint16_t pulse1Pos = 0;
            static uint16_t pulse2Pos = NUM_LEDS / 2;
            static unsigned long lastDualPulseUpdate = 0;

            uint16_t pulseSpeed = map(ledState.speed, 1, 255, 80, 5);

            if (now - lastDualPulseUpdate > pulseSpeed) {
                pulse1Pos = (pulse1Pos + 1) % NUM_LEDS;
                pulse2Pos = (pulse2Pos > 0) ? (pulse2Pos - 1) : (NUM_LEDS - 1);
                lastDualPulseUpdate = now;
            }

            CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
            fill_solid(leds, NUM_LEDS, baseColor);

            const uint8_t pulseWidth = 10;

            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);

                int16_t dist1 = abs((int16_t)i - (int16_t)pulse1Pos);
                int16_t dist2 = abs((int16_t)i - (int16_t)pulse2Pos);

                uint8_t brightness = 150;
                if (dist1 < pulseWidth) {
                    brightness = max(brightness, (uint8_t)map(dist1, 0, pulseWidth, 255, 150));
                }
                if (dist2 < pulseWidth) {
                    brightness = max(brightness, (uint8_t)map(dist2, 0, pulseWidth, 255, 150));
                }

                leds[physicalIndex] = baseColor;
                leds[physicalIndex].fadeToBlackBy(255 - brightness);
            }
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

                // Flash bianco che sovrappone il colore base
                CRGB baseColor = CRGB(ledState.r, ledState.g, ledState.b);
                CRGB flashColor = CRGB(255, 255, 255);

                for (uint16_t i = 0; i < NUM_LEDS; i++) {
                    uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);
                    leds[physicalIndex] = blend(baseColor, flashColor, clashBrightness);
                }
            } else {
                // Colore base quando non c'è clash
                fill_solid(leds, NUM_LEDS, CRGB(ledState.r, ledState.g, ledState.b));
            }

        } else if (ledState.effect == "rainbow_blade") {
            // EFFETTO: Arcobaleno che percorre la lama linearmente (rispetta piegatura)
            static uint8_t rainbowHue = 0;

            uint8_t hueStep = ledState.speed / 10;
            if (hueStep == 0) hueStep = 1;

            for (uint16_t i = 0; i < NUM_LEDS; i++) {
                uint16_t physicalIndex = mapLedIndex(i, ledState.foldPoint);

                // Calcola hue basato sulla posizione logica (non fisica!)
                uint8_t hue = rainbowHue + (i * 256 / NUM_LEDS);
                leds[physicalIndex] = CHSV(hue, 255, 255);
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

    // 5. Configura e avvia l'advertising DOPO aver inizializzato tutti i servizi
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(LED_SERVICE_UUID);
    pAdvertising->addServiceUUID(OTA_SERVICE_UUID);
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
        updateStatusLed(bleConnected, false);
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
    }

    yield();
}
