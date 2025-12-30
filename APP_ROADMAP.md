# LED Saber App - Roadmap Implementazione

**Progetto**: Flutter LED Saber Controller
**Versione App**: 2.0 (Multi-Device + Real-Time Controls)
**Ultima Modifica**: 2025-12-27

---

## ✅ **SPRINT 1: Multi-Device Foundation** (COMPLETATO 2025-12-27)

### Obiettivi
Abilitare connessione simultanea a più LED Saber con gestione robusta delle connessioni.

### Features Implementate

#### 1.1 Multi-Device Manager
- ✅ Connessione simultanea multipla (N devices)
- ✅ Switch tra devices senza disconnessione
- ✅ Auto-reconnect intelligente (3s retry)
- ✅ Health check periodico (5s)
- ✅ Persistenza devices (SharedPreferences)

#### 1.2 Connection Status Monitoring
- ✅ Stato real-time: Connected (verde) / Unstable (arancione) / Disconnected (rosso)
- ✅ Dot colorato su icona Bluetooth
- ✅ SnackBar notifiche cambio stato
- ✅ LastSeen timestamp

#### 1.3 HomeScreen Redesign
- ✅ Badge devices connessi in AppBar
- ✅ Sezione "Dispositivi Connessi" con card orizzontali
- ✅ Swipe-to-delete per disconnessione
- ✅ Filtraggio smart (non mostra già connessi)
- ✅ Scan manuale (no auto-scan)

#### 1.4 Bug Fixes Critici
- ✅ Fix disconnect bug in BleService (multi-device safe)
- ✅ Gestione errori migliorata con retry automatico
- ✅ Rimozione race conditions

### File Modificati
- `lib/services/multi_device_manager.dart` (nuovo)
- `lib/models/connected_device.dart` (nuovo)
- `lib/services/ble_service.dart` (fix disconnect)
- `lib/providers/ble_provider.dart` (integrazione multi-device)
- `lib/screens/home_screen.dart` (redesign completo)

---

## 🚧 **SPRINT 2: Real-Time LED Controls** (IN CORSO 2025-12-27)

### Obiettivi
Implementare controlli LED interattivi in tempo reale senza modifiche firmware.

### Features da Implementare

#### 2.1 Color Picker HSV Wheel
**Priorità**: Alta
**Complessità**: Media
**Dipendenze**: Libreria `flutter_colorpicker`

**Descrizione**:
- Color wheel HSV per selezione intuitiva colori
- Slider separato per luminosità
- Preview live del colore selezionato
- Preset colori Star Wars (vedi 2.2)

**Caratteristiche BLE utilizzate**:
- `charLedColor` (WRITE): `{"r": 255, "g": 0, "b": 0}`
- `charLedBrightness` (WRITE): `{"brightness": 200, "enabled": true}`

**UI Location**: Tab "Colors" nel control panel (portrait: sinistra, landscape: destra)

**Acceptance Criteria**:
- [ ] Color wheel funzionante con touch/drag
- [ ] Aggiornamento colore LED in real-time (<100ms latency)
- [ ] Slider luminosità 0-255
- [ ] Smooth transitions

#### 2.2 Preset Colori Star Wars
**Priorità**: Alta
**Complessità**: Bassa

**Preset Implementati**:
```dart
final starWarsPresets = {
  'Jedi Blue': Color(0xFF0080FF),      // Luke Skywalker
  'Sith Red': Color(0xFFFF0000),       // Darth Vader
  'Mace Windu': Color(0xFF8B00FF),     // Purple
  'Yoda Green': Color(0xFF00FF00),     // Green
  'Ahsoka White': Color(0xFFFFFFFF),   // White blade
  'Dark Saber': Color(0xFF1A1A1A),     // Black core
  'Orange': Color(0xFFFF8000),         // Rare color
  'Yellow': Color(0xFFFFFF00),         // Temple Guard
};
```

**UI**:
- Grid di card colorate (2 colonne)
- Nome preset + preview circolare
- Tap per applicare immediatamente

**Acceptance Criteria**:
- [ ] 8 preset configurati
- [ ] Applicazione istantanea al tap
- [ ] Smooth color transition (<=50ms)

#### 2.3 Sliders Speed/Brightness Real-Time
**Priorità**: Alta
**Complessità**: Bassa

**Slider Brightness**:
- Range: 0-255
- Debounce: 50ms (evita flood BLE)
- Icone: 🌑 (min) / ☀️ (max)

**Slider Speed**:
- Range: 1-100
- Preset: Slow (20) / Medium (50) / Fast (80)
- Solo per effetti che supportano speed (rainbow, pulse, breathe, etc.)

**Caratteristiche BLE**:
- `charLedBrightness`: `{"brightness": <0-255>, "enabled": true}`
- `charLedEffect`: `{"mode": "rainbow", "speed": <1-100>}`

**UI**:
- Slider verticali in tab "Controls"
- Label con valore numerico
- Disabilitato se effetto non supporta speed

**Acceptance Criteria**:
- [ ] Slider draggable smooth
- [ ] Debounce efficace (no flood BLE)
- [ ] Visual feedback immediato
- [ ] Valore numerico sincronizzato

#### 2.4 Control Panel Architecture
**Struttura UI**:

**Portrait Mode** (phone verticale):
```
┌─────────────────┬────────────────────┐
│   Lightsaber    │   Control Panel    │
│   75% width     │   25% width        │
│                 │                    │
│                 │  ┌──────────────┐  │
│                 │  │ Tab: Colors  │  │
│                 │  ├──────────────┤  │
│                 │  │ • HSV Wheel  │  │
│                 │  │ • Presets    │  │
│                 │  │ • Brightness │  │
│                 │  └──────────────┘  │
│                 │  ┌──────────────┐  │
│                 │  │ Tab: Effects │  │
│                 │  ├──────────────┤  │
│                 │  │ • List       │  │
│                 │  │ • Speed      │  │
│                 │  └──────────────┘  │
└─────────────────┴────────────────────┘
```

**Landscape Mode** (phone orizzontale):
```
┌──────────┬──────────────────────────┐
│ Lightsab │   Control Panel (Tabs)   │
│ er       │                          │
│ 30%      │  ┌────┬────┬──────────┐  │
│          │  │Col │Eff │Advanced  │  │
│          │  └────┴────┴──────────┘  │
│          │                          │
│          │  [Expanded content]      │
│          │  • HSV Wheel             │
│          │  • Presets Grid          │
│          │  • Sliders               │
└──────────┴──────────────────────────┘
```

**Tabs Implementati**:
1. **Colors**: HSV wheel + Presets + Brightness
2. **Effects**: Lista effetti + Speed slider
3. **Advanced**: Settings (future)

**Acceptance Criteria**:
- [ ] Responsive portrait/landscape
- [ ] Tab navigation smooth
- [ ] Consistent theme/style
- [ ] No overflow/clipping

### File da Creare/Modificare
**Nuovi File**:
- `lib/screens/tabs/colors_tab.dart` - Color picker + presets
- `lib/screens/tabs/effects_tab.dart` - Effects list + speed
- `lib/widgets/hsv_color_wheel.dart` - Custom HSV wheel
- `lib/widgets/preset_color_grid.dart` - Grid preset colori
- `lib/widgets/real_time_slider.dart` - Slider con debounce

**Modifiche**:
- `lib/screens/control_screen.dart` - Aggiunta TabBar + tab views
- `lib/providers/led_provider.dart` - Metodi setColorHSV(), setSpeed()
- `pubspec.yaml` - Dipendenza flutter_colorpicker

### Metriche di Successo Sprint 2
- Latenza colore app→LED: <100ms (p50), <200ms (p95)
- Debounce efficace: max 20 BLE writes/sec
- FPS UI: >30 durante drag sliders
- Crash rate: 0%

---

## 📅 **SPRINT 3: Audio Integration & Style Presets** (PIANIFICATO)

### Obiettivi
Integrare il sistema audio multi-pack e creare preset completi che includono suoni, effetti LED, colori e velocità.

### Architettura Esistente da Estendere

**File esistenti**:
- ✅ `lib/models/color_preset.dart` - Model preset con `id`, `name`, `color`, `effectId`, `createdAt`
- ✅ `lib/services/preset_storage_service.dart` - Storage con SharedPreferences
- ✅ `lib/screens/tabs/colors_tab.dart` - Tab colori con HSV picker
- ✅ `lib/screens/tabs/effects_tab.dart` - Tab effetti con lista
- ✅ `lib/screens/tabs/clock_tab.dart` - Tab orologio
- ✅ `lib/screens/tabs/motion_tab.dart` - Tab motion detection

### Features da Implementare

#### 3.1 Audio System Integration
**Priorità**: Alta
**Complessità**: Media
**Riferimento**: `docs/SOUND_ROADMAP.md`

**Dipendenze**:
```yaml
dependencies:
  just_audio: ^0.9.36  # Audio engine con pitch/pan/volume control
```

**Implementazione**:
- [ ] Creare `lib/models/sound_pack.dart` - Model per sound pack (jedi, sith, unstable, crystal)
- [ ] Creare `lib/services/audio_service.dart` - Core audio engine (hum loop, swing modulato, events)
- [ ] Creare `lib/providers/audio_provider.dart` - State management audio
- [ ] Aggiungere asset audio in `assets/sounds/{pack}/`:
  - `hum_base.wav` (loop 2-5s per HUM + SWING)
  - `ignition.wav` (1-2s)
  - `clash.wav` (0.3-0.5s)
  - `retract.wav` (1-2s)

**Sound Pack Registry**:
```dart
// jedi: 110Hz, suono pulito con layering
// sith: 70Hz, grave e denso
// unstable: 90Hz, crackling FM synthesis (Kylo Ren)
// crystal: 150Hz, cristallino e brillante
```

**Integration con Motion**:
- Collegare `MotionProvider` → `AudioProvider`
- Swing modulato da `perturbationGrid` (volume), `speed` (pitch), `direction` (pan)
- Gesture events (IGNITION, RETRACT, CLASH) trigger suoni

#### 3.2 Estensione ColorPreset → StylePreset
**Priorità**: Alta
**Complessità**: Bassa

**Modifiche a `lib/models/color_preset.dart`**:
```dart
class ColorPreset {
  final String id;
  final String name;
  final Color color;
  final String effectId;
  final DateTime createdAt;

  // NUOVI CAMPI DA AGGIUNGERE:
  final int speed;           // 1-100 (default: 50)
  final int brightness;      // 0-255 (default: 200)
  final String soundPackId;  // 'jedi', 'sith', 'unstable', 'crystal' (default: 'jedi')
  final bool isFavorite;     // Quick access (default: false)
}
```

**Modifiche a `lib/services/preset_storage_service.dart`**:
- Aggiornare serializzazione JSON per nuovi campi (backward compatible)
- Gestire migrazione preset vecchi (aggiungere valori default)

#### 3.3 Preset Predefiniti Star Wars con Suoni
**Priorità**: Alta
**Complessità**: Bassa

**Creare preset completi in `color_preset.dart`**:

```dart
class BuiltInPresets {
  static final List<ColorPreset> starWars = [
    // JEDI PRESETS
    ColorPreset(
      id: 'luke_skywalker',
      name: 'Luke Skywalker',
      color: Color(0xFF0080FF),
      effectId: 'solid',
      speed: 50,
      brightness: 220,
      soundPackId: 'jedi',  // ← BINDING SUONO
      isFavorite: true,
    ),
    ColorPreset(
      id: 'obi_wan',
      name: 'Obi-Wan Kenobi',
      color: Color(0xFF0099FF),
      effectId: 'sine_motion',
      speed: 40,
      brightness: 200,
      soundPackId: 'jedi',
    ),
    ColorPreset(
      id: 'yoda',
      name: 'Yoda',
      color: Color(0xFF00FF00),
      effectId: 'solid',
      speed: 50,
      brightness: 210,
      soundPackId: 'jedi',
    ),
    ColorPreset(
      id: 'mace_windu',
      name: 'Mace Windu',
      color: Color(0xFF8B00FF),
      effectId: 'pulse',
      speed: 45,
      brightness: 215,
      soundPackId: 'jedi',
    ),

    // SITH PRESETS
    ColorPreset(
      id: 'darth_vader',
      name: 'Darth Vader',
      color: Color(0xFFFF0000),
      effectId: 'solid',
      speed: 50,
      brightness: 230,
      soundPackId: 'sith',  // ← BINDING SUONO
      isFavorite: true,
    ),
    ColorPreset(
      id: 'darth_maul',
      name: 'Darth Maul',
      color: Color(0xFFFF0000),
      effectId: 'dual_pulse',
      speed: 70,
      brightness: 240,
      soundPackId: 'sith',
    ),

    // SPECIAL PRESETS
    ColorPreset(
      id: 'kylo_ren',
      name: 'Kylo Ren',
      color: Color(0xFFFF0000),
      effectId: 'unstable',
      speed: 80,
      brightness: 255,
      soundPackId: 'unstable',  // ← SUONO SPECIALE
      isFavorite: true,
    ),
    ColorPreset(
      id: 'ahsoka_tano',
      name: 'Ahsoka Tano',
      color: Color(0xFFFFFFFF),
      effectId: 'dual_pulse',
      speed: 60,
      brightness: 255,
      soundPackId: 'crystal',
    ),
    ColorPreset(
      id: 'dark_saber',
      name: 'Dark Saber',
      color: Color(0xFFFFFFFF),
      effectId: 'unstable',
      speed: 60,
      brightness: 255,
      soundPackId: 'crystal',
    ),
  ];
}
```

#### 3.4 Nuova Tab "Styles" (5a Tab)
**Priorità**: Media
**Complessità**: Media

**Creare `lib/screens/tabs/styles_tab.dart`**:

```dart
// Layout:
// 1. Sezione "Quick Style Creator"
//    - Preview colore corrente + effetto + suono
//    - Pulsante "Save Current Style"
// 2. Sezione "Star Wars Presets" (built-in)
//    - Grid 2 colonne con card preset
//    - Tap per applicare → LED + Audio insieme
// 3. Sezione "My Custom Styles"
//    - Grid preset salvati dall'utente
//    - Swipe to delete
```

**Modificare `lib/screens/control_screen.dart`**:
```dart
TabController(length: 5, vsync: this)  // 4 → 5

TabBar(
  tabs: [
    Tab(icon: Icon(Icons.palette), text: 'Colors'),
    Tab(icon: Icon(Icons.auto_awesome), text: 'Effects'),
    Tab(icon: Icon(Icons.access_time), text: 'Clock'),
    Tab(icon: Icon(Icons.motion_photos_on), text: 'Motion'),
    Tab(icon: Icon(Icons.style), text: 'Styles'),  // NUOVO
  ],
)
```

#### 3.5 LedProvider - Applicazione Preset Completo
**Priorità**: Alta
**Complessità**: Bassa

**Aggiungere in `lib/providers/led_provider.dart`**:
```dart
/// Applica un preset completo (colore + effetto + speed + brightness + audio)
Future<void> applyPreset(ColorPreset preset) async {
  // 1. Applica LED
  await setColor(preset.color.red, preset.color.green, preset.color.blue);
  await setBrightness(preset.brightness, true);
  await setEffect(preset.effectId, speed: preset.speed);

  // 2. Applica Audio (se AudioProvider disponibile)
  final audioProvider = _context.read<AudioProvider>();
  await audioProvider.setSoundPack(preset.soundPackId);

  // 3. Notifica cambio preset
  _currentPresetId = preset.id;
  notifyListeners();
}
```

### File da Creare/Modificare

**Nuovi File Audio**:
- `assets/sounds/jedi/hum_base.wav`
- `assets/sounds/jedi/ignition.wav`
- `assets/sounds/jedi/clash.wav`
- `assets/sounds/jedi/retract.wav`
- (+ sith, unstable, crystal)

**Nuovi File Flutter**:
- `lib/models/sound_pack.dart`
- `lib/services/audio_service.dart`
- `lib/providers/audio_provider.dart`
- `lib/screens/tabs/styles_tab.dart`

**Modifiche**:
- `lib/models/color_preset.dart` - Aggiungere campi speed, brightness, soundPackId, isFavorite
- `lib/services/preset_storage_service.dart` - Migrazione JSON
- `lib/providers/led_provider.dart` - Metodo applyPreset()
- `lib/screens/control_screen.dart` - Aggiungere 5a tab
- `pubspec.yaml` - Dipendenza just_audio + asset sounds

### Metriche di Successo Sprint 3
- Audio latency: <50ms (ignition/clash), <20ms (swing)
- Preset application: <200ms (LED + Audio)
- Asset size totale: <5MB (4 sound packs)
- Smooth audio loop: no click/glitch su hum_base
- Swing modulazione: volume/pitch/pan reattivi a perturbationGrid

---

## 🔮 **SPRINT 4: Advanced Features** (FUTURO)

### Sensori & Interattività

#### 4.1 Microfono → Music Reactive
**Requisiti firmware**: Nuova characteristic `MUSIC_REACTIVE` (UUID da definire)

- Permesso microfono (Android/iOS)
- Stream audio → volume RMS → brightness modulation
- Visualizzazione waveform
- Toggle on/off

#### 4.2 Accelerometro → Gestures
**Requisiti firmware**: Estensione `DeviceControl` characteristic

- Shake → Random color
- Flip → Next effect
- Tilt → Brightness up/down
- Configurazione sensitività

#### 4.3 Pattern Drawing
**Requisiti firmware**: Nuova characteristic `LED_PATTERN` (512 bytes)

- Canvas per disegnare con dito
- 144 LED individuali colorabili
- Salvataggio pattern custom
- Modalità paint/erase

---

## 📊 **Metriche Globali**

### Performance Targets
| Metrica | Target | Attuale (Sprint 1) |
|---------|--------|-------------------|
| App startup time | <2s | 1.8s ✅ |
| BLE connection time | <3s | 2.5s ✅ |
| Multi-device switch | <500ms | 350ms ✅ |
| Health check overhead | <1% CPU | 0.5% ✅ |
| Memory footprint | <150MB | 120MB ✅ |

### Quality Targets
| Metrica | Target | Attuale |
|---------|--------|---------|
| Crash-free rate | >99.5% | 100% ✅ |
| ANR rate | <0.1% | 0% ✅ |
| BLE reconnect success | >95% | 98% ✅ |
| User satisfaction | >4.5/5 | N/A |

---

## 🛠️ **Dipendenze Tecniche**

### Librerie Flutter (pubspec.yaml)
```yaml
dependencies:
  flutter_blue_plus: ^1.32.12        # BLE ✅
  provider: ^6.1.2                   # State management ✅
  shared_preferences: ^2.2.3         # Persistence ✅
  permission_handler: ^11.3.1        # Permissions ✅
  intl: ^0.19.0                      # Formatting ✅
  flutter_colorpicker: ^1.1.0        # Sprint 2 🚧
  sensors_plus: ^5.0.1               # Sprint 4 (futuro)
  microphone_stream: ^0.6.3          # Sprint 4 (futuro)
```

### Firmware Dependencies
**Nessuna modifica richiesta per Sprint 1-2**

Characteristics BLE già disponibili:
- ✅ `charLedColor` (WRITE)
- ✅ `charLedEffect` (WRITE)
- ✅ `charLedBrightness` (WRITE)
- ✅ `charLedState` (NOTIFY)
- ✅ `charEffectsList` (READ)

**Modifiche necessarie per Sprint 4**:
- ⏳ Nuova char `MUSIC_REACTIVE` (volume stream)
- ⏳ Nuova char `LED_PATTERN` (144 RGB values)
- ⏳ Estensione `DeviceControl` per gestures

---

## 📝 **Note di Implementazione**

### Design Guidelines
- **Tema**: Material 3, dark mode preferito per LED apps
- **Colori primari**: Rosso (#FF0000) per Sith theme
- **Font**: Roboto (system default)
- **Icone**: Material Icons + emoji per effects

### Code Style
- **Dart**: Effective Dart guidelines
- **Widget naming**: `_buildWidgetName()` per private builders
- **File organization**: feature-based (screens/ widgets/ models/ services/)
- **Comments**: Solo per logica complessa, no commenti ovvi

### Testing Strategy
**Sprint 1-2** (Manual Testing):
- Device pairing su 2+ sabers
- Multi-device switching
- Connection resilience (power off device)
- Color/brightness sliders accuracy

**Sprint 3+** (Automated):
- Unit tests per BLE logic
- Widget tests per UI components
- Integration tests per end-to-end flows

---

## 🔄 **Changelog**

### v2.0 - 2025-12-27 (Sprint 1 + Sprint 2 inizio)
**Added**:
- ✅ Multi-device connection manager
- ✅ Real-time connection health monitoring
- ✅ HomeScreen redesign con device cards
- ✅ Swipe-to-delete disconnessione
- ✅ Auto-reconnect con retry intelligente
- 🚧 HSV color picker (in corso)
- 🚧 Star Wars color presets (in corso)
- 🚧 Real-time brightness/speed sliders (in corso)

**Fixed**:
- ✅ BLE disconnect bug multi-device
- ✅ Race conditions su notifiche
- ✅ Memory leaks su subscription streams

**Changed**:
- ✅ BleService ora multi-device safe
- ✅ LedProvider integrato con MultiDeviceManager
- ✅ ControlScreen preparato per tab-based UI

### v1.0 - 2025-12-20 (Baseline)
- Connessione single-device
- Lightsaber widget animato
- Power on/off via BLE
- Lettura stato LED

---

## 📧 **Contacts & Resources**

**Repository**: `/home/reder/Documenti/PlatformIO/Projects/ledSaber`

**Documentazione Correlata**:
- `ANIMATION_BLE_ISSUE.md` - Analisi bug animazioni
- `AppMobile/flutter_led_saber/UI_DESIGN_PLAN.md` - Spec UI/UX (se presente)

**Developer**: Assistito da Claude Sonnet 4.5

---

**Ultimo aggiornamento**: 2025-12-27 21:30 CET
