#include <Arduino.h>
#include <FastLED.h>
#include <BluetoothSerial.h>
#include <BLEDevice.h>

#include "BluetoothLogger.h"
#include "BLELedController.h"

// GPIO
static constexpr uint8_t STATUS_LED_PIN = 4;   // LED integrato per stato connessione
static constexpr uint8_t LED_STRIP_PIN = 12;   // Striscia WS2812B
static constexpr uint16_t NUM_LEDS = 144;
static constexpr uint8_t DEFAULT_BRIGHTNESS = 30;

CRGB leds[NUM_LEDS];

// Bluetooth
BluetoothSerial SerialBT;
BluetoothLogger logger(&SerialBT);
LedState ledState;
BLELedController bleController(&ledState);

static void initPeripherals() {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
}

static void updateStatusLed(bool bleConnected, bool btConnected) {
    static unsigned long lastBlink = 0;
    static bool ledOn = false;
    unsigned long now = millis();

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

static void handleBtCommands() {
    if (!SerialBT.available()) {
        return;
    }

    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();

    if (cmd == "ping") {
        logger.log("pong");
    } else if (cmd == "status") {
        logger.logf("Uptime: %lu ms", millis());
        logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
        logger.logf("BT connected: %s", logger.isConnected() ? "YES" : "NO");
        logger.logf("BLE connected: %s", bleController.isConnected() ? "YES" : "NO");
        logger.logf("LED: R=%d G=%d B=%d Brightness=%d Effect=%s Enabled=%d",
            ledState.r, ledState.g, ledState.b, ledState.brightness,
            ledState.effect.c_str(), ledState.enabled);
    } else if (cmd == "reset") {
        logger.log("Rebooting in 500ms...");
        delay(500);
        ESP.restart();
    } else {
        logger.logf("Unknown command: %s", cmd.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== BOOT LED DENSE (BLE TEST) ===");

    initPeripherals();

    // Avvia Bluetooth Classic per logging
    logger.begin("LedDense-Log");
    logger.log("*** Bluetooth Classic pronto (logging) ***");

    // Avvia BLE GATT per controllo LED
    bleController.begin("LedDense-BLE");
    logger.log("*** BLE GATT Server avviato ***");

    logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
    logger.log("*** SISTEMA PRONTO (TEST BLE) ***");
}

void loop() {
    static unsigned long lastBleNotify = 0;
    const unsigned long now = millis();

    const bool bleConnected = bleController.isConnected();
    const bool btConnected = logger.isConnected();

    updateStatusLed(bleConnected, btConnected);
    renderLedStrip();

    // Notifica stato BLE ogni 500ms se c'è una connessione
    if (bleConnected && now - lastBleNotify > 500) {
        bleController.notifyState();
        lastBleNotify = now;
    }

    handleBtCommands();
    yield();
}
