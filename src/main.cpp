#include <Arduino.h>
#include <FastLED.h>
#include <BLEDevice.h>
#include <esp_gap_ble_api.h>

#include "BLELedController.h"
#include "OTAManager.h"
#include "ConfigManager.h"

// GPIO
static constexpr uint8_t STATUS_LED_PIN = 4;   // LED integrato per stato connessione
static constexpr uint8_t LED_STRIP_PIN = 13;   // Striscia WS2812B
static constexpr uint16_t NUM_LEDS = 144;
static constexpr uint8_t DEFAULT_BRIGHTNESS = 30;

// Limite di sicurezza per alimentatore 2A
// Calcolo: 5V * 2A = 10W disponibili
// 144 LED * 60mA (max) = 8.64A teorici a luminosità 255
// Luminosità massima sicura: (2A / 8.64A) * 255 ≈ 59
static constexpr uint8_t MAX_SAFE_BRIGHTNESS = 60;

CRGB leds[NUM_LEDS];

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
// FUNZIONI DI STATO E GRAFICA
// ============================================================================

static void initPeripherals() {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

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
        digitalWrite(STATUS_LED_PIN, ledOn ? HIGH : LOW);
        lastBlink = now;
    }
}

static void updateStatusLed(bool bleConnected, bool btConnected) {
    static unsigned long lastBlink = 0;
    static bool ledOn = false;
    unsigned long now = millis();

    // Se il LED è disabilitato via BLE, lo spegniamo
    if (!ledState.statusLedEnabled) {
        digitalWrite(STATUS_LED_PIN, LOW);
        return;
    }

    // LED fisso acceso se c'è almeno una connessione
    if (bleConnected || btConnected) {
        digitalWrite(STATUS_LED_PIN, HIGH);
        ledOn = true;
        return;
    }

    // Blink lento quando nessuna connessione è presente
    if (now - lastBlink > 500) {
        ledOn = !ledOn;
        digitalWrite(STATUS_LED_PIN, ledOn ? HIGH : LOW);
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

    // Debug loop ogni 2 secondi (disabilitato durante OTA per non rallentare)
    if (!otaManager.isOTAInProgress() && now - lastLoopDebug > 2000) {
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
