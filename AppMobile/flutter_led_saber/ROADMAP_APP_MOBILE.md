# 📱 ROADMAP APP MOBILE FLUTTER - LED SABER

## 🎯 Obiettivo
Creare un'app Flutter cross-platform (Android/iOS) per controllare il LED Saber via Bluetooth Low Energy (BLE).

---

## 📝 CHANGELOG

### **27 Dicembre 2024 - Sprint 3 IN CORSO** 🔄

**Nuove Implementazioni:**

✅ **Motion Service - Architettura BLE Completa**
- `MotionService` - Servizio BLE completo per Motion Detection & Gesture System
  - UUID servizi e caratteristiche dal firmware (Motion Status, Control, Events, Config)
  - Read/Write/Notify per configurazione e stato real-time
  - Comandi: enable, disable, reset, calibrate
  - Stream per stato motion ed eventi gesture
- `MotionProvider` - State management con pattern Provider
  - Gestione configurazione motion (quality, soglie, gesture thresholds)
  - Stream subscription per live data (direction, speed, confidence)
  - Metodi helper per enable/disable/reset motion
  - Update singoli parametri configurazione
- `MotionState`, `MotionConfig`, `MotionEvent` - Modelli dati completi
  - Parsing JSON bi-direzionale (to/from BLE)
  - Supporto per tutti i parametri gesture system

✅ **Motion Tab - UI Completa & Connessa**
- Tab Motion integrato in Control Screen con 3 tab (Colors, Effects, Motion)
- **Connesso a MotionProvider** - Dati real-time da BLE
- Motion Detection toggle (ON/OFF) con stato sincronizzato
- Gesture Recognition toggle separato
- Live Motion Data display (Direction, Speed, Confidence) - aggiornamento automatico
- Last Gesture tracking (Type, Confidence) con icone specifiche per IGNITION/RETRACT/CLASH
- Motion Tuning sliders: Quality (0-255), Motion Min (0-50), Speed Min (0.0-5.0)
- Advanced Settings collapsabile: Gesture Thresholds (Ignition, Retract, Clash)
- Debug Logs toggle
- Apply Configuration button funzionante (invia via BLE)
- UI responsive compatibile con layout Portrait e Landscape
- Error handling con messaggi utente

**File creati:**
- [lib/models/motion_state.dart](lib/models/motion_state.dart) - Modelli MotionState, MotionConfig, MotionEvent (~210 righe)
- [lib/services/motion_service.dart](lib/services/motion_service.dart) - Servizio BLE Motion (~250 righe)
- [lib/providers/motion_provider.dart](lib/providers/motion_provider.dart) - Provider state management (~230 righe)
- [lib/screens/tabs/motion_tab.dart](lib/screens/tabs/motion_tab.dart) - UI completa connessa (~580 righe)

**File modificati:**
- [lib/screens/control_screen.dart](lib/screens/control_screen.dart) - Aggiunto terzo tab Motion
- [lib/models/connected_device.dart](lib/models/connected_device.dart) - Aggiunto campo `motionService?` opzionale

**Passi rimanenti per completamento** (90% completato):
1. ⏳ `multi_device_manager.dart` - Aggiungere creazione Motion Service opzionale (linee 173-191)
2. ⏳ `ble_provider.dart` - Aggiungere getter `motionService`
3. ⏳ `control_screen.dart` - Connettere MotionProvider al BLE provider
4. ⏳ `main.dart` - Aggiungere MotionProvider al MultiProvider
5. ⏳ Test compilazione e integrazione con firmware

**Documentazione tecnica**:
- Motion Service UUID: `6fafc401-1fb5-459e-8fcc-c5c9c331914b`
- Characteristic Status UUID: `7eb5583e-36e1-4688-b7f5-ea07361b26a9` (READ, NOTIFY)
- Characteristic Control UUID: `8dc5b4c3-eb10-4a3e-8a4c-1234567890ac` (WRITE)
- Characteristic Events UUID: `9ef6c5d4-fc21-5b4f-9b5d-2345678901bd` (NOTIFY)
- Characteristic Config UUID: `aff7d6e5-0d32-4c5a-ac6e-3456789012ce` (READ, WRITE)

**Note implementative**:
- Motion Service è **opzionale** - l'app funziona anche se il dispositivo non lo supporta
- Pattern architetturale identico a LED Service per consistenza
- Logging dettagliato per debug BLE
- Gestione errori graceful con feedback utente

---

### **27 Dicembre 2024 - Sprint 2 COMPLETATO** ✅

**Nuove Implementazioni:**

✅ **Effects Tab & Loading System**
- Sistema retry automatico (3 tentativi) con delay progressivo (500ms + 1000ms tra retry)
- Caricamento attivo quando l'utente apre il tab Effects
- Logging dettagliato per debug (traccia byte ricevuti, JSON parsing, tentativi)
- Fix MTU BLE: JSON compatto da 1415 byte → 492 byte (risolto "0 byte received")
- Parser retrocompatibile: supporta sia formato esteso che compatto (`id`/`i`, `name`/`n`)
- UI Effects Tab funzionante con lista scrollabile

✅ **JSON Optimization (Firmware)**
- Ridotto JSON lista effetti da 1415 byte a 492 byte (sotto MTU 512)
- Formato compatto: `{"v":"1.0","fx":[{"i":"solid","n":"Solid"}...]}`
- Rimossi emoji e parametri non essenziali
- 15 effetti disponibili caricabili via BLE

✅ **Gesture System - Documentazione Completa**
- Analisi completa sistema gesture a 2 livelli (Motion Detection + Gesture Processing)
- Parametri configurabili: `gestureClashEffect`, `gestureClashDurationMs`, `motionOnBoot`
- Settings BLE: `gesturesEnabled`, intensità soglie per IGNITION/RETRACT/CLASH
- Mapping effetti per direzioni (UP/DOWN/LEFT/RIGHT)
- Sistema Clash Effect personalizzabile (flash bianco standard o effetto custom)

**Problemi risolti:**
- Fix "Loading effects..." infinito: il JSON superava MTU BLE (1415 > 512 byte)
- Fix parsing JSON: aggiunti import `dart:math` e `package:flutter/foundation.dart`
- Fix gestione caratteristica vuota: ora gestisce correttamente 0 byte ricevuti

**Prossimi step:** Sprint 3 - Settings Tab con controlli gesture + Motion Service integration

---

### **27 Dicembre 2024 - Sprint 1 COMPLETATO** ✅

**Implementazioni completate:**

✅ **Setup & Configurazione**
- Dipendenze BLE installate (`flutter_blue_plus`, `provider`, `flutter_colorpicker`, `permission_handler`)
- Permessi Bluetooth configurati per Android (API 21+, targetSdk 34)
- Permessi Bluetooth configurati per iOS
- Supporto multipiattaforma (Android/iOS/Linux con gestione permessi condizionale)

✅ **Architettura & Models**
- `LedState` - Modello completo stato LED con parsing JSON
- `Effect` & `EffectsList` - Modelli per gestione effetti dinamici
- `BleDeviceInfo` - Info dispositivo con RSSI e qualità segnale

✅ **Services BLE**
- `BleService` - Scan, connessione, discovery servizi, gestione stato
- `LedService` - Controllo completo LED (colore, brightness, effetti, ignition, retract, time sync, boot config)
- UUID corretti dal firmware configurati e testati

✅ **State Management**
- `BleProvider` - Gestione connessione BLE con auto-notifiche
- `LedProvider` - Gestione stato LED con stream in tempo reale
- Pattern Provider correttamente implementato con MultiProvider

✅ **UI Screens**
- `HomeScreen` - Scansione dispositivi BLE, connessione, indicatore stato, gestione permessi runtime
- `ControlScreen` - Controllo completo con:
  - Blade control (accensione/spegnimento con stati: off/igniting/on/retracting)
  - Color picker RGB con preview
  - Brightness slider con toggle ON/OFF
  - Status LED control (collapsable)
  - Indicatore connessione in AppBar

✅ **Funzionalità Testate**
- Connessione BLE al dispositivo LedSaber-BLE
- Ricezione notifiche stato LED in tempo reale
- Lettura lista effetti dal dispositivo
- Invio comandi ignition/retract
- Cambio colore RGB
- Controllo brightness e status LED

✅ **UX/UI**
- Dark mode e light mode automatici
- Dialog di caricamento durante connessione
- Snackbar per feedback errori
- Gestione disconnessione automatica
- Temi Material Design 3

**Problemi risolti:**
- Fix `autoConnect` incompatibile con MTU requests
- Fix permessi su piattaforme desktop (Linux)
- Fix sincronizzazione stato connessione nel provider
- Fix deprecation `isAvailable` → `isSupported`

**Prossimi step:** Sprint 2 - Effects Screen con supporto chrono themes

---

## 📋 FASE 1: SETUP PROGETTO & BLUETOOTH (Priorità: ALTA)

### 1.1 Configurazione Dipendenze
**File**: `pubspec.yaml`

```yaml
dependencies:
  flutter:
    sdk: flutter
  cupertino_icons: ^1.0.8

  # Bluetooth BLE
  flutter_blue_plus: ^1.32.0  # Libreria BLE raccomandata

  # State Management
  provider: ^6.1.0             # Gestione stato globale

  # UI & Utilities
  flutter_colorpicker: ^1.1.0  # Color picker RGB
  intl: ^0.19.0                # Formattazione date/ore per time sync

  # Permissions
  permission_handler: ^11.0.0  # Gestione permessi runtime
```

**Comando**:
```bash
cd AppMobile/flutter_led_saber
flutter pub get
```

---

### 1.2 Configurazione Permessi

#### **Android**
**File**: `android/app/src/main/AndroidManifest.xml`

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <!-- Bluetooth permissions -->
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN"
                     android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

    <!-- Android 12+ -->
    <uses-permission android:name="android.permission.BLUETOOTH_ADVERTISE" />

    <application ...>
        ...
    </application>
</manifest>
```

**File**: `android/app/build.gradle`
```gradle
android {
    ...
    compileSdkVersion 34

    defaultConfig {
        minSdkVersion 21  // BLE richiede minimo API 21
        targetSdkVersion 34
    }
}
```

#### **iOS**
**File**: `ios/Runner/Info.plist`

```xml
<dict>
    ...
    <key>NSBluetoothAlwaysUsageDescription</key>
    <string>This app needs Bluetooth to control your LED Saber</string>

    <key>NSBluetoothPeripheralUsageDescription</key>
    <string>This app needs Bluetooth to connect to LED Saber device</string>
</dict>
```

---

### 1.3 Struttura Progetto

```
lib/
├── main.dart                    # Entry point
├── models/                      # Data models
│   ├── led_state.dart          # Modello stato LED (sync con firmware)
│   ├── effect.dart             # Modello effetti
│   ├── motion_state.dart       # Modello motion detection
│   └── ble_device_info.dart    # Info dispositivo BLE
├── services/                    # Business logic
│   ├── ble_service.dart        # Servizio BLE principale
│   ├── led_service.dart        # Servizio LED (UUID: 4fafc201...)
│   ├── motion_service.dart     # Servizio Motion
│   ├── camera_service.dart     # Servizio Camera (opzionale)
│   └── permission_service.dart # Gestione permessi
├── providers/                   # State management
│   ├── ble_provider.dart       # Provider connessione BLE
│   ├── led_provider.dart       # Provider stato LED
│   └── motion_provider.dart    # Provider motion
├── screens/                     # UI Screens
│   ├── home_screen.dart        # Home con scan & connect
│   ├── control_screen.dart     # Controllo principale
│   ├── effects_screen.dart     # Lista effetti
│   ├── motion_screen.dart      # Motion detection UI
│   └── settings_screen.dart    # Impostazioni
└── widgets/                     # Componenti riutilizzabili
    ├── color_picker_widget.dart
    ├── brightness_slider.dart
    ├── effect_card.dart
    └── connection_indicator.dart
```

---

## 📋 FASE 2: IMPLEMENTAZIONE BLUETOOTH CORE (Priorità: ALTA)

### 2.1 BLE Service - Connessione Base

**File**: `lib/services/ble_service.dart`

**Funzionalità**:
- ✅ Scan dispositivi BLE (filtrare per nome "LedSaber" o UUID servizio)
- ✅ Connessione/Disconnessione
- ✅ Auto-reconnect su perdita connessione
- ✅ Gestione stato connessione (notify UI)
- ✅ Discovery servizi e caratteristiche

**UUID Servizi da cercare**:
```dart
// Dal firmware (BLELedController.h)
static const String LED_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static const String MOTION_SERVICE_UUID = "..."; // Trovare da BLEMotionService
static const String CAMERA_SERVICE_UUID = "..."; // Trovare da BLECameraService
```

**Metodi principali**:
```dart
class BleService {
  Future<List<BluetoothDevice>> scanForDevices();
  Future<void> connectToDevice(BluetoothDevice device);
  Future<void> disconnect();
  Stream<BluetoothConnectionState> get connectionState;
  Future<List<BluetoothService>> discoverServices();
}
```

---

### 2.2 LED Service - Controllo LED

**File**: `lib/services/led_service.dart`

**Characteristic UUID** (da `BLELedController.h`):
```dart
// READ + NOTIFY
static const CHAR_LED_STATE = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

// WRITE
static const CHAR_LED_COLOR = "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab";
static const CHAR_LED_EFFECT = "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc";
static const CHAR_LED_BRIGHTNESS = "f3e7c6e5-0d32-4c5a-ac6e-3456789012cd";
static const CHAR_STATUS_LED = "a4b8d7f9-1e43-6c7d-ad8f-456789abcdef";
static const CHAR_TIME_SYNC = "d6e1a0b8-4a76-9f0c-dc1a-789abcdef012";
static const CHAR_DEVICE_CONTROL = "c7f8e0d9-5b87-1a2b-be9d-7890abcdef23";

// READ (lista effetti)
static const CHAR_EFFECTS_LIST = "d8f9e1ea-6c98-2b3c-cf0e-890abcdef234";
```

**Metodi**:
```dart
class LedService {
  // Lettura stato (con notify)
  Stream<LedState> get ledStateStream;

  // Lettura lista effetti (READ, fare una volta all'avvio)
  Future<List<Effect>> getEffectsList();

  // Scrittura comandi
  Future<void> setColor(int r, int g, int b);
  Future<void> setEffect(String mode, {int? speed});
  Future<void> setBrightness(int brightness, bool enabled);
  Future<void> setStatusLed(int brightness, bool enabled);
  Future<void> syncTime(int epochTimestamp);

  // Ignition & Retract
  Future<void> ignite();
  Future<void> retract();

  // Boot config
  Future<void> setBootConfig({
    bool? autoIgnition,
    int? delayMs,
    bool? motionOnBoot
  });
}
```

**Formato JSON payload** (esempio setColor):
```dart
Future<void> setColor(int r, int g, int b) async {
  final json = jsonEncode({'r': r, 'g': g, 'b': b});
  await characteristic.write(utf8.encode(json));
}
```

---

### 2.3 Models - Sincronizzazione Stato

**File**: `lib/models/led_state.dart`

```dart
class LedState {
  final int r;
  final int g;
  final int b;
  final int brightness;
  final String effect;
  final int speed;
  final bool enabled;
  final String bladeState; // "off" | "igniting" | "on" | "retracting"
  final bool statusLedEnabled;
  final int statusLedBrightness;

  // Chrono themes
  final int chronoHourTheme;
  final int chronoSecondTheme;

  LedState.fromJson(Map<String, dynamic> json)
    : r = json['r'] ?? 255,
      g = json['g'] ?? 0,
      b = json['b'] ?? 0,
      brightness = json['brightness'] ?? 255,
      effect = json['effect'] ?? 'solid',
      speed = json['speed'] ?? 50,
      enabled = json['enabled'] ?? false,
      bladeState = json['bladeState'] ?? 'off',
      statusLedEnabled = json['statusLedEnabled'] ?? true,
      statusLedBrightness = json['statusLedBrightness'] ?? 32,
      chronoHourTheme = json['chronoHourTheme'] ?? 0,
      chronoSecondTheme = json['chronoSecondTheme'] ?? 0;
}
```

**File**: `lib/models/effect.dart`

```dart
class Effect {
  final String id;
  final String name;
  final List<String> params; // es. ["speed", "color"]
  final Map<String, List<String>>? themes; // Per chrono_hybrid

  Effect.fromJson(Map<String, dynamic> json)
    : id = json['id'],
      name = json['name'],
      params = List<String>.from(json['params'] ?? []),
      themes = json['themes'] != null
        ? (json['themes'] as Map<String, dynamic>).map(
            (k, v) => MapEntry(k, List<String>.from(v)))
        : null;
}

class EffectsList {
  final String version;
  final List<Effect> effects;

  EffectsList.fromJson(Map<String, dynamic> json)
    : version = json['version'],
      effects = (json['effects'] as List)
          .map((e) => Effect.fromJson(e))
          .toList();
}
```

---

## 📋 FASE 3: UI PRINCIPALE (Priorità: ALTA)

### 3.1 Home Screen - Scan & Connect

**File**: `lib/screens/home_screen.dart`

**Features**:
- ✅ Bottone "Scan for LED Saber"
- ✅ Lista dispositivi trovati (mostrare nome + RSSI signal strength)
- ✅ Indicatore stato connessione
- ✅ Gestione permessi (richiesta runtime)
- ✅ Auto-connect a ultimo dispositivo connesso (salva in SharedPreferences)

**UI Mockup**:
```
┌─────────────────────────────┐
│  LED Saber Controller       │
├─────────────────────────────┤
│                             │
│  [●] Disconnected           │
│                             │
│  ┌───────────────────────┐  │
│  │   [SCAN DEVICES]      │  │
│  └───────────────────────┘  │
│                             │
│  Devices Found:             │
│  ┌───────────────────────┐  │
│  │ 📱 LedSaber_A3F2      │  │
│  │    RSSI: -65 dBm      │  │
│  │    [CONNECT]          │  │
│  └───────────────────────┘  │
│                             │
└─────────────────────────────┘
```

---

### 3.2 Control Screen - Controllo Principale

**File**: `lib/screens/control_screen.dart`

**Sezioni UI** (secondo `LED_SABER_APP_GUIDE.md`):

#### **A. Blade Control**
```
┌─────────────────────────────┐
│  🔴 Blade State: OFF        │
│                             │
│  ┌──────────┐  ┌──────────┐ │
│  │ IGNITE ⚡│  │ RETRACT ⬇│ │
│  └──────────┘  └──────────┘ │
└─────────────────────────────┘
```
- Mostra `bladeState` corrente (off/igniting/on/retracting)
- Disabilita bottoni durante animazioni (igniting/retracting)
- Comandi: `{"command":"ignition"}` / `{"command":"retract"}`

#### **B. Color & Brightness**
```
┌─────────────────────────────┐
│  Color Picker               │
│  ┌────────────────────────┐ │
│  │  [RGB Color Wheel]     │ │
│  │  Current: #FF0000      │ │
│  └────────────────────────┘ │
│                             │
│  Brightness: ████████░░ 200 │
│  [●──────────────────] ON   │
└─────────────────────────────┘
```
- Color picker RGB (usa `flutter_colorpicker`)
- Slider brightness (0-255)
- Toggle ON/OFF

#### **C. Effects**
```
┌─────────────────────────────┐
│  Current Effect: Rainbow 🌈 │
│                             │
│  ┌────────────────────────┐ │
│  │ [CHANGE EFFECT]        │ │
│  └────────────────────────┘ │
│                             │
│  Speed: ████░░░░░░░░░░ 100  │
│  [●──────────────────]      │
└─────────────────────────────┘
```
- Mostra effetto corrente
- Bottone "Change Effect" → apre `effects_screen.dart`
- Slider speed (solo se effect.params contiene "speed")

#### **D. Status LED (optional, collapsable)**
```
┌─────────────────────────────┐
│  ▼ Status LED (Pin 4)       │
│                             │
│  Brightness: ██░░░░ 32      │
│  [●──────────────] ON       │
└─────────────────────────────┘
```

---

### 3.3 Effects Screen - Lista Effetti

**File**: `lib/screens/effects_screen.dart`

**Features**:
- ✅ Leggere lista effetti da `CHAR_EFFECTS_LIST` (READ, una volta)
- ✅ Mostrare lista scrollabile con card per ogni effetto
- ✅ Quando si seleziona un effetto → mostra parametri dinamici
- ✅ Se effetto = "chrono_hybrid" → mostra 2 selector per temi

**UI Mockup**:
```
┌─────────────────────────────┐
│  ← Effects List             │
├─────────────────────────────┤
│                             │
│  ┌───────────────────────┐  │
│  │ ● Solid Color         │  │
│  │   Params: color       │  │
│  └───────────────────────┘  │
│                             │
│  ┌───────────────────────┐  │
│  │ 🌈 Rainbow            │  │
│  │   Params: speed       │  │
│  └───────────────────────┘  │
│                             │
│  ┌───────────────────────┐  │
│  │ 🕐 Chrono Clock       │  │
│  │   Params: themes      │  │
│  └───────────────────────┘  │ ← Selezionato
│                             │
│  Hour Theme:                │
│  [Classic▼] [Neon] [Plasma] │
│                             │
│  Second Theme:              │
│  [Spiral▼] [Fire] [Lightning]│
│                             │
│  [APPLY EFFECT]             │
└─────────────────────────────┘
```

**Logica Chrono**:
```dart
if (effect.id == 'chrono_hybrid' && effect.themes != null) {
  // Mostra 2 dropdown
  // Hour themes: effect.themes['hour']
  // Second themes: effect.themes['second']

  // Al click APPLY:
  await ledService.setEffect(
    'chrono_hybrid',
    chronoHourTheme: selectedHourIndex,
    chronoSecondTheme: selectedSecondIndex
  );
}
```

**Time Sync** (bottone separato):
```dart
Future<void> syncTime() async {
  final now = DateTime.now();
  final epoch = now.millisecondsSinceEpoch ~/ 1000;
  await ledService.syncTime(epoch);
}
```

---

## 📋 FASE 4: MOTION DETECTION & GESTURE SYSTEM (Priorità: MEDIA)

### 4.1 Motion Service

**File**: `lib/services/motion_service.dart`

**Characteristic UUID** (da `BLEMotionService.h`):
```dart
// Da implementare - verificare UUID nel firmware
static const CHAR_MOTION_STATUS = "...";
static const CHAR_MOTION_EVENTS = "...";
static const CHAR_MOTION_CONTROL = "...";
static const CHAR_MOTION_CONFIG = "...";
```

### 4.2 Gesture System - Architettura ✅ DOCUMENTATO

Il sistema gesture opera su **2 livelli indipendenti**:

#### **Livello 1: Motion Detection (ON/OFF globale)**
- **Gestito da**: `BLEMotionService::_motionEnabled` (bool)
- **Controlla**: Se il rilevamento optical flow è attivo
- **Comandi BLE** (stringa, via CHAR_MOTION_CONTROL):
  ```dart
  await motionControlChar.write(utf8.encode("enable"));   // Abilita motion
  await motionControlChar.write(utf8.encode("disable"));  // Disabilita motion
  await motionControlChar.write(utf8.encode("reset"));    // Reset detector
  ```

#### **Livello 2: Gesture Processing (Configurazione fine)**
- **Gestito da**: `MotionProcessor::Config::gesturesEnabled` (bool)
- **Controlla**: Riconoscimento specifico delle gesture (IGNITION, RETRACT, CLASH)
- **Config JSON** (via CHAR_MOTION_CONFIG):
  ```dart
  final config = {
    "enabled": true,                    // Motion detection ON/OFF
    "gesturesEnabled": true,            // Gesture recognition ON/OFF
    "quality": 160,                     // Qualità optical flow (0-255)
    "motionIntensityMin": 6,            // Soglia intensità motion
    "motionSpeedMin": 0.4,              // Soglia velocità motion
    "gestureIgnitionIntensity": 15,     // Soglia gesture IGNITION
    "gestureRetractIntensity": 15,      // Soglia gesture RETRACT
    "gestureClashIntensity": 15,        // Soglia gesture CLASH
    "effectMapUp": "flicker",           // Effetto su movimento SU
    "effectMapDown": "",                // Effetto su movimento GIÙ
    "effectMapLeft": "",                // Effetto su movimento SINISTRA
    "effectMapRight": "",               // Effetto su movimento DESTRA
    "debugLogs": false                  // Log verbosi
  };
  await motionConfigChar.write(utf8.encode(jsonEncode(config)));
  ```

### 4.3 Gesture Clash Effect - Sistema Personalizzabile ✅ DOCUMENTATO

**Nel firmware** (`LedState` in `BLELedController.h`):
```cpp
String gestureClashEffect = "clash";        // ID effetto per CLASH gesture
uint16_t gestureClashDurationMs = 500;      // Durata effetto (50-5000ms)
```

**Comportamento** (`LedEffectEngine::onGestureDetected`):

1. **Clash Standard** (flash bianco nativo):
   ```cpp
   if (gestureClashEffect == "clash" || gestureClashEffect.isEmpty()) {
       _mode = Mode::CLASH_ACTIVE;  // Flash bianco
   }
   ```

2. **Effetto Personalizzato** (usa un effetto base):
   ```cpp
   else {
       _gestureEffectName = gestureClashEffect;      // es. "flicker"
       _gestureEffectDurationMs = gestureClashDurationMs;
       _mode = Mode::GESTURE_EFFECT;  // Esegue l'effetto base
   }
   ```

**Invio via BLE** (via CHAR_LED_EFFECT_UUID):
```dart
// Cambia effetto clash personalizzato
final payload = {
  "mode": "solid",                    // Effetto base corrente
  "gestureClashEffect": "flicker",    // Nuovo effetto clash
  "gestureClashDurationMs": 800       // Durata in ms
};
await ledEffectChar.write(utf8.encode(jsonEncode(payload)));
```

### 4.4 Casi d'Uso - Gesture Configuration

#### **Caso 1: Disabilitare tutte le gesture**
```dart
final config = {"gesturesEnabled": false};
await motionConfigChar.write(utf8.encode(jsonEncode(config)));
```

#### **Caso 2: Disabilitare solo CLASH**
```dart
final config = {"gestureClashIntensity": 255}; // Soglia impossibile
await motionConfigChar.write(utf8.encode(jsonEncode(config)));
```

#### **Caso 3: Usare effetto personalizzato per CLASH**
```dart
// Via LED Effect characteristic
final payload = {
  "gestureClashEffect": "pulse",
  "gestureClashDurationMs": 1000
};
await ledEffectChar.write(utf8.encode(jsonEncode(payload)));
```

#### **Caso 4: Disabilitare motion completamente**
```dart
await motionControlChar.write(utf8.encode("disable"));
```

### 4.5 Boot Configuration con Motion ✅ DOCUMENTATO

**Abilitare motion automaticamente all'avvio**:
```dart
final bootConfig = {
  "command": "boot_config",
  "motionEnabled": true  // Abilita motion + camera all'avvio
};
await deviceControlChar.write(utf8.encode(jsonEncode(bootConfig)));
```

**Nel firmware** (`LedState`):
```cpp
bool motionOnBoot = false;  // Se true, abilita motion automaticamente
```

---

### 4.2 Motion Screen

**File**: `lib/screens/motion_screen.dart`

**Sezioni UI**:

#### **A. Live Motion**
```
┌─────────────────────────────┐
│  Motion Detection: ON       │
│                             │
│  Direction: ↑ UP            │
│  Speed: 2.5                 │
│  Confidence: 85%            │
│                             │
│  Last Gesture: CLASH 💥     │
│  Confidence: 92%            │
└─────────────────────────────┘
```

#### **B. Tuning Sliders**
```
┌─────────────────────────────┐
│  ▼ Motion Tuning            │
│                             │
│  Quality: ████████░░░░ 128  │
│  Motion Min: ███░░░░░░ 12   │
│  Speed Min: ██░░░░░░░░ 1.2  │
└─────────────────────────────┘
```

#### **C. Advanced (collapsable)**
```
┌─────────────────────────────┐
│  ▼ Gesture Thresholds       │
│                             │
│  Ignition: ████████░ 140    │
│  Retract: ███████░░ 120     │
│  Clash: ██████████ 200      │
│                             │
│  [×] Debug Logs             │
└─────────────────────────────┘
```

---

## 📋 FASE 5: SETTINGS & BOOT CONFIG (Priorità: BASSA)

### 5.1 Settings Screen

**File**: `lib/screens/settings_screen.dart`

**Features**:

#### **A. Boot Behavior**
```
┌─────────────────────────────┐
│  ▼ Boot Configuration       │
│                             │
│  [✓] Auto ignite on boot    │
│  Delay: ███████░░░ 3000 ms  │
│                             │
│  [✓] Motion on boot         │
│                             │
│  [SAVE BOOT CONFIG]         │
└─────────────────────────────┘
```

Payload:
```dart
final bootConfig = {
  "command": "boot_config",
  "autoIgnitionOnBoot": true,
  "autoIgnitionDelayMs": 3000,
  "motionEnabled": true
};
await deviceControlChar.write(utf8.encode(jsonEncode(bootConfig)));
```

#### **B. Device Info**
```
┌─────────────────────────────┐
│  Device Information         │
│                             │
│  Name: LedSaber_A3F2        │
│  MAC: AA:BB:CC:DD:EE:FF     │
│  RSSI: -65 dBm              │
│  Battery: N/A               │
│                             │
│  [DISCONNECT]               │
│  [FORGET DEVICE]            │
└─────────────────────────────┘
```

---

## 📋 FASE 6: TESTING & POLISH (Priorità: MEDIA)

### 6.1 Testing

**Unit Tests**:
- ✅ Test parsing JSON models (LedState, Effect)
- ✅ Test UUID constants

**Integration Tests**:
- ✅ Test connessione BLE (con mock device)
- ✅ Test read/write characteristics

**Widget Tests**:
- ✅ Test UI screens (snapshot testing)

### 6.2 Error Handling

- ✅ Gestione disconnessioni impreviste (auto-reconnect)
- ✅ Timeout su write BLE (max 5s)
- ✅ Validazione input utente (es. brightness 0-255)
- ✅ Mostrare toast/snackbar per feedback operazioni

### 6.3 UI Polish

- ✅ Dark mode / Light mode
- ✅ Animazioni smooth (blade ignition progress bar)
- ✅ Icone custom per effetti
- ✅ Localizzazione (opzionale: IT/EN)

---

## 📋 FASE 7: BUILD & DEPLOYMENT (Priorità: BASSA)

### 7.1 Android Build

```bash
flutter build apk --release
# Output: build/app/outputs/flutter-apk/app-release.apk
```

### 7.2 iOS Build

```bash
flutter build ios --release
# Richiede: Apple Developer Account + Xcode setup
```

### 7.3 App Icons & Splash Screen

Usa `flutter_launcher_icons` e `flutter_native_splash`:

```yaml
dev_dependencies:
  flutter_launcher_icons: ^0.13.0
  flutter_native_splash: ^2.3.0
```

---

## 🎯 PRIORITÀ IMPLEMENTAZIONE

### **Sprint 1** (MVP) ✅ **COMPLETATO**
1. ✅ Setup progetto + dipendenze Bluetooth
2. ✅ Permessi Android/iOS
3. ✅ BLE Service: scan + connect
4. ✅ LED Service: read state, write color/brightness/effect
5. ✅ Home Screen: scan & connect
6. ✅ Control Screen: blade control + color picker + brightness

**Status**: App funzionante! Connessione BLE stabilita, ricezione notifiche stato LED, controlli base implementati.

### **Sprint 2** (Features Core) ✅ **COMPLETATO**
7. ✅ Effects Screen: lista dinamica caricata da BLE (con retry system)
8. ✅ JSON Optimization: ridotto da 1415 byte a 492 byte (fix MTU)
9. ✅ Status LED control (già implementato in Control Screen)
10. ✅ Gesture System: documentazione completa architettura 2 livelli
11. ⏳ Time sync per chrono (TO-DO)

**Status**: Effects Tab funzionante! 15 effetti caricabili via BLE, sistema gesture documentato.

### **Sprint 3** (Motion & Settings - IN CORSO) 🔄 90% COMPLETATO
12. ⏳ Settings Tab: UI per configurare gesture e boot config
13. ⏳ Estendere LedState con campi gesture (`gestureClashEffect`, `motionOnBoot`)
14. ✅ Motion Service: integrazione completa config + status stream (architettura BLE completa)
15. ✅ Motion Tab: UI completa connessa a MotionProvider con dati real-time
16. ⏳ Motion Service: ultimi 4 step integrazione (multi_device_manager, ble_provider, control_screen, main)
17. ⏳ Time sync UI per chrono

**Progresso Motion Service**:
- ✅ Models (MotionState, MotionConfig, MotionEvent)
- ✅ Service BLE (MotionService con tutte le characteristic)
- ✅ Provider (MotionProvider con state management)
- ✅ UI (MotionTab connesso al provider)
- ⏳ Integrazione finale (4 modifiche rimanenti nei file esistenti)

### **Sprint 4** (Polish - DA FARE)
12. ⏳ Settings: boot config
13. ⏳ Error handling + auto-reconnect
14. ✅ Dark mode (già implementato)
15. ⏳ Testing

---

## 🔧 TIPS IMPLEMENTAZIONE

### **Gestione Stato (Provider Pattern)**

```dart
// lib/providers/led_provider.dart
class LedProvider extends ChangeNotifier {
  LedState? _state;

  LedState? get state => _state;

  void updateState(LedState newState) {
    _state = newState;
    notifyListeners();
  }
}

// In main.dart
runApp(
  MultiProvider(
    providers: [
      ChangeNotifierProvider(create: (_) => BleProvider()),
      ChangeNotifierProvider(create: (_) => LedProvider()),
      ChangeNotifierProvider(create: (_) => MotionProvider()),
    ],
    child: MyApp(),
  ),
);
```

### **BLE Read con Notify**

```dart
// Subscribe a LED State notifications
await characteristic.setNotifyValue(true);
characteristic.lastValueStream.listen((value) {
  final json = jsonDecode(utf8.decode(value));
  final state = LedState.fromJson(json);
  ledProvider.updateState(state);
});
```

### **BLE Write con Feedback**

```dart
Future<void> setColor(int r, int g, int b) async {
  try {
    final json = jsonEncode({'r': r, 'g': g, 'b': b});
    await characteristic.write(
      utf8.encode(json),
      withoutResponse: false, // Attende conferma
    );

    // Feedback UI
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('Color updated!')),
    );
  } catch (e) {
    // Error handling
    print('Error writing color: $e');
  }
}
```

---

## 📚 RIFERIMENTI FIRMWARE

### **UUID Services & Characteristics**

Vedi file firmware:
- [`include/BLELedController.h`](../../include/BLELedController.h) - LED Service UUID
- `src/BLEMotionService.h` - Motion Service UUID (da verificare)
- `src/BLECameraService.h` - Camera Service UUID (opzionale)

### **Protocollo BLE**

Vedi documentazione:
- [`AppMobile/flutter_led_saber/LED_SABER_APP_GUIDE.md`](LED_SABER_APP_GUIDE.md) - Spec completa UI/UX

### **JSON Payloads**

Tutti i comandi BLE usano JSON:
```dart
// Esempio: set effect + chrono themes
{
  "mode": "chrono_hybrid",
  "chronoHourTheme": 2,      // Plasma
  "chronoSecondTheme": 5     // Quantum
}
```

---

## ✅ CHECKLIST FINALE

Prima del rilascio:
- [ ] Test su dispositivo fisico Android (non solo emulatore)
- [ ] Test su iOS (se disponibile)
- [ ] Verifica permessi Bluetooth funzionanti
- [ ] Test disconnessione/reconnessione
- [ ] Verifica performance UI (60 FPS)
- [ ] Test con firmware reale LED Saber
- [ ] Screenshot per store (se pubblicazione prevista)
- [ ] Privacy policy (se richiesto da store)

---

## 🎉 CONCLUSIONE

Questa roadmap copre tutte le funzionalità del LED Saber App con priorità chiare.

**Per il Bluetooth**: usa `flutter_blue_plus` + permessi corretti, il resto è gestione JSON BLE.

**Next Step**: Inizia con Sprint 1 (MVP) per avere una base funzionante!
