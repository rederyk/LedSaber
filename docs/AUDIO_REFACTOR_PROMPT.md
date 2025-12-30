# 🎯 Prompt per Refactoring Architettura Audio

## Obiettivo

Pulire e sistemare l'architettura del sistema audio della LedSaber app, eliminando race conditions, migliorando la sincronizzazione con l'UI, e rendendo il codice più robusto e manutenibile.

---

## 📋 Prompt per Agent

```
Analizza e refactora il sistema audio dell'app Flutter LedSaber seguendo questi requisiti:

## CONTESTO
L'app controlla una spada laser via Bluetooth. Il sistema audio ha tre layer:
1. **HUM base** (loop continuo quando lama accesa)
2. **SWING** (modulazione theremin in tempo reale basata su movimento)
3. **EVENTI** (ignition, retract, clash - one-shot)

Il sistema FUNZIONA ma ha problemi architetturali:
- Race conditions tra stopHum() e updateSwing()
- Logica di sincronizzazione sparsa tra AudioProvider e AudioService
- Stato duplicato (_humPlaying, _swingPlaying, _lastBladeState)
- Gestione async/await inconsistente
- Troppi print() di debug

## FILE DA ANALIZZARE
- lib/services/audio_service.dart
- lib/providers/audio_provider.dart
- lib/models/sound_pack.dart
- docs/SOUND_ROADMAP.md (per capire l'architettura desiderata)

## PROBLEMI DA RISOLVERE

### 1. Race Condition Management
**Problema attuale:**
```dart
// stopHum() è async, quindi _humPlaying rimane true durante await
Future<void> stopHum() async {
  _humPlaying = false;  // Fix temporaneo: impostato PRIMA
  await _humPlayer.stop();
}
```

**Cosa fare:**
- Valuta se separare "stato logico" da "stato player"
- Considera un pattern State Machine per gestire transizioni blade state
- Elimina possibilità di race condition tra stop/start

### 2. Sincronizzazione Blade State ↔ Audio
**Problema attuale:**
```dart
// AudioProvider.syncWithBladeState() ha logica complessa con isFirstSync
void syncWithBladeState(String? bladeState) {
  final isFirstSync = _lastBladeState == null;
  if (bladeState == _lastBladeState && !isFirstSync) return;

  switch (bladeState) {
    case 'igniting': ...
    case 'on': ...
    case 'retracting': ...
    case 'off': ...
  }
}
```

**Cosa fare:**
- Semplifica la logica di sincronizzazione
- Considera un pattern Observer più pulito
- Rimuovi logica "isFirstSync" se possibile (es. inizializzare con stato corretto)

### 3. Gestione Stato
**Problema attuale:**
- `_humPlaying` in AudioService
- `_swingPlaying` in AudioService
- `_lastBladeState` in AudioProvider
- Stato sparso senza single source of truth

**Cosa fare:**
- Centralizza stato audio in un unico posto (considera AudioState class)
- Usa pattern immutabile per state updates
- Considera Cubit/Bloc per gestione stato se appropriato

### 4. Async/Await Consistency
**Problema attuale:**
```dart
// Mix di void e Future<void>
void syncWithBladeState(String? bladeState) {  // void
  _audioService.stopHum();  // Future<void> chiamato senza await
  _audioService.playRetract();  // Future<void> chiamato senza await
}
```

**Cosa fare:**
- Definisci pattern chiaro: quando usare await vs fire-and-forget
- Documenta perché alcune chiamate sono non-blocking
- Gestisci errori in modo consistente

### 5. Debug Logging
**Problema attuale:**
- print() sparsi ovunque
- Nessun livello di log (debug, info, error)
- Log non disabilitabili in production

**Cosa fare:**
- Introduci logger package (es. logger, loggy)
- Definisci livelli di log appropriati
- Rendi log configurabili (disabilita in release)

## REQUISITI ARCHITETTURALI

### Must Have
✅ **GARANZIA:** Impossibile avere UI spenta + audio attivo
✅ **GARANZIA:** Impossibile avere UI accesa + audio fermo
✅ **Sincronizzazione atomica** tra blade state e audio state
✅ **No race conditions** tra stop e start
✅ **Swing theremin continuo** (no restart del file)
✅ **Connessione con lama già accesa** funziona correttamente

### Should Have
- Codice leggibile e manutenibile
- Pattern architetturali consistenti
- Gestione errori robusta
- Logging configurabile
- Testing più facile (separazione concerns)

### Nice to Have
- State machine esplicita per blade transitions
- Dependency injection per AudioService
- Unit tests per logica sincronizzazione
- Integration tests per scenari critici

## OUTPUT RICHIESTO

### 1. Analisi Problemi
Documenta i problemi architetturali trovati con:
- Descrizione problema
- Codice attuale problematico
- Impatto (critical/high/medium/low)

### 2. Proposta Architetturale
Proponi una nuova architettura con:
- Diagramma stato/flusso (testuale va bene)
- Nuova struttura file/classi se necessario
- Pattern utilizzati (Observer, State Machine, etc.)
- Benefici vs architettura attuale

### 3. Piano Implementazione
Fornisci step-by-step plan:
1. Quali file modificare
2. Cosa cambiare in ordine
3. Come testare ogni step
4. Backward compatibility se necessario

### 4. Codice Refactorato
Implementa il refactoring mantenendo:
- ✅ Funzionalità esistenti (no regressioni)
- ✅ Garanzie architetturali (vedi Must Have)
- ✅ Compatibilità con resto dell'app
- ✅ Documentazione inline dove necessario

## VINCOLI

### NON modificare
- Libreria just_audio (già in uso)
- Struttura file audio (hum_base.wav per hum+swing)
- API pubbliche chiamate da altri provider (es. updateSwing)
- Logica modulazione theremin (funziona bene)

### MANTIENI
- Swing theremin continuo (no restart)
- Race condition fix (_humPlaying = false PRIMA di await)
- Primo sync con lama già accesa
- Stop immediato in retracting

### MIGLIORA
- Architettura generale
- Separazione concerns
- Gestione stato
- Logging
- Testabilità

## ESEMPIO OUTPUT

```markdown
# Audio System Refactoring

## Problems Found

### 1. Race Condition in State Management [CRITICAL]
**Current:**
```dart
Future<void> stopHum() async {
  _humPlaying = false;  // Workaround
  await _humPlayer.stop();
}
```

**Issue:** Mixing boolean state with async operations creates timing issues

**Proposed:**
```dart
class AudioState {
  final bool humActive;
  final bool swingActive;
  // Immutable state
}

void stopHum() {
  _setState(humActive: false);  // Sync
  _humPlayer.stop().catchError(...);  // Fire and forget
}
```

### 2. Complex Synchronization Logic [HIGH]
...

## Proposed Architecture

### State Machine
```
OFF -> IGNITING -> ON -> RETRACTING -> OFF
 |                  |
 +------ (direct) --+
```

### New Classes
- AudioState (immutable)
- AudioStateMachine (transitions)
- AudioService (player management)
- AudioProvider (bridge to UI)

...
```

## DOMANDE DA CONSIDERARE

1. È meglio un AudioState immutabile o mantenere stato mutabile?
2. Conviene introdurre un State Machine esplicito o mantenere switch/case?
3. Come gestire meglio l'async: fire-and-forget vs await everywhere?
4. Serve dependency injection per AudioService o va bene singleton?
5. Come rendere il codice più testabile senza framework pesanti?
6. Vale la pena introdurre Cubit/Bloc o è overkill per questo caso?

## DELIVERABLES

1. **Documento analisi** (markdown)
2. **Codice refactorato** (file .dart modificati)
3. **Guida migrazione** (cosa è cambiato per developer)
4. **Test checklist** (scenari da testare dopo refactor)

## SUCCESS CRITERIA

✅ Tutti i log di test mostrano sincronizzazione corretta
✅ Nessun warning o errore di compilazione
✅ Codice più leggibile (feedback peer review)
✅ Garanzie architetturali mantenute
✅ Meno di 50% aumento LoC (no over-engineering)
```

---

## 📚 File di Riferimento

Fornisci all'agente accesso a questi file:

### Core Implementation
- `AppMobile/flutter_led_saber/lib/services/audio_service.dart`
- `AppMobile/flutter_led_saber/lib/providers/audio_provider.dart`
- `AppMobile/flutter_led_saber/lib/models/sound_pack.dart`

### Documentation
- `docs/SOUND_ROADMAP.md` - Architettura desiderata
- `docs/SWING_THEREMIN_IMPLEMENTATION.md` - Dettagli theremin
- `docs/AUDIO_SYNC_DEBUG.md` - Problemi noti e fix
- `docs/AUDIO_FIX_FINAL.md` - Ultimo fix race condition

### Related Files (per contesto)
- `AppMobile/flutter_led_saber/lib/providers/motion_provider.dart`
- `AppMobile/flutter_led_saber/lib/models/motion_state.dart`

---

## 🎯 Focus Areas Priority

1. **[HIGH]** Eliminare race conditions definitivamente
2. **[HIGH]** Semplificare logica syncWithBladeState
3. **[MEDIUM]** Centralizzare gestione stato
4. **[MEDIUM]** Logging professionale
5. **[LOW]** Pattern architetturali avanzati (solo se migliorano leggibilità)

---

## ⚠️ Anti-Patterns da Evitare

❌ Over-engineering (es. 10 classi per gestire 3 stati)
❌ Breaking changes alle API pubbliche senza motivo
❌ Rimuovere fix esistenti che funzionano
❌ Introdurre dipendenze pesanti (es. GetX, Riverpod) se non necessario
❌ Cambiare logica modulazione theremin (funziona bene)

---

## ✅ Good Patterns da Considerare

✅ Single Responsibility Principle
✅ Immutable state dove possibile
✅ Clear separation: business logic vs platform calls
✅ Explicit error handling
✅ Self-documenting code (naming, structure)
✅ Testability (dependency injection leggera)

---

## 📊 Expected Outcome

Dopo il refactoring, il codice dovrebbe:

1. **Essere più robusto:** Zero race conditions possibili
2. **Essere più chiaro:** Logica sincronizzazione evidente
3. **Essere più testabile:** Unit tests fattibili
4. **Mantenere funzionalità:** Nessuna regressione
5. **Mantenere performance:** Stesso comportamento runtime

**Tempo stimato:** 2-4 ore per refactoring completo + testing

---

## 🚀 Come Usare Questo Prompt

1. Copia il contenuto della sezione "Prompt per Agent"
2. Fornisci accesso ai file elencati in "File di Riferimento"
3. L'agente analizzerà e proporrà refactoring
4. Rivedi la proposta prima di implementare
5. Testa ogni step del piano di implementazione
6. Commit incrementali (non tutto in una volta)

---

**Note:** Questo è un refactoring architetturale, NON un rewrite. Mantieni ciò che funziona, migliora ciò che è problematico.
