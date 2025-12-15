#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <FastLED.h>
#include "WebSocketLogger.h"
#include "MinimalDashboard.h"

// WiFi
static const char* SSID = "FASTWEB-2";
static const char* PASSWORD = "lemarmottediinvernofannolacaccaverde";

// GPIO
static constexpr uint8_t LED_PIN = 4;

// Striscia LED FastLED
static constexpr uint8_t LED_STRIP_PIN = 12;
static constexpr uint16_t NUM_LEDS = 144;
static constexpr uint8_t LED_BRIGHTNESS = 30;
CRGB leds[NUM_LEDS];

// Networking
AsyncWebServer* server = nullptr;
AsyncWebSocket ws("/ws");
WebSocketLogger logger(&ws);

// Helper per attendere il WiFi con feedback visivo
static void waitForWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.printf("Connessione a %s", SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  Serial.printf("\nWiFi connesso - IP: %s\n", WiFi.localIP().toString().c_str());
  digitalWrite(LED_PIN, HIGH);
}

// Handler pagina principale (PROGMEM) con intestazioni base
static void handleRoot(AsyncWebServerRequest* request) {
  AsyncWebServerResponse* res = request->beginResponse_P(
      200, "text/html",
      reinterpret_cast<const uint8_t*>(MINIMAL_DASHBOARD),
      strlen_P(MINIMAL_DASHBOARD));
  res->addHeader("Cache-Control", "no-store");
  request->send(res);
}

// Handler OTA (POST /update)
static void handleOtaUpload(AsyncWebServerRequest* request, String filename, size_t index,
                            uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    logger.logf("OTA start: %s", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      logger.log("OTA: begin fallita");
      return;
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
    logger.log("OTA: write fallita");
    return;
  }

  if (final) {
    if (Update.end(true)) {
      logger.logf("OTA ok: %u bytes", static_cast<unsigned>(index + len));
    } else {
      Update.printError(Serial);
      logger.log("OTA: end fallita");
    }
  }
}

static void handleOtaCompleted(AsyncWebServerRequest* request) {
  AsyncWebServerResponse* response =
      request->beginResponse(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
  response->addHeader("Connection", "close");
  request->send(response);

  if (!Update.hasError()) {
    logger.log("OTA completata, riavvio...");
    delay(500);
    ESP.restart();
  }
}

// Inizializza periferiche e server
static void initPeripherals() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
}

static void initServer() {
  if (!server) {
    Serial.println("ERRORE: server non allocato");
    return;
  }
  logger.begin();

  ws.onEvent([](AsyncWebSocket* s, AsyncWebSocketClient* c, AwsEventType t,
                void* arg, uint8_t* data, size_t len) {
    logger.handleWebSocketEvent(s, c, t, arg, data, len);
  });
  server->addHandler(&ws);

  server->on("/", HTTP_GET, handleRoot);
  server->on("/update", HTTP_POST, handleOtaCompleted, handleOtaUpload);
  server->begin();

  logger.logf("Pagina @ 0x%08x", reinterpret_cast<uint32_t>(MINIMAL_DASHBOARD));
  logger.log("*** SISTEMA AVVIATO ***");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BOOT LED DENSE ===");

  // Instanzia server dinamicamente per evitare potenziali problemi di init statico
  server = new AsyncWebServer(80);

  initPeripherals();
  waitForWiFi();
  initServer();

  Serial.println("Server pronto su porta 80");
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  static unsigned long lastCleanup = 0;
  static unsigned long lastPattern = 0;
  static uint8_t blinkState = 0;
  static bool ledState = false;
  static unsigned long lastStrip = 0;
  static uint8_t hue = 0;

  const unsigned long now = millis();

  // Cleanup WebSocket client ogni 2s
  if (now - lastCleanup > 2000) {
    ws.cleanupClients();
    lastCleanup = now;
  }

  // Pattern LED: 3 blink veloci, pausa 2s
  if (blinkState < 6) {
    if (now - lastPattern > 100) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      blinkState++;
      lastPattern = now;
    }
  } else if (now - lastPattern > 2000) {
    blinkState = 0;
    lastPattern = now;
  }

  // Effetto arcobaleno striscia
  if (now - lastStrip > 20) {
    fill_rainbow(leds, NUM_LEDS, hue, 256 / NUM_LEDS);
    FastLED.show();
    hue++;
    lastStrip = now;
  }

  yield();
}
