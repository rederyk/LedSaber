#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <FastLED.h>

// Configurazione WiFi
const char* ssid = "FASTWEB-2";
const char* password = "lemarmottediinvernofannolacaccaverde";

// LED built-in per feedback visivo
const int LED_PIN = 4;

// Configurazione striscia LED FastLED
#define LED_STRIP_PIN 12
#define NUM_LEDS 144
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

// Web server sulla porta 80
WebServer server(80);

// Pagina HTML per upload OTA
const char* serverIndex =
"<form method='POST' action='/update' enctype='multipart/form-data'>"
"<h1>ESP32 OTA Update</h1>"
"<input type='file' name='update'>"
"<input type='submit' value='Update'>"
"</form>";

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========================================");
  Serial.println("   FIRMWARE VERSIONE 2.0 - OTA TEST");
  Serial.println("========================================\n");

  pinMode(LED_PIN, OUTPUT);

  // Inizializza striscia LED FastLED
  FastLED.addLeds<LED_TYPE, LED_STRIP_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(30); // Luminosità al 50% per test
  Serial.println("Striscia LED FastLED inizializzata (144 LED sul pin 2)");

  // Connessione WiFi
  Serial.print("Connessione a ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  Serial.println("\nWiFi connesso!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_PIN, HIGH);

  // Configura Web Server per OTA
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("HTTP server avviato");
  Serial.print("Vai su: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();

  // CODICE APPLICATIVO - Pattern LED V2.0 (3 blink veloci + pausa)
  static unsigned long lastAction = 0;
  static int blinkCount = 0;
  static bool ledState = false;

  unsigned long currentMillis = millis();

  // Nuovo pattern: 3 blink veloci (100ms) poi pausa 2 secondi
  if (blinkCount < 6) {  // 3 blink = 6 toggle (on/off)
    if (currentMillis - lastAction > 100) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      lastAction = currentMillis;
      blinkCount++;

      if (blinkCount == 6) {
        Serial.println(">>> V2.0: Pattern completato - 3 BLINK VELOCI <<<");
      }
    }
  } else {
    // Pausa di 2 secondi prima di ripartire
    if (currentMillis - lastAction > 2000) {
      blinkCount = 0;
      Serial.println("*** FIRMWARE V2.0 ATTIVO - OTA FUNZIONA! ***");
    }
  }

  // TEST STRISCIA LED - Effetto arcobaleno rotante
  static unsigned long lastStripUpdate = 0;
  static uint8_t hue = 0;

  if (currentMillis - lastStripUpdate > 20) {  // Aggiorna ogni 20ms
    // Riempie la striscia con un effetto arcobaleno rotante
    fill_rainbow(leds, NUM_LEDS, hue, 256 / NUM_LEDS);
    FastLED.show();

    hue++;  // Incrementa la tonalità per effetto rotante
    lastStripUpdate = currentMillis;
  }

  yield();
}
