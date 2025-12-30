# 🎵 Implementazione Swing Theremin - LedSaber Audio

## 🎯 Obiettivo Raggiunto

Il sistema audio dello swing è stato modificato da **trigger one-shot** a **modulazione continua in tempo reale** (effetto theremin).

---

## 🔄 Cosa è Cambiato

### Prima (One-Shot Mode)
```dart
playSwing() {
  // Riavvia il file ogni volta
  await _swingPlayer.setAsset(hum_base.wav);
  await _swingPlayer.seek(Duration.zero);
  await _swingPlayer.play();

  // Cooldown 100ms tra trigger
}
```

**Problema:** La traccia saltava invece di modularsi fluidamente.

### Dopo (Theremin Mode)
```dart
playSwing() {
  if (volume < 0.05) {
    stop_swing();  // Auto-stop quando no movimento
  }

  if (!swing_playing) {
    start_swing_loop();  // Avvia loop solo la prima volta
  }

  // Modula in tempo reale (THEREMIN!)
  _swingPlayer.setVolume(volume);
  _swingPlayer.setSpeed(pitch);
}
```

**Risultato:** Il suono si modula fluidamente come un theremin!

---

## 📝 Modifiche ai File

### 1. `audio_service.dart`

#### Aggiunto stato swing
```dart
bool _swingPlaying = false;  // Traccia se swing è attivo
```

#### Configurato loop swing
```dart
// In _initializePlayers()
_swingPlayer.setLoopMode(LoopMode.one);
```

#### Riscritto metodo playSwing
- ✅ Avvia loop solo al primo trigger
- ✅ Modula volume/pitch in tempo reale
- ✅ Auto-stop quando volume < 5%
- ✅ Nessun debouncing (aggiornamenti continui)
- ✅ Nessun restart del file

#### Aggiunto metodo stopSwing
```dart
Future<void> stopSwing() async {
  if (!_swingPlaying) return;
  await _swingPlayer.stop();
  _swingPlaying = false;
}
```

#### Integrato stop swing in stopHum
Quando il hum si ferma (lama spenta), lo swing si ferma automaticamente.

---

### 2. `audio_provider.dart`

#### Semplificato updateSwing
- ❌ Rimosso check `state.motionDetected`
- ❌ Rimosso check `state.intensity < _swingMinIntensity`
- ✅ Chiama sempre `playSwing()` - decide internamente se modulare o fermare

**Motivo:** Il calcolo del volume dalla griglia gestisce già il threshold. Se la griglia è vuota/bassa, volume sarà < 5% e lo swing si fermerà automaticamente.

---

## 🎛️ Come Funziona il Theremin

### Workflow Continuo

```
┌──────────────────────────────────────────────────────────┐
│  1. PRIMO MOVIMENTO RILEVATO                              │
├──────────────────────────────────────────────────────────┤
│  perturbationGrid: [5, 12, 18, ...]                      │
│  volume = calculateVolumeFromGrid() = 0.35               │
│  pitch = calculatePitchFromSpeed(8.5) = 1.2x             │
│                                                           │
│  → Swing NON sta suonando                                │
│  → Avvia loop hum_base.wav                               │
│  → Imposta volume = 0.35, pitch = 1.2x                   │
│  → _swingPlaying = true                                  │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│  2. MOVIMENTO CONTINUA (aggiornamenti a ~20-50 Hz)        │
├──────────────────────────────────────────────────────────┤
│  perturbationGrid: [8, 20, 25, ...]  (movimento veloce)  │
│  volume = 0.55  ↑                                         │
│  pitch = 1.4x   ↑                                         │
│                                                           │
│  → Swing GIÀ in loop                                     │
│  → Aggiorna solo volume = 0.55, pitch = 1.4x             │
│  → NESSUN restart del file!                              │
│  → Modulazione fluida in tempo reale                     │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│  3. MOVIMENTO RALLENTA                                    │
├──────────────────────────────────────────────────────────┤
│  perturbationGrid: [2, 5, 3, ...]  (movimento lento)     │
│  volume = 0.12  ↓                                         │
│  pitch = 0.8x   ↓                                         │
│                                                           │
│  → Aggiorna volume = 0.12, pitch = 0.8x                  │
│  → Suono diventa più grave e silenzioso                  │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│  4. MOVIMENTO SI FERMA                                    │
├──────────────────────────────────────────────────────────┤
│  perturbationGrid: [0, 0, 1, 0, ...]  (no movimento)     │
│  volume = 0.02  (< threshold 0.05)                       │
│                                                           │
│  → Volume troppo basso                                   │
│  → AUTO-STOP: _swingPlayer.stop()                        │
│  → _swingPlaying = false                                 │
│  → Silenzio (solo hum base continua)                     │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│  5. MOVIMENTO RIPRENDE                                    │
├──────────────────────────────────────────────────────────┤
│  perturbationGrid: [10, 15, 12, ...]                     │
│  volume = 0.40                                            │
│                                                           │
│  → Swing riavvia automaticamente il loop                 │
│  → Ciclo ricomincia da step 1                            │
└──────────────────────────────────────────────────────────┘
```

---

## 🎚️ Parametri di Modulazione

### Volume (da griglia 8x8)
```dart
double _calculateVolumeFromGrid(List<int> grid) {
  int totalEnergy = grid.reduce((a, b) => a + b);
  double normalized = totalEnergy / 16320.0;  // Max = 64×255
  double curved = pow(normalized, 0.7);       // Gamma correction
  return curved.clamp(0.0, 1.0);
}
```

- **Range:** 0.0 - 1.0
- **Threshold auto-stop:** < 0.05 (5%)
- **Gamma:** 0.7 (boost valori medi)

### Pitch (da velocità movimento)
```dart
double _calculatePitchFromSpeed(double speed) {
  const double minPitch = 0.7;  // Lento = grave
  const double maxPitch = 1.5;  // Veloce = acuto
  double normalized = (speed / 20.0).clamp(0.0, 1.0);
  return minPitch + (normalized * (maxPitch - minPitch));
}
```

- **Range:** 0.7x - 1.5x
- **Mapping:** Lineare da speed (0-20 px/frame)
- **Effetto:** Movimento veloce = suono più acuto

---

## 🔧 Tuning Consigliato

### Se lo swing è troppo sensibile
Aumenta il threshold auto-stop:
```dart
// In playSwing()
if (volume < 0.10) {  // Invece di 0.05
  await _swingPlayer.stop();
}
```

### Se il pitch varia troppo
Riduci il range:
```dart
const double minPitch = 0.85;  // Invece di 0.7
const double maxPitch = 1.15;  // Invece di 1.5
```

### Se il volume è troppo aggressivo
Aumenta il gamma correction:
```dart
double curved = pow(normalized, 0.9);  // Invece di 0.7
```

---

## ✅ Checklist Test

Testa il sistema con questi scenari:

- [ ] **Movimento lento:** swing parte e suono è grave/silenzioso
- [ ] **Movimento veloce:** pitch aumenta, volume aumenta
- [ ] **Movimento continuo:** suono modula fluidamente (NO salti)
- [ ] **Stop movimento:** swing si ferma automaticamente dopo ~0.5s
- [ ] **Riprendi movimento:** swing riprende senza lag
- [ ] **Lama spenta:** swing si ferma insieme al hum
- [ ] **Cambio sound pack:** swing usa il nuovo hum_base.wav

---

## 🎵 Effetto Sonoro Finale

Con queste modifiche, il lightsaber ora suona come:

1. **Hum base:** drone continuo costante (background)
2. **Swing theremin:** si sovrappone al hum quando muovi la spada
   - Volume ∝ intensità movimento
   - Pitch ∝ velocità movimento
   - Start/stop automatici
   - Transizioni fluide

**Risultato:** effetto realistico "whoosh" che si intensifica con il movimento, proprio come nei film di Star Wars! 🎬

---

## 📚 Riferimenti

- **File modificati:**
  - [`lib/services/audio_service.dart`](../AppMobile/flutter_led_saber/lib/services/audio_service.dart)
  - [`lib/providers/audio_provider.dart`](../AppMobile/flutter_led_saber/lib/providers/audio_provider.dart)

- **Documentazione:**
  - [`SOUND_ROADMAP.md`](./SOUND_ROADMAP.md)

- **Libreria audio:**
  - [just_audio](https://pub.dev/packages/just_audio) - Flutter audio player

---

## 🚀 Prossimi Miglioramenti (Opzionali)

1. **Stereo Panning:** aggiungere pan L/R basato su direzione movimento
   - Richiede libreria con supporto balance (just_audio non supporta nativamente)

2. **High-Pass Filter:** enfatizzare acuti nello swing
   - Richiede DSP plugin o switch a flutter_soloud

3. **Velocity smoothing:** smussare picchi di volume/pitch
   - Aggiungere interpolazione temporale per transizioni più morbide

4. **Adaptive threshold:** regolare auto-stop in base a rumori ambiente
   - Analizzare baseline griglia e adattare soglia dinamicamente
