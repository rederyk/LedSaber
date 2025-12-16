#include <Arduino.h>
#include <FastLED.h>
#include <BLEDevice.h>

#include "BLELedController.h"
#include "OTAManager.h"

// GPIO
static constexpr uint8_t STATUS_LED_PIN = 4;   // LED integrato per stato connessione
static constexpr uint8_t LED_STRIP_PIN = 12;   // Striscia WS2812B
static constexpr uint16_t NUM_LEDS = 144;
static constexpr uint8_t DEFAULT_BRIGHTNESS = 30;

CRGB leds[NUM_LEDS];

// BLE GATT
LedState ledState;
BLELedController bleController(&ledState);
OTAManager otaManager;

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
            FastLED.setBrightness(scale8(breath, ledState.brightness));
            FastLED.show();
            FastLED.setBrightness(ledState.brightness);
            lastUpdate = now;
            return;
        }

        FastLED.setBrightness(ledState.brightness);
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

    // Avvia BLE GATT per controllo LED
    bleController.begin("LedSaber-BLE");
    Serial.println("*** BLE GATT Server avviato ***");

    // Inizializza OTA Manager (usa lo stesso BLE Server)
    otaManager.begin(bleController.getServer());
    Serial.println("*** OTA Manager avviato ***");

    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("*** SISTEMA PRONTO ***");
}

void loop() {
    static unsigned long lastBleNotify = 0;
    static unsigned long lastLoopDebug = 0;
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
    }

    yield();
}
