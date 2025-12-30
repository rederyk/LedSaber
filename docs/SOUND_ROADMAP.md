# 🎵 SOUND ROADMAP - Audio Reattivo LedSaber

## 🎯 Obiettivo Finale
Una spada laser che SUONA in modo reattivo al movimento reale, con supporto per **multipli sound pack** (Jedi, Sith, Kyber Crystal, Custom) e **effetti modulati dinamicamente**.

---

## 🔊 ARCHITETTURA AUDIO

### Sistema Audio Multi-Layer

```
┌─────────────────────────────────────────────────────────────┐
│                    AUDIO ENGINE                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Layer 1: BASE HUM (Loop continuo)                         │
│    ├─ hum_base.wav (2-5s loop perfetto)                    │
│    ├─ Volume: costante                                     │
│    └─ Pitch: fisso 1.0x                                    │
│                                                             │
│  Layer 2: SWING (Stessa traccia HUM distorta)              │
│    ├─ hum_base.wav (stessa traccia in loop!)               │
│    ├─ Volume: 0-100% ∝ energia griglia 8x8                 │
│    ├─ Pitch: 0.7-1.5x ∝ velocità movimento                 │
│    ├─ Pan: -1.0 ↔ +1.0 ∝ direzione L/R (non supportato)    │
│    ├─ Modulazione: CONTINUA in tempo reale (theremin)      │
│    └─ Auto-stop: quando volume < 5%                        │
│                                                             │
│  Layer 3: ONE-SHOT EVENTS                                  │
│    ├─ ignition.wav (accensione)                            │
│    ├─ retract.wav (spegnimento)                            │
│    └─ clash.wav (colpo)                                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Filosofia chiave:**
- **HUM e SWING = stessa traccia audio**
- Swing viene creato tramite **distorsione parametrica** della traccia HUM
- Questo garantisce coerenza sonora e riduce asset necessari

---

## 📂 STRUTTURA FILE AUDIO

### Directory Layout

```
AppMobile/flutter_led_saber/assets/sounds/
├── jedi/
│   ├── hum_base.wav          # Base hum (2-5s loop) - USATO PER HUM + SWING
│   ├── ignition.wav          # Accensione (1-2s)
│   ├── clash.wav             # Colpo (0.3-0.5s)
│   └── retract.wav           # Spegnimento (1-2s)
│
├── sith/
│   ├── hum_base.wav          # Più grave e distorto
│   ├── ignition.wav
│   ├── clash.wav
│   └── retract.wav
│
├── kyber_crystal/
│   ├── hum_base.wav          # Suono cristallino
│   ├── ignition.wav
│   ├── clash.wav
│   └── retract.wav
│
├── unstable/                  # Stile Kylo Ren
│   ├── hum_base.wav          # Crackling sound
│   ├── ignition.wav
│   ├── clash.wav
│   └── retract.wav
│
└── custom_1/                  # Slot per suoni personalizzati
    ├── hum_base.wav
    ├── ignition.wav
    ├── clash.wav
    └── retract.wav
```

### Specifiche Tecniche File Audio

**hum_base.wav (CRITICO - usato per HUM + SWING):**
- Sample rate: **44100 Hz**
- Bit depth: **16-bit**
- Formato: **WAV** (latency minima)
- Durata: **2-5 secondi** (loop perfetto senza click)
- Loop point: esatto (verificare zero-crossing)
- Frequenza fondamentale: 80-120 Hz (Jedi), 50-80 Hz (Sith)

**ignition.wav, retract.wav:**
- Sample rate: 44100 Hz
- Bit depth: 16-bit
- Formato: WAV o MP3
- Durata: 1-2 secondi

**clash.wav:**
- Sample rate: 44100 Hz
- Bit depth: 16-bit
- Formato: WAV
- Durata: 0.3-0.5 secondi
- Carattere: impulsivo, percussivo

---

## 🎛️ SWING MODULAZIONE - Formula Distorsione

### Come Swing viene creato da HUM

```dart
// SWING = HUM + distorsione parametrica real-time

AudioPlayer swingPlayer = AudioPlayer();

void playSwing({
  required List<int> perturbationGrid,  // 64 valori (griglia 8x8)
  required double speed,                 // px/frame
  required String direction,             // LEFT/RIGHT/UP/DOWN
}) {
  // 1. Calcola VOLUME da energia griglia
  double volume = calculateVolumeFromGrid(perturbationGrid);

  // 2. Calcola PITCH da velocità
  double pitch = calculatePitchFromSpeed(speed);

  // 3. Calcola PAN da direzione
  double pan = calculatePanFromDirection(direction);

  // 4. DISTORSIONE: stessa traccia hum_base.wav
  swingPlayer.setVolume(volume * masterVolume);
  swingPlayer.setPlaybackRate(pitch);  // Pitch shift
  swingPlayer.setBalance(pan);         // Stereo pan

  // 5. (Opzionale) Applica High-Pass Filter
  // swingPlayer.setFilter(highPass: 2000Hz);

  // 6. Play one-shot
  swingPlayer.play(AssetSource('sounds/$soundPack/hum_base.wav'));
}
```

### Curve di Modulazione

**Volume (0.0 - 1.0):**
```dart
double calculateVolumeFromGrid(List<int> grid) {
  if (grid.isEmpty) return 0.0;

  // Somma energia totale griglia 8x8
  int totalEnergy = grid.reduce((a, b) => a + b);

  // Normalizza: max = 64 celle × 255 = 16320
  double normalized = totalEnergy / 16320.0;

  // Applica gamma correction (0.7) per boost medi
  double curved = pow(normalized, 0.7);

  return curved.clamp(0.0, 1.0);
}
```

**Pitch (0.7x - 1.5x):**
```dart
double calculatePitchFromSpeed(double speed) {
  // speed range: 0-20 px/frame (tipico 0-10)
  const double minPitch = 0.7;  // Movimento lento = grave
  const double maxPitch = 1.5;  // Movimento veloce = acuto

  // Mappa lineare
  double normalized = (speed / 20.0).clamp(0.0, 1.0);

  return minPitch + (normalized * (maxPitch - minPitch));
}
```

**Pan (-1.0 ↔ +1.0):**
```dart
double calculatePanFromDirection(String direction) {
  const double panStrength = 0.8; // 0.0 = no pan, 1.0 = full pan

  switch (direction.toUpperCase()) {
    case 'LEFT':
      return -panStrength;  // Suono a sinistra
    case 'RIGHT':
      return panStrength;   // Suono a destra
    case 'UP':
    case 'DOWN':
      return 0.0;           // Centro (no pan)
    default:
      return 0.0;
  }
}
```

---

## 🏗️ IMPLEMENTAZIONE FLUTTER

### FASE 1: Dependencies Setup

**File:** `pubspec.yaml`

```yaml
dependencies:
  # Audio engine (scelta consigliata)
  just_audio: ^0.9.36         # Controllo granulare (pitch, pan, volume)

  # Alternative (più semplice ma meno controllo)
  # audioplayers: ^6.0.0

flutter:
  assets:
    - assets/sounds/jedi/
    - assets/sounds/sith/
    - assets/sounds/kyber_crystal/
    - assets/sounds/unstable/
    - assets/sounds/custom_1/
```

---

### FASE 2: Sound Pack Registry

**Nuovo file:** `lib/models/sound_pack.dart`

```dart
/// Definizione di un Sound Pack
class SoundPack {
  final String id;
  final String name;
  final String description;
  final String assetPath;       // Base path per gli asset
  final SoundPackType type;

  const SoundPack({
    required this.id,
    required this.name,
    required this.description,
    required this.assetPath,
    required this.type,
  });

  // Getters per file audio
  String get humBasePath => '$assetPath/hum_base.wav';
  String get ignitionPath => '$assetPath/ignition.wav';
  String get clashPath => '$assetPath/clash.wav';
  String get retractPath => '$assetPath/retract.wav';
}

enum SoundPackType {
  builtin,    // Predefiniti (jedi, sith)
  custom,     // Caricati dall'utente
}

/// Registry di tutti i sound pack disponibili
class SoundPackRegistry {
  static const List<SoundPack> availablePacks = [
    SoundPack(
      id: 'jedi',
      name: 'Jedi Classic',
      description: 'Suono tradizionale della lama Jedi',
      assetPath: 'assets/sounds/jedi',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'sith',
      name: 'Sith Dark Side',
      description: 'Suono più grave e minaccioso',
      assetPath: 'assets/sounds/sith',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'kyber_crystal',
      name: 'Kyber Crystal',
      description: 'Suono cristallino e puro',
      assetPath: 'assets/sounds/kyber_crystal',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'unstable',
      name: 'Unstable Blade',
      description: 'Stile Kylo Ren - crackling instabile',
      assetPath: 'assets/sounds/unstable',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'custom_1',
      name: 'Custom Pack 1',
      description: 'Suoni personalizzati',
      assetPath: 'assets/sounds/custom_1',
      type: SoundPackType.custom,
    ),
  ];

  static SoundPack? getPackById(String id) {
    try {
      return availablePacks.firstWhere((pack) => pack.id == id);
    } catch (e) {
      return null;
    }
  }
}
```

---

### FASE 3: Audio Service (Core Engine)

**Nuovo file:** `lib/services/audio_service.dart`

```dart
import 'package:just_audio/just_audio.dart';
import '../models/sound_pack.dart';

class AudioService {
  // Audio players pool
  late AudioPlayer _humPlayer;       // Loop continuo
  late AudioPlayer _swingPlayer;     // Swing modulato (stessa traccia hum)
  late AudioPlayer _eventPlayer;     // One-shot events (ignition, clash, retract)

  // State
  SoundPack? _currentPack;
  bool _soundsEnabled = true;
  double _masterVolume = 0.8;
  bool _humPlaying = false;

  // Swing debouncing
  DateTime? _lastSwingTime;
  static const Duration swingCooldown = Duration(milliseconds: 100);

  AudioService() {
    _initializePlayers();
  }

  /// Inizializza i player audio
  Future<void> _initializePlayers() async {
    _humPlayer = AudioPlayer();
    _swingPlayer = AudioPlayer();
    _eventPlayer = AudioPlayer();

    // Setup hum loop infinito
    _humPlayer.setLoopMode(LoopMode.one);
  }

  /// Imposta il sound pack corrente
  Future<void> setSoundPack(String packId) async {
    final pack = SoundPackRegistry.getPackById(packId);
    if (pack == null) {
      throw Exception('Sound pack non trovato: $packId');
    }

    _currentPack = pack;

    // Ricarica hum se già attivo
    if (_humPlaying) {
      await stopHum();
      await startHum();
    }
  }

  /// Abilita/disabilita suoni
  void setSoundsEnabled(bool enabled) {
    _soundsEnabled = enabled;
    if (!enabled) {
      stopHum();
    }
  }

  /// Imposta volume master
  void setMasterVolume(double volume) {
    _masterVolume = volume.clamp(0.0, 1.0);
    _updateHumVolume();
  }

  // ════════════════════════════════════════════════════════════
  // BLADE STATE SOUNDS
  // ════════════════════════════════════════════════════════════

  /// Suona accensione lama
  Future<void> playIgnition() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      await _eventPlayer.setAsset(_currentPack!.ignitionPath);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      await _eventPlayer.play();
    } catch (e) {
      print('[AudioService] Errore playIgnition: $e');
    }
  }

  /// Avvia hum continuo
  Future<void> startHum() async {
    if (!_soundsEnabled || _currentPack == null || _humPlaying) return;

    try {
      await _humPlayer.setAsset(_currentPack!.humBasePath);
      await _humPlayer.setVolume(_masterVolume);
      await _humPlayer.play();
      _humPlaying = true;
    } catch (e) {
      print('[AudioService] Errore startHum: $e');
    }
  }

  /// Ferma hum continuo
  Future<void> stopHum() async {
    if (!_humPlaying) return;

    try {
      await _humPlayer.stop();
      _humPlaying = false;
    } catch (e) {
      print('[AudioService] Errore stopHum: $e');
    }
  }

  /// Suona spegnimento lama
  Future<void> playRetract() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      await _eventPlayer.setAsset(_currentPack!.retractPath);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      await _eventPlayer.play();
    } catch (e) {
      print('[AudioService] Errore playRetract: $e');
    }
  }

  /// Suona colpo
  Future<void> playClash() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      await _eventPlayer.setAsset(_currentPack!.clashPath);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      await _eventPlayer.play();
    } catch (e) {
      print('[AudioService] Errore playClash: $e');
    }
  }

  // ════════════════════════════════════════════════════════════
  // SWING MODULATO (STESSA TRACCIA HUM DISTORTA)
  // ════════════════════════════════════════════════════════════

  /// Suona swing modulato
  /// IMPORTANTE: usa hum_base.wav con distorsione parametrica
  Future<void> playSwing({
    required List<int> perturbationGrid,
    required double speed,
    required String direction,
  }) async {
    if (!_soundsEnabled || _currentPack == null) return;

    // Debouncing per evitare spam
    final now = DateTime.now();
    if (_lastSwingTime != null &&
        now.difference(_lastSwingTime!) < swingCooldown) {
      return;
    }
    _lastSwingTime = now;

    try {
      // Calcola parametri modulazione
      final volume = _calculateVolumeFromGrid(perturbationGrid);
      final pitch = _calculatePitchFromSpeed(speed);
      final pan = _calculatePanFromDirection(direction);

      // Skip se volume troppo basso
      if (volume < 0.05) return;

      // Setup swing player (STESSA TRACCIA HUM)
      await _swingPlayer.setAsset(_currentPack!.humBasePath);
      await _swingPlayer.setVolume(volume * _masterVolume);
      await _swingPlayer.setSpeed(pitch);  // Pitch shift
      await _swingPlayer.setBalance(pan);   // Stereo pan

      // Play one-shot
      await _swingPlayer.seek(Duration.zero);
      await _swingPlayer.play();

    } catch (e) {
      print('[AudioService] Errore playSwing: $e');
    }
  }

  // ════════════════════════════════════════════════════════════
  // CALCOLI MODULAZIONE
  // ════════════════════════════════════════════════════════════

  double _calculateVolumeFromGrid(List<int> grid) {
    if (grid.isEmpty) return 0.0;

    // Somma energia totale griglia 8x8
    int totalEnergy = grid.reduce((a, b) => a + b);

    // Normalizza: max = 64 celle × 255 = 16320
    double normalized = totalEnergy / 16320.0;

    // Gamma correction (0.7) per boost valori medi
    double curved = pow(normalized, 0.7);

    return curved.clamp(0.0, 1.0);
  }

  double _calculatePitchFromSpeed(double speed) {
    const double minPitch = 0.7;  // Lento = grave
    const double maxPitch = 1.5;  // Veloce = acuto

    // Mappa lineare da speed (0-20 px/frame)
    double normalized = (speed / 20.0).clamp(0.0, 1.0);

    return minPitch + (normalized * (maxPitch - minPitch));
  }

  double _calculatePanFromDirection(String direction) {
    const double panStrength = 0.8;

    switch (direction.toUpperCase()) {
      case 'LEFT':
        return -panStrength;
      case 'RIGHT':
        return panStrength;
      case 'UP':
      case 'DOWN':
      default:
        return 0.0;
    }
  }

  void _updateHumVolume() {
    if (_humPlaying) {
      _humPlayer.setVolume(_masterVolume);
    }
  }

  /// Cleanup
  void dispose() {
    _humPlayer.dispose();
    _swingPlayer.dispose();
    _eventPlayer.dispose();
  }
}
```

---

### FASE 4: Audio Provider (State Management)

**Nuovo file:** `lib/providers/audio_provider.dart`

```dart
import 'package:flutter/foundation.dart';
import '../services/audio_service.dart';
import '../models/sound_pack.dart';
import '../models/motion_state.dart';

class AudioProvider extends ChangeNotifier {
  final AudioService _audioService = AudioService();

  // Settings
  bool _soundsEnabled = true;
  double _masterVolume = 0.8;
  String _currentSoundPackId = 'jedi';
  bool _bladeOn = false;

  // Swing parameters
  double _swingMinIntensity = 10.0;

  // Getters
  bool get soundsEnabled => _soundsEnabled;
  double get masterVolume => _masterVolume;
  String get currentSoundPackId => _currentSoundPackId;
  bool get bladeOn => _bladeOn;
  List<SoundPack> get availableSoundPacks => SoundPackRegistry.availablePacks;
  SoundPack? get currentSoundPack => SoundPackRegistry.getPackById(_currentSoundPackId);

  AudioProvider() {
    _initialize();
  }

  Future<void> _initialize() async {
    await _audioService.setSoundPack(_currentSoundPackId);
  }

  // ══════════════════════════════════════════════════════════
  // SETTINGS
  // ══════════════════════════════════════════════════════════

  Future<void> setSoundsEnabled(bool enabled) async {
    _soundsEnabled = enabled;
    _audioService.setSoundsEnabled(enabled);

    if (!enabled && _bladeOn) {
      await _audioService.stopHum();
    } else if (enabled && _bladeOn) {
      await _audioService.startHum();
    }

    notifyListeners();
  }

  Future<void> setMasterVolume(double volume) async {
    _masterVolume = volume.clamp(0.0, 1.0);
    _audioService.setMasterVolume(_masterVolume);
    notifyListeners();
  }

  Future<void> setSoundPack(String packId) async {
    _currentSoundPackId = packId;
    await _audioService.setSoundPack(packId);
    notifyListeners();
  }

  void setSwingMinIntensity(double intensity) {
    _swingMinIntensity = intensity.clamp(0.0, 255.0);
    notifyListeners();
  }

  // ══════════════════════════════════════════════════════════
  // GESTURE HANDLERS
  // ══════════════════════════════════════════════════════════

  Future<void> handleGesture(String gesture) async {
    if (!_soundsEnabled) return;

    final gestureType = gesture.toLowerCase();

    switch (gestureType) {
      case 'ignition':
        await _audioService.playIgnition();
        await _audioService.startHum();
        _bladeOn = true;
        break;

      case 'retract':
        await _audioService.stopHum();
        await _audioService.playRetract();
        _bladeOn = false;
        break;

      case 'clash':
        await _audioService.playClash();
        break;
    }

    notifyListeners();
  }

  // ══════════════════════════════════════════════════════════
  // MOTION-REACTIVE SWING
  // ══════════════════════════════════════════════════════════

  Future<void> updateSwing(MotionState state) async {
    if (!_soundsEnabled || !_bladeOn) return;

    // Solo se c'è movimento sopra threshold
    if (!state.motionDetected || state.intensity < _swingMinIntensity) {
      return;
    }

    // Griglia perturbazione (64 valori)
    final grid = state.perturbationGrid ?? [];
    if (grid.isEmpty) return;

    await _audioService.playSwing(
      perturbationGrid: grid,
      speed: state.speed,
      direction: state.direction,
    );
  }

  @override
  void dispose() {
    _audioService.dispose();
    super.dispose();
  }
}
```

---

### FASE 5: Integrazione BLE → Audio

**File da modificare:** `lib/providers/motion_provider.dart`

```dart
import 'audio_provider.dart';

class MotionProvider extends ChangeNotifier {
  // ... codice esistente ...

  StreamSubscription? _motionEventsSub;
  StreamSubscription? _motionStateSub;

  /// Collega eventi BLE agli audio triggers
  void setupAudioIntegration(AudioProvider audioProvider) {
    // Eventi gesture (IGNITION, RETRACT, CLASH)
    _motionEventsSub = motionService.motionEventStream.listen((event) {
      final gesture = event.gesture?.toLowerCase();
      if (gesture != null && gesture != 'none') {
        audioProvider.handleGesture(gesture);
      }
    });

    // Stato continuo per swing reattivo
    _motionStateSub = motionService.motionStateStream.listen((state) {
      audioProvider.updateSwing(state);
    });
  }

  @override
  void dispose() {
    _motionEventsSub?.cancel();
    _motionStateSub?.cancel();
    super.dispose();
  }
}
```

---

### FASE 6: UI Controls (Motion Tab)

**File da modificare:** `lib/screens/tabs/motion_tab.dart`

```dart
import '../../providers/audio_provider.dart';
import 'package:provider/provider.dart';

// Nella build() del Motion Tab
Widget build(BuildContext context) {
  final audioProvider = Provider.of<AudioProvider>(context);

  return ListView(
    children: [
      // ... altri widget esistenti ...

      // ═══════════════════════════════════════════════════
      // SEZIONE AUDIO
      // ═══════════════════════════════════════════════════
      Card(
        margin: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Header
            Padding(
              padding: EdgeInsets.all(16),
              child: Text(
                'Audio Settings',
                style: Theme.of(context).textTheme.titleLarge,
              ),
            ),

            Divider(),

            // Enable Sounds
            SwitchListTile(
              title: Text('Enable Sounds'),
              subtitle: Text('Master audio switch'),
              value: audioProvider.soundsEnabled,
              onChanged: (value) => audioProvider.setSoundsEnabled(value),
            ),

            // Master Volume
            ListTile(
              title: Text('Master Volume'),
              subtitle: Column(
                children: [
                  Slider(
                    value: audioProvider.masterVolume,
                    min: 0.0,
                    max: 1.0,
                    divisions: 20,
                    label: '${(audioProvider.masterVolume * 100).toInt()}%',
                    onChanged: audioProvider.soundsEnabled
                      ? (value) => audioProvider.setMasterVolume(value)
                      : null,
                  ),
                  Text('${(audioProvider.masterVolume * 100).toInt()}%'),
                ],
              ),
            ),

            Divider(),

            // Sound Pack Selection
            ListTile(
              title: Text('Sound Pack'),
              subtitle: Text(audioProvider.currentSoundPack?.name ?? 'None'),
            ),

            // Grid di sound pack
            Padding(
              padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
              child: Wrap(
                spacing: 8,
                runSpacing: 8,
                children: audioProvider.availableSoundPacks.map((pack) {
                  final isSelected = pack.id == audioProvider.currentSoundPackId;

                  return ChoiceChip(
                    label: Text(pack.name),
                    selected: isSelected,
                    onSelected: audioProvider.soundsEnabled
                      ? (selected) {
                          if (selected) {
                            audioProvider.setSoundPack(pack.id);
                          }
                        }
                      : null,
                  );
                }).toList(),
              ),
            ),

            // Descrizione pack selezionato
            if (audioProvider.currentSoundPack != null)
              Padding(
                padding: EdgeInsets.all(16),
                child: Text(
                  audioProvider.currentSoundPack!.description,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),

            Divider(),

            // Blade Status Indicator
            ListTile(
              leading: Icon(
                audioProvider.bladeOn ? Icons.lightbulb : Icons.lightbulb_outline,
                color: audioProvider.bladeOn ? Colors.green : Colors.grey,
              ),
              title: Text('Blade Status'),
              subtitle: Text(audioProvider.bladeOn ? 'ON (Hum active)' : 'OFF'),
            ),
          ],
        ),
      ),
    ],
  );
}
```

---

## 🎮 COMPORTAMENTO SISTEMA

### Workflow Completo

```
┌─────────────────────────────────────────────────────────┐
│  1. UTENTE ACCENDE LAMA (gesture IGNITION)             │
├─────────────────────────────────────────────────────────┤
│  ESP32 → BLE Event: { gesture: "ignition" }            │
│  Flutter → AudioProvider.handleGesture("ignition")     │
│  AudioService:                                          │
│    - Play ignition.wav (one-shot)                      │
│    - Start hum_base.wav (loop infinito)                │
│  Blade Status: ON                                       │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  2. UTENTE MUOVE LA SPADA (motion detected)            │
├─────────────────────────────────────────────────────────┤
│  ESP32 → BLE Status: {                                 │
│    intensity: 120,                                      │
│    speed: 8.5,                                          │
│    direction: "RIGHT",                                  │
│    perturbationGrid: [0,5,12,...]  // 64 valori        │
│  }                                                      │
│  Flutter → AudioProvider.updateSwing(state)            │
│  AudioService:                                          │
│    - Volume = f(perturbationGrid) = 0.65               │
│    - Pitch = f(speed) = 1.2x                           │
│    - Pan = f(direction) = +0.8 (destra)                │
│    - Play hum_base.wav (STESSA TRACCIA) distorta      │
│  Hum continuo: CONTINUA in background                  │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  3. COLPO (gesture CLASH)                               │
├─────────────────────────────────────────────────────────┤
│  ESP32 → BLE Event: { gesture: "clash" }               │
│  Flutter → AudioProvider.handleGesture("clash")        │
│  AudioService:                                          │
│    - Play clash.wav (one-shot)                         │
│  Hum continuo: CONTINUA in background                  │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  4. UTENTE SPEGNE LAMA (gesture RETRACT)                │
├─────────────────────────────────────────────────────────┤
│  ESP32 → BLE Event: { gesture: "retract" }             │
│  Flutter → AudioProvider.handleGesture("retract")      │
│  AudioService:                                          │
│    - Stop hum_base.wav (loop)                          │
│    - Play retract.wav (one-shot)                       │
│  Blade Status: OFF                                      │
└─────────────────────────────────────────────────────────┘
```

---

## 🔧 PARAMETRI DI TUNING

### Swing Modulazione

```dart
// In AudioService o AudioProvider

// Volume curve
static const double volumeGamma = 0.7;  // 0.5 = aggressive, 1.0 = linear

// Pitch range
static const double minPitch = 0.7;     // Più basso = più drammatico
static const double maxPitch = 1.5;     // Più alto = più acuto

// Pan strength
static const double panStrength = 0.8;  // 0.0 = no stereo, 1.0 = full

// Swing trigger
static const double swingMinIntensity = 10.0;  // Threshold per trigger
static const Duration swingCooldown = Duration(milliseconds: 100);
```

### HUM Loop

```dart
// Volume hum costante
static const double humBaseVolume = 0.8;  // 0.0-1.0
```

---

## 📊 VERIFICA FIRMWARE BLE

### Controlla che perturbationGrid sia inviato

**File da verificare:** `src/BLEMotionService.cpp`

Cerca nel metodo `_getStatusJson()` (linea ~231):

```cpp
String BLEMotionService::_getStatusJson() {
    JsonDocument doc;

    // ... altro codice ...

    // VERIFICA CHE QUESTO SIA PRESENTE:
    JsonArray gridArray = doc["perturbationGrid"].to<JsonArray>();
    for (uint8_t row = 0; row < OpticalFlowDetector::GRID_ROWS; row++) {
        for (uint8_t col = 0; col < OpticalFlowDetector::GRID_COLS; col++) {
            // Ottieni valore da MotionProcessor
            gridArray.add(perturbationValue);
        }
    }

    // ... altro codice ...
}
```

**Se NON presente, il firmware NON sta inviando la griglia.**

---

## 📝 CHECKLIST IMPLEMENTAZIONE

### Preparazione Asset
- [ ] Trova/crea 4 file audio per "jedi" pack:
  - [ ] `hum_base.wav` (2-5s loop perfetto)
  - [ ] `ignition.wav` (1-2s)
  - [ ] `clash.wav` (0.3-0.5s)
  - [ ] `retract.wav` (1-2s)
- [ ] (Opzionale) Prepara pack "sith"
- [ ] Verifica loop point di `hum_base.wav` (no click)

### Flutter Implementation
- [ ] **Fase 1:** Aggiungi `just_audio` in pubspec.yaml
- [ ] **Fase 1:** Crea cartella `assets/sounds/` con pack
- [ ] **Fase 2:** Implementa `SoundPack` model
- [ ] **Fase 2:** Implementa `SoundPackRegistry`
- [ ] **Fase 3:** Implementa `AudioService` completo
- [ ] **Fase 4:** Implementa `AudioProvider`
- [ ] **Fase 5:** Collega BLE → AudioProvider
- [ ] **Fase 6:** Aggiungi UI controls in Motion Tab
- [ ] **Fase 7:** Test gesture → sound workflow

### Firmware (Verifica)
- [ ] Verifica che `perturbationGrid` sia nel JSON BLE
- [ ] (Se manca) Aggiungi serializzazione griglia 8x8

### Testing & Tuning
- [ ] Test IGNITION → suona ignition + hum loop
- [ ] Test movimento → swing modulato (volume/pitch/pan)
- [ ] Test CLASH → suona clash (hum continua)
- [ ] Test RETRACT → hum stop + retract sound
- [ ] Tuning parametri (volume curve, pitch range, pan)

---

## 🚀 ORDINE DI IMPLEMENTAZIONE

1. **Prepara asset audio** (jedi pack base)
2. **Setup dependencies** (`just_audio`)
3. **Implementa models** (`SoundPack`, `SoundPackRegistry`)
4. **AudioService foundation** (API base)
5. **Test gesture sounds** (IGNITION/RETRACT/CLASH)
6. **Implementa HUM loop**
7. **Implementa SWING modulato** (CHIAVE!)
8. **AudioProvider** (state management)
9. **Integra BLE events**
10. **UI controls** (Motion Tab)
11. **Fine tuning**

---

## 💡 NOTE TECNICHE

### Perché HUM e SWING sono la stessa traccia?

1. **Coerenza sonora:** lo swing è il "movimento" del hum
2. **Riduzione asset:** serve solo 1 file invece di 2
3. **Realismo:** nella realtà, il suono dello swing è il hum distorto dal movimento Doppler
4. **Flessibilità:** cambiando `hum_base.wav` cambiano automaticamente entrambi

### Alternative a just_audio

Se `just_audio` ha problemi:
- **audioplayers:** più semplice ma meno controllo (no pitch shift nativo)
- **flutter_soloud:** engine avanzato con effetti DSP
- **dart_vlc:** solo desktop (no mobile)

### Limitazioni Android/iOS

- **Latency:** Android può avere 50-100ms, iOS è meglio (~20ms)
- **Pitch shift:** alcuni device hanno limitazioni qualità
- **Background playback:** richiede permessi extra

---

## 📚 RISORSE AUDIO

### Dove trovare suoni lightsaber

1. **Freesound.org** (license CC)
2. **Star Wars Sound Effects** (unofficial packs)
3. **Synthetizzare in DAW** (Reaper, Ableton)
4. **Online generators:** lightsaber sound generator tools

### Come creare loop perfetto

1. Registra/trova suono hum lungo (5-10s)
2. Trova zero-crossing per loop point
3. Applica crossfade 50-100ms
4. Verifica in loop che non si senta "click"

### Tool consigliati

- **Audacity** (free) - editing + loop tuning
- **Reaper** (trial) - DAW completo
- **RX** (paid) - denoising professionale

---

---

## 🐛 BUG FIX: DESINCRONIZZAZIONE AUDIO-ANIMAZIONE (30 Dic 2025)

### 🔍 PROBLEMA RILEVATO

L'audio (ignition/retract) era **completamente desincronizzato** dall'animazione della lama nell'UI:
- **Sintomo:** Ignition iniziava e veniva subito interrotto da retract
- **Causa root:** AudioProvider reagiva a **OGNI cambio di stato** dal firmware, anche quelli intermedi
- **Workflow errato:**
  ```
  User tap → Firmware: off → igniting → on (in <100ms)
              ↓          ↓          ↓
            Audio:  START   STOP    START  ❌ CAOS!
  ```

### 🔬 ANALISI DELL'ARCHITETTURA

**Il sistema ha 2 componenti che gestivano le animazioni indipendentemente:**

1. **LightsaberWidget** ([lightsaber_widget.dart:86-176](../AppMobile/flutter_led_saber/lib/widgets/lightsaber_widget.dart#L86-L176))
   - Usa `AnimationController` con timing precisi:
     - **Ignition:** 1500ms (forward)
     - **Retraction:** 1000ms (reverse)
   - **Ignora stati intermedi** se l'animazione è già in corso
   - Logica (`_updateAnimationState`):
     ```dart
     case 'igniting':
       if (!_ignitionController.isAnimating && !_ignitionController.isCompleted) {
         _ignitionController.forward();  // Solo se non già in corso
       }
     ```

2. **AudioProvider** (VECCHIO - BUGGY)
   - Reagiva a **tutti gli stati** ricevuti dal firmware
   - **Nessun timer**, nessuna gestione animazioni
   - Logica buggy:
     ```dart
     case 'igniting': playIgnition() + startHum()
     case 'on': if (from == 'igniting') { skip } ❌ NON FUNZIONAVA!
     case 'retracting': stopHum() + playRetract()
     case 'off': stopHum()
     ```

**Risultato:** Il firmware mandava `off→igniting→on→retracting→off` in pochi millisecondi, e l'audio eseguiva TUTTE le azioni senza rispettare i 1500ms/1000ms dell'animazione visiva.

### ✅ SOLUZIONE IMPLEMENTATA

**Principio:** **L'audio deve seguire ESATTAMENTE il timing dell'animazione del widget**

Refactor completo di `audio_provider.dart` per **simulare l'AnimationController** del widget:

#### 1. Aggiunti Timer per Sincronizzazione

```dart
// Animation timing - MUST match LightsaberWidget animation durations
static const _ignitionDuration = Duration(milliseconds: 1500);
static const _retractDuration = Duration(milliseconds: 1000);

// State tracking
String? _lastBladeState;
String? _targetBladeState;      // Stato finale desiderato
Timer? _animationTimer;         // Timer che simula AnimationController
bool _isAnimating = false;      // Flag animazione in corso
```

#### 2. Logica di Transizione Identica al Widget

```dart
void _handleStateTransition(String? from, String to, bool isFirstSync) {
  switch (to) {
    case 'igniting':
      // Analogo a: if (!_ignitionController.isAnimating && !_ignitionController.isCompleted)
      if (!_isAnimating && from != 'on') {
        _startIgnitionAnimation();  // ✅ IGNORA se già in corso
      }
      break;

    case 'on':
      if (from == 'igniting' && _isAnimating && _targetBladeState == 'on') {
        // Firmware dice "on" ma stiamo ancora animando → IGNORA
        print('🕒 On ricevuto ma ignition ancora in corso, continuo animazione');
      }
      break;

    // ... stesso pattern per retracting/off
  }
}
```

#### 3. Animazioni con Timer Precisi

```dart
void _startIgnitionAnimation() {
  print('[AudioProvider] 🔥 Ignition START (1500ms)');

  _isAnimating = true;
  _targetBladeState = 'on';
  _animationTimer?.cancel();

  // Audio immediato (come nel widget)
  _audioService.playIgnition();
  Future.delayed(const Duration(milliseconds: 50), () {
    if (_targetBladeState == 'on') {
      _audioService.startHum();
    }
  });

  // Timer che SIMULA la durata dell'AnimationController
  _animationTimer = Timer(_ignitionDuration, () {
    print('[AudioProvider] 🔥 Ignition COMPLETE');
    _isAnimating = false;
    // Verifica finale stato
    if (_targetBladeState == 'on' && !_audioService.isHumPlaying) {
      _audioService.startHum();  // Safety fallback
    }
  });
}

void _startRetractAnimation() {
  print('[AudioProvider] 🔴 Retract START (1000ms)');

  _isAnimating = true;
  _targetBladeState = 'off';
  _animationTimer?.cancel();

  // Audio immediato
  _audioService.stopHum();
  _audioService.playRetract();

  // Timer 1000ms (come AnimationController reverse)
  _animationTimer = Timer(_retractDuration, () {
    print('[AudioProvider] 🔴 Retract COMPLETE');
    _isAnimating = false;
    _ensureAudioStopped();  // Safety cleanup
  });
}
```

### 📊 RISULTATO FINALE

**Workflow corretto ora:**
```
User tap → Firmware: off → igniting → on
            ↓
          Audio: START Ignition (1500ms timer)
                 ↓ (50ms)
                 START Hum
                 ↓ (firmware dice "on" dopo 100ms)
                 IGNORA (timer ancora attivo)
                 ↓ (1500ms totali)
                 COMPLETE ✅
```

**Stati ignorati correttamente:**
- `igniting` ricevuto durante animazione → SKIP
- `on` ricevuto durante ignition → SKIP (timer gestisce)
- `retracting` ricevuto durante animazione → SKIP
- `off` ricevuto durante retract → SKIP (timer gestisce)

### 🎯 COMPORTAMENTO ATTESO

1. **User accende lama:**
   - Firmware: `off → igniting`
   - Audio: Parte ignition.wav + hum (loop)
   - Animazione widget: 0.0 → 1.0 in 1500ms
   - Audio timer: 1500ms (sincronizzato)
   - Firmware: `igniting → on` (dopo ~100ms)
   - Audio: **IGNORA** (sta ancora animando)
   - Timer scade: `_isAnimating = false` ✅

2. **User spegne lama:**
   - Firmware: `on → retracting`
   - Audio: Stop hum + parte retract.wav
   - Animazione widget: 1.0 → 0.0 in 1000ms
   - Audio timer: 1000ms (sincronizzato)
   - Firmware: `retracting → off` (dopo ~100ms)
   - Audio: **IGNORA** (sta ancora animando)
   - Timer scade: `_isAnimating = false` ✅

### 📁 FILE MODIFICATI

- [audio_provider.dart](../AppMobile/flutter_led_saber/lib/providers/audio_provider.dart)
  - Aggiunti: `_ignitionDuration`, `_retractDuration`, `_animationTimer`, `_isAnimating`, `_targetBladeState`
  - Riscritto: `syncWithBladeState()` → `_handleStateTransition()`
  - Nuovi metodi: `_startIgnitionAnimation()`, `_startRetractAnimation()`, `_ensureAudioStopped()`
  - Aggiunto cleanup: `dispose()` cancella timer

### 🧪 TEST ESEGUITO

- ✅ Build APK: `app-release-audio-widget-sync.apk`
- ✅ Timing verificati: 1500ms ignition, 1000ms retract
- ✅ Stati intermedi ignorati correttamente
- ✅ Nessuna desincronizzazione rilevata

### 💡 LESSON LEARNED

**Mai fidarsi degli stati del firmware per gestire animazioni UI!**

Il firmware può mandare stati intermedi molto rapidamente (es. `igniting → on` in 100ms) perché la sua logica è diversa dall'UI. L'audio e le animazioni visive devono essere **sincronizzate tra loro**, non con il firmware.

**Pattern corretto:**
1. Widget UI: `AnimationController` con timing precisi
2. Audio: `Timer` con **STESSI timing** del widget
3. Firmware: Source of truth per lo **stato logico**, non per il timing

---

## ✅ FINE ROADMAP

Quando tutti i task sono completati, avrai:
- ✅ Sistema audio multi-pack
- ✅ HUM continuo + SWING modulato (stessa traccia)
- ✅ Gesture sounds (ignition, retract, clash)
- ✅ Volume/pitch/pan reattivi alla griglia 8x8
- ✅ UI completa per controllo audio
- ✅ **Audio sincronizzato perfettamente con animazione lama** (fix 30 Dic 2025)

**Prossimo step:** Test approfonditi e fine tuning parametri swing
