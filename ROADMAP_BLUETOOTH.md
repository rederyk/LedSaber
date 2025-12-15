# 📋 ROADMAP: Migrazione da WiFi a Bluetooth Classic

## 📊 Situazione Attuale (DATI REALI)

### Memoria Flash
```
Flash totale:     1966080 bytes (1.87 MB) - app0 slot
Flash utilizzata:  833273 bytes (813 KB)
Flash disponibile: 1132807 bytes (1.08 MB)
Utilizzo:         42.4%
```

### Memoria RAM
```
RAM totale:       327680 bytes (320 KB)
RAM utilizzata:    46964 bytes (45.8 KB)
RAM disponibile:  280716 bytes (274 KB)
Utilizzo:         14.3%
```

### Partizioni Attuali (4MB flash totale)
```csv
# Name,   Type, SubType, Offset,  Size,      Hex Size
nvs,      data, nvs,     0x9000,  20KB,      0x5000
otadata,  data, ota,     0xe000,  8KB,       0x2000
app0,     app,  ota_0,   0x10000, 1.9MB,     0x1E0000
app1,     app,  ota_1,   0x1F0000,1.9MB,     0x1E0000
spiffs,   data, spiffs,  0x3D0000,192KB,     0x30000
```

### Stack WiFi Attuale da Rimuovere
**Librerie:**
- `WiFi.h` (framework ESP32)
- `AsyncTCP` (~20-30KB)
- `ESPAsyncWebServer` (~30-50KB)
- `Update.h` (OTA)
- `WebSocketLogger` (custom)
- `MinimalDashboard.h` (HTML in PROGMEM)

**Memoria stimata liberabile:**
- Flash: ~150-200KB
- RAM: ~80-120KB (stack WiFi, buffer WebSocket, HTML)
- CPU: ~15-25% (gestione connessioni, WebSocket keepalive)

---

## 🎯 FASE 1: Preparazione Ambiente Bluetooth

### 1.1 Modificare `platformio.ini`

**RIMUOVERE:**
```ini
lib_deps =
    WebServer
    ottowinter/ESPAsyncWebServer-esphome@^3.1.0
    me-no-dev/AsyncTCP@^1.1.1
```

**MANTENERE:**
```ini
lib_deps =
    fastled/FastLED@^3.7.0
```

**NOTE:** `BluetoothSerial` è incluso nel framework Arduino ESP32, non serve aggiungerlo.

---

## 🎯 FASE 2: Implementazione Bluetooth Classic SPP

### 2.1 Creare `include/BluetoothLogger.h`

```cpp
#ifndef BLUETOOTH_LOGGER_H
#define BLUETOOTH_LOGGER_H

#include <BluetoothSerial.h>

class BluetoothLogger {
private:
    BluetoothSerial* bt;
    bool enabled;

public:
    BluetoothLogger(BluetoothSerial* bluetooth);
    void begin(const char* deviceName);
    void log(const String& message);
    void logf(const char* format, ...);
    bool isConnected();
};

#endif
```

---

### 2.2 Creare `src/BluetoothLogger.cpp`

```cpp
#include "BluetoothLogger.h"
#include <cstdarg>

BluetoothLogger::BluetoothLogger(BluetoothSerial* bluetooth) {
    bt = bluetooth;
    enabled = false;
}

void BluetoothLogger::begin(const char* deviceName) {
    if (!bt->begin(deviceName)) {
        Serial.println("[BT ERROR] Failed to start Bluetooth!");
        enabled = false;
    } else {
        Serial.printf("[BT OK] Bluetooth started: %s\n", deviceName);
        enabled = true;
    }
}

void BluetoothLogger::log(const String& message) {
    unsigned long ms = millis();
    String timestamped = "[" + String(ms) + "ms] " + message;

    // Serial USB (sempre attivo)
    Serial.println(timestamped);

    // Bluetooth (se connesso)
    if (enabled && bt->hasClient()) {
        bt->println(timestamped);
    }
}

void BluetoothLogger::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(String(buffer));
}

bool BluetoothLogger::isConnected() {
    return enabled && bt->hasClient();
}
```

---

### 2.3 Modificare `src/main.cpp`

**RIMUOVERE:**
```cpp
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "WebSocketLogger.h"
#include "MinimalDashboard.h"

// WiFi credentials
const char* ssid = "...";
const char* password = "...";

// Server e WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WebSocketLogger logger(&ws);

// Funzioni WiFi
void waitForWiFi() { ... }
void handleRoot(AsyncWebServerRequest *request) { ... }
void handleOtaUpload(...) { ... }
void initServer() { ... }
```

**AGGIUNGERE:**
```cpp
#include <BluetoothSerial.h>
#include "BluetoothLogger.h"

BluetoothSerial SerialBT;
BluetoothLogger logger(&SerialBT);
```

---

**MODIFICARE `setup()`:**
```cpp
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== BOOT LED DENSE (Bluetooth) ===");

    initPeripherals();

    // Avvia Bluetooth
    logger.begin("LedDense-ESP32");  // Nome visibile su Android/PC
    logger.log("*** SISTEMA AVVIATO ***");
    logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
}
```

---

**MODIFICARE `loop()` (opzionale: comandi Bluetooth):**
```cpp
void loop() {
    static unsigned long lastStrip = 0;
    static uint8_t hue = 0;

    const unsigned long now = millis();

    // Effetto arcobaleno striscia LED
    if (now - lastStrip > 20) {
        fill_rainbow(leds, NUM_LEDS, hue, 256 / NUM_LEDS);
        FastLED.show();
        hue++;
        lastStrip = now;
    }

    // Gestisci comandi Bluetooth (opzionale)
    if (SerialBT.available()) {
        String cmd = SerialBT.readStringUntil('\n');
        cmd.trim();

        if (cmd == "ping") {
            logger.log("pong");
        } else if (cmd == "status") {
            logger.logf("Uptime: %lu ms", millis());
            logger.logf("Free heap: %u bytes", ESP.getFreeHeap());
            logger.logf("BT connected: %s", logger.isConnected() ? "YES" : "NO");
        } else if (cmd == "reset") {
            logger.log("Rebooting...");
            delay(500);
            ESP.restart();
        } else {
            logger.logf("Unknown command: %s", cmd.c_str());
        }
    }

    yield();
}
```

---

## 🎯 FASE 3: Metodi di Connessione

### 3.1 Da PC/Linux (Terminale)

**Usando `bluetoothctl`:**
```bash
# Scansione dispositivi
bluetoothctl scan on
# Aspetta di vedere "LedDense-ESP32" con MAC XX:XX:XX:XX:XX:XX

# Pairing
bluetoothctl pair XX:XX:XX:XX:XX:XX
bluetoothctl trust XX:XX:XX:XX:XX:XX
bluetoothctl connect XX:XX:XX:XX:XX:XX

# Connessione seriale
sudo rfcomm bind 0 XX:XX:XX:XX:XX:XX
screen /dev/rfcomm0 115200
```

**Oppure con `minicom`:**
```bash
sudo rfcomm bind /dev/rfcomm0 XX:XX:XX:XX:XX:XX
minicom -D /dev/rfcomm0 -b 115200
```

---

### 3.2 Da Android - App Consigliate

#### A) Serial Bluetooth Terminal ⭐ CONSIGLIATA
- 📱 [Google Play Store](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal)
- ✅ Gratuita, open source
- ✅ Supporto Bluetooth Classic SPP
- ✅ Invio comandi personalizzati
- ✅ Logging in tempo reale con timestamp
- ✅ Macro/pulsanti configurabili

**Configurazione:**
1. Apri app → Menu → Devices
2. Cerca "LedDense-ESP32"
3. Pairing (codice: 1234 o 0000)
4. Connect
5. Invia comandi (es: `ping`, `status`)

---

#### B) Bluetooth Electronics (Arduino-friendly)
- 📱 [Google Play](https://play.google.com/store/apps/details?id=com.keuwl.arduinobluetooth)
- ✅ UI personalizzabile (bottoni, slider)
- ✅ Invio comandi predefiniti

---

#### C) BlueTerm (Semplice terminale)
- 📱 Minimale ma funzionale
- ✅ Solo testo, ideale per debug

---

## 🎯 FASE 4: Ordine di Implementazione

### Step 1: Backup
```bash
git add .
git commit -m "Backup before Bluetooth migration"
```

### Step 2: Rimuovere dipendenze WiFi
- [x] Modificare `platformio.ini` (rimuovere AsyncTCP, ESPAsyncWebServer)
- [x] Rimuovere `WebSocketLogger.h/cpp`
- [x] Rimuovere `MinimalDashboard.h`

### Step 3: Creare logger Bluetooth
- [ ] Creare `include/BluetoothLogger.h`
- [ ] Creare `src/BluetoothLogger.cpp`

### Step 4: Modificare `main.cpp`
- [ ] Rimuovere tutto il codice WiFi (setup/loop/handlers)
- [ ] Aggiungere codice Bluetooth
- [ ] Semplificare `loop()` (rimuovere OTA/WebSocket)

### Step 5: Test
```bash
pio run -t upload -t monitor
```

### Step 6: Connessione da Android
- [ ] Installare "Serial Bluetooth Terminal"
- [ ] Pairing con "LedDense-ESP32"
- [ ] Verificare ricezione log

---

## 📊 Risultati Attesi

| Metrica | Prima (WiFi) | Dopo (Bluetooth) | Risparmio |
|---------|--------------|------------------|-----------|
| Flash usata | 833 KB | ~650 KB | -180 KB |
| RAM libera | 280 KB | ~350 KB | +70 KB |
| Boot time | ~3-5s | ~1-2s | -60% |
| CPU idle | ~60% | ~85% | +25% |

---

## ⚠️ Limitazioni Bluetooth vs WiFi

| Funzione | WiFi | Bluetooth Classic |
|----------|------|-------------------|
| Velocità | ~50 Mbps | ~1 Mbps |
| Range | ~50m | ~10-30m |
| OTA | ✅ Veloce | ❌ Troppo lento |
| Multi-client | ✅ Sì | ❌ No (1 client) |
| Logging | ✅ WebSocket | ✅ SPP |
| RAM usage | Alto | Basso |
| Consumo | Alto | Medio |

---

## 🚀 Alternative per OTA Futuro

Visto che rinunci a OTA via rete:

1. **USB (attuale)** - ⭐ Raccomandato
   - Veloce (~500 KB/s)
   - Affidabile
   - Zero overhead RAM

2. **SD Card**
   - Caricare `.bin` su SD
   - Update al boot
   - Richiede hardware extra

3. **Bluetooth OTA** - ❌ Sconsigliato
   - Troppo lento (<20 KB/s)
   - 813 KB firmware = ~40 secondi
   - Rischio disconnessione

---

## 📝 Note Finali

### Pro Migrazione Bluetooth:
✅ RAM liberata: ~70-100 KB
✅ Flash liberata: ~180 KB
✅ Boot più veloce
✅ Codice più semplice
✅ Meno dipendenze
✅ Minor consumo energetico

### Contro Migrazione Bluetooth:
❌ Niente OTA wireless
❌ Range ridotto
❌ Solo 1 client connesso
❌ Velocità inferiore

---

**Vuoi procedere con l'implementazione?**
Posso iniziare da quale fase preferisci!
