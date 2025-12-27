# Controllo Dinamico Gesture tramite BLE

## Problema
Durante alcuni effetti (es. `chrono_hybrid`, `rainbow`, ecc.) è utile disattivare temporaneamente le gesture (ignition, retract, clash) per evitare interruzioni indesiderate, mantenendo però il motion detection attivo per gli effetti che lo utilizzano.

## Soluzione Implementata

### Firmware (ESP32)

È stato aggiunto il parametro `gesturesEnabled` al protocollo BLE Motion Config, permettendo di abilitare/disabilitare dinamicamente il gesture recognition senza toccare le soglie o il motion detection globale.

#### Modifiche al Firmware

**File modificato**: `src/BLEMotionService.cpp`

1. **Aggiunto campo `gesturesEnabled` in lettura** (`_getConfigJson()`):
   ```cpp
   if (_processor) {
       const MotionProcessor::Config& cfg = _processor->getConfig();
       doc["gesturesEnabled"] = cfg.gesturesEnabled;  // ← NUOVO
       doc["gestureIgnitionIntensity"] = cfg.ignitionIntensityThreshold;
       // ... altri campi ...
   }
   ```

2. **Aggiunto supporto scrittura** (`ConfigCallbacks::onWrite()`):
   ```cpp
   const bool hasGesturesEnabled = !doc["gesturesEnabled"].isNull();

   if (_service->_processor && (hasGesturesEnabled || ...)) {
       MotionProcessor::Config cfg = _service->_processor->getConfig();
       if (hasGesturesEnabled) {
           cfg.gesturesEnabled = (bool)doc["gesturesEnabled"];
       }
       // ... altri campi ...
       _service->_processor->setConfig(cfg);
   }
   ```

#### Protocollo BLE Aggiornato

**Characteristic**: Motion Config (`aff7d6e5-0d32-4c5a-ac6e-3456789012ce`)
**Operazioni**: READ, WRITE
**Formato**: JSON

**Esempio JSON completo**:
```json
{
  "enabled": true,
  "quality": 160,
  "motionIntensityMin": 15,
  "motionSpeedMin": 1.2,
  "gesturesEnabled": false,          ← NUOVO PARAMETRO
  "gestureIgnitionIntensity": 14,
  "gestureRetractIntensity": 15,
  "gestureClashIntensity": 12,
  "effectMapUp": "",
  "effectMapDown": "",
  "effectMapLeft": "",
  "effectMapRight": "",
  "debugLogs": false
}
```

### Parametri Config

- **`enabled`** (bool): Abilita/disabilita il motion detection globale
- **`gesturesEnabled`** (bool): **NUOVO** - Abilita/disabilita gesture recognition (ignition, retract, clash). Quando `false`, il motion detection rimane attivo ma le gesture non vengono rilevate
- **`quality`** (0-255): Qualità rilevamento optical flow
- **`motionIntensityMin`** (0-255): Soglia minima intensità
- **`motionSpeedMin`** (float): Soglia minima velocità
- Altri parametri gesture threshold...

## Come Usare da App Mobile

### Esempio Flutter (pseudo-codice)

```dart
// Disattiva gesture durante effetto chrono_hybrid
Future<void> disableGestures() async {
  final config = {
    "gesturesEnabled": false
  };
  await motionConfigChar.write(
    utf8.encode(jsonEncode(config)),
    withoutResponse: false
  );
}

// Riattiva gesture
Future<void> enableGestures() async {
  final config = {
    "gesturesEnabled": true
  };
  await motionConfigChar.write(
    utf8.encode(jsonEncode(config)),
    withoutResponse: false
  );
}
```

### Strategia Consigliata per App

1. **Leggere la configurazione corrente** al caricamento effect screen
2. **Mostrare toggle UI** "Enable Gestures" con stato sincronizzato
3. **Per effetti specifici** (es. chrono_hybrid), suggerire all'utente di disattivare le gesture oppure disattivarle automaticamente con conferma
4. **Salvare preferenza** per effetto in local storage

## Vantaggi

✅ **Controllo granulare**: Disattiva solo gesture, non tutto il motion detection
✅ **Nessun side-effect**: Mantiene le soglie configurate, non c'è bisogno di re-impostarle
✅ **Reversibile**: Basta un toggle per riattivare
✅ **Dinamico**: Modificabile in tempo reale durante effetto attivo

## Casi d'Uso

### Caso 1: Effetto Chrono Clock
L'effetto chrono_hybrid usa il motion per perturbazioni ma non vuole essere interrotto da gesture clash.

**Soluzione**: Disattiva `gesturesEnabled` all'attivazione dell'effetto, riattiva all'uscita.

### Caso 2: Effetto Rainbow con Motion
L'effetto rainbow può usare il motion per variazioni dinamiche, ma le gesture ignition/retract potrebbero confondere.

**Soluzione**: Disattiva gesture quando rainbow è attivo.

### Caso 3: Modalità "Locked"
L'utente vuole evitare accensioni/spegnimenti accidentali durante trasporto o esibizioni.

**Soluzione**: Disattiva `gesturesEnabled` tramite toggle in settings.

## Backward Compatibility

✅ **Firmware esistenti** (senza questa patch): Il campo `gesturesEnabled` viene ignorato (default `true`)
✅ **App esistenti**: Il comportamento rimane invariato se non si scrive il campo

## Testing

Per testare la funzionalità:

1. **Flash firmware** con modifiche
2. **Connetti via BLE** con app/script Python
3. **Leggi Motion Config**: Verifica che `gesturesEnabled` sia presente nel JSON
4. **Scrivi `{"gesturesEnabled": false}`**: Verifica che gesture non vengano più rilevate (controllare Motion Events)
5. **Scrivi `{"gesturesEnabled": true}`**: Verifica che gesture tornino a funzionare

## File Modificati

- `src/BLEMotionService.cpp` - Aggiunti parsing e serializzazione `gesturesEnabled`
- `AppMobile/flutter_led_saber/BLE_API_SPECIFICATION.md` - Documentazione protocollo BLE aggiornata

## Note Implementative

Il parametro `gesturesEnabled` è già presente nel `MotionProcessor::Config` (src/MotionProcessor.h:37) con default `true`. Questa modifica espone semplicemente il parametro esistente tramite BLE, senza modificare la logica interna del gesture processor.

## Prossimi Step (App Mobile)

1. Aggiungere model/service per Motion Config in Flutter
2. Implementare toggle UI in Settings o Effect Screen
3. Salvare preferenza gesture per effetto in SharedPreferences
4. Testare con firmware aggiornato

---

**Data**: 2024-12-27
**Autore**: Claude
**Status**: ✅ Implementato e compilato con successo
