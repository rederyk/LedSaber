# OTA via Web Browser - ESP32

## Il Problema
ArduinoOTA è instabile e spesso non funziona. La soluzione migliore è usare un **web server HTTP** integrato nell'ESP32.

## La Soluzione: Web Server OTA

### Librerie necessarie
```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
```

### Codice minimo funzionante

```cpp
WebServer server(80);

const char* serverIndex =
"<form method='POST' action='/update' enctype='multipart/form-data'>"
"<h1>ESP32 OTA Update</h1>"
"<input type='file' name='update'>"
"<input type='submit' value='Update'>"
"</form>";

void setup() {
  // Connetti WiFi...

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
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      Update.end(true);
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();
  // Il tuo codice qui...
}
```

## Come Usare OTA

### 1. Prima compilazione (via USB)
```bash
pio run -t upload
```

### 2. Aggiornamenti successivi (via browser)
1. Compila il firmware: `pio run`
2. Apri browser: `http://[IP_ESP32]`
3. Seleziona file: `.pio/build/esp32dev/firmware.bin`
4. Clicca "Update"
5. L'ESP32 si riavvia con il nuovo firmware

## Vantaggi
✅ Funziona sempre
✅ Nessuna dipendenza da mDNS
✅ Interfaccia web semplice
✅ Non serve software aggiuntivo
✅ Debugging chiaro via Serial

## Note
- Il file binario è sempre in `.pio/build/[env]/firmware.bin`
- Dopo l'upload via USB la prima volta, si può usare solo il browser
- Non serve più `upload_protocol = espota` in `platformio.ini`
