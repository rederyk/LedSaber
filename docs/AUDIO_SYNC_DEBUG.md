# 🐛 Debug Audio Sync - Guida Risoluzione Problemi

## ✅ Modifiche Applicate

### 1. Rimosso Provider Duplicato in main.dart
**Problema:** `AudioProvider` veniva creato **due volte** (linea 23 e 25-32)
- Il primo riceveva i dati dall'UI ma non veniva usato
- Il secondo veniva usato dall'app ma non riceveva la sincronizzazione

**Soluzione:** Rimosso il provider duplicato alla linea 23

```dart
// PRIMA (SBAGLIATO)
ChangeNotifierProvider(create: (_) => AudioProvider()),  // ❌ Duplicato
ChangeNotifierProxyProvider<LedProvider, AudioProvider>( // ✅ Quello giusto
  create: (_) => AudioProvider(),
  update: (_, ledProvider, audioProvider) {
    audioProvider!.syncWithBladeState(ledProvider.currentState?.bladeState);
    return audioProvider;
  },
),

// DOPO (CORRETTO)
ChangeNotifierProxyProvider<LedProvider, AudioProvider>( // ✅ Solo questo
  create: (_) => AudioProvider(),
  update: (_, ledProvider, audioProvider) {
    audioProvider!.syncWithBladeState(ledProvider.currentState?.bladeState);
    return audioProvider;
  },
),
```

### 2. Modificato syncWithBladeState da async a sync
**Problema:** Il metodo era `Future<void>` con `await` interni, ma veniva chiamato senza `await` in main.dart, causando problemi di timing

**Soluzione:** Convertito a `void` con callback non-bloccanti

```dart
// PRIMA
Future<void> syncWithBladeState(String? bladeState) async {
  ...
  await Future.delayed(const Duration(milliseconds: 50));
  _audioService.startHum();
  ...
}

// DOPO
void syncWithBladeState(String? bladeState) {
  ...
  Future.delayed(const Duration(milliseconds: 50), () {
    _audioService.startHum();
  });
  ...
}
```

---

## 🔍 Come Verificare che Funzioni

### Test 1: Verifica Log alla Startup
Quando avvii l'app, dovresti vedere nei log:

```
[AudioProvider] Constructor chiamato - inizializzazione in corso...
[AudioService] setSoundPack chiamato - packId: jedi
[AudioService] Sound pack caricato: Jedi Classic - path: assets/sounds/jedi
[AudioProvider] Inizializzazione completata - sound pack: jedi
```

### Test 2: Accensione Lama (off → igniting → on)
Quando premi il pulsante di accensione:

```
[AudioProvider] bladeState cambiato: off -> igniting
[AudioProvider] Avvio ignition audio
[AudioService] playIgnition chiamato - enabled: true, pack: jedi
[AudioService] playIgnition loading: assets/sounds/jedi/ignition.wav
[AudioService] playIgnition playing...
[AudioService] startHum chiamato - enabled: true, pack: jedi
[AudioService] startHum loading: assets/sounds/jedi/hum_base.wav
[AudioService] startHum playing loop...
[AudioService] startHum started successfully - loop active
```

Poi dopo ~500ms quando l'animazione completa:

```
[AudioProvider] bladeState cambiato: igniting -> on
```

### Test 3: Spegnimento Lama (on → retracting → off)
Quando premi il pulsante di spegnimento:

```
[AudioProvider] bladeState cambiato: on -> retracting
[AudioProvider] Avvio retract audio
[AudioService] playRetract chiamato - enabled: true, pack: jedi
[AudioService] playRetract loading: assets/sounds/jedi/retract.wav
[AudioService] stopHum chiamato - currently playing: true
[AudioService] stopHum - hum stopped successfully
[AudioService] Swing stopped
```

Poi dopo ~500ms quando l'animazione completa:

```
[AudioProvider] bladeState cambiato: retracting -> off
```

### Test 4: Swing Durante Movimento
Quando muovi la spada (lama accesa):

```
[AudioService] Starting swing loop...
[AudioService] Swing stopped - volume too low    # Quando ti fermi
[AudioService] Starting swing loop...             # Quando riprendi
```

---

## 🚨 Problemi Comuni e Soluzioni

### Problema: Hum non si avvia quando accendo la lama
**Causa:** AudioProvider non riceve la notifica di cambio bladeState

**Debug:**
1. Controlla se vedi il log `[AudioProvider] bladeState cambiato: ...`
   - Se **NO**: il problema è in main.dart (ChangeNotifierProxyProvider non funziona)
   - Se **SI**: il problema è in audio_service.dart

2. Verifica che `_soundsEnabled = true` (default)
   ```dart
   // In audio_provider.dart
   print('[AudioProvider] soundsEnabled: $_soundsEnabled');
   ```

**Soluzione:**
- Assicurati che ci sia **UN SOLO** ChangeNotifierProxyProvider per AudioProvider in main.dart
- Verifica che il sound pack sia caricato (vedi log `[AudioProvider] Inizializzazione completata`)

### Problema: Hum continua a suonare anche con lama spenta
**Causa:** Lo stato 'off' non viene ricevuto o gestito male

**Debug:**
1. Verifica che arrivi il log `[AudioProvider] bladeState cambiato: ... -> off`
2. Controlla se `_audioService.isHumPlaying` è corretto

**Soluzione:**
```dart
// In audio_provider.dart, case 'off'
case 'off':
  print('[AudioProvider] DEBUG - isHumPlaying: ${_audioService.isHumPlaying}');
  if (_audioService.isHumPlaying) {
    print('[AudioProvider] Lama spenta ma loop ancora attivo - lo fermo');
    _audioService.stopHum();
  }
  break;
```

### Problema: Swing non si ferma quando smetto di muovere
**Causa:** La griglia perturbation continua a inviare valori > 0

**Debug:**
1. Decommenta il log in playSwing:
   ```dart
   // In audio_service.dart, playSwing()
   print('[AudioService] Swing modulated - vol: ${(volume * 100).toInt()}%, pitch: ${pitch.toStringAsFixed(2)}x');
   ```

2. Verifica se volume scende sotto 0.05 quando ti fermi

**Soluzione:**
- Se volume non scende mai < 0.05: problema nel firmware (perturbationGrid non si azzera)
- Se volume scende ma swing continua: verifica che `_swingPlayer.stop()` venga chiamato

### Problema: Audio non funziona affatto (nessun suono)
**Causa:** File audio mancanti o path errati

**Debug:**
1. Verifica che i file esistano:
   ```bash
   ls -la AppMobile/flutter_led_saber/assets/sounds/jedi/
   ```

2. Verifica che siano dichiarati in pubspec.yaml:
   ```yaml
   flutter:
     assets:
       - assets/sounds/jedi/
   ```

3. Ricompila dopo aver aggiunto asset:
   ```bash
   flutter clean
   flutter pub get
   flutter build apk
   ```

**Soluzione:**
- Se i file mancano: aggiungi i file audio (vedi SOUND_ROADMAP.md)
- Se i file ci sono: verifica i permessi audio su Android (vedi AndroidManifest.xml)

---

## 📋 Checklist Completa

Prima di testare, verifica:

- [ ] ✅ Un solo `ChangeNotifierProxyProvider<LedProvider, AudioProvider>` in main.dart
- [ ] ✅ `syncWithBladeState` è `void` (non `Future<void>`)
- [ ] ✅ File audio presenti in `assets/sounds/jedi/`
- [ ] ✅ Asset dichiarati in `pubspec.yaml`
- [ ] ✅ App ricompilata dopo modifiche (`flutter clean && flutter build apk`)

Durante i test, verifica:

- [ ] Log `[AudioProvider] Constructor chiamato` all'avvio
- [ ] Log `[AudioProvider] bladeState cambiato: off -> igniting` quando accendi
- [ ] Suono ignition.wav si sente
- [ ] Loop hum_base.wav si sente dopo ignition
- [ ] Log `[AudioProvider] bladeState cambiato: on -> retracting` quando spegni
- [ ] Suono retract.wav si sente
- [ ] Loop hum si ferma dopo retract
- [ ] Swing si avvia quando muovi la spada (lama accesa)
- [ ] Swing si ferma quando smetti di muoverti
- [ ] Pitch aumenta con movimento veloce
- [ ] Volume aumenta con movimento intenso

---

## 🛠️ Debug Avanzato

### Aggiungi Log Dettagliati in LedProvider

Per capire se il problema è nella propagazione dello stato:

```dart
// In led_provider.dart, _handleStateUpdate()
if (bladeStateChanged) {
  print('[LedProvider] ⚡ bladeState cambiato: $_lastBladeState -> $currentBladeState');
  _lastBladeState = currentBladeState;
  _lastStateSignature = stateSignature;
  _currentState = state;
  _debounceTimer?.cancel();
  notifyListeners();  // ← Questo dovrebbe triggerare AudioProvider
  print('[LedProvider] ✅ notifyListeners() chiamato');
}
```

### Aggiungi Log in main.dart update()

Per verificare che la sincronizzazione venga chiamata:

```dart
ChangeNotifierProxyProvider<LedProvider, AudioProvider>(
  create: (_) => AudioProvider(),
  update: (_, ledProvider, audioProvider) {
    final bladeState = ledProvider.currentState?.bladeState;
    print('[main.dart] update() chiamato - bladeState: $bladeState');
    audioProvider!.syncWithBladeState(bladeState);
    return audioProvider;
  },
),
```

### Verifica Ordine di Inizializzazione Provider

```dart
// In main.dart, all'inizio di build()
@override
Widget build(BuildContext context) {
  print('[MyApp] build() chiamato - creazione providers...');
  return MultiProvider(
    ...
  );
}
```

---

## ✅ Risultato Atteso

Dopo le modifiche, il comportamento corretto è:

1. **App si avvia:**
   - AudioProvider viene creato UNA SOLA volta
   - Sound pack 'jedi' viene caricato
   - Nessun suono (lama spenta)

2. **Premi pulsante accensione:**
   - UI mostra animazione igniting
   - Suona ignition.wav
   - Dopo 50ms inizia loop hum_base.wav
   - Quando animazione finisce, UI mostra 'on'
   - Hum continua a suonare

3. **Muovi la spada:**
   - Swing loop inizia (stessa traccia hum modulata)
   - Volume/pitch cambiano in base al movimento
   - Quando ti fermi, swing si auto-stoppa dopo ~0.5s
   - Hum base continua sempre

4. **Premi pulsante spegnimento:**
   - UI mostra animazione retracting
   - Suona retract.wav
   - Dopo 100ms hum si ferma
   - Swing si ferma automaticamente
   - Quando animazione finisce, UI mostra 'off'
   - Silenzio totale

---

## 📞 Se Ancora Non Funziona

Se dopo tutte queste verifiche l'audio non è sincronizzato con l'UI:

1. **Cattura i log completi:**
   ```bash
   flutter run --verbose > debug.log 2>&1
   ```

2. **Filtra i log rilevanti:**
   ```bash
   grep -E "\[Audio|\[Led|\[main.dart\]" debug.log
   ```

3. **Controlla l'ordine degli eventi:**
   - LedProvider notifica → main.dart update() → AudioProvider syncWithBladeState()
   - Se manca un pezzo, il problema è in quel punto della catena

4. **Verifica permessi Android:**
   ```xml
   <!-- In android/app/src/main/AndroidManifest.xml -->
   <uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
   <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
   ```

5. **Ricompila completamente:**
   ```bash
   flutter clean
   rm -rf build/
   flutter pub get
   flutter build apk --release
   ```
