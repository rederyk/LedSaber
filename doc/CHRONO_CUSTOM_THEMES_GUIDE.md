# 🎨 Guida: Creare Temi Chrono Personalizzati

Questa guida ti mostra come creare nuovi temi per l'effetto Chrono, aggiungere palette colori personalizzate e estendere il sistema modulare.

---

## 📚 Indice

1. [Architettura del Sistema](#architettura-del-sistema)
2. [Creare un Nuovo Tema Hours](#creare-un-nuovo-tema-hours)
3. [Creare un Nuovo Tema Seconds](#creare-un-nuovo-tema-seconds)
4. [Aggiungere Palette Colori](#aggiungere-palette-colori)
5. [Integrare nella Dashboard](#integrare-nella-dashboard)
6. [Esempi Completi](#esempi-completi)
7. [Best Practices](#best-practices)

---

## 🏗️ Architettura del Sistema

Il sistema Chrono è **modulare a 2 layer**:

```
┌─────────────────────────────────────┐
│     CHRONO HYBRID EFFECT            │
├─────────────────────────────────────┤
│  Layer 1: HOUR MARKERS              │
│  - Sfondo/griglia delle ore         │
│  - 4 temi disponibili (0-3)         │
├─────────────────────────────────────┤
│  Layer 2: SECOND/MINUTE CURSORS     │
│  - Cursori animati tempo corrente   │
│  - 6 temi disponibili (0-5)         │
└─────────────────────────────────────┘
```

### Parametri Condivisi

Ogni funzione di rendering riceve:
- `foldPoint`: Numero LED logici (metà strip fisica)
- `baseColor`: Colore RGB primario (`state.r, state.g, state.b`)
- `hours`, `minutes`, `seconds`: Tempo corrente
- `visualOffset`: Offset motion (distorsione temporale)

---

## 🌅 Creare un Nuovo Tema Hours

### Step 1: Aggiungere l'Enum

**File:** `src/LedEffectEngine.h`

```cpp
enum class ChronoHourTheme : uint8_t {
    CLASSIC = 0,
    NEON_CYBERPUNK = 1,
    PLASMA = 2,
    DIGITAL_GLITCH = 3,
    GALAXY = 4          // ← Nuovo tema!
};
```

### Step 2: Dichiarare la Funzione

**File:** `src/LedEffectEngine.h`

```cpp
// Hour marker themes
void renderChronoHours_Classic(uint16_t foldPoint, CRGB baseColor);
void renderChronoHours_Neon(uint16_t foldPoint, CRGB baseColor, uint8_t hours);
void renderChronoHours_Plasma(uint16_t foldPoint, uint8_t hours, uint8_t minutes);
void renderChronoHours_Digital(uint16_t foldPoint, CRGB baseColor, uint8_t seconds);
void renderChronoHours_Galaxy(uint16_t foldPoint, CRGB baseColor, uint8_t hours);  // ← Nuovo!
```

### Step 3: Implementare la Funzione

**File:** `src/LedEffectEngine.cpp` (aggiungi alla fine, dopo gli altri temi)

```cpp
void LedEffectEngine::renderChronoHours_Galaxy(uint16_t foldPoint, CRGB baseColor, uint8_t hours) {
    // Tema Galaxy: stelle pulsanti che ruotano lentamente

    unsigned long now = millis();
    uint8_t rotationPhase = (now / 100) % 256;  // Rotazione lenta

    for (uint16_t i = 0; i < foldPoint; i++) {
        // Crea pattern stellare
        uint8_t starSeed = (i * 7 + rotationPhase) % 256;
        uint8_t brightness = 0;

        // Solo alcuni pixel diventano "stelle"
        if (starSeed < 20) {  // 8% dei pixel
            // Stella brillante
            brightness = sin8(now / 10 + i * 13) / 2 + 128;
        } else if (starSeed < 40) {  // Altri 8% stelle dim
            brightness = sin8(now / 15 + i * 7) / 4 + 30;
        }

        if (brightness > 0) {
            // Colore stellare: blu/viola profondo
            CRGB starColor = CHSV(160 + sin8(i * 5) / 8, 200, brightness);
            setLedPair(i, foldPoint, starColor);
        }
    }

    // Marker delle ore: pulsar luminose
    for (uint8_t h = 0; h < 12; h++) {
        uint16_t pos = map(h, 0, 12, 0, foldPoint);

        // Pulsar che pulsa più veloce per l'ora corrente
        uint8_t pulseSpeed = (h == hours) ? 50 : 200;
        uint8_t pulseBright = sin8((now / pulseSpeed) + h * 20);

        CRGB pulsarColor = baseColor;
        pulsarColor.nscale8(pulseBright / 2 + 128);

        // Glow esteso
        for (int8_t j = -1; j <= 1; j++) {
            int16_t glowPos = pos + j;
            if (glowPos >= 0 && glowPos < foldPoint) {
                CRGB glow = pulsarColor;
                glow.nscale8(abs(j) == 1 ? 180 : 255);
                setLedPair(glowPos, foldPoint, glow);
            }
        }
    }
}
```

### Step 4: Integrare nello Switch

**File:** `src/LedEffectEngine.cpp` (funzione `renderChronoHybrid`)

```cpp
// A. MARKER ORE (tema selezionabile)
switch (state.chronoHourTheme) {
    case 0:  // Classic
        renderChronoHours_Classic(state.foldPoint, baseColor);
        break;
    case 1:  // Neon Cyberpunk
        renderChronoHours_Neon(state.foldPoint, baseColor, hours);
        break;
    case 2:  // Plasma
        renderChronoHours_Plasma(state.foldPoint, hours, minutes);
        break;
    case 3:  // Digital Glitch
        renderChronoHours_Digital(state.foldPoint, baseColor, seconds);
        break;
    case 4:  // Galaxy ← Aggiungi qui!
        renderChronoHours_Galaxy(state.foldPoint, baseColor, hours);
        break;
    default:
        renderChronoHours_Classic(state.foldPoint, baseColor);
        break;
}
```

### Step 5: Aggiornare la Dashboard

**File:** `saber_dashboard.py`

```python
class ChronoThemesCard(Static):
    HOUR_THEMES = ["Classic", "Neon", "Plasma", "Digital", "Galaxy"]  # ← Aggiungi nome
    SECOND_THEMES = ["Classic", "Spiral", "Fire", "Lightning", "Particle", "Quantum"]

    def render(self):
        # ... existing code ...
        hour_icons = ["◎", "◉", "◈", "◆", "✨"]  # ← Aggiungi icona
```

---

## ⏱️ Creare un Nuovo Tema Seconds

### Step 1: Aggiungere l'Enum

**File:** `src/LedEffectEngine.h`

```cpp
enum class ChronoSecondTheme : uint8_t {
    CLASSIC = 0,
    TIME_SPIRAL = 1,
    FIRE_CLOCK = 2,
    LIGHTNING = 3,
    PARTICLE_FLOW = 4,
    QUANTUM_WAVE = 5,
    METEOR_SHOWER = 6   // ← Nuovo tema!
};
```

### Step 2: Dichiarare la Funzione

```cpp
void renderChronoSeconds_MeteorShower(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor);
```

### Step 3: Implementare la Funzione

**File:** `src/LedEffectEngine.cpp`

```cpp
void LedEffectEngine::renderChronoSeconds_MeteorShower(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    // Tema Meteor Shower: meteore che cadono dalla punta alla base

    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    unsigned long now = millis();

    // Numero di meteore basato sui secondi (aumenta col tempo)
    uint8_t numMeteors = 2 + (visualSeconds / 15);  // 2-5 meteore

    for (uint8_t m = 0; m < numMeteors; m++) {
        // Offset unico per ogni meteora
        uint16_t meteorPhase = (now / (50 + m * 10) + m * 100) % 256;

        // Posizione meteora (cade dall'alto verso il basso)
        uint16_t meteorPos = map(meteorPhase, 0, 255, foldPoint - 1, 0);

        // Colore meteora (hue diverso per ogni meteora)
        uint8_t meteorHue = (m * 60 + visualSeconds * 2) % 256;

        // Disegna trail della meteora (5 LED)
        for (int8_t trail = 0; trail < 5; trail++) {
            int16_t pos = meteorPos + trail;
            if (pos >= 0 && pos < foldPoint) {
                uint8_t trailBrightness = 255 - (trail * 50);
                CRGB meteorColor = CHSV(meteorHue, 255, trailBrightness);

                // Blend additivo per sovrapposizione
                uint16_t physicalIdx = pos < foldPoint ? pos : (2 * foldPoint - pos - 1);
                if (physicalIdx < _numLeds) {
                    _leds[physicalIdx] += meteorColor;
                }
            }
        }
    }

    // Cursore minuti: punto fisso brillante
    uint16_t minutePos = map(minutes, 0, 60, 0, foldPoint);
    CRGB minuteColor = baseColor;
    minuteColor.nscale8(200);

    for (int8_t i = -1; i <= 1; i++) {
        int16_t pos = minutePos + i;
        if (pos >= 0 && pos < foldPoint) {
            setLedPair(pos, foldPoint, minuteColor);
        }
    }
}
```

### Step 4: Integrare nello Switch

```cpp
case 6:  // Meteor Shower
    renderChronoSeconds_MeteorShower(state.foldPoint, minutes, seconds, _visualOffset, baseColor);
    break;
```

### Step 5: Aggiornare Dashboard

```python
SECOND_THEMES = ["Classic", "Spiral", "Fire", "Lightning", "Particle", "Quantum", "Meteor"]
second_icons = ["○", "◐", "🔥", "⚡", "●", "◯", "☄"]
```

---

## 🎨 Aggiungere Palette Colori

### Problema: Temi Ignora Colore Base

Alcuni temi (es. Plasma, Digital) usano palette HSV fisse e ignorano `state.r, state.g, state.b`.

### Soluzione 1: Palette Parametrica

Modifica il tema per derivare i colori dal `baseColor`:

```cpp
void LedEffectEngine::renderChronoHours_Plasma(uint16_t foldPoint, uint8_t hours, uint8_t minutes) {
    // BEFORE: colori fissi
    // CRGB plasmaColor = CHSV(hue, 255, 60);

    // AFTER: deriva hue da baseColor
    CRGB baseColor = CRGB(state.r, state.g, state.b);
    CHSV baseHSV = rgb2hsv_approximate(baseColor);  // FastLED helper

    uint8_t baseHue = baseHSV.hue;  // Hue del colore impostato
    uint8_t timeHue = (hours * 21 + minutes / 3) % 256;

    for (uint16_t i = 0; i < foldPoint; i++) {
        uint8_t wave = sin8(i * 8 + timeHue);
        uint8_t hue = baseHue + wave / 4;  // ← Varia attorno al colore base
        CRGB plasmaColor = CHSV(hue, 255, 60);

        // ... rest of code ...
    }
}
```

### Soluzione 2: Palette Custom con Blend

Crea palette che sfumano tra colori correlati:

```cpp
void LedEffectEngine::renderChronoHours_Sunset(uint16_t foldPoint, CRGB baseColor, uint8_t hours) {
    // Palette tramonto: sfuma tra 3 colori
    CRGB palette[3];

    if (baseColor.r > baseColor.b) {
        // Tonalità calde (rosso/arancione)
        palette[0] = CRGB(255, 50, 0);    // Arancione profondo
        palette[1] = CRGB(255, 150, 30);  // Arancione chiaro
        palette[2] = CRGB(255, 200, 100); // Giallo rosa
    } else {
        // Tonalità fredde (blu/viola)
        palette[0] = CRGB(20, 0, 80);     // Viola profondo
        palette[1] = CRGB(80, 40, 150);   // Viola medio
        palette[2] = CRGB(150, 100, 200); // Lavanda
    }

    unsigned long now = millis();

    for (uint16_t i = 0; i < foldPoint; i++) {
        // Sfuma tra i 3 colori della palette
        uint8_t blendIndex = (i * 255 / foldPoint + (now / 50)) % 256;

        CRGB color;
        if (blendIndex < 128) {
            color = blend(palette[0], palette[1], blendIndex * 2);
        } else {
            color = blend(palette[1], palette[2], (blendIndex - 128) * 2);
        }

        color.nscale8(60);  // Dim for background
        setLedPair(i, foldPoint, color);
    }

    // Marker ore...
}
```

### Soluzione 3: Palette Dinamica da State

Passa una palette completa tramite state:

**File:** `include/BLELedController.h`

```cpp
struct LedState {
    // ... existing fields ...

    // Chrono palette (opzionale)
    CRGB chronoPalette[4] = {
        CRGB(255, 0, 0),    // Colore 1
        CRGB(0, 255, 0),    // Colore 2
        CRGB(0, 0, 255),    // Colore 3
        CRGB(255, 255, 0)   // Colore 4
    };
    bool useChronoPalette = false;
};
```

**Rendering:**

```cpp
void LedEffectEngine::renderChronoHours_PaletteMode(uint16_t foldPoint, const LedState& state, uint8_t hours) {
    if (!state.useChronoPalette) {
        // Fallback al colore base
        renderChronoHours_Classic(foldPoint, CRGB(state.r, state.g, state.b));
        return;
    }

    // Usa palette custom
    for (uint8_t h = 0; h < 12; h++) {
        uint16_t pos = map(h, 0, 12, 0, foldPoint);

        // Cicla tra i 4 colori della palette
        CRGB markerColor = state.chronoPalette[h % 4];
        markerColor.nscale8(h == hours ? 200 : 60);

        setLedPair(pos, foldPoint, markerColor);
    }
}
```

---

## 🖥️ Integrare nella Dashboard

### Aggiungere Selettore Palette

**File:** `saber_dashboard.py`

```python
class ChronoPaletteCard(Static):
    """Card per selezionare palette preset"""

    palette_index: reactive[int] = reactive(0)
    connected: reactive[bool] = reactive(False)

    PALETTES = [
        "Default",
        "Fire",
        "Ocean",
        "Forest",
        "Sunset",
        "Cyberpunk"
    ]

    def render(self):
        if not self.connected:
            content = Text("Connect\nfirst", style="dim")
        else:
            palette_name = self.PALETTES[self.palette_index % len(self.PALETTES)]

            table = Table.grid(padding=(0, 1))
            table.add_column(justify="center")

            palette_text = Text()
            palette_text.append("🎨 ", style="yellow")
            palette_text.append(palette_name, style="yellow bold")

            table.add_row(palette_text)
            table.add_row(Text("F8 to cycle", style="dim"))

            content = table

        return Panel(
            Align.center(content, vertical="middle"),
            title="[bold yellow]PALETTE[/]",
            border_style="yellow",
            box=box.ROUNDED,
            height=5
        )
```

### Comandi BLE per Palette

```python
elif cmd == "palette":
    if not args:
        self._log("Palettes: 0=Default, 1=Fire, 2=Ocean, 3=Forest, 4=Sunset, 5=Cyberpunk", "cyan")
        return

    palette_idx = int(args[0])
    await self._send_palette(palette_idx)

async def _send_palette(self, palette_idx: int):
    # Definisci palette RGB
    palettes = {
        0: [(255, 255, 255)] * 4,  # Default (bianco)
        1: [(255, 0, 0), (255, 100, 0), (255, 200, 0), (255, 255, 0)],  # Fire
        2: [(0, 50, 255), (0, 150, 255), (50, 200, 255), (150, 255, 255)],  # Ocean
        # ... altri preset
    }

    colors = palettes.get(palette_idx, palettes[0])

    # Invia via BLE (richiede estensione protocollo)
    command = {
        "cmd": "set_palette",
        "colors": colors
    }

    await self.client.send_custom_command(command)
```

---

## 📖 Esempi Completi

### Esempio 1: Tema "Aurora Boreale"

Onde di colore che fluiscono lentamente, cambiano intensità con i secondi:

```cpp
void LedEffectEngine::renderChronoSeconds_Aurora(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    unsigned long now = millis();

    for (uint16_t i = 0; i < foldPoint; i++) {
        // Tre onde sovrapposte con frequenze diverse
        uint8_t wave1 = sin8(i * 6 + (now / 30));
        uint8_t wave2 = sin8(i * 4 - (now / 50));
        uint8_t wave3 = sin8(i * 8 + (now / 70));

        // Combina le onde
        uint8_t combinedWave = (wave1 / 3) + (wave2 / 3) + (wave3 / 3);

        // Colori aurora: verde/blu/viola
        uint8_t hue = 80 + combinedWave / 4;  // 80-143 (verde -> cyan -> blu)
        uint8_t saturation = 200 + (wave1 / 4);
        uint8_t brightness = 40 + (combinedWave / 4) + (visualSeconds * 2);

        CRGB auroraColor = CHSV(hue, saturation, brightness);
        setLedPair(i, foldPoint, auroraColor);
    }

    // Cursore minuti: stella polare fissa
    uint16_t minutePos = map(minutes, 0, 60, 0, foldPoint);
    CRGB starColor = CRGB(255, 255, 200);  // Bianco caldo
    starColor.nscale8(sin8(now / 20) / 2 + 128);  // Pulse lento

    for (int8_t i = -2; i <= 2; i++) {
        int16_t pos = minutePos + i;
        if (pos >= 0 && pos < foldPoint) {
            CRGB fade = starColor;
            fade.nscale8(255 - abs(i) * 60);
            setLedPair(pos, foldPoint, fade);
        }
    }
}
```

### Esempio 2: Tema "Matrix Code Rain"

Codice che scorre, sincronizzato con i secondi:

```cpp
void LedEffectEngine::renderChronoSeconds_MatrixRain(uint16_t foldPoint, uint8_t minutes, uint8_t seconds, float visualOffset, CRGB baseColor) {
    int visualSeconds = seconds + (int)visualOffset;
    while (visualSeconds < 0) visualSeconds += 60;
    while (visualSeconds >= 60) visualSeconds -= 60;

    unsigned long now = millis();

    // Numero di "colonne" di codice
    uint8_t numColumns = 8;

    for (uint8_t col = 0; col < numColumns; col++) {
        // Velocità variabile per ogni colonna
        uint16_t speed = 30 + (col * 10);
        uint16_t columnPhase = (now / speed + col * 50) % (foldPoint + 20);

        // Disegna trail di codice
        for (uint8_t trail = 0; trail < 12; trail++) {
            int16_t pos = columnPhase - trail;

            if (pos >= 0 && pos < foldPoint) {
                // Verde matrice, più brillante in testa
                uint8_t brightness = trail == 0 ? 255 : (255 - trail * 20);
                CRGB matrixColor = CHSV(96, 255, brightness);  // Verde

                // Glitch occasionale (sincronizzato con secondi)
                if ((visualSeconds + col) % 7 == 0 && trail < 3) {
                    matrixColor = CRGB(255, 255, 255);  // Bianco glitch
                }

                setLedPair(pos, foldPoint, matrixColor);
            }
        }
    }

    // Cursore minuti: linea rossa orizzontale
    uint16_t minutePos = map(minutes, 0, 60, 0, foldPoint);
    CRGB redLine = CRGB(255, 0, 0);

    for (int8_t i = -3; i <= 3; i++) {
        int16_t pos = minutePos + i;
        if (pos >= 0 && pos < foldPoint) {
            setLedPair(pos, foldPoint, redLine);
        }
    }
}
```

---

## ✅ Best Practices

### Performance

1. **Evita operazioni pesanti nel loop:**
   ```cpp
   // ❌ BAD: Calcolo pesante ripetuto
   for (uint16_t i = 0; i < foldPoint; i++) {
       float result = sin(i * 0.1) * cos(i * 0.2);  // Lento!
   }

   // ✅ GOOD: Usa lookup table FastLED
   for (uint16_t i = 0; i < foldPoint; i++) {
       uint8_t result = sin8(i * 25);  // Fast!
   }
   ```

2. **Limita chiamate setLedPair:**
   - Ogni chiamata scrive 2 LED (strip piegata)
   - Evita loop annidati inutili

3. **Usa millis() parsimony:**
   ```cpp
   // ✅ GOOD: Una sola chiamata
   unsigned long now = millis();
   uint8_t phase1 = (now / 50) % 256;
   uint8_t phase2 = (now / 100) % 256;
   ```

### Colori

1. **Usa CHSV per transizioni smooth:**
   ```cpp
   CRGB color = CHSV(hue, saturation, value);
   ```

2. **Scale brightness correttamente:**
   ```cpp
   color.nscale8(128);  // ✅ Dimezza brightness preservando colore
   color /= 2;          // ❌ Altera i rapporti RGB
   ```

3. **Blend additivo per sovrapposizioni:**
   ```cpp
   _leds[i] += newColor;  // Somma invece di sovrascrivere
   ```

### Motion Integration

1. **Sempre gestisci visualOffset:**
   ```cpp
   int visualSeconds = seconds + (int)visualOffset;
   while (visualSeconds < 0) visualSeconds += 60;
   while (visualSeconds >= 60) visualSeconds -= 60;
   ```

2. **Effetti speciali quando motion attivo:**
   ```cpp
   if (abs(visualOffset) > 2.0f) {
       // Aggiungi glitch, cambio colore, etc.
       color = CRGB::Cyan;  // Esempio
   }
   ```

### Memory

1. **Variabili statiche per state persistente:**
   ```cpp
   static uint8_t lastHour = 0;
   if (hours != lastHour) {
       // Evento cambio ora
       lastHour = hours;
   }
   ```

2. **Evita array dinamici:**
   ```cpp
   // ❌ BAD
   CRGB* tempArray = new CRGB[foldPoint];

   // ✅ GOOD: Usa buffer esistente _leds[]
   ```

---

## 🚀 Workflow Sviluppo

1. **Prototipa in Python** (optional)
   - Test logica con matplotlib/pygame
   - Più facile debug animazioni

2. **Implementa in C++**
   - Copia struttura da tema esistente
   - Compila: `platformio run`

3. **Test su Hardware**
   - Upload firmware
   - Usa dashboard per cycling rapido

4. **Tweaking**
   - Regola velocità, colori, intensità
   - Test con motion attivo/inattivo

5. **Documenta**
   - Aggiungi commenti nel codice
   - Aggiorna [CHRONO_THEMES.md](CHRONO_THEMES.md)

---

## 📚 Risorse

### FastLED Helpers

- `sin8(x)`: Sine wave 0-255 → 0-255
- `cos8(x)`: Cosine wave
- `beat8(bpm)`: Sawtooth per BPM
- `beatsin8(bpm, min, max)`: Sine oscillator
- `qadd8(a, b)`: Saturating add (no overflow)
- `blend(color1, color2, amount)`: Color interpolation

### Color Utils

```cpp
rgb2hsv_approximate(CRGB rgb) → CHSV
hsv2rgb_rainbow(CHSV hsv) → CRGB
nblend(existing, new, amount) → Mix in-place
```

### Documentazione

- [FastLED Wiki](https://github.com/FastLED/FastLED/wiki)
- [Color Math](https://github.com/FastLED/FastLED/wiki/Pixel-reference#color-math)
- [HSV Color](https://github.com/FastLED/FastLED/wiki/Pixel-reference#hsv-colors)

---

## 💡 Tips Creativi

1. **Combina più pattern:**
   - Base wave + pulse + particle = effetto complesso

2. **Usa noise functions:**
   - `inoise8(x, y, time)` per texture organiche

3. **Sincronizza con beat:**
   - `beat8(120)` per 120 BPM rhythm

4. **Layer multipli:**
   - Disegna background → overlay → highlights

5. **Fisica simulata:**
   - Gravità, rimbalzi, attrito per particelle

---

## 🎓 Esercizi

### Esercizio 1: "Breathing Stars"
Crea tema hours con stelle che pulsano a velocità diverse.

### Esercizio 2: "Waterfall"
Crea tema seconds con acqua che scorre, velocità = minuti.

### Esercizio 3: "Palette Switcher"
Implementa comando BLE per cambiare palette runtime.

### Esercizio 4: "Sound Reactive"
Integra microfono per modulare brightness con musica.

---

**Buon coding! 🚀**

Per domande: apri issue su GitHub o consulta [CHRONO_THEMES.md](CHRONO_THEMES.md) per esempi built-in.
