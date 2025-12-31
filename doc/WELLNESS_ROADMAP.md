# 🌿 Wellness Circadian Lighting - Roadmap Completa

## Obiettivo
Trasformare ledSaber in una lampada wellness che segue ritmi circadiani naturali, con transizioni lente e rilassanti invece di precisione temporale.

---

## 📋 Temi da Implementare

### ✅ FASE 1: Foundation (COMPLETATO)
- [x] Sistema base ChronoHybrid con time sync
- [x] 6 temi ore esistenti (Classic, Neon, Plasma, Digital, Inferno, Storm)
- [x] 6 temi secondi esistenti (Classic, Time Spiral, Fire Clock, Lightning, Particle, Quantum)

---

### 🎯 FASE 2: Circadian Core (IN CORSO)

#### **Theme 1: CIRCADIAN RHYTHM** ⭐ PRIORITY
**Status**: 🟡 In sviluppo
**File**: `LedEffectEngine.cpp`

**Specifiche**:
- **Ore (Background)**: Transizione temperatura colore 2700K → 6500K → 2200K nel ciclo 24h
  - 00:00-05:00: Blu notte profondo (2700K warm dim, 10% brightness)
  - 05:00-07:00: Alba rosa/arancio crescente (3500K, 30-60%)
  - 07:00-12:00: Luce diurna bianco freddo (5500-6500K, 80-100%)
  - 12:00-17:00: Pomeriggio bianco neutro stabile (4500K, 90%)
  - 17:00-20:00: Tramonto ambra calda (3200K, 60-40%)
  - 20:00-24:00: Sera rosso/viola/blu notte (2500-2200K, 30-10%)

- **Minuti (Breathing)**: Pulsazione lenta 4-6 BPM (meditazione)
  - Sine wave beatsin8(5, 180, 255) - nessun cursore veloce
  - Ampiezza respirazione varia con ora: più profonda al tramonto/alba

- **Secondi**: ELIMINATI - solo saturazione ultra-lenta (cambio ogni 30-60s)

- **Motion Behavior**:
  - Motion DISABLED: Transizioni gradualissime (blend 2-5% per frame)
  - Motion ENABLED: Accelera leggermente il ciclo (1.5x max), no glitch violenti

**Implementazione**:
- Funzione: `renderChronoHours_CircadianRhythm(foldPoint, hours, minutes)`
- Funzione: `renderChronoMinutes_CircadianBreathing(foldPoint, minutes)`
- Helper: `getCircadianColorTemp(hourFloat)` → ritorna CRGB calibrato

**Testing**:
- [ ] Verifica smooth transitions tra fasce orarie
- [ ] Controllo temperature colore con colorimetro (se disponibile)
- [ ] Test breathing rate (deve essere rilassante, non distrattivo)
- [ ] Verifica consumo energetico (brightness auto-scaling)

---

### 🌲 FASE 3: Nature Themes

#### **Theme 2: FOREST CANOPY**
**Status**: 📝 Pianificato
**Priorità**: Alta
**ETA**: Post-Circadian

**Specifiche**:
- **Ore**: Verde filtrato che varia con luce diurna
  - 06:00-09:00: Verde lime + raggi dorati (sunrise through leaves)
  - 09:00-16:00: Verde smeraldo saturo (canopy full light)
  - 16:00-19:00: Verde muschio + ombre viola (dusk)
  - 19:00-06:00: Verde bioluminescente fioco (night forest)

- **Minuti**: Brezza - onde sinusoidali lente (periodo 15-20s)
  - Pattern: `sin8(pos * 8 + timePhase)` con timePhase molto lento
  - Direzione: base → punta (come vento che sale)

- **Secondi**: Fireflies/lucciole sparse
  - Random sparkles: 1-3 LED per volta, durata 200-500ms
  - Frequenza: 1 sparkle ogni 5-10 secondi
  - Colore: Giallo-verde bioluminescente (HUE 64-96)

**Implementazione**:
- Funzione: `renderChronoHours_ForestCanopy(foldPoint, hours, minutes)`
- Funzione: `renderChronoMinutes_Breeze(foldPoint, minutes)`
- Helper: `spawnFirefly()` → gestisce sparkle casuali

**Assets**:
- Palette verde custom (8 shades da lime a dark forest)
- Noise pattern per foglie mosse dal vento

---

#### **Theme 3: OCEAN DEPTH**
**Status**: 📝 Pianificato
**Priorità**: Media

**Specifiche**:
- **Ore**: Discesa attraverso zone marine (0-1000m metaforicamente)
  - 00:00-06:00: Abisso (nero-blu, bioluminescenza sporadica)
  - 06:00-10:00: Zona crepuscolare (blu profondo 200-1000m)
  - 10:00-16:00: Zona fotica (azzurro-turchese, superficie)
  - 16:00-20:00: Ritorno crepuscolo
  - 20:00-24:00: Discesa abisso

- **Minuti**: Onde marea (slow sine vertical sweep)
  - Periodo 45-60 secondi per onda completa
  - Brightness wave: simula luce che penetra superficie

- **Secondi**: Plankton bioluminescente drift
  - Particelle che si muovono lentamente (1-2 LED/s)
  - Colore: Cyan/blu elettrico (biolum)
  - Trail corto (2-3 LED fade)

**Implementazione**:
- Funzione: `renderChronoHours_OceanDepth(foldPoint, hours)`
- Funzione: `renderChronoMinutes_TideWave(foldPoint, minutes)`
- Helper: `renderPlanktonDrift()` con array particelle

---

### 🔥 FASE 4: Elemental Themes

#### **Theme 4: EMBER BED (Wellness Edition)**
**Status**: 📝 Pianificato
**Priorità**: Alta (refactor di Inferno esistente)

**Specifiche**:
- **Ore**: Intensità braci (da quasi spente a rosso vivo)
  - Ciclo 12h: Freddo (ash grey) → Caldo (orange-red) → Freddo
  - NO fiamme alte, solo letto di braci pulsanti

- **Minuti**: "Respiro del fuoco" organico
  - Beatsin8(3, ...) - molto lento
  - Heat map con inoise8 per pattern organico

- **Secondi**: Scintille rare ascendenti
  - 1 scintilla ogni 8-12 secondi
  - Movimento: base → punta, fade out
  - Colore: Arancio brillante (HUE 16-32)

**Implementazione**:
- Refactor: `renderChronoHours_Inferno()` → aggiungi slow mode
- Nuova: `renderChronoMinutes_EmberBreath(foldPoint, minutes)`
- Helper: `spawnEmberSpark()` → gestisce scintille

**Note**:
- Usare palette HeatColors_p di FastLED
- Temperatura colore max: 3000K (no bianco, solo warm)

---

#### **Theme 5: MOONLIGHT PHASES**
**Status**: 📝 Pianificato
**Priorità**: Media

**Specifiche**:
- **Ore**: Ciclo lunare (24h = ciclo completo 29.5 giorni metaforico)
  - Luna nuova (0%): Quasi nero, solo stelle
  - Crescente (25%): Grigio argento crescente da base
  - Piena (50%): Bianco argenteo brillante (tutto acceso)
  - Calante (75%): Grigio che diminuisce verso punta
  - Nuova (100%): Ritorno a nero

- **Minuti**: Soft glow pulsante (marea gravitazionale)
  - Beatsin8(2, 150, 220) - molto sottile
  - Solo su "lato illuminato" della luna

- **Secondi**: Starfield statico con twinkle rari
  - 5-8 "stelle" fisse (posizioni random ma costanti)
  - Twinkle: 1 stella brilla ogni 10-15s

**Implementazione**:
- Funzione: `renderChronoHours_MoonPhases(foldPoint, hourFloat)`
- Funzione: `renderChronoMinutes_LunarGlow(foldPoint, minutes, phase)`
- Helper: `calculateMoonPhase(hourFloat)` → 0.0-1.0

**Assets**:
- Palette silver/white (cool white 6500K)
- Starfield bitmap (8-10 pixel fissi)

---

#### **Theme 6: AURORA BOREALIS (Slow Mode)**
**Status**: 📝 Pianificato
**Priorità**: Bassa (più complesso)

**Specifiche**:
- **Ore**: Intensità aurora (notte polare)
  - 00:00-06:00: Aurora vivida (verde/viola/rosa)
  - 06:00-18:00: Aurora pallida/invisibile (giorno artico)
  - 18:00-24:00: Aurora crescente

- **Minuti**: Curtain waves verticali lente
  - Sine waves multiple (3-4 onde sovrapposte)
  - Periodo 20-40 secondi per onda
  - Colori: Verde (HUE 96) + Viola (HUE 192) + Rosa (HUE 224)

- **Secondi**: Shimmer sottile
  - Noise overlay con inoise8
  - Brightness modulation ±10%

**Implementazione**:
- Funzione: `renderChronoHours_AuroraBorealis(foldPoint, hours)`
- Funzione: `renderChronoMinutes_AuroraCurtains(foldPoint, minutes)`
- Helper: `blendAuroraColors(pos, timePhase)` → multi-hue blend

**Note**:
- Richiede palette custom con 3-4 colori primari
- Usare inoise16 per smoothness migliore

---

## 🔧 Modifiche Architettura

### Nuove Variabili in `LedState` (BLELedController.h)
```cpp
// Wellness mode configuration (IMPLEMENTATO)
uint8_t chronoHourTheme = 0;   // 0-6: supporta Circadian (6) ✅
bool chronoWellnessMode = false;  // true = wellness (slow transitions, breathing) ✅
uint8_t breathingRate = 5;        // BPM (2-8, default 5) ✅
uint16_t chronoCycleDuration = 1440; // Minuti per ciclo (default 1440 = 24h) ✅
```

### Enums in `LedEffectEngine.h`
```cpp
enum class ChronoHourTheme : uint8_t {
    CLASSIC = 0,
    NEON_CYBERPUNK = 1,
    PLASMA = 2,
    DIGITAL_GLITCH = 3,
    INFERNO = 4,
    STORM = 5,
    CIRCADIAN_RHYTHM = 6  // ✅ IMPLEMENTATO
};
```

### Funzioni Helper (IMPLEMENTATE ✅)
```cpp
// Color temperature helpers
CRGB getCircadianColorTemp(float hourFloat);  // 0.0-24.0 → CRGB ✅
CRGB kelvinToRGB(uint16_t kelvin);            // 2200-6500K → CRGB ✅

// Breathing patterns
void renderChronoMinutes_CircadianBreathing(uint16_t foldPoint, uint8_t bpm); ✅

// Particle systems (future)
void updateWellnessParticles();               // Fireflies, plankton, sparks
void spawnParticle(ParticleType type);
```

---

## 📱 Integrazione Dashboard & Mobile

### Dashboard Python (`saber_dashboard.py`)

#### Stato Attuale
- **ChronoThemesCard** implementata con:
  - 6 Hour Themes: Classic, Neon, Plasma, Digital, Inferno, Storm
  - 6 Second Themes: Classic, Spiral, Fire, Lightning, Particle, Quantum
  - Keybindings: F6 (cycle hours), F7 (cycle seconds)
  - Comando console: `chrono <hour> <second>`

#### Modifiche Necessarie per Wellness

**File**: `saber_dashboard.py`

**1. Aggiornare ChronoThemesCard (linea 694-746)**

```python
class ChronoThemesCard(Static):
    chrono_hour_theme: reactive[int] = reactive(0)
    chrono_second_theme: reactive[int] = reactive(0)
    wellness_mode: reactive[bool] = reactive(False)  # ⬅️ NUOVO
    breathing_rate: reactive[int] = reactive(5)      # ⬅️ NUOVO
    connected: reactive[bool] = reactive(False)

    # Aggiornare lista temi ore
    HOUR_THEMES = [
        "Classic", "Neon", "Plasma", "Digital",
        "Inferno", "Storm",
        "Circadian"  # ⬅️ NUOVO (index 6)
    ]

    # Icons update
    hour_icons = ["◎", "◉", "◈", "◆", "🔥", "⚡", "🌅"]  # ⬅️ Aggiunto sunrise
```

**2. Aggiungere Comando `wellness` (linea 1300+)**

```python
elif cmd == "wellness":
    # wellness <on|off> [bpm]
    if not args:
        self._log("Usage: wellness <on|off> [bpm 2-8]", "red")
        return

    enable = args[0].lower() == "on"
    bpm = int(args[1]) if len(args) > 1 else 5

    await self._send_wellness_config(enable, bpm)
```

**3. Nuova Funzione `_send_wellness_config()`**

```python
async def _send_wellness_config(self, enable: bool, bpm: int = 5):
    """Configura wellness mode"""
    try:
        # Aggiorna widget
        if self.chrono_themes_card:
            self.chrono_themes_card.wellness_mode = enable
            self.chrono_themes_card.breathing_rate = bpm

        # Comando JSON
        command = {
            "mode": "chrono_hybrid",
            "chronoHourTheme": 6,  # Force Circadian
            "chronoWellnessMode": enable,
            "breathingRate": bpm
        }

        await self.client.set_effect_raw(command)

        mode_str = "ON" if enable else "OFF"
        self._log(f"Wellness mode: {mode_str} (breathing {bpm} BPM)", "green")
    except Exception as e:
        self._log(f"Failed to set wellness mode: {e}", "red")
```

**4. Aggiungere Keybinding (linea 1089+)**

```python
BINDINGS = [
    # ... existing bindings ...
    Binding("f11", "wellness_toggle", "Wellness Mode"),  # ⬅️ NUOVO
]
```

```python
def action_wellness_toggle(self) -> None:
    """F11: Toggle wellness mode"""
    if not self.chrono_themes_card or not self.chrono_themes_card.connected:
        self._log("Connect device first", "yellow")
        return

    current = self.chrono_themes_card.wellness_mode
    new_state = not current
    bpm = self.chrono_themes_card.breathing_rate

    self.run_worker(self._send_wellness_config(new_state, bpm))
```

---

### App Mobile Flutter (`AppMobile/flutter_led_saber`)

#### Stato Attuale
- **ClockTab** implementato con:
  - Dropdown Hour Themes (6 temi)
  - Dropdown Second Themes (6 temi)
  - Sync Time button
  - Apply clock button

#### File da Modificare

**1. `lib/models/led_state.dart` (linee 15-32)**

```dart
class LedState {
  final int r, g, b, brightness;
  final String effect;
  final int speed;
  final bool enabled;
  final String bladeState;
  final bool statusLedEnabled;
  final int statusLedBrightness;
  final int chronoHourTheme;
  final int chronoSecondTheme;

  // ⬅️ AGGIUNGI:
  final bool chronoWellnessMode;
  final int breathingRate;

  LedState({
    // ... existing fields ...
    required this.chronoHourTheme,
    required this.chronoSecondTheme,
    // ⬅️ AGGIUNGI:
    required this.chronoWellnessMode,
    required this.breathingRate,
  });

  factory LedState.fromJson(Map<String, dynamic> json) {
    return LedState(
      // ... existing fields ...
      chronoHourTheme: json['chronoHourTheme'] ?? 0,
      chronoSecondTheme: json['chronoSecondTheme'] ?? 0,
      // ⬅️ AGGIUNGI:
      chronoWellnessMode: json['chronoWellnessMode'] ?? false,
      breathingRate: json['breathingRate'] ?? 5,
    );
  }

  LedState copyWith({
    // ... existing params ...
    int? chronoHourTheme,
    int? chronoSecondTheme,
    // ⬅️ AGGIUNGI:
    bool? chronoWellnessMode,
    int? breathingRate,
  }) {
    return LedState(
      // ... existing fields ...
      chronoHourTheme: chronoHourTheme ?? this.chronoHourTheme,
      chronoSecondTheme: chronoSecondTheme ?? this.chronoSecondTheme,
      // ⬅️ AGGIUNGI:
      chronoWellnessMode: chronoWellnessMode ?? this.chronoWellnessMode,
      breathingRate: breathingRate ?? this.breathingRate,
    );
  }
}
```

**2. `lib/screens/tabs/clock_tab.dart` (linee 20-36)**

```dart
class _ClockTabState extends State<ClockTab> {
  int _hourTheme = 0;
  int _secondTheme = 0;
  String _lastSyncLabel = 'Not synced';
  bool _isSyncing = false;
  // ⬅️ AGGIUNGI:
  bool _wellnessMode = false;
  double _breathingRate = 5.0;

  static const List<String> _defaultHourThemes = [
    'Classic', 'Neon', 'Plasma', 'Digital',
    'Inferno', 'Storm',
    'Circadian',  // ⬅️ NUOVO (index 6)
  ];

  // ... rest of code ...
}
```

**3. UI Widget - Aggiungere Switch & Slider (dopo linea 180)**

```dart
// Dentro build() Column, dopo DropdownButton second theme:

const SizedBox(height: 16),
const Divider(),
const SizedBox(height: 8),

// Wellness Mode Section
Text(
  'Wellness Mode',
  style: Theme.of(context).textTheme.titleSmall?.copyWith(
    fontWeight: FontWeight.bold,
  ),
),
const SizedBox(height: 8),

// Switch Wellness Mode
SwitchListTile(
  title: const Text('Enable Wellness Mode'),
  subtitle: const Text('Slow transitions, breathing effect'),
  value: _wellnessMode,
  onChanged: (value) {
    setState(() {
      _wellnessMode = value;
      if (value) {
        // Force Circadian theme quando wellness attivo
        _hourTheme = 6;
      }
    });
    _applyChronoThemes();
  },
),

// Breathing Rate Slider (solo se wellness attivo)
if (_wellnessMode) ...[
  const SizedBox(height: 8),
  Text(
    'Breathing Rate: ${_breathingRate.toInt()} BPM',
    style: Theme.of(context).textTheme.bodySmall,
  ),
  Slider(
    value: _breathingRate,
    min: 2,
    max: 8,
    divisions: 6,
    label: '${_breathingRate.toInt()} BPM',
    onChanged: (value) {
      setState(() {
        _breathingRate = value;
      });
    },
    onChangeEnd: (value) {
      _applyChronoThemes();
    },
  ),
  Text(
    _breathingRate <= 4
      ? 'Deep meditation'
      : (_breathingRate <= 6 ? 'Relaxation' : 'Normal'),
    style: Theme.of(context).textTheme.bodySmall?.copyWith(
      color: Colors.grey,
    ),
  ),
],
```

**4. Aggiornare `_applyChronoThemes()` (linea 83)**

```dart
void _applyChronoThemes() {
  final ledProvider = Provider.of<LedProvider>(context, listen: false);
  ledProvider.setEffect(
    'chrono_hybrid',
    chronoHourTheme: _hourTheme,
    chronoSecondTheme: _secondTheme,
    // ⬅️ AGGIUNGI:
    chronoWellnessMode: _wellnessMode,
    breathingRate: _breathingRate.toInt(),
  );
}
```

**5. `lib/providers/led_provider.dart` - Aggiornare `setEffect()`**

```dart
Future<void> setEffect(
  String effectId, {
  int? chronoHourTheme,
  int? chronoSecondTheme,
  // ⬅️ AGGIUNGI:
  bool? chronoWellnessMode,
  int? breathingRate,
}) async {
  final command = <String, dynamic>{
    'effect': effectId,
    if (chronoHourTheme != null) 'chronoHourTheme': chronoHourTheme,
    if (chronoSecondTheme != null) 'chronoSecondTheme': chronoSecondTheme,
    // ⬅️ AGGIUNGI:
    if (chronoWellnessMode != null) 'chronoWellnessMode': chronoWellnessMode,
    if (breathingRate != null) 'breathingRate': breathingRate,
  };

  await _ledService.setEffect(command);
  notifyListeners();
}
```

---

## 📊 Timeline Sviluppo

### Sprint 1 (Week 1)
- [x] Sistema ChronoHybrid base
- [ ] **Circadian Rhythm implementation** ⭐
- [ ] Testing & tuning color temps
- [ ] Documentation update

### Sprint 2 (Week 2)
- [ ] Forest Canopy implementation
- [ ] Ember Bed refactor
- [ ] Particle system base

### Sprint 3 (Week 3)
- [ ] Ocean Depth implementation
- [ ] Moonlight Phases implementation
- [ ] Advanced particle behaviors

### Sprint 4 (Week 4)
- [ ] Aurora Borealis implementation
- [ ] Performance optimization
- [ ] User testing & feedback
- [ ] Final polish

---

## 🎨 Design Principles

### Wellness-First Approach
1. **Nessun movimento rapido**: Max 1 cambio visibile ogni 5-10 secondi
2. **Blue light reduction**: Evitare blu puro dopo le 18:00
3. **Smooth transitions**: Blend rate 2-10% per frame (quasi impercettibili)
4. **Low cognitive load**: Pattern ripetitivi e prevedibili
5. **Energy efficiency**: Brightness auto-scaling (night = 10-30%, day = 80-100%)

### Technical Constraints
- **Frame rate**: 30-60 FPS rendering, ma solo 1-2 state changes/sec
- **Memory**: Max 512 bytes per particle systems
- **CPU**: <15% usage per rendering (lascia headroom per motion/camera)
- **Smooth blending**: Usare `blend()` o `nblend()` di FastLED, no hard cuts

---

## 🧪 Testing Checklist

### Per ogni tema:
- [ ] Transizioni smooth tra fasce orarie (no flickering)
- [ ] Breathing/pulsing non disturba (rate corretto)
- [ ] Colori scientificamente accurati (se applicabile)
- [ ] Motion disabled = zero accelerazioni/glitch
- [ ] Consumo energetico accettabile (mAh test 1h)
- [ ] Esperienza utente rilassante (user test 10+ min)

### Integration Tests:
- [ ] Combo wellness theme + motion processor (graceful degradation)
- [ ] Time sync loss recovery (fallback a pattern statico)
- [ ] BLE disconnect durante rendering (no crash)
- [ ] Power cycling mid-cycle (riprende da stato corretto)

---

## 📚 Riferimenti Scientifici

### Circadian Lighting
- **Blue light suppression**: <3000K dopo 20:00 (melatonin production)
- **Morning activation**: 5000-6500K alle 06:00-09:00
- **Midday stability**: 4000-4500K evita affaticamento

### Breathing Rates
- **Meditazione**: 4-6 BPM
- **Rilassamento**: 6-8 BPM
- **Neutro**: 8-10 BPM

### Color Psychology
- **Verde**: Calma, natura (Forest, Ocean surface)
- **Blu scuro**: Profondità, notte (Ocean depth, Night sky)
- **Rosso/Arancio caldo**: Calore, sicurezza (Ember, Sunset)
- **Viola/Rosa**: Mistero, sogno (Aurora, Moon)

---

## 🚀 Quick Start (Post-Implementation)

### Come attivare Circadian Mode:
```json
// Via BLE command
{
  "effect": "chrono_hybrid",
  "chronoWellnessTheme": 0,  // Circadian Rhythm
  "chronoSlowMode": true,
  "breathingRate": 5
}
```

### Dashboard Integration:
- [ ] Aggiungere UI toggle "Wellness Mode"
- [ ] Slider per breathing rate (2-8 BPM)
- [ ] Preview ciclo 24h (time-lapse UI)

---

## 📝 Checklist Integrazione Dashboard & Mobile

### Dashboard Python
- [ ] Aggiornare `ChronoThemesCard.HOUR_THEMES` con "Circadian" (index 6)
- [ ] Aggiungere `wellness_mode` e `breathing_rate` reactive variables
- [ ] Implementare comando `wellness <on|off> [bpm]`
- [ ] Implementare funzione `_send_wellness_config()`
- [ ] Aggiungere keybinding F11 per wellness toggle
- [ ] Aggiornare help text con nuovi comandi
- [ ] Test: `wellness on 5` → verifica breathing

### App Mobile Flutter
- [ ] Aggiornare `LedState` model (add `chronoWellnessMode`, `breathingRate`)
- [ ] Aggiornare `ClockTab` con wellness controls
- [ ] Aggiungere "Circadian" ai `_defaultHourThemes`
- [ ] Implementare UI: Switch wellness + Slider breathing rate
- [ ] Aggiornare `_applyChronoThemes()` per inviare nuovi parametri
- [ ] Aggiornare `LedProvider.setEffect()` signature
- [ ] Test: Toggle wellness → verifica smooth transitions su device

### Firmware (✅ COMPLETATO)
- [x] Variabili `chronoWellnessMode`, `breathingRate` in LedState
- [x] Enum `CIRCADIAN_RHYTHM = 6`
- [x] Funzione `renderChronoHours_CircadianRhythm()`
- [x] Funzione `renderChronoMinutes_CircadianBreathing()`
- [x] Helper `getCircadianColorTemp()`, `kelvinToRGB()`
- [x] Integrazione in `renderChronoHybrid()` main loop
- [x] Compilazione SUCCESS (72% flash, 14.6% RAM)

---

**Ultima Modifica**: 2025-12-31
**Autore**: Claude Sonnet 4.5
**Status**: 🟢 Fase 2 - Circadian Rhythm COMPLETATO (firmware)
**Next**: Integrazione Dashboard & Mobile App
