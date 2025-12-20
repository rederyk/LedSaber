# Implementazione LittleFS per Configurazione Utente Persistente

## Obiettivo
Implementare un sistema di persistenza delle configurazioni utente usando LittleFS con file JSON, salvando solo i valori modificati rispetto ai default.

## Contesto Attuale
- **Partizione disponibile**: 192KB SPIFFS a offset 0x3D0000 (definita in `partitions_ota.csv:6`)
- **Stato attuale**: Nessuna persistenza - tutti i valori si perdono al riavvio
- **Valori da persistere** (attualmente in `LedState` struct):
  - `r, g, b` (colore RGB)
  - `brightness` (luminosità 0-255)
  - `effect` (stringa: "solid", "rainbow", "breathe")
  - `speed` (velocità effetto 0-255)
  - `enabled` (LED strip on/off)
  - `statusLedEnabled` (LED integrato pin 4 on/off)
  - `autoIgnitionOnBoot` (accensione automatica lama al boot)
  - `autoIgnitionDelayMs` (delay in ms, default 5000)

## Valori Default (hardcoded in main.cpp)
```cpp
DEFAULT_BRIGHTNESS = 30
MAX_SAFE_BRIGHTNESS = 60  // limite di sicurezza per alimentatore 2A
NUM_LEDS = 144
```

## Requisiti dell'Implementazione

### 1. Struttura File System
- **Path file configurazione**: `/config.json`
- **Formato JSON**: Salvare SOLO valori diversi dai default
- **Esempio**:
```json
{
  "brightness": 50,
  "effect": "rainbow",
  "speed": 80,
  "statusLedEnabled": false
}
```
- Se un campo non è presente nel JSON → usa il valore di default

### 2. Classe ConfigManager da Creare

#### Header (`src/ConfigManager.h`):
```cpp
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "BLELedController.h"

class ConfigManager {
private:
    static constexpr const char* CONFIG_FILE = "/config.json";
    LedState* ledState;

    // Valori di default (stessi di main.cpp)
    struct DefaultConfig {
        uint8_t brightness = 30;
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        String effect = "solid";
        uint8_t speed = 50;
        bool enabled = true;
        bool statusLedEnabled = true;
    };
    DefaultConfig defaults;

    bool mountFilesystem();
    void createDefaultConfig();

public:
    explicit ConfigManager(LedState* state);

    bool begin();           // Inizializza LittleFS e carica config
    bool loadConfig();      // Carica config da JSON
    bool saveConfig();      // Salva SOLO valori diversi dai default
    void resetToDefaults(); // Ripristina default ed elimina config.json
    void printDebugInfo();  // Stampa stato filesystem e config
};

#endif
```

#### Implementazione (`src/ConfigManager.cpp`):

**Funzioni da implementare**:

1. **`begin()`**:
   - Monta LittleFS con `LittleFS.begin(true)` (true = formatta se necessario)
   - Gestisci errori di mount (flash corrotta, partizione mancante)
   - Verifica spazio disponibile: `LittleFS.totalBytes()`, `LittleFS.usedBytes()`
   - Chiama `loadConfig()`
   - Ritorna `true` se tutto OK, `false` in caso di errore
   - **Gestione errori**: Se mount fallisce, usa valori default in RAM e continua (non bloccare boot)

2. **`loadConfig()`**:
   - Verifica esistenza file con `LittleFS.exists(CONFIG_FILE)`
   - Se non esiste → usa defaults (è normale al primo avvio, non è un errore)
   - Apri file in lettura: `File file = LittleFS.open(CONFIG_FILE, "r")`
   - Gestisci errore apertura file
   - Deserializza JSON usando ArduinoJson (già presente nelle dipendenze)
   - Dimensione documento JSON: usa `JsonDocument doc` con capacità adeguata
   - Per ogni campo nel JSON:
     - Se presente → carica in `ledState`
     - Se assente → mantieni default
   - Chiudi file
   - Valida valori caricati (es: brightness <= 255)
   - Stampa log dei valori caricati
   - **Gestione errori JSON malformato**: Se parsing fallisce, elimina file corrotto e usa defaults

3. **`saveConfig()`**:
   - Crea JSON SOLO con valori diversi dai default
   - Esempio logica:
     ```cpp
     JsonDocument doc;
     if (ledState->brightness != defaults.brightness) {
         doc["brightness"] = ledState->brightness;
     }
     if (ledState->r != defaults.r || ledState->g != defaults.g || ledState->b != defaults.b) {
         doc["r"] = ledState->r;
         doc["g"] = ledState->g;
         doc["b"] = ledState->b;
     }
     // ... etc per tutti i campi
     ```
   - Se JSON vuoto (tutti valori = default) → elimina file config invece di salvarlo
   - Apri file in scrittura: `File file = LittleFS.open(CONFIG_FILE, "w")`
   - Gestisci errore apertura (filesystem pieno, flash read-only)
   - Serializza con `serializeJson(doc, file)`
   - Chiudi file e verifica successo scrittura
   - **Gestione errori**: Se scrittura fallisce, NON corrompere lo stato in RAM

4. **`resetToDefaults()`**:
   - Applica tutti i defaults a `ledState`
   - Elimina file config: `LittleFS.remove(CONFIG_FILE)`
   - Stampa conferma reset

5. **`printDebugInfo()`**:
   - Stampa spazio totale/usato filesystem
   - Elenca tutti i file: `File root = LittleFS.open("/"); while(file = root.openNextFile()) {...}`
   - Stampa contenuto di config.json (se esiste)

### 3. Integrazione in main.cpp

#### Setup:
```cpp
#include "ConfigManager.h"

ConfigManager configManager(&ledState);

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== LEDSABER (BLE GATT + OTA) ===");

    // 1. Inizializza periferiche (LED pin 4 + FastLED)
    initPeripherals();

    // 2. Carica configurazione da LittleFS PRIMA di inizializzare BLE
    if (!configManager.begin()) {
        Serial.println("[CONFIG] Warning: using default values");
    }
    configManager.printDebugInfo();

    // 3. Inizializza BLE (adesso ledState ha i valori caricati)
    BLEDevice::init("LedSaber-BLE");
    // ... resto del setup BLE
}
```

#### Trigger di salvataggio:
Modificare le callbacks BLE in `BLELedController.cpp` per salvare dopo ogni modifica:

**Opzione A - Salvataggio immediato** (più sicuro, più scritture):
```cpp
void ColorCallbacks::onWrite(BLECharacteristic *pChar) {
    // ... codice esistente che modifica ledState ...

    // Salva configurazione
    extern ConfigManager configManager;
    if (!configManager.saveConfig()) {
        Serial.println("[CONFIG] Failed to save color change");
    }
}
```

**Opzione B - Salvataggio ritardato** (meno scritture, più efficiente):
- Imposta un flag `configDirty = true` nelle callbacks
- Nel `loop()` principale, se `configDirty && (millis() - lastSave > 5000)`:
  - Salva config
  - Resetta flag

**Raccomandazione**: Usa **Opzione B** per ridurre usura flash

### 4. Modifiche a BLELedController

Aggiungi metodo per notificare che config è cambiata:
```cpp
class BLELedController {
public:
    // ... metodi esistenti ...
    void setConfigDirty(bool dirty); // Flag per salvataggio ritardato
    bool isConfigDirty();

private:
    bool configDirty = false;
};
```

### 5. Gestione Errori Critica

**Scenari da gestire**:
1. **Flash corrotta**: Mount fallisce → usa defaults, non bloccare boot
2. **JSON malformato**: Parsing fallisce → elimina file, usa defaults
3. **Filesystem pieno**: Scrittura fallisce → mantieni vecchio file, log errore
4. **Partizione mancante**: `LittleFS.begin()` fallisce → usa defaults
5. **Durante OTA**: NON salvare config durante `otaManager.isOTAInProgress()`

**Pattern di gestione errori**:
```cpp
if (!LittleFS.begin(true)) {
    Serial.println("[CRITICAL] LittleFS mount failed - using RAM defaults");
    return false; // Continua senza persistenza
}
```

### 6. Logging e Debug

Ogni operazione deve loggare:
```cpp
Serial.printf("[CONFIG] Loaded: brightness=%d, effect=%s, statusLed=%s\n", ...);
Serial.printf("[CONFIG] Filesystem: %lu/%lu bytes used\n", used, total);
Serial.printf("[CONFIG] Saved 3 modified values (brightness, effect, speed)\n");
```

### 7. Test da Implementare

1. **Primo avvio**: Verifica che crei filesystem e usi defaults
2. **Modifica via BLE**: Cambia brightness, verifica salvataggio JSON
3. **Riavvio**: Verifica che ricarichi il valore salvato
4. **Reset**: Testa `resetToDefaults()` elimina il file
5. **JSON corrotto**: Crea manualmente JSON invalido, verifica recovery
6. **Filesystem pieno**: Riempi partizione, verifica gestione errore

### 8. Comandi BLE Aggiuntivi (Opzionale)

Aggiungi characteristic per gestione config:
- **READ**: Ritorna JSON con valori salvati (non quelli in RAM)
- **WRITE**: `{"action": "reset"}` → chiama `resetToDefaults()`
- **WRITE**: `{"action": "save"}` → forza salvataggio immediato

### 9. File da Creare/Modificare

**Nuovi file**:
- `src/ConfigManager.h`
- `src/ConfigManager.cpp`

**File da modificare**:
- `src/main.cpp` (aggiungere inizializzazione ConfigManager)
- `src/BLELedController.h` (aggiungere flag configDirty)
- `src/BLELedController.cpp` (chiamare saveConfig() nelle callbacks)

### 10. Dipendenze

Già presenti in `platformio.ini`:
- ✅ ArduinoJson (per JSON parsing)

Da verificare:
- LittleFS è integrato in ESP32 Arduino Core (no dipendenze extra)

### 11. Note Importanti

1. **Usura Flash**:
   - ESP32 flash garantisce ~10.000-100.000 cicli scrittura
   - Salvataggio ritardato (ogni 5s max) protegge la flash
   - Se cambiano 10 parametri in 1s → 1 sola scrittura

2. **Sicurezza OTA**:
   - BLOCCA salvataggi durante `otaManager.isOTAInProgress()`
   - La scrittura flash durante OTA può corrompere entrambi

3. **Memory Usage**:
   - JsonDocument con capacità 512 bytes è sufficiente
   - Config file tipico: ~100-150 bytes

4. **Compatibilità Futura**:
   - Struttura JSON estendibile
   - Nuovi campi ignorati da vecchie versioni
   - Vecchi campi mantenuti per retrocompatibilità

## Esempio Output Atteso

```
=== LEDSABER (BLE GATT + OTA) ===
[CONFIG] Mounting LittleFS...
[CONFIG] Filesystem mounted: 192000/192000 bytes (0 used)
[CONFIG] Loading /config.json...
[CONFIG] File not found - first boot, using defaults
[CONFIG] Loaded: brightness=30, r=255, g=255, b=255, effect=solid
*** BLE LED Service avviato ***

... dopo modifica via BLE ...

[BLE] Brightness requested: 50 (enabled: 1)
[CONFIG] Config marked dirty, will save in 5s...
[CONFIG] Saving modified values...
[CONFIG] Saved 1 field: brightness=50
[CONFIG] Filesystem: 48/192000 bytes used

... dopo riavvio ...

[CONFIG] Loading /config.json...
[CONFIG] Found saved config
[CONFIG] Loaded: brightness=50 (custom), effect=solid (default)
```

## Priorità Implementazione

1. ✅ Creare ConfigManager base (begin, load, save)
2. ✅ Integrare in main.cpp con caricamento all'avvio
3. ✅ Aggiungere salvataggio ritardato nelle callbacks BLE
4. ✅ Gestione errori completa
5. ⚠️ Testing con JSON corrotto e scenari limite
6. 🔧 (Opzionale) Characteristic BLE per reset config

## Riferimenti Codice Esistente

- Struct LedState: `src/BLELedController.h`
- Callbacks BLE: `src/BLELedController.cpp:4-105`
- Default brightness: `src/main.cpp:13`
- Partizioni: `partitions_ota.csv:6`
- Check OTA in progress: `otaManager.isOTAInProgress()` in `main.cpp:207`
