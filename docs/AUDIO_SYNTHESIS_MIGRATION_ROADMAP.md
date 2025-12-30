# 🎵 ROADMAP: Migrazione a Audio Sintetizzato Ibrido

**Data creazione:** 2025-12-30
**Obiettivo:** Migrare da file audio pre-registrati a generazione sintetizzata real-time con architettura ibrida
**Stima totale:** 5-7 giorni lavorativi

---

## 📋 INDICE

1. [Overview Architettura](#overview-architettura)
2. [Fase 0: Preparazione](#fase-0-preparazione)
3. [Fase 1: Setup Dependencies](#fase-1-setup-dependencies)
4. [Fase 2: Generatore Texture Layer](#fase-2-generatore-texture-layer)
5. [Fase 3: SynthAudioService Core](#fase-3-synthaudioservice-core)
6. [Fase 4: Integrazione con Provider](#fase-4-integrazione-con-provider)
7. [Fase 5: Testing & Tuning](#fase-5-testing--tuning)
8. [Fase 6: Cleanup & Ottimizzazione](#fase-6-cleanup--ottimizzazione)
9. [Fase 7: Documentazione](#fase-7-documentazione)
10. [Rollback Plan](#rollback-plan)

---

## 🎯 OVERVIEW ARCHITETTURA

### Architettura Attuale (File-Based)

```
┌─────────────────────────────────────────┐
│  AudioService (just_audio)              │
├─────────────────────────────────────────┤
│  ├─ _humPlayer      → hum_base.wav      │
│  ├─ _swingPlayer    → hum_base.wav      │
│  ├─ _eventPlayer    → clash.wav         │
│  └─ _retractPlayer  → retract.wav       │
└─────────────────────────────────────────┘
         ↓
    Problemi:
    ❌ Latency 50-100ms
    ❌ Race conditions
    ❌ Audio ducking complesso
    ❌ 5 livelli watchdog
    ❌ Pitch shift device-dependent
```

### Architettura Target (Synth Hybrid)

```
┌──────────────────────────────────────────────────┐
│  SynthAudioService (flutter_soloud)              │
├──────────────────────────────────────────────────┤
│                                                  │
│  OSCILLATORE PRINCIPALE (sempre attivo)          │
│  ├─ HUM base       → sine wave 110Hz            │
│  ├─ SWING          → modulazione real-time       │
│  ├─ IGNITION       → freq sweep 20→110 Hz       │
│  └─ RETRACT        → freq sweep 110→15 Hz       │
│                                                  │
│  TEXTURE LAYER (opzionali, temporanei)           │
│  ├─ Ignition burst → noise burst 300ms          │
│  ├─ Retract tail   → reverb lungo               │
│  └─ Clash impact   → sawtooth percussivo        │
│                                                  │
└──────────────────────────────────────────────────┘
         ↓
    Vantaggi:
    ✅ Latency < 10ms
    ✅ Zero race conditions
    ✅ Modulazione nativa
    ✅ Architettura semplice
    ✅ Pitch shift perfetto
```

---

## ⚙️ FASE 0: PREPARAZIONE

**Durata:** 0.5 giorni
**Obiettivo:** Backup e analisi codebase esistente

### Task 0.1: Backup Completo

```bash
# Crea branch per migrazione
cd /home/reder/Documenti/PlatformIO/Projects/ledSaber
git checkout -b feature/audio-synthesis-migration
git add .
git commit -m "Backup before audio synthesis migration"

# Tag versione corrente
git tag v1.0.0-pre-synthesis
```

### Task 0.2: Documentazione Stato Attuale

**File da documentare:**
- [x] `/lib/services/audio_service.dart` - implementazione attuale
- [x] `/lib/providers/audio_provider.dart` - state management
- [x] `/lib/models/sound_pack.dart` - sound pack registry
- [x] Parametri audio attuali (volume, pitch range, etc.)

**Output:** Documento `AUDIO_SERVICE_CURRENT_STATE.md`

### Task 0.3: Test Suite Baseline

Crea test per comportamento attuale (riferimento per regression):

```dart
// test/audio_service_baseline_test.dart
void main() {
  test('HUM starts correctly', () async { ... });
  test('SWING modulates volume/pitch', () async { ... });
  test('IGNITION plays with ducking', () async { ... });
  test('RETRACT stops HUM correctly', () async { ... });
}
```

**Esegui e salva risultati:**
```bash
cd AppMobile/flutter_led_saber
flutter test > test_results_baseline.txt
```

### ✅ Checklist Fase 0

- [ ] Branch `feature/audio-synthesis-migration` creato
- [ ] Tag `v1.0.0-pre-synthesis` applicato
- [ ] Documentazione stato attuale completata
- [ ] Test baseline eseguiti e salvati
- [ ] File audio assets inventariati

---

## 📦 FASE 1: SETUP DEPENDENCIES

**Durata:** 0.5 giorni
**Obiettivo:** Installare flutter_soloud e configurare ambiente

### Task 1.1: Aggiorna `pubspec.yaml`

**File:** `AppMobile/flutter_led_saber/pubspec.yaml`

```yaml
dependencies:
  # ... dipendenze esistenti ...

  # Audio - NUOVO: Synthesis engine
  flutter_soloud: ^2.0.0  # Generazione audio real-time

  # Audio - DEPRECATO: Mantenere temporaneamente per rollback
  just_audio: ^0.9.36     # Da rimuovere in Fase 6
```

**Comando:**
```bash
cd AppMobile/flutter_led_saber
flutter pub add flutter_soloud
flutter pub get
```

### Task 1.2: Test Installazione

**Crea file test:** `lib/test_soloud.dart`

```dart
import 'package:flutter_soloud/flutter_soloud.dart';

Future<void> testSoLoudInstallation() async {
  print('[TEST] Inizializzando SoLoud...');

  final soloud = SoLoud.instance;
  await soloud.init();

  print('[TEST] ✓ SoLoud inizializzato');

  // Test creazione waveform
  final testWave = await soloud.loadWaveform(
    WaveForm.sine,
    superWave: false,
    scale: 1.0,
    detune: 0.0,
  );

  print('[TEST] ✓ Waveform creata');

  // Test playback
  final handle = await soloud.play(testWave, volume: 0.5);
  await Future.delayed(Duration(milliseconds: 500));
  await soloud.stop(handle);

  print('[TEST] ✓ Playback funzionante');

  await soloud.deinit();
  print('[TEST] ✓ SoLoud cleanup completato');
}
```

**Esegui test:**
```bash
flutter run -d <device> --target=lib/test_soloud.dart
```

### Task 1.3: Configura Android Permissions

**File:** `AppMobile/flutter_led_saber/android/app/src/main/AndroidManifest.xml`

Verifica permessi audio (già presenti se usi just_audio):
```xml
<uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
```

### Task 1.4: Configura iOS Permissions

**File:** `AppMobile/flutter_led_saber/ios/Runner/Info.plist`

Verifica:
```xml
<key>UIBackgroundModes</key>
<array>
  <string>audio</string>
</array>
```

### ✅ Checklist Fase 1

- [ ] `flutter_soloud` aggiunto a pubspec.yaml
- [ ] `flutter pub get` eseguito con successo
- [ ] Test installazione superato
- [ ] Permissions Android/iOS verificate
- [ ] Build test su device reale OK

---

## 🎨 FASE 2: GENERATORE TEXTURE LAYER

**Durata:** 1 giorno
**Obiettivo:** Estendere `audio_generator.py` per generare texture layers

### Task 2.1: Backup Generatore Attuale

```bash
cp tools/audio_generator.py tools/audio_generator_v1_backup.py
```

### Task 2.2: Aggiungi Classe TextureGenerator

**File:** `tools/audio_generator.py`

Aggiungi dopo la classe `LightsaberSoundGenerator`:

```python
class TextureLayerGenerator:
    """Generatore per texture layer (noise burst, reverb tail, ecc.)"""

    def __init__(self, sample_rate=44100):
        self.sample_rate = sample_rate

    # ═══════════════════════════════════════════════════════════════════
    # IGNITION TEXTURES
    # ═══════════════════════════════════════════════════════════════════

    def generate_ignition_burst(self, duration=0.3, style='jedi'):
        """
        Genera noise burst per accensione lama

        Uso: Layer addizionale durante ignition per impatto energetico

        Args:
            duration: Durata in secondi (default 0.3s = 300ms)
            style: 'jedi', 'sith', 'unstable', 'crystal'

        Returns:
            numpy array con noise burst
        """
        print(f"  Generando ignition_burst.wav ({style}, {duration}s)...")

        t = np.linspace(0, duration, int(self.sample_rate * duration))

        if style == 'jedi':
            # Noise burst pulito con banda limitata
            noise = np.random.normal(0, 0.35, len(t))

            # Filtro passa-banda per carattere "energetico"
            noise_filtered = self.apply_bandpass(noise, 400, 3000)

            # Envelope esplosivo
            envelope = np.exp(-t * 12)  # Decay rapido

            burst = noise_filtered * envelope

        elif style == 'sith':
            # Noise più denso e distorto
            noise = np.random.normal(0, 0.45, len(t))

            # Filtro più basso (più "dark")
            noise_filtered = self.apply_bandpass(noise, 200, 2000)

            # Aggiungi componente sub per "punch"
            sub = 0.3 * np.sin(2 * np.pi * 50 * t)

            envelope = np.exp(-t * 10)
            burst = (noise_filtered + sub) * envelope

        elif style == 'unstable':
            # Crackle irregolare
            noise = np.random.normal(0, 0.5, len(t))

            # Burst casuali
            impulse_mask = np.random.rand(len(noise)) > 0.95
            noise = noise * (1 + impulse_mask * 2)

            noise_filtered = self.apply_bandpass(noise, 300, 4000)

            envelope = np.exp(-t * 15) * (1 + 0.3 * np.sin(2 * np.pi * 20 * t))
            burst = noise_filtered * envelope

        elif style == 'crystal':
            # Shimmer cristallino
            noise = np.random.normal(0, 0.25, len(t))

            # Filtraggio alto per brillantezza
            noise_filtered = self.apply_highpass(noise, 800)
            noise_filtered = self.apply_lowpass(noise_filtered, 8000)

            # Modulazione cristallina
            shimmer = 1 + 0.2 * np.sin(2 * np.pi * 12 * t)

            envelope = np.exp(-t * 10)
            burst = noise_filtered * envelope * shimmer

        else:
            raise ValueError(f"Style non riconosciuto: {style}")

        # Normalizza
        return self.normalize(burst, target_amplitude=0.8)

    # ═══════════════════════════════════════════════════════════════════
    # RETRACT TEXTURES
    # ═══════════════════════════════════════════════════════════════════

    def generate_retract_tail(self, duration=1.5, style='jedi'):
        """
        Genera reverb tail per spegnimento lama

        Uso: Coda atmosferica durante retract

        Args:
            duration: Durata tail (default 1.5s)
            style: 'jedi', 'sith', 'unstable', 'crystal'
        """
        print(f"  Generando retract_tail.wav ({style}, {duration}s)...")

        t = np.linspace(0, duration, int(self.sample_rate * duration))

        # Base: tono basso decrescente
        base_freq = 80 if style == 'jedi' else 60
        tone = 0.3 * np.sin(2 * np.pi * base_freq * t)

        # Noise ambientale filtrato
        noise = np.random.normal(0, 0.15, len(t))
        noise_filtered = self.apply_lowpass(noise, 2000)

        # Envelope decay lungo
        envelope = np.exp(-t * 1.8)

        tail = (tone + noise_filtered) * envelope

        # Reverb profondo
        tail = self.apply_reverb(tail, delay_ms=200, decay=0.6, num_echoes=5)

        return self.normalize(tail, target_amplitude=0.6)

    # ═══════════════════════════════════════════════════════════════════
    # CLASH TEXTURES
    # ═══════════════════════════════════════════════════════════════════

    def generate_clash_impact(self, duration=0.4, style='jedi'):
        """
        Genera texture metallic impact per clash

        Uso: Layer percussivo per clash
        """
        print(f"  Generando clash_impact.wav ({style}, {duration}s)...")

        t = np.linspace(0, duration, int(self.sample_rate * duration))

        # Noise burst molto breve
        noise = np.random.normal(0, 0.6, len(t))
        noise_filtered = self.apply_bandpass(noise, 800, 8000)

        # Risonanze metalliche multiple
        resonances = np.zeros_like(t)
        freqs = [180, 240, 320, 480, 720]  # Armoniche metalliche

        for i, freq in enumerate(freqs):
            decay_rate = 8 + i * 3
            amplitude = 0.3 / (i + 1)
            resonances += amplitude * np.sin(2 * np.pi * freq * t) * np.exp(-t * decay_rate)

        # Envelope percussivo
        envelope = np.exp(-t * 18)

        impact = (noise_filtered * 0.5 + resonances) * envelope

        return self.normalize(impact, target_amplitude=0.95)

    # ═══════════════════════════════════════════════════════════════════
    # UTILITY METHODS
    # ═══════════════════════════════════════════════════════════════════

    def apply_bandpass(self, sig, lowcut, highcut, order=4):
        """Filtro passa-banda Butterworth"""
        from scipy import signal as sp_signal

        nyquist = self.sample_rate / 2
        low = lowcut / nyquist
        high = min(highcut / nyquist, 0.99)

        sos = sp_signal.butter(order, [low, high], btype='band', output='sos')
        return sp_signal.sosfilt(sos, sig)

    def apply_lowpass(self, sig, cutoff_freq=5000, order=4):
        """Filtro passa-basso"""
        from scipy import signal as sp_signal

        nyquist = self.sample_rate / 2
        normal_cutoff = min(cutoff_freq / nyquist, 0.99)
        sos = sp_signal.butter(order, normal_cutoff, btype='low', output='sos')
        return sp_signal.sosfilt(sos, sig)

    def apply_highpass(self, sig, cutoff_freq=50, order=2):
        """Filtro passa-alto"""
        from scipy import signal as sp_signal

        nyquist = self.sample_rate / 2
        normal_cutoff = cutoff_freq / nyquist
        sos = sp_signal.butter(order, normal_cutoff, btype='high', output='sos')
        return sp_signal.sosfilt(sos, sig)

    def apply_reverb(self, sig, delay_ms=150, decay=0.4, num_echoes=3):
        """Reverb semplice"""
        output = np.copy(sig)
        delay_samples = int(self.sample_rate * delay_ms / 1000)

        for i in range(1, num_echoes + 1):
            current_delay = delay_samples * i
            current_decay = decay ** i
            padded = np.zeros(len(sig) + current_delay)
            padded[current_delay:] = sig * current_decay
            output[:len(padded)] += padded[:len(output)]

        return output

    def normalize(self, signal, target_amplitude=0.9):
        """Normalizza segnale"""
        max_val = np.max(np.abs(signal))
        if max_val > 0:
            return signal * (target_amplitude / max_val)
        return signal
```

### Task 2.3: Integra TextureGenerator in CLI

**Modifica funzione `generate_pack()`** in `LightsaberSoundGenerator`:

```python
def generate_pack(self, pack_name, output_dir, include_textures=True):
    """
    Genera un pack completo di suoni + texture layer

    Args:
        pack_name: 'jedi', 'sith', 'unstable', 'crystal'
        output_dir: Directory output
        include_textures: Se True, genera anche texture layer
    """
    print(f"\n{'='*60}")
    print(f"Generando sound pack: {pack_name.upper()}")
    if include_textures:
        print("(Include texture layers)")
    print(f"{'='*60}\n")

    # ... codice esistente per hum/ignition/retract/clash ...

    # NUOVO: Genera texture layer
    if include_textures:
        print("\n--- Generazione Texture Layer ---\n")
        texture_gen = TextureLayerGenerator(sample_rate=self.sample_rate)

        textures = {
            'ignition_burst.wav': texture_gen.generate_ignition_burst(
                duration=0.3,
                style=pack_name
            ),
            'retract_tail.wav': texture_gen.generate_retract_tail(
                duration=1.5,
                style=pack_name
            ),
            'clash_impact.wav': texture_gen.generate_clash_impact(
                duration=0.4,
                style=pack_name
            ),
        }

        # Salva texture
        for filename, signal in textures.items():
            filepath = output_path / filename
            signal_int16 = np.int16(signal * 32767)
            wavfile.write(str(filepath), self.sample_rate, signal_int16)

            duration = len(signal) / self.sample_rate
            size_kb = filepath.stat().st_size / 1024
            print(f"  ✓ {filename:25s} - {duration:.2f}s - {size_kb:.1f}KB")
```

### Task 2.4: Aggiorna CLI Arguments

```python
def main():
    parser = argparse.ArgumentParser(
        description='Lightsaber Sound Generator - Genera suoni procedurali per spada laser'
    )

    # ... argomenti esistenti ...

    parser.add_argument(
        '--textures',
        action='store_true',
        default=True,
        help='Genera anche texture layer (default: True)'
    )

    parser.add_argument(
        '--no-textures',
        dest='textures',
        action='store_false',
        help='Non generare texture layer'
    )
```

### Task 2.5: Genera Texture per Tutti i Pack

```bash
cd /home/reder/Documenti/PlatformIO/Projects/ledSaber

# Usa virtual environment
./venv/bin/python tools/audio_generator.py \
  --pack all \
  --textures \
  --output AppMobile/flutter_led_saber/assets/sounds
```

**Output atteso:**
```
assets/sounds/jedi/
  ├─ hum_base.wav          (esistente, opzionale per fallback)
  ├─ ignition.wav          (esistente, opzionale)
  ├─ retract.wav           (esistente, opzionale)
  ├─ clash.wav             (esistente, opzionale)
  ├─ ignition_burst.wav    (NUOVO - texture)
  ├─ retract_tail.wav      (NUOVO - texture)
  └─ clash_impact.wav      (NUOVO - texture)
```

### ✅ Checklist Fase 2

- [ ] Classe `TextureLayerGenerator` implementata
- [ ] Metodi per ignition_burst, retract_tail, clash_impact
- [ ] Integrazione in `generate_pack()`
- [ ] CLI arguments `--textures` / `--no-textures`
- [ ] Texture generate per tutti i pack (jedi, sith, unstable, crystal)
- [ ] File verificati in `assets/sounds/*/`

---

## 🎵 FASE 3: SYNTHAUDIOSERVICE CORE

**Durata:** 2 giorni
**Obiettivo:** Implementare nuovo servizio audio sintetizzato

### Task 3.1: Crea File Service

**Nuovo file:** `lib/services/synth_audio_service.dart`

```dart
import 'dart:async';
import 'dart:math';
import 'package:flutter_soloud/flutter_soloud.dart';

/// Servizio audio basato su sintesi real-time (flutter_soloud)
///
/// Architettura IBRIDA:
/// - Oscillatore principale per HUM/SWING (modulazione continua)
/// - Texture layer opzionali per realismo (ignition burst, retract tail)
class SynthAudioService {
  // ══════════════════════════════════════════════════════════════
  // PROPERTIES
  // ══════════════════════════════════════════════════════════════

  final SoLoud _soloud = SoLoud.instance;

  // OSCILLATORE PRINCIPALE (HUM/SWING combinati)
  SoundHandle? _humHandle;
  AudioSource? _humSource;

  // TEXTURE LAYER (opzionali, caricati da file)
  AudioSource? _ignitionBurstSource;
  AudioSource? _retractTailSource;
  AudioSource? _clashImpactSource;

  // State
  bool _initialized = false;
  bool _soundsEnabled = true;
  bool _humPlaying = false;
  double _masterVolume = 0.8;
  String _currentPack = 'jedi';

  // Parametri modulazione correnti
  double _currentVolume = 0.8;
  double _currentPitch = 1.0;
  double _currentPan = 0.0;

  // Sound pack frequencies (Hz)
  static const Map<String, double> _packFrequencies = {
    'jedi': 110.0,
    'sith': 70.0,
    'unstable': 90.0,
    'crystal': 150.0,
  };

  // ══════════════════════════════════════════════════════════════
  // INITIALIZATION
  // ══════════════════════════════════════════════════════════════

  /// Inizializza engine audio SoLoud
  Future<void> initialize() async {
    if (_initialized) {
      print('[SynthAudioService] Già inizializzato');
      return;
    }

    print('[SynthAudioService] Inizializzazione...');

    try {
      // Inizializza SoLoud engine
      await _soloud.init();

      // Crea oscillatore principale (HUM/SWING)
      _humSource = await _soloud.loadWaveform(
        WaveForm.sine,
        superWave: true,    // Multi-layer per spessore
        scale: 1.0,
        detune: 0.003,      // Detuning leggero per chorusing
      );

      // Imposta frequenza base del pack corrente
      final baseFreq = _packFrequencies[_currentPack]!;
      _soloud.setWaveformFreq(_humSource!, baseFreq);

      _initialized = true;
      print('[SynthAudioService] ✓ Inizializzato (oscillatore a ${baseFreq}Hz)');

    } catch (e) {
      print('[SynthAudioService] ❌ Errore inizializzazione: $e');
      rethrow;
    }
  }

  /// Carica texture layer per un sound pack
  Future<void> _loadTextureLayers(String packId) async {
    if (!_initialized) return;

    print('[SynthAudioService] Caricamento texture per pack: $packId');

    try {
      final basePath = 'assets/sounds/$packId';

      // Carica ignition burst
      _ignitionBurstSource = await _soloud.loadAsset(
        '$basePath/ignition_burst.wav',
      );

      // Carica retract tail
      _retractTailSource = await _soloud.loadAsset(
        '$basePath/retract_tail.wav',
      );

      // Carica clash impact
      _clashImpactSource = await _soloud.loadAsset(
        '$basePath/clash_impact.wav',
      );

      print('[SynthAudioService] ✓ Texture caricate');

    } catch (e) {
      print('[SynthAudioService] ⚠️  Texture non trovate (fallback a synth puro): $e');
      // Non è critico, continua con synth puro
    }
  }

  // ══════════════════════════════════════════════════════════════
  // SOUND PACK MANAGEMENT
  // ══════════════════════════════════════════════════════════════

  /// Cambia sound pack corrente
  Future<void> setSoundPack(String packId) async {
    if (!_packFrequencies.containsKey(packId)) {
      print('[SynthAudioService] ⚠️  Sound pack non valido: $packId');
      return;
    }

    print('[SynthAudioService] Cambio pack: $packId');

    _currentPack = packId;
    final newFreq = _packFrequencies[packId]!;

    // Aggiorna frequenza oscillatore principale
    if (_humSource != null) {
      _soloud.setWaveformFreq(_humSource!, newFreq);
      print('[SynthAudioService] ✓ Frequenza aggiornata: ${newFreq}Hz');
    }

    // Carica nuove texture layer
    await _loadTextureLayers(packId);
  }

  /// Abilita/disabilita suoni
  void setSoundsEnabled(bool enabled) {
    _soundsEnabled = enabled;

    if (!enabled && _humPlaying) {
      stopHum();
    }

    print('[SynthAudioService] Suoni: ${enabled ? "ON" : "OFF"}');
  }

  /// Imposta volume master (0.0 - 1.0)
  void setMasterVolume(double volume) {
    _masterVolume = volume.clamp(0.0, 1.0);

    // Aggiorna volume HUM se attivo
    if (_humHandle != null && _humPlaying) {
      _soloud.setVolume(_humHandle!, _masterVolume);
    }

    print('[SynthAudioService] Volume master: ${(_masterVolume * 100).toInt()}%');
  }

  // ══════════════════════════════════════════════════════════════
  // HUM CONTROL
  // ══════════════════════════════════════════════════════════════

  /// Avvia HUM continuo (loop infinito)
  Future<void> startHum() async {
    if (!_initialized || !_soundsEnabled || _humPlaying) {
      return;
    }

    print('[SynthAudioService] 🎵 Avvio HUM...');

    try {
      _humHandle = await _soloud.play(
        _humSource!,
        volume: _masterVolume,
        looping: true,
      );

      _humPlaying = true;

      // Reset parametri modulazione a valori base
      _currentVolume = _masterVolume;
      _currentPitch = 1.0;
      _currentPan = 0.0;

      print('[SynthAudioService] ✓ HUM attivo (loop infinito)');

    } catch (e) {
      print('[SynthAudioService] ❌ Errore startHum: $e');
    }
  }

  /// Ferma HUM con fade out
  Future<void> stopHum() async {
    if (_humHandle == null || !_humPlaying) return;

    print('[SynthAudioService] 🔇 Stop HUM...');

    try {
      // Fade out smooth (300ms)
      _soloud.fadeVolume(_humHandle!, 0.0, Duration(milliseconds: 300));

      await Future.delayed(Duration(milliseconds: 300));

      await _soloud.stop(_humHandle!);
      _humHandle = null;
      _humPlaying = false;

      print('[SynthAudioService] ✓ HUM fermato');

    } catch (e) {
      print('[SynthAudioService] ❌ Errore stopHum: $e');
    }
  }

  // ══════════════════════════════════════════════════════════════
  // SWING MODULATION (THEREMIN EFFECT)
  // ══════════════════════════════════════════════════════════════

  /// Modula oscillatore HUM in tempo reale per effetto swing
  ///
  /// Questo è il cuore del sistema theremin:
  /// - NON crea nuovo audio
  /// - Modifica parametri dell'oscillatore che sta già suonando
  /// - Zero latency, zero gap
  void updateSwing({
    required List<int> perturbationGrid,
    required double speed,
    required String direction,
  }) {
    if (!_initialized || !_soundsEnabled || !_humPlaying) return;
    if (_humHandle == null) return;

    // Calcola parametri modulazione da movimento
    final swingVolume = _calculateVolumeFromGrid(perturbationGrid);
    final swingPitch = _calculatePitchFromSpeed(speed);
    final swingPan = _calculatePanFromDirection(direction);

    // COMPOSIZIONE: base + modulazione
    // Volume: boost fino a +30% durante movimento
    final totalVolume = _masterVolume * (1.0 + swingVolume * 0.3);

    // Pitch: shift 0.7x-1.5x
    final totalPitch = 1.0 * swingPitch;

    // Pan: stereo -1.0 ↔ +1.0
    final totalPan = swingPan;

    // APPLICA MODULAZIONE REAL-TIME (< 1ms latency!)
    _soloud.setVolume(_humHandle!, totalVolume.clamp(0.0, 1.0));
    _soloud.setRelativePlaySpeed(_humHandle!, totalPitch);
    _soloud.setPan(_humHandle!, totalPan);

    // Salva stato corrente
    _currentVolume = totalVolume;
    _currentPitch = totalPitch;
    _currentPan = totalPan;
  }

  /// Reset oscillatore a parametri base (nessun movimento)
  void resetToBaseHum() {
    if (_humHandle == null || !_humPlaying) return;

    // Smooth transition verso valori base (150ms)
    _soloud.fadeVolume(_humHandle!, _masterVolume, Duration(milliseconds: 150));
    _soloud.setRelativePlaySpeed(_humHandle!, 1.0);
    _soloud.setPan(_humHandle!, 0.0);

    _currentVolume = _masterVolume;
    _currentPitch = 1.0;
    _currentPan = 0.0;
  }

  // ══════════════════════════════════════════════════════════════
  // EVENT SOUNDS (IGNITION / RETRACT / CLASH)
  // ══════════════════════════════════════════════════════════════

  /// Suona IGNITION (accensione lama)
  ///
  /// Architettura ibrida:
  /// 1. Frequency sweep dell'oscillatore principale (20Hz → baseFreq)
  /// 2. Texture layer (noise burst) per impatto energetico
  Future<void> playIgnition() async {
    if (!_initialized || !_soundsEnabled) return;

    print('[SynthAudioService] ⚡ IGNITION...');

    // Avvia HUM se non attivo
    if (!_humPlaying) {
      await startHum();
    }

    final baseFreq = _packFrequencies[_currentPack]!;

    // LAYER 1: Frequency sweep (20Hz → baseFreq in 1.5s)
    _soloud.setWaveformFreq(_humSource!, 20.0);

    // LAYER 2: Texture burst (se disponibile)
    if (_ignitionBurstSource != null) {
      _soloud.play(
        _ignitionBurstSource!,
        volume: _masterVolume * 0.6,
      );
    }

    // LAYER 3: Animazione frequency sweep
    await _animateFrequencySweep(
      source: _humSource!,
      startFreq: 20.0,
      endFreq: baseFreq,
      durationMs: 1500,
    );

    print('[SynthAudioService] ✓ IGNITION completato');
  }

  /// Suona RETRACT (spegnimento lama)
  Future<void> playRetract() async {
    if (!_initialized || !_soundsEnabled) return;
    if (_humHandle == null) return;

    print('[SynthAudioService] 🔻 RETRACT...');

    final baseFreq = _packFrequencies[_currentPack]!;

    // LAYER 1: Frequency sweep (baseFreq → 15Hz in 1.2s)
    await _animateFrequencySweep(
      source: _humSource!,
      startFreq: baseFreq,
      endFreq: 15.0,
      durationMs: 1200,
    );

    // LAYER 2: Reverb tail (se disponibile)
    if (_retractTailSource != null) {
      _soloud.play(
        _retractTailSource!,
        volume: _masterVolume * 0.5,
      );
    }

    // Stop HUM definitivo
    await stopHum();

    print('[SynthAudioService] ✓ RETRACT completato');
  }

  /// Suona CLASH (colpo)
  Future<void> playClash() async {
    if (!_initialized || !_soundsEnabled) return;

    print('[SynthAudioService] ⚔️  CLASH');

    // OPZIONE A: Usa texture (se disponibile)
    if (_clashImpactSource != null) {
      await _soloud.play(
        _clashImpactSource!,
        volume: _masterVolume,
      );
      return;
    }

    // OPZIONE B: Genera sawtooth on-the-fly
    final clashSource = await _soloud.loadWaveform(
      WaveForm.saw,
      superWave: true,
      scale: 0.8,
      detune: 0.02,  // Detune alto = metallico
    );

    _soloud.setWaveformFreq(clashSource, 200.0);

    final handle = await _soloud.play(clashSource, volume: _masterVolume);

    // Fade out rapido (percussivo)
    _soloud.fadeVolume(handle, 0.0, Duration(milliseconds: 400));

    // Cleanup
    Future.delayed(Duration(milliseconds: 400), () {
      _soloud.stop(handle);
      _soloud.disposeSource(clashSource);
    });
  }

  // ══════════════════════════════════════════════════════════════
  // UTILITY FUNCTIONS
  // ══════════════════════════════════════════════════════════════

  /// Calcola volume da griglia perturbazione 8x8
  double _calculateVolumeFromGrid(List<int> grid) {
    if (grid.isEmpty) return 0.0;

    final totalEnergy = grid.reduce((a, b) => a + b);
    final normalized = totalEnergy / 16320.0;  // Max = 64 × 255
    final curved = pow(normalized, 0.7).toDouble();  // Gamma correction

    return curved.clamp(0.0, 1.0);
  }

  /// Calcola pitch multiplier da velocità movimento
  double _calculatePitchFromSpeed(double speed) {
    const minPitch = 0.7;
    const maxPitch = 1.5;

    final normalized = (speed / 20.0).clamp(0.0, 1.0);
    return minPitch + (normalized * (maxPitch - minPitch));
  }

  /// Calcola pan stereo da direzione
  double _calculatePanFromDirection(String direction) {
    const panStrength = 0.8;

    switch (direction.toUpperCase()) {
      case 'LEFT':
        return -panStrength;
      case 'RIGHT':
        return panStrength;
      default:
        return 0.0;
    }
  }

  /// Anima frequency sweep con step discreti
  ///
  /// NOTA: flutter_soloud non ha sweep nativo continuo.
  /// Workaround: step discreti a 60 FPS per smoothness percepita.
  Future<void> _animateFrequencySweep({
    required AudioSource source,
    required double startFreq,
    required double endFreq,
    required int durationMs,
  }) async {
    const fps = 60;
    final totalFrames = (durationMs / 1000 * fps).round();
    final frameDuration = Duration(milliseconds: (1000 / fps).round());

    for (int frame = 0; frame <= totalFrames; frame++) {
      final progress = frame / totalFrames;

      // Interpolazione esponenziale (più naturale)
      final expProgress = pow(progress, 1.5).toDouble();
      final currentFreq = startFreq + (endFreq - startFreq) * expProgress;

      _soloud.setWaveformFreq(source, currentFreq);

      await Future.delayed(frameDuration);
    }

    // Assicura valore finale esatto
    _soloud.setWaveformFreq(source, endFreq);
  }

  // ══════════════════════════════════════════════════════════════
  // CLEANUP
  // ══════════════════════════════════════════════════════════════

  /// Cleanup risorse
  void dispose() {
    print('[SynthAudioService] Cleanup...');

    if (_humHandle != null) {
      _soloud.stop(_humHandle!);
    }

    _soloud.deinit();
    _initialized = false;

    print('[SynthAudioService] ✓ Disposed');
  }
}
```

### ✅ Checklist Fase 3

- [ ] File `synth_audio_service.dart` creato
- [ ] Metodi core implementati:
  - [ ] `initialize()` / `dispose()`
  - [ ] `setSoundPack()` / `setSoundsEnabled()` / `setMasterVolume()`
  - [ ] `startHum()` / `stopHum()`
  - [ ] `updateSwing()` (theremin real-time)
  - [ ] `playIgnition()` / `playRetract()` / `playClash()`
- [ ] Utility functions (volume/pitch/pan calculations)
- [ ] Frequency sweep animation
- [ ] Texture layer loading

---

## 🔌 FASE 4: INTEGRAZIONE CON PROVIDER

**Durata:** 1 giorno
**Obiettivo:** Sostituire AudioService con SynthAudioService in AudioProvider

### Task 4.1: Backup AudioProvider Attuale

```bash
cp lib/providers/audio_provider.dart lib/providers/audio_provider_backup.dart
```

### Task 4.2: Modifica AudioProvider

**File:** `lib/providers/audio_provider.dart`

```dart
import 'package:flutter/foundation.dart';
// import '../services/audio_service.dart';  // OLD
import '../services/synth_audio_service.dart';  // NEW
import '../models/sound_pack.dart';
import '../models/motion_state.dart';

class AudioProvider extends ChangeNotifier {
  // MIGRAZIONE: Usa SynthAudioService invece di AudioService
  final SynthAudioService _audioService = SynthAudioService();

  // Settings (invariati)
  bool _soundsEnabled = true;
  double _masterVolume = 0.8;
  String _currentSoundPackId = 'jedi';
  bool _bladeOn = false;

  // Swing parameters (invariati)
  double _swingMinIntensity = 10.0;

  // Getters (invariati)
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
    await _audioService.initialize();
    await _audioService.setSoundPack(_currentSoundPackId);
  }

  // ══════════════════════════════════════════════════════════
  // SETTINGS (metodi invariati, chiamano SynthAudioService)
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
  // GESTURE HANDLERS (semplificati con SynthAudioService)
  // ══════════════════════════════════════════════════════════

  Future<void> handleGesture(String gesture) async {
    if (!_soundsEnabled) return;

    final gestureType = gesture.toLowerCase();

    switch (gestureType) {
      case 'ignition':
        await _audioService.playIgnition();
        _bladeOn = true;
        break;

      case 'retract':
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
  // MOTION-REACTIVE SWING (invariato, chiama updateSwing)
  // ══════════════════════════════════════════════════════════

  Future<void> updateSwing(MotionState state) async {
    if (!_soundsEnabled || !_bladeOn) return;

    // Solo se c'è movimento sopra threshold
    if (!state.motionDetected || state.intensity < _swingMinIntensity) {
      // Reset a HUM base quando movimento si ferma
      _audioService.resetToBaseHum();
      return;
    }

    // Griglia perturbazione (64 valori)
    final grid = state.perturbationGrid ?? [];
    if (grid.isEmpty) return;

    // THEREMIN REAL-TIME: modula oscillatore
    _audioService.updateSwing(
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

### Task 4.3: Test Integrazione

**Crea test file:** `test/audio_provider_synth_test.dart`

```dart
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_led_saber/providers/audio_provider.dart';
import 'package:flutter_led_saber/models/motion_state.dart';

void main() {
  group('AudioProvider con SynthAudioService', () {
    late AudioProvider provider;

    setUp(() async {
      provider = AudioProvider();
      await Future.delayed(Duration(milliseconds: 500)); // Init time
    });

    tearDown(() {
      provider.dispose();
    });

    test('Inizializzazione corretta', () {
      expect(provider.soundsEnabled, true);
      expect(provider.masterVolume, 0.8);
      expect(provider.currentSoundPackId, 'jedi');
      expect(provider.bladeOn, false);
    });

    test('Cambio sound pack', () async {
      await provider.setSoundPack('sith');
      expect(provider.currentSoundPackId, 'sith');
    });

    test('Ignition avvia HUM', () async {
      await provider.handleGesture('ignition');
      expect(provider.bladeOn, true);

      await Future.delayed(Duration(seconds: 2)); // Attendi ignition
    });

    test('Swing modula parametri', () async {
      await provider.handleGesture('ignition');
      await Future.delayed(Duration(seconds: 2));

      final motionState = MotionState(
        enabled: true,
        motionDetected: true,
        intensity: 150,
        speed: 12.5,
        direction: 'RIGHT',
        confidence: 85,
        lastGesture: 'NONE',
        perturbationGrid: List.filled(64, 100),
      );

      await provider.updateSwing(motionState);

      // Verifica che non ci siano errori
      await Future.delayed(Duration(milliseconds: 500));
    });

    test('Retract ferma HUM', () async {
      await provider.handleGesture('ignition');
      await Future.delayed(Duration(seconds: 2));

      await provider.handleGesture('retract');
      await Future.delayed(Duration(milliseconds: 1500));

      expect(provider.bladeOn, false);
    });
  });
}
```

**Esegui test:**
```bash
cd AppMobile/flutter_led_saber
flutter test test/audio_provider_synth_test.dart
```

### ✅ Checklist Fase 4

- [ ] AudioProvider modificato per usare SynthAudioService
- [ ] Backup `audio_provider_backup.dart` creato
- [ ] Metodi provider aggiornati
- [ ] Test integrazione creati
- [ ] Test superati
- [ ] Build su device reale OK

---

## 🧪 FASE 5: TESTING & TUNING

**Durata:** 1 giorno
**Obiettivo:** Test approfondito e ottimizzazione parametri

### Task 5.1: Test Device Reali

**Checklist test su Android:**

```
Device: _______________ (marca/modello)
Android Version: _______________

[ ] Avvio app senza crash
[ ] Ignition suona correttamente (sweep 20→110Hz percepito)
[ ] HUM stabile in loop senza click
[ ] Swing modula volume/pitch in real-time
[ ] Pan stereo funziona (LEFT/RIGHT distinguibili)
[ ] Retract sweep down percepito
[ ] Clash suona percussivo
[ ] Switching pack cambia tono (jedi alto, sith basso)
[ ] Latency percepita: _____ ms (stima soggettiva)
[ ] Audio ducking durante ignition/retract funziona
[ ] Nessun audio glitch/distortion
```

**Checklist test su iOS (se disponibile):**

(Stessi criteri)

### Task 5.2: Tuning Parametri

**Crea file tuning:** `lib/services/synth_audio_tuning.dart`

```dart
/// Parametri audio tweakable per tuning
class SynthAudioTuning {
  // Frequency pack (Hz)
  static const Map<String, double> packFrequencies = {
    'jedi': 110.0,      // Provare: 105-115 range
    'sith': 70.0,       // Provare: 65-75 range
    'unstable': 90.0,   // Provare: 85-95 range
    'crystal': 150.0,   // Provare: 145-160 range
  };

  // Oscillatore setup
  static const double detuneAmount = 0.003;  // Provare: 0.001-0.01
  static const bool useSuperWave = true;

  // Swing modulation
  static const double swingVolumeBoost = 0.3;  // Max boost (30%)
  static const double swingMinPitch = 0.7;     // Provare: 0.6-0.8
  static const double swingMaxPitch = 1.5;     // Provare: 1.3-1.7
  static const double swingPanStrength = 0.8;  // Provare: 0.5-1.0

  // Volume gamma correction
  static const double volumeGamma = 0.7;       // Provare: 0.5-1.0

  // Ignition/Retract
  static const int ignitionDurationMs = 1500;  // Provare: 1200-1800
  static const int retractDurationMs = 1200;   // Provare: 1000-1500
  static const double ignitionStartFreq = 20.0;
  static const double retractEndFreq = 15.0;

  // Texture layer mix
  static const double ignitionBurstVolume = 0.6;  // Provare: 0.4-0.8
  static const double retractTailVolume = 0.5;
  static const double clashImpactVolume = 1.0;

  // Frequency sweep
  static const int sweepFPS = 60;  // Provare: 30-120
  static const double sweepExponent = 1.5;  // 1.0=linear, >1=exp
}
```

**Integra in SynthAudioService:**
```dart
import 'synth_audio_tuning.dart';

// Sostituisci costanti hardcoded con SynthAudioTuning.*
```

### Task 5.3: A/B Test con Vecchia Implementazione

**Crea toggle per confronto:**

```dart
// In audio_provider.dart
class AudioProvider extends ChangeNotifier {
  // Feature flag per A/B test
  bool _useSynthAudio = true;  // Toggle manualmente per test

  late dynamic _audioService;

  AudioProvider() {
    if (_useSynthAudio) {
      _audioService = SynthAudioService();
    } else {
      _audioService = AudioService();  // Vecchia implementazione
    }
    _initialize();
  }
}
```

**Test comparativo:**
- Registra latency percepita
- Qualità audio soggettiva
- Stabilità (crash/glitch)
- Consumo batteria (monitor Android battery)

### Task 5.4: Logging Metriche

**Aggiungi logging performance:**

```dart
// In SynthAudioService
import 'dart:developer' as developer;

void updateSwing(...) {
  final stopwatch = Stopwatch()..start();

  // ... logica esistente ...

  stopwatch.stop();
  if (stopwatch.elapsedMicroseconds > 1000) {  // > 1ms
    developer.log(
      'updateSwing latency: ${stopwatch.elapsedMicroseconds}μs',
      name: 'SynthAudioService',
    );
  }
}
```

### ✅ Checklist Fase 5

- [ ] Test su almeno 2 device Android (low-end, high-end)
- [ ] Test su iOS (se disponibile)
- [ ] Parametri tuning documentati in `SynthAudioTuning`
- [ ] A/B test completato (synth vs file-based)
- [ ] Metriche latency raccolte
- [ ] Nessun crash/glitch rilevato
- [ ] Feedback soggettivo positivo

---

## 🧹 FASE 6: CLEANUP & OTTIMIZZAZIONE

**Durata:** 0.5 giorni
**Obiettivo:** Rimuovere codice legacy e ottimizzare

### Task 6.1: Rimuovi Vecchia Implementazione

```bash
# Backup finale prima di rimuovere
git add .
git commit -m "Audio synth implementato e testato"

# Rimuovi vecchio AudioService
mv lib/services/audio_service.dart lib/services/audio_service_DEPRECATED.dart

# Rimuovi just_audio dependency (se non usato altrove)
# Modifica pubspec.yaml manualmente
```

**In `pubspec.yaml`:**
```yaml
dependencies:
  # Audio - DEPRECATO
  # just_audio: ^0.9.36  # Commentato, da rimuovere se non serve

  # Audio - ATTIVO
  flutter_soloud: ^2.0.0
```

### Task 6.2: Rimuovi File Audio Non Necessari (Opzionale)

Se decidi di usare **solo synth puro** (senza texture layer):

```bash
# Rimuovi file WAV vecchi (backup prima!)
mv AppMobile/flutter_led_saber/assets/sounds \
   AppMobile/flutter_led_saber/assets/sounds_BACKUP

# Tieni solo texture (se usate)
mkdir AppMobile/flutter_led_saber/assets/sounds
cp -r assets/sounds_BACKUP/jedi/*_burst.wav assets/sounds/jedi/
cp -r assets/sounds_BACKUP/jedi/*_tail.wav assets/sounds/jedi/
# ... ripeti per altri pack
```

**In `pubspec.yaml`:**
```yaml
flutter:
  assets:
    # Solo texture layer (file piccoli ~20KB ciascuno)
    - assets/sounds/jedi/
    - assets/sounds/sith/
    # ... etc
```

### Task 6.3: Ottimizza Startup

**Lazy loading texture:**

```dart
// In SynthAudioService
AudioSource? _ignitionBurstSource;  // null = non caricato

Future<void> _ensureTextureLoaded() async {
  if (_ignitionBurstSource != null) return;
  await _loadTextureLayers(_currentPack);
}

Future<void> playIgnition() async {
  // Lazy load solo quando serve
  await _ensureTextureLoaded();

  // ... resto del codice
}
```

### Task 6.4: Memory Profiling

**Android Studio Profiler:**
1. Avvia app in debug mode
2. Apri Android Studio → Profiler
3. Monitora Memory:
   - Baseline (app idle)
   - Durante ignition/retract
   - Durante swing continuo
4. Verifica no memory leak

**Target:** < 5MB memoria aggiuntiva rispetto a baseline

### ✅ Checklist Fase 6

- [ ] `audio_service.dart` rimosso/deprecated
- [ ] `just_audio` rimosso da pubspec (se non serve)
- [ ] File audio vecchi archiviati/rimossi
- [ ] Lazy loading texture implementato
- [ ] Memory profiling OK (< 5MB overhead)
- [ ] Build release testata

---

## 📖 FASE 7: DOCUMENTAZIONE

**Durata:** 0.5 giorni
**Obiettivo:** Documentare nuova architettura

### Task 7.1: Aggiorna SOUND_ROADMAP.md

**File:** `docs/SOUND_ROADMAP.md`

Aggiungi sezione finale:

```markdown
## ✅ IMPLEMENTAZIONE COMPLETATA (2025-12-30)

### Architettura Finale: IBRIDA SYNTH

**Engine:** flutter_soloud (SoLoud C++ via FFI)

**Componenti:**
1. Oscillatore principale (sine wave, superWave)
   - HUM continuo (loop infinito)
   - SWING (modulazione real-time di volume/pitch/pan)
   - IGNITION/RETRACT (frequency sweep 20-176Hz)

2. Texture layer (opzionali, file WAV ~20KB)
   - ignition_burst.wav (noise 300ms)
   - retract_tail.wav (reverb 1.5s)
   - clash_impact.wav (metallic 400ms)

**Performance:**
- Latency: < 10ms (vs 50-100ms precedente)
- Memoria: ~1MB engine + ~60KB texture per pack
- CPU: < 1% durante swing modulation

**Parametri Tuning:**
- Vedere `lib/services/synth_audio_tuning.dart`

**Vantaggi rispetto a file-based:**
- ✅ Zero race conditions
- ✅ Architettura semplificata (no watchdog)
- ✅ Modulazione perfetta (math-based)
- ✅ Switching pack istantaneo (solo freq change)
- ✅ Personalizzazione illimitata
```

### Task 7.2: Crea Architecture Diagram

**File:** `docs/AUDIO_ARCHITECTURE_DIAGRAM.md`

```markdown
# Audio Architecture Diagram

## Flutter Layer
```
┌─────────────────────────────────────────────┐
│  AudioProvider (State Management)           │
│  ├─ soundsEnabled                           │
│  ├─ masterVolume                            │
│  ├─ currentSoundPack                        │
│  └─ bladeOn                                 │
└────────────┬────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────┐
│  SynthAudioService (Audio Engine)           │
│  ├─ initialize()                            │
│  ├─ startHum() / stopHum()                  │
│  ├─ updateSwing() ← REAL-TIME              │
│  ├─ playIgnition() / playRetract()          │
│  └─ playClash()                             │
└────────────┬────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────┐
│  flutter_soloud (FFI → C++)                 │
│  ├─ SoLoud.instance                         │
│  ├─ loadWaveform() → AudioSource            │
│  ├─ play() → SoundHandle                    │
│  ├─ setVolume() / setRelativePlaySpeed()    │
│  ├─ setPan() / fadeVolume()                 │
│  └─ setWaveformFreq()                       │
└────────────┬────────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────────┐
│  SoLoud C++ Engine (miniaudio backend)      │
│  ├─ Oscillator generation                   │
│  ├─ Real-time mixing                        │
│  ├─ Audio buffer management                 │
│  └─ Platform audio output (OpenSL/AAudio)   │
└─────────────────────────────────────────────┘
```
```

### Task 7.3: Crea Migration Guide

**File:** `docs/AUDIO_MIGRATION_GUIDE.md`

```markdown
# Migration Guide: File-Based → Synth Audio

## Per Developer

Se hai fork/branch basati sulla vecchia implementazione:

### Passaggi Migrazione

1. **Update dependencies**
   ```yaml
   flutter_soloud: ^2.0.0
   ```

2. **Sostituisci import**
   ```dart
   // OLD
   import '../services/audio_service.dart';

   // NEW
   import '../services/synth_audio_service.dart';
   ```

3. **Nessun cambio API provider**
   - `AudioProvider` API invariata
   - `handleGesture()` / `updateSwing()` identici

4. **Rigenera texture (opzionale)**
   ```bash
   ./venv/bin/python tools/audio_generator.py --pack all --textures
   ```

## Breaking Changes

Nessuno! L'API pubblica è retrocompatibile.

## Rollback

Se serve tornare a file-based:

1. Restore `audio_service.dart` da backup
2. Revert `pubspec.yaml` (torna a `just_audio`)
3. Restore asset audio WAV

```bash
git checkout v1.0.0-pre-synthesis -- lib/services/audio_service.dart
git checkout v1.0.0-pre-synthesis -- pubspec.yaml
```
```

### ✅ Checklist Fase 7

- [ ] SOUND_ROADMAP.md aggiornato
- [ ] Architecture diagram creato
- [ ] Migration guide scritto
- [ ] API reference per SynthAudioService
- [ ] Code comments aggiunti

---

## 🔄 ROLLBACK PLAN

In caso di problemi critici durante la migrazione:

### Rollback Completo

```bash
# 1. Torna al tag pre-migrazione
git checkout v1.0.0-pre-synthesis

# 2. Oppure, revert commit specifici
git revert <commit-hash-synth-implementation>

# 3. Restore file specifici
git checkout v1.0.0-pre-synthesis -- lib/services/audio_service.dart
git checkout v1.0.0-pre-synthesis -- lib/providers/audio_provider.dart
git checkout v1.0.0-pre-synthesis -- pubspec.yaml

# 4. Rebuild
flutter pub get
flutter build apk --release
```

### Rollback Parziale (Mantieni Synth, Usa Fallback)

```dart
// In audio_provider.dart
class AudioProvider extends ChangeNotifier {
  bool _useSynthAudio = false;  // Fallback a file-based

  AudioProvider() {
    try {
      if (_useSynthAudio) {
        _audioService = SynthAudioService();
      } else {
        _audioService = AudioService();
      }
    } catch (e) {
      print('Errore synth, fallback a file-based');
      _audioService = AudioService();
    }
  }
}
```

---

## 📊 TIMELINE & CHECKLIST GENERALE

### Timeline Stimata

| Fase | Durata | Inizio | Fine |
|------|--------|--------|------|
| Fase 0: Preparazione | 0.5d | Giorno 1 | Giorno 1 |
| Fase 1: Dependencies | 0.5d | Giorno 1 | Giorno 1 |
| Fase 2: Texture Generator | 1d | Giorno 2 | Giorno 2 |
| Fase 3: SynthAudioService | 2d | Giorno 3 | Giorno 4 |
| Fase 4: Integrazione Provider | 1d | Giorno 5 | Giorno 5 |
| Fase 5: Testing & Tuning | 1d | Giorno 6 | Giorno 6 |
| Fase 6: Cleanup | 0.5d | Giorno 7 | Giorno 7 |
| Fase 7: Documentazione | 0.5d | Giorno 7 | Giorno 7 |
| **TOTALE** | **7 giorni** | | |

### Checklist Generale

**Setup Iniziale:**
- [ ] Branch migrazione creato
- [ ] Tag backup applicato
- [ ] Test baseline eseguiti

**Implementazione:**
- [ ] flutter_soloud installato
- [ ] TextureLayerGenerator implementato
- [ ] Texture generate (tutti i pack)
- [ ] SynthAudioService implementato
- [ ] AudioProvider aggiornato

**Testing:**
- [ ] Test su Android (2+ device)
- [ ] Test su iOS (se disponibile)
- [ ] A/B test completato
- [ ] Performance profiling OK

**Cleanup:**
- [ ] Codice legacy rimosso
- [ ] Memoria ottimizzata
- [ ] Build release OK

**Documentazione:**
- [ ] SOUND_ROADMAP aggiornato
- [ ] Architecture diagram
- [ ] Migration guide
- [ ] Code comments

**Merge:**
- [ ] Pull request creato
- [ ] Code review superato
- [ ] Merge in main branch
- [ ] Tag nuova versione (v2.0.0-synth)

---

## 📞 SUPPORT & TROUBLESHOOTING

### Problemi Comuni

**1. flutter_soloud non si installa**
```bash
flutter clean
flutter pub cache repair
flutter pub get
```

**2. Latency percepita alta**
- Verifica `sweepFPS` in tuning (provare 120 FPS)
- Check Android audio settings (disabilita effetti DSP)

**3. Crash durante playback**
- Verifica init completa prima di play
- Check logs: `flutter logs --verbose`

**4. Texture non caricate**
- Verifica path: `assets/sounds/{pack}/ignition_burst.wav`
- Check pubspec.yaml assets section

**5. Switching pack non funziona**
- Verifica `_loadTextureLayers()` chiamato
- Check frequency map aggiornata

### Log Debug

Abilita logging dettagliato:

```dart
// In synth_audio_service.dart
void _log(String message) {
  if (kDebugMode) {
    print('[SynthAudioService] $message');
  }
}
```

---

## 🎯 SUCCESS CRITERIA

Migrazione considerata **completa e di successo** quando:

✅ **Funzionalità:**
- [ ] HUM suona in loop continuo senza glitch
- [ ] SWING modula in real-time (< 20ms latency percepita)
- [ ] IGNITION/RETRACT sweep percepiti come smooth
- [ ] CLASH suona percussivo e realistico
- [ ] Tutti i pack funzionano (jedi, sith, unstable, crystal)

✅ **Performance:**
- [ ] Latency < 20ms (test soggettivo)
- [ ] Memoria overhead < 5MB
- [ ] CPU usage < 2% durante swing
- [ ] Nessun crash su 100 ignition/retract cycles

✅ **Qualità:**
- [ ] Audio quality soggettiva ≥ file-based
- [ ] Nessun click/pop durante modulazione
- [ ] Frequency sweep smooth (no step artifacts)

✅ **Codice:**
- [ ] Test coverage > 80%
- [ ] Zero warning nel build
- [ ] Codice legacy rimosso
- [ ] Documentazione completa

---

## 📚 RISORSE

### Documentazione flutter_soloud
- [GitHub](https://github.com/alnitak/flutter_soloud)
- [pub.dev](https://pub.dev/packages/flutter_soloud)
- [API Docs](https://pub.dev/documentation/flutter_soloud/latest/)
- [Examples](https://github.com/alnitak/flutter_soloud_example)

### Audio Synthesis Concepts
- [Waveform Types](https://en.wikipedia.org/wiki/Waveform)
- [Frequency Modulation](https://en.wikipedia.org/wiki/Frequency_modulation_synthesis)
- [ADSR Envelope](https://en.wikipedia.org/wiki/Envelope_(music))

### Flutter Audio Performance
- [Flutter Performance Best Practices](https://docs.flutter.dev/perf/best-practices)
- [Audio Latency Android](https://source.android.com/docs/core/audio/latency)

---

## ✅ FINE ROADMAP

**Prossimo step:** Inizia da **Fase 0: Preparazione**

Buona migrazione! 🚀
