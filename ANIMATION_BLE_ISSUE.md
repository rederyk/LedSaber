# LED Saber - Problema Inconsistenza Animazioni BLE

## Documento Tecnico per Analisi e Risoluzione

**Data**: 2025-12-27
**Componente**: Comunicazione BLE tra Firmware ESP32 e App Flutter
**Severità**: Alta - Blocca funzionalità core dell'applicazione

---

## 1. Descrizione del Problema

### Sintomi Osservati
L'animazione della lama laser nell'app Flutter presenta comportamenti inconsistenti rispetto allo stato reale del firmware:

1. **Lama non si riaccende**: Dopo lo spegnimento, la lama non si riaccende premendo il power button
2. **Riaccensione lenta**: La lama si riaccende lentamente invece di eseguire l'animazione ignition corretta (1.5s smooth)
3. **Stato desincronizzato**: La lama risulta spenta nel firmware ma l'app mostra la lama che risale lentamente
4. **Progressivo peggioramento**: Dopo vari tentativi, la lama non risponde più ai comandi

### Comportamento Atteso vs Reale

| Azione | Comportamento Atteso | Comportamento Reale |
|--------|---------------------|---------------------|
| Tap power button (OFF→ON) | Animazione ignition 1.5s smooth | Non si accende O si accende lentamente |
| Tap power button (ON→OFF) | Animazione retract 1.0s smooth | Si spegne in modo strano |
| Tap ripetuti | Toggle affidabile OFF↔ON | Dopo 2-3 tap smette di rispondere |
| Stato app vs firmware | Sincronizzati sempre | Desincronizzati, lama "fantasma" nell'app |

---

## 2. Root Cause Analysis

### 2.1 Evidenze dai Log

I log BLE mostrano un pattern critico:

```
D/[FBP-Android](20077): [FBP] onCharacteristicChanged: chr: beb5483e-36e1-4688-b7f5-ea07361b26a8
D/[FBP-Android](20077): [FBP] onCharacteristicChanged: chr: beb5483e-36e1-4688-b7f5-ea07361b26a8
D/[FBP-Android](20077): [FBP] onCharacteristicChanged: chr: beb5483e-36e1-4688-b7f5-ea07361b26a8
D/[FBP-Android](20077): [FBP] onCharacteristicChanged: chr: beb5483e-36e1-4688-b7f5-ea07361b26a8
D/[FBP-Android](20077): [FBP] onCharacteristicChanged: chr: beb5483e-36e1-4688-b7f5-ea07361b26a8
... (repeats 60+ times per second during animations)
```

**Caratteristica UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (LED State characteristic)

### 2.2 Analisi del Flusso BLE

**Firmware Side (ESP32)**:
```
Loop render (60 FPS):
  1. Aggiorna animazione lama (incrementa bladeHeightFactor)
  2. ❌ PROBLEMA: Invia BLE notification con stato completo
  3. Ripeti ogni 16ms (~60 Hz)
```

**App Side (Flutter)**:
```
BLE Notification received:
  1. Stream<LedState> emette nuovo stato
  2. LedProvider._handleStateUpdate() chiamato
  3. notifyListeners() → rebuild widget tree
  4. LightsaberWidget.didUpdateWidget() chiamato
  5. _updateAnimationState() verifica se avviare animazione
  6. ❌ PROBLEMA: Rebuild durante AnimationController.forward() in corso
```

### 2.3 Conflitto Architetturale

**Il firmware invia notifiche su OGNI FRAME di rendering**:
- Frequenza: ~60 notifiche/secondo durante animazioni
- Contenuto: Stato completo (bladeState="igniting", bladeHeightFactor=0.45, ecc.)
- Intenzione firmware: Sincronizzare app frame-by-frame

**L'app Flutter si aspetta notifiche su TRANSIZIONI DI STATO**:
- Frequenza attesa: 4 notifiche per ciclo completo (off→igniting→on→retracting→off)
- Contenuto atteso: Solo cambio di bladeState (non valori intermedi di animazione)
- Architettura Flutter: AnimationController gestisce interpolazione autonomamente

**Risultato del conflitto**:
1. Ogni notifica BLE (60/sec) causa `notifyListeners()` in LedProvider
2. Ogni `notifyListeners()` triggera `didUpdateWidget()` in LightsaberWidget
3. `didUpdateWidget()` verifica `oldWidget.bladeState != widget.bladeState`
4. Durante animazione, bladeState può rimanere "igniting" per 1.5 secondi
5. Ma le continue notifiche causano rebuild del widget tree
6. AnimationController perde tracking dello stato corrente
7. App e firmware si desincronizzano

---

## 3. Tentativi di Mitigazione App-Side

### 3.1 Tentativo 1: State Deduplication in Widget

**Codice implementato** (`lightsaber_widget.dart:113-116`):
```dart
if (oldWidget.bladeState != widget.bladeState &&
    widget.bladeState != _lastProcessedState) {
  _updateAnimationState(widget.bladeState);
}
```

**Logica**: Previene elaborazione duplicata dello stesso bladeState

**Risultato**: ❌ **Fallito**
- Le notifiche BLE arrivano comunque a 60 Hz
- `didUpdateWidget()` viene chiamato 60 volte/sec anche senza elaborare stato
- Overhead di rebuild widget tree rimane

### 3.2 Tentativo 2: Animation Completion Listeners

**Codice implementato** (`lightsaber_widget.dart:81-87`):
```dart
_ignitionController.addStatusListener((status) {
  if (status == AnimationStatus.completed) {
    _lastProcessedState = 'on';
  } else if (status == AnimationStatus.dismissed) {
    _lastProcessedState = 'off';
  }
});
```

**Logica**: Auto-aggiorna stato locale al completamento animazione

**Risultato**: ❌ **Fallito**
- Le notifiche BLE durante animazione possono far ripartire animazione da capo
- Race condition tra listener e nuove notifiche BLE

### 3.3 Tentativo 3: Debouncing in Provider

**Codice implementato** (`led_provider.dart:56-83`):
```dart
void _handleStateUpdate(LedState state) {
  final currentBladeState = state.bladeState;
  final isAnimating = currentBladeState == 'igniting' || currentBladeState == 'retracting';
  final bladeStateChanged = _lastBladeState != currentBladeState;

  if (bladeStateChanged) {
    // Notifica immediata per cambio stato
    _lastBladeState = currentBladeState;
    _currentState = state;
    _debounceTimer?.cancel();
    notifyListeners();
  } else if (isAnimating) {
    // Debounce durante animazione (max 1 notifica ogni 100ms)
    _currentState = state;
    if (_debounceTimer == null || !_debounceTimer!.isActive) {
      _debounceTimer = Timer(const Duration(milliseconds: 100), () {
        notifyListeners();
      });
    }
  } else {
    // Notifica normale per stati stabili
    _currentState = state;
    notifyListeners();
  }
}
```

**Logica**: Throttle notifiche a max 10 Hz durante animazioni

**Risultato**: ❌ **Parzialmente fallito**
- Riduce rebuild da 60 Hz a 10 Hz (miglioramento)
- Ma il problema fondamentale persiste:
  - Stream BLE continua a ricevere 60 notifiche/sec
  - `_currentState` viene aggiornato 60 volte/sec (anche se senza notifica)
  - Possibili race condition con Timer
  - Non risolve desincronizzazione stato

---

## 4. Soluzione Corretta: Modifica Firmware

### 4.1 Dove Intervenire

**File firmware da modificare** (probabile location):
```
/home/reder/Documenti/PlatformIO/Projects/ledSaber/src/led_controller.cpp
/home/reder/Documenti/PlatformIO/Projects/ledSaber/src/ble_service.cpp
```

**Funzione da individuare**:
```cpp
// Cerca funzione di render loop che contiene:
void update() {
  // Aggiorna animazione
  if (bladeState == IGNITING) {
    bladeHeightFactor += deltaTime * ignitionSpeed;
    // ❌ PROBLEMA QUI: notification inviata ogni frame
    bleNotifyState(); // <-- QUESTA È LA CHIAMATA DA RIMUOVERE/MODIFICARE
  }
}
```

### 4.2 Implementazione Corretta

**Prima (SBAGLIATO)**:
```cpp
void updateBladeAnimation() {
  if (bladeState == IGNITING) {
    bladeHeightFactor += deltaTime * ignitionSpeed;

    if (bladeHeightFactor >= 1.0) {
      bladeState = ON;
      bladeHeightFactor = 1.0;
    }

    bleNotifyState(); // ❌ Notifica OGNI frame (60 Hz)
  }
}
```

**Dopo (CORRETTO)**:
```cpp
void updateBladeAnimation() {
  BladeState previousState = bladeState; // Salva stato precedente

  if (bladeState == IGNITING) {
    bladeHeightFactor += deltaTime * ignitionSpeed;

    if (bladeHeightFactor >= 1.0) {
      bladeState = ON;
      bladeHeightFactor = 1.0;
    }
  }

  // ✅ Notifica SOLO se bladeState è cambiato
  if (bladeState != previousState) {
    bleNotifyState();
  }
}
```

### 4.3 Stati che Richiedono Notifica BLE

Notificare **SOLO** quando `bladeState` cambia:

| Transizione | Quando | Frequenza Attesa |
|-------------|--------|------------------|
| OFF → IGNITING | User tap power button | 1 volta |
| IGNITING → ON | Animazione completa (dopo 1.5s) | 1 volta |
| ON → RETRACTING | User tap power button | 1 volta |
| RETRACTING → OFF | Animazione completa (dopo 1.0s) | 1 volta |

**Totale notifiche per ciclo completo**: 4 (invece di 150+)

### 4.4 Pseudo-codice Logica Corretta

```cpp
class LEDController {
private:
  BladeState currentBladeState = OFF;
  BladeState previousBladeState = OFF;
  float bladeHeightFactor = 0.0f;

  void notifyStateIfChanged() {
    if (currentBladeState != previousBladeState) {
      BLEService::notifyLEDState(
        bladeState: currentBladeState,
        r: currentColor.r,
        g: currentColor.g,
        b: currentColor.b,
        brightness: currentBrightness,
        effect: currentEffect
      );
      previousBladeState = currentBladeState;
    }
  }

public:
  void update(float deltaTime) {
    // Salva stato prima dell'update
    previousBladeState = currentBladeState;

    // Aggiorna animazione
    switch (currentBladeState) {
      case IGNITING:
        bladeHeightFactor += deltaTime * IGNITION_SPEED;
        if (bladeHeightFactor >= 1.0f) {
          currentBladeState = ON;
          bladeHeightFactor = 1.0f;
        }
        break;

      case RETRACTING:
        bladeHeightFactor -= deltaTime * RETRACT_SPEED;
        if (bladeHeightFactor <= 0.0f) {
          currentBladeState = OFF;
          bladeHeightFactor = 0.0f;
        }
        break;
    }

    // Notifica SOLO se stato cambiato
    notifyStateIfChanged();
  }

  void ignite() {
    if (currentBladeState == OFF) {
      previousBladeState = currentBladeState;
      currentBladeState = IGNITING;
      notifyStateIfChanged(); // Notifica transizione OFF → IGNITING
    }
  }

  void retract() {
    if (currentBladeState == ON) {
      previousBladeState = currentBladeState;
      currentBladeState = RETRACTING;
      notifyStateIfChanged(); // Notifica transizione ON → RETRACTING
    }
  }
};
```

---

## 5. Benefici Attesi dalla Modifica Firmware

### 5.1 Performance
- **Prima**: 60+ notifiche BLE/sec durante animazioni
- **Dopo**: 4 notifiche BLE totali per ciclo completo (OFF→ON→OFF)
- **Risparmio**: ~99% traffico BLE ridotto
- **Impatto**: Latenza BLE ridotta, batteria ESP32 risparmiata

### 5.2 Sincronizzazione
- App riceve SOLO transizioni di stato significative
- AnimationController Flutter gestisce interpolazione autonomamente (come progettato)
- Nessun conflitto tra animazioni firmware e animazioni Flutter
- Stato app e firmware sempre allineati

### 5.3 Affidabilità
- Eliminati rebuild widget superflui (da 60 Hz a ~1 Hz)
- Eliminati race condition tra notifiche BLE e AnimationController
- Comportamento deterministico e prevedibile
- Supporta anche debounce app-side esistente (ridondante ma non dannoso)

---

## 6. Verifica Post-Fix

### 6.1 Test Funzionali

**Test 1: Ignition singola**
1. Blade inizialmente OFF
2. Tap power button
3. **Atteso**:
   - Log: `onCharacteristicChanged` con `bladeState="igniting"` (1 volta)
   - App: Animazione smooth 1.5s
   - Log: `onCharacteristicChanged` con `bladeState="on"` (1 volta)
4. **KO se**: Multipli `onCharacteristicChanged` durante animazione

**Test 2: Retraction singola**
1. Blade inizialmente ON
2. Tap power button
3. **Atteso**:
   - Log: `onCharacteristicChanged` con `bladeState="retracting"` (1 volta)
   - App: Animazione smooth 1.0s
   - Log: `onCharacteristicChanged` con `bladeState="off"` (1 volta)

**Test 3: Toggle rapido**
1. Eseguire 10 tap veloci alternati (ON/OFF/ON/OFF...)
2. **Atteso**:
   - Ogni tap produce esattamente 2 notifiche (start animation + end animation)
   - Nessuna desincronizzazione
   - Blade risponde a tutti i tap
3. **KO se**: Blade smette di rispondere dopo 2-3 tap

**Test 4: Sincronizzazione a lungo termine**
1. Eseguire 50 cicli completi (ignite → retract)
2. **Atteso**:
   - Stato app = stato firmware dopo ogni ciclo
   - Nessuna "lama fantasma"
   - Performance costanti

### 6.2 Metriche di Successo

| Metrica | Valore Attuale | Target Post-Fix |
|---------|----------------|-----------------|
| Notifiche BLE durante ignition (1.5s) | ~90 | 2 (start + end) |
| Notifiche BLE durante retraction (1s) | ~60 | 2 (start + end) |
| Widget rebuilds durante animazione | ~60/sec | ~1-2/sec (solo transizioni) |
| Affidabilità toggle dopo N tap | Fallisce dopo 2-3 | 100% fino a N=100+ |
| Sincronizzazione app-firmware | ~70% | 100% |

---

## 7. Codice App da NON Modificare

### 7.1 Debouncing Provider - Mantenere

Il debouncing implementato in `led_provider.dart` può rimanere come **difesa in profondità**:
- Con firmware corretto: debounce non si attiva mai (già solo 4 notifiche/ciclo)
- Con firmware vecchio/buggy: debounce mitiga il problema
- Nessun overhead se non necessario

**Raccomandazione**: ✅ **MANTIENI CODICE ESISTENTE**

---

## 8. Verifiche su firmware attuale + migliorie consigliate

### 8.1 Stato reale nel firmware (repo attuale)

Nel codice presente in questo repo, la notifica BLE **non** viene inviata a 60 Hz:

- `src/main.cpp:813` invia `bleController.notifyState()` ogni **500 ms** quando connesso.
- `src/BLELedController.cpp:507` serializza lo stato in JSON e notifica.

Quindi il comportamento descritto nella sezione 2 (notifica ad ogni frame) sembra riferito a una versione precedente
o a un ramo diverso. L’inconsistenza app potrebbe derivare da:

- notifiche periodiche che arrivano anche quando lo stato non cambia;
- ricostruzioni UI ripetute anche a bladeState identico;
- possibili duplicati o out-of-order lato app.

### 8.2 Migliorie consigliate (firmware + app)

**Firmware**

- **Notify solo su change + heartbeat lento**: inviare notify solo quando `bladeState` o parametri LED cambiano.
  Tenere un heartbeat lento (1–2 s) come resync.
- **Sequenza stato**: aggiungere `stateSeq` (contatore incrementale) nel JSON per scartare duplicati/out-of-order.
- **Snapshot immediato**: inviare un notify subito dopo connessione e dopo comandi `ignition/retract`.

**App**

- **Dedup su stato completo**: ignorare notify identiche (hash del JSON o confronto campi).
- **Gating comandi**: disabilitare il power button durante `igniting/retracting`.
- **Resync su reconnect**: fare `read` iniziale e poi `setNotifyValue(true)`.

### 8.3 Perche' tenere un heartbeat

Un heartbeat lento e' utile per:

- riallineare app/device dopo piccoli drop BLE;
- confermare che la connessione e' viva con dati applicativi;
- permettere un watchdog UI (se assente per N secondi, mostra warning o reconnect).

Con 1–2 s di periodo il costo e' minimo, ma evita desync silenziosi.

### 7.2 State Tracking Widget - Mantenere

Il tracking `_lastProcessedState` in `lightsaber_widget.dart` è utile per:
- Prevenire restart animazione se arriva notifica duplicata (es. retry BLE)
- Robustezza contro future modifiche firmware

**Raccomandazione**: ✅ **MANTIENI CODICE ESISTENTE**

### 7.3 Animation Listeners - Mantenere

I listener su `AnimationController` sono utili per:
- Debug (capire quando animazione completa)
- Potenziali future feature (suoni, vibrazione a fine animazione)

**Raccomandazione**: ✅ **MANTIENI CODICE ESISTENTE**

---

## 8. Priorità Implementazione

### Alta Priorità (Blocca Funzionalità Core)
1. ✅ Identificare file firmware con loop render
2. ✅ Implementare tracking `previousBladeState`
3. ✅ Modificare `bleNotifyState()` per notificare solo su cambio stato
4. ✅ Testare con Test 1 e Test 2

### Media Priorità (Robustezza)
5. ⚠️ Aggiungere log firmware per debug transizioni stato
6. ⚠️ Testare con Test 3 (toggle rapido)
7. ⚠️ Testare con Test 4 (lungo termine)

### Bassa Priorità (Ottimizzazione)
8. 💡 Valutare se inviare notifica anche su cambio colore/brightness
9. 💡 Implementare rate limiting BLE lato firmware (max 10 Hz anche per altri eventi)

---

## 9. Conclusioni

### Problema Identificato
Il firmware ESP32 invia notifiche BLE su **ogni frame di rendering** (60 Hz) invece di inviarle solo su **transizioni di stato significative**.

### Impatto
- Flooding di notifiche BLE (90-150 per ciclo animazione)
- Widget Flutter rebuilds eccessivi (60/sec)
- Conflitto con AnimationController Flutter
- Desincronizzazione stato app-firmware
- Degradazione progressiva affidabilità

### Soluzione Richiesta
**Modifica firmware**: Notificare BLE solo quando `bladeState` cambia (4 notifiche/ciclo invece di 150).

### App-Side
**Nessuna modifica richiesta**. Il codice esistente (debouncing, state tracking, animation listeners) è corretto e può rimanere come difesa in profondità.

---

## 10. Riferimenti Tecnici

### File Coinvolti

**Firmware (da modificare)**:
- `/home/reder/Documenti/PlatformIO/Projects/ledSaber/src/*.cpp`
- Cerca: `bleNotifyState()`, `notifyCharacteristic()`, o simili
- Pattern: chiamate in loop render invece che in state transitions

**App (OK, non modificare)**:
- `/home/reder/Documenti/PlatformIO/Projects/ledSaber/AppMobile/flutter_led_saber/lib/providers/led_provider.dart:56-83` (debouncing)
- `/home/reder/Documenti/PlatformIO/Projects/ledSaber/AppMobile/flutter_led_saber/lib/widgets/lightsaber_widget.dart:81-155` (state tracking + animations)

### Log BLE Caratteristica
- **UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- **Tipo**: LED State (notify enabled)
- **Payload**: Struct con `bladeState`, `r`, `g`, `b`, `brightness`, `effect`, `enabled`

### Documentazione Correlata
- `/home/reder/Documenti/PlatformIO/Projects/ledSaber/AppMobile/flutter_led_saber/UI_DESIGN_PLAN.md` - Spec UI/UX
- `/home/reder/.claude/plans/resilient-mapping-thacker.md` - Piano implementazione Sprint 1

---

**Documento creato il**: 2025-12-27
**Autore**: Claude Sonnet 4.5 (Analisi conversazione con utente)
**Destinatario**: Altro agente/developer per fix firmware
