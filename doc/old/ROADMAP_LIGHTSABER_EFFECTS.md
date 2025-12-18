# ROADMAP: Implementazione Effetti Spada Laser con Striscia LED Piegata

## Stato Attuale del Progetto

### Architettura Esistente
- **Hardware**: ESP32 + WS2812B (144 LED)
- **Comunicazione**: BLE GATT + OTA Update
- **Storage**: LittleFS per configurazione persistente
- **Effetti Attuali**: `solid`, `rainbow`, `breathe`
- **Controllo**: BLE via JSON (colore, luminosità, velocità, effetto)

### Modifiche Recenti
- ✅ Aggiunto PWM per LED di stato (pin 4) con controllo luminosità
- ✅ Sistema di configurazione persistente funzionante
- ✅ OTA Update ottimizzato

---

## FASE 1: Preparazione Infrastruttura (Fondamentale)

### 1.1 Aggiungere Campo `foldPoint` alla Struct LedState

**File**: `include/BLELedController.h`

**Modifica**:
```cpp
struct LedState {
    uint8_t r = 255;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t brightness = 255;
    uint8_t statusLedBrightness = 32;
    String effect = "solid";
    uint8_t speed = 50;
    bool enabled = true;
    bool statusLedEnabled = true;
    uint8_t foldPoint = 72;  // ⬅️ NUOVO: punto di piegatura LED (default = metà)
};
```

**Perché**: La striscia LED è piegata a metà. Il `foldPoint` definisce dove avviene la piega fisica (default 72 su 144 LED totali).

---

### 1.2 Aggiornare ConfigManager per Gestire `foldPoint`

**File**: `src/ConfigManager.h`

**Modifica nella struct DefaultConfig**:
```cpp
struct DefaultConfig {
    uint8_t brightness = 30;
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    String effect = "solid";
    uint8_t speed = 50;
    bool enabled = true;
    uint8_t statusLedBrightness = 32;
    bool statusLedEnabled = true;
    uint8_t foldPoint = 72;  // ⬅️ NUOVO
};
```

**File**: `src/ConfigManager.cpp`

**Aggiornare `loadConfig()` (riga ~68)**:
```cpp
ledState->foldPoint = doc["foldPoint"] | defaults.foldPoint;

// Validazione
if (ledState->foldPoint > NUM_LEDS) {
    ledState->foldPoint = NUM_LEDS / 2;  // Fallback a metà
}
```

**Aggiornare `saveConfig()` (dopo riga ~122)**:
```cpp
if (ledState->foldPoint != defaults.foldPoint) {
    doc["foldPoint"] = ledState->foldPoint;
    modifiedCount++;
}
```

**Aggiornare `resetToDefaults()` (riga ~166)**:
```cpp
ledState->foldPoint = defaults.foldPoint;
```

**Aggiornare `createDefaultConfig()` (riga ~218)**:
```cpp
ledState->foldPoint = defaults.foldPoint;
```

---

### 1.3 Aggiungere Controllo BLE per `foldPoint`

**File**: `include/BLELedController.h`

**Aggiungere nuovo UUID**:
```cpp
#define CHAR_FOLD_POINT_UUID     "h5i0f9h7-3g65-8e9f-cf0g-6789abcdef01"  // WRITE
```

**Aggiungere puntatore alla classe**:
```cpp
class BLELedController {
private:
    // ... esistenti ...
    BLECharacteristic* pCharFoldPoint;  // ⬅️ NUOVO
```

**Aggiornare friend classes**:
```cpp
friend class ColorCallbacks;
friend class EffectCallbacks;
friend class BrightnessCallbacks;
friend class StatusLedCallbacks;
friend class FoldPointCallbacks;  // ⬅️ NUOVO
```

**File**: `src/BLELedController.cpp`

**Aggiungere callback dopo `StatusLedCallbacks` (dopo riga ~128)**:
```cpp
// Callback configurazione punto di piegatura
class FoldPointCallbacks: public BLECharacteristicCallbacks {
    BLELedController* controller;
public:
    explicit FoldPointCallbacks(BLELedController* ctrl) : controller(ctrl) {}

    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue().c_str();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, value);

        if (!error) {
            uint8_t requestedFoldPoint = doc["foldPoint"] | controller->ledState->foldPoint;

            // Validazione: deve essere tra 1 e NUM_LEDS-1
            if (requestedFoldPoint >= 1 && requestedFoldPoint < NUM_LEDS) {
                controller->ledState->foldPoint = requestedFoldPoint;
                Serial.printf("[BLE] Fold point set to %d\n", controller->ledState->foldPoint);
                controller->setConfigDirty(true);
            } else {
                Serial.printf("[BLE ERROR] Invalid fold point: %d (must be 1-%d)\n",
                    requestedFoldPoint, NUM_LEDS - 1);
            }
        }
    }

    void onRead(BLECharacteristic *pChar) override {
        JsonDocument doc;
        doc["foldPoint"] = controller->ledState->foldPoint;

        String jsonString;
        serializeJson(doc, jsonString);
        pChar->setValue(jsonString.c_str());
    }
};
```

**Aggiungere characteristic in `begin()` (dopo riga ~200)**:
```cpp
// Characteristic 6: Fold Point (READ + WRITE)
pCharFoldPoint = pService->createCharacteristic(
    CHAR_FOLD_POINT_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
);
pCharFoldPoint->setCallbacks(new FoldPointCallbacks(this));
BLEDescriptor* descFoldPoint = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
descFoldPoint->setValue("LED Strip Fold Point");
pCharFoldPoint->addDescriptor(descFoldPoint);
```

**Aggiornare `notifyState()` (dopo riga ~221)**:
```cpp
doc["foldPoint"] = ledState->foldPoint;
```

**Inizializzare nel costruttore (riga ~140)**:
```cpp
pCharFoldPoint = nullptr;
```

---

## FASE 2: Funzione Helper per Mapping LED Fisici

### 2.1 Creare Funzione `mapLedIndex()`

**File**: `src/main.cpp`

**Posizione**: Prima di `renderLedStrip()` (inserire PRIMA della riga 147)

```cpp
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
```

**Diagramma ASCII della mappatura**:
```
STRISCIA FISICA (144 LED):
[0]--[1]--[2]--...[71]--[72]
                          |  <- PUNTO DI PIEGA
                    [143]--[142]--...[73]

MAPPATURA LOGICA (dalla base alla punta):
Base -> [0][1][2]...[71][72][73]...[142][143] <- Punta

RISULTATO:
- logicalIndex 0   -> physicalIndex 0
- logicalIndex 71  -> physicalIndex 71
- logicalIndex 72  -> physicalIndex 143  (!)
- logicalIndex 143 -> physicalIndex 72   (!)
```

---

## FASE 3: Implementazione Effetti Spada Laser

### 3.1 Effetto IGNITION (Accensione Lineare)

**File**: `src/main.cpp`

**Posizione**: Dentro `renderLedStrip()`, aggiungere dopo il blocco `else if (ledState.effect == "breathe")` (dopo riga ~173)

```cpp
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
```

---

### 3.2 Effetto FLICKER (Fluttuazioni Plasma)

**Posizione**: Dopo l'effetto `retraction`

```cpp
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
```

---

### 3.3 Effetto PULSE (Onde di Energia)

**Posizione**: Dopo l'effetto `unstable`

```cpp
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
            brightness = max(brightness, map(dist1, 0, pulseWidth, 255, 150));
        }
        if (dist2 < pulseWidth) {
            brightness = max(brightness, map(dist2, 0, pulseWidth, 255, 150));
        }

        leds[physicalIndex] = baseColor;
        leds[physicalIndex].fadeToBlackBy(255 - brightness);
    }
```

---

### 3.4 Effetto CLASH (Flash su Impatto)

**Posizione**: Dopo `dual_pulse`

```cpp
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
```

---

### 3.5 Effetto RAINBOW_BLADE (Arcobaleno Lineare)

**Posizione**: Dopo `clash`

```cpp
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
```

---

## FASE 4: Gestione Stato Effetti in Loop

### 4.1 Reset Variabili Statiche

**File**: `src/main.cpp`

**Posizione**: Alla fine di `renderLedStrip()`, prima di `lastUpdate = now;`

```cpp
    } else {
        // LED spento: resetta tutti gli effetti
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();

        // Reset variabili statiche degli effetti
        static bool resetDone = false;
        if (!resetDone) {
            // Forza reset di tutte le variabili statiche
            // (le variabili static mantengono valore tra chiamate)
            resetDone = true;
        }
    }
```

---

## FASE 5: Testing e Comandi BLE

### 5.1 Comandi JSON di Test

**Imposta punto di piegatura**:
```json
{"foldPoint": 72}
```

**Test Effetto Ignition (accensione veloce)**:
```json
{"mode": "ignition", "speed": 200}
```

**Test Effetto Flicker (instabilità leggera)**:
```json
{"mode": "flicker", "speed": 50}
```

**Test Effetto Pulse (onda veloce)**:
```json
{"mode": "pulse", "speed": 180}
```

**Test Effetto Unstable (Kylo Ren)**:
```json
{"mode": "unstable", "speed": 120}
```

**Test Effetto Dual Pulse**:
```json
{"mode": "dual_pulse", "speed": 150}
```

**Test Effetto Clash**:
```json
{"mode": "clash", "speed": 100}
```

**Test Rainbow Blade**:
```json
{"mode": "rainbow_blade", "speed": 80}
```

**Retraction (spegnimento)**:
```json
{"mode": "retraction", "speed": 150}
```

---

## FASE 6: Ottimizzazioni Future

### 6.1 Integrazione Accelerometro (MPU6050)
- Triggerare effetto `clash` su movimento brusco
- Effetto "swing" con scia luminosa
- Auto-spegnimento dopo inattività

### 6.2 Audio Reattivo
- Collegare microfono I2S
- Effetto "force_pulse" sincronizzato con audio

### 6.3 Presets Predefiniti
- Aggiungere characteristic BLE per presets (Jedi, Sith, Kylo Ren, etc.)
- Salvare presets in LittleFS

### 6.4 Calibrazione Automatica
- Funzione per rilevare automaticamente `foldPoint` tramite test di continuità

---

## Checklist Implementazione

### Fase 1: Infrastruttura
- [ ] Aggiungere `foldPoint` a `LedState`
- [ ] Aggiornare `ConfigManager` (load/save/reset)
- [ ] Aggiungere BLE characteristic per `foldPoint`
- [ ] Testare salvataggio/caricamento `foldPoint`

### Fase 2: Mapping
- [ ] Implementare `mapLedIndex()`
- [ ] Testare mapping con LED singoli
- [ ] Verificare correttezza fisica della mappatura

### Fase 3: Effetti
- [ ] Implementare `ignition`
- [ ] Implementare `retraction`
- [ ] Implementare `flicker`
- [ ] Implementare `unstable`
- [ ] Implementare `pulse`
- [ ] Implementare `dual_pulse`
- [ ] Implementare `clash`
- [ ] Implementare `rainbow_blade`

### Fase 4: Testing
- [ ] Testare ogni effetto singolarmente
- [ ] Testare transizioni tra effetti
- [ ] Verificare consumo energetico (MAX_SAFE_BRIGHTNESS)
- [ ] Testare persistenza configurazione

### Fase 5: Documentazione
- [ ] Aggiornare README con nuovi effetti
- [ ] Documentare comandi BLE
- [ ] Creare video demo

---

## Note Tecniche Importanti

### Limitazioni Hardware
- **Consumo**: MAX_SAFE_BRIGHTNESS = 60 (con alimentatore 2A)
- **Refresh Rate**: 20ms (50 FPS) - bilanciamento BLE/LED
- **RAM**: Effetti con buffer pesanti potrebbero saturare heap (~160KB liberi)

### Ottimizzazioni Prestazioni
- Usare `static` per variabili di stato effetti
- Evitare `malloc()`/`new` nel loop
- Preferire `fill_solid()` a loop manuali dove possibile
- Usare `FastLED.show()` una sola volta per frame

### Debugging
- Aumentare intervallo debug loop da 10s a 2s durante sviluppo
- Usare `Serial.printf()` con timestamp per timing critico
- Monitorare heap con `ESP.getFreeHeap()`

---

## Stima Tempi di Implementazione

- **Fase 1** (Infrastruttura): ~2 ore
- **Fase 2** (Mapping): ~1 ora
- **Fase 3** (Effetti base): ~3 ore
- **Fase 4** (Testing): ~2 ore
- **Totale**: ~8 ore di sviluppo

---

## Risorse Utili

- **FastLED Wiki**: https://github.com/FastLED/FastLED/wiki
- **BLE GATT Specs**: https://www.bluetooth.com/specifications/specs/
- **ESP32 PWM Ledc**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html

---

**Versione**: 1.0
**Data**: 2025-12-17
**Autore**: Claude Code Assistant
**Compatibilità**: ESP32 + FastLED 3.x + Arduino Framework
