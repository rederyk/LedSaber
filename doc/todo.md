## ✅ COMPLETATI (Gruppo 1 - Effetti Rendering)

- ✅ unstable: mapping invertito, perturbazioni scure/spente, più contrasto
  - Implementato in `LedEffectEngine.cpp:360-372`
  - Mapping invertito: heat alto = scuro (60-255 range)

- ✅ dual_pulse: più difficile da scurire, più luminoso quando chiaro
  - Implementato in `LedEffectEngine.cpp:454-491`
  - Base brightness: 150→180, peak: 255→200
  - Motion darkening ridotto (30→15), brightening aumentato (60→80)

- ✅ pulse: seamless con carica plasma dalla base e folded index
  - Implementato in `LedEffectEngine.cpp:377-466`
  - Fase CHARGING: carica piano alla base (primi 10 LED)
  - Fase TRAVELING: esce completamente dalla punta
  - Ritmo basato su foldPoint per sincronizzazione


## ✅ COMPLETATI (Gruppo 2 - Lifecycle Management)

- ✅ ignition: lanciare per un ciclo solo all'avvio e alla connessione BLE
  - Implementato in `LedEffectEngine.h/cpp`, `main.cpp`
  - Variabili: `_ignitionOneShot`, `_ignitionCompleted`
  - Metodo: `triggerIgnitionOneShot()`
  - Trigger: all'avvio (setup) e su BLE connect (MainServerCallbacks)

- ✅ retraction: implementare per spegnimento futuro
  - Implementato in `LedEffectEngine.h/cpp`
  - Variabili: `_retractionOneShot`, `_retractionCompleted`
  - Metodo: `triggerRetractionOneShot()`
  - Pronto per trigger su disconnect/shutdown


## ✅ COMPLETATI (Gruppo 3 - Rainbow Refactor)

- ✅ rainbow_effect: lama bianca perturbata con colori per direzione
  - Implementato in `LedEffectEngine.h/cpp`
  - Funzione: `renderRainbowEffect()`, `getHueFromDirection()`
  - Lama bianca base + blend con colori basati su direzione movimento
  - Luminosity boost per colori più brillanti

- ✅ pulse fix: accelerazione con perturbazioni senza spegnimento
  - Modificato `renderPulse()` in `LedEffectEngine.cpp:383-500`
  - Calcolo global perturbation per accelerare movimento
  - Brightness minima aumentata (100 invece di 80)
  - Intensification boost fino a +120 brightness
  - Nessun salto di impulsi, flusso continuo

- ✅ ignition animation bugfix: risolti problemi interruzione e doppia riproduzione
  - Implementato in `LedEffectEngine.cpp:826,829,852-853,863-864`
  - Bug 1: Reset flags one-shot nelle gesture triggers (permettono riproduzione dopo boot)
  - Bug 2: Timeout aumentato da 2s a 5s (previene interruzione animazione)
  - Risultato: animazione completa sia all'accensione che durante gesture


## ✅ COMPLETATI (Gruppo 4 - Pulse Refinement)

- ✅ pulse: riduzione sfarfallio e miglioramento definizione
  - Implementato in `LedEffectEngine.cpp:408-570`
  - Lunghezza impulso inversamente proporzionale a velocità (20→3 pixel)
  - Dark base brightness: 40 (ottimizzato per contrasto)
  - Perturbazione aumenta velocità fino a 3x con curva logaritmica
  - Flusso continuo senza interruzioni

- ✅ pulse: spawn secondari indipendenti con curva di velocità plasma
  - Implementato in `LedEffectEngine.h:87-95`, `LedEffectEngine.cpp:478-532`
  - **Secondary pulses**: fino a 5 pulse secondari attivi contemporaneamente
  - **Spawn indipendente**: appaiono in posizioni casuali (logica tipo unstable)
  - **Curva velocità plasma**:
    - Fase 0-85: partenza veloce (1-3ms per pixel)
    - Fase 86-170: rallentamento (3-12ms per pixel)
    - Fase 171-255: esplosione massima velocità (1ms per pixel)
  - **Spawn dinamico**: probabilità aumenta con velocità e movimento
  - Pulse secondari più piccoli (metà larghezza) e leggermente meno luminosi (200 vs 255)




##
  - forzare free heap and disable camera or other thing block ota, also a safer mode to reboot in if no memory is an idea,

  ```

Successfully created esp32 image.
================================================= [SUCCESS] Took 16.97 seconds =================================================


╔═══════════════════════════════════════╗
║     🔧 LEDSABER OTA UPDATER 🔧        ║s
╚═══════════════════════════════════════╝

    
💾 Versione firmware locale: b69f5ab-dirty
🗂 Firmware copiato in temp: /tmp/ledsaber_ota_fix98hfe/firmware.bin
📝 Versione salvata in: /tmp/ledsaber_ota_fix98hfe/firmware_version.txt
🔍 Scansione dispositivi LedSaber...
✓ Trovato: LedSaber-BLE (7C:9E:BD:65:BE:2E)
🔁 Connessione automatica a LedSaber-BLE (7C:9E:BD:65:BE:2E)
📡 Connessione a 7C:9E:BD:65:BE:2E...
✓ Connesso!
📊 MTU riportato da Bleak: 517 bytes
📦 Versione firmware corrente: e83c3c7-dirty

✗ Errore OTA: Insufficient memory
✓ Notifiche OTA abilitate

⚠ ATTENZIONE: Durante l'aggiornamento NON spegnere il dispositivo!
✓ Flag -YY attivo: procedo automaticamente con l'aggiornamento

📤 Upload firmware:
  File: firmware.bin
  Dimensione: 1405744 bytes (1372.80 KB)
  Versione target: b69f5ab-dirty
📡 Invio comando START...

✗ Errore OTA: Insufficient memory
⏳ Attendo che il dispositivo elabori il comando START...

✗ Errore OTA: Insufficient memory
✗ Errore durante START: Insufficient memory

✗ Aggiornamento fallito
📡 Disconnesso

👋 Completato!

```
