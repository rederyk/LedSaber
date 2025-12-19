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


## 🔮 FUTURO (Gruppo 3 - Rainbow Refactor)

- ⏳ rainbow_effect: deprecare rainbow base, creare nuovo effetto
  - File da modificare: `LedEffectEngine.cpp`, `BLELedController.cpp`
  - Lama bianca perturbata con tutti i colori in base alla direzione
  - Colori più luminosi con base bianca


- pulse sembra spegnersi  e rompere il ritmo quando perturbato invece dovrebbe accendersi di piu e velocizzare il flusso senza saltare impulsi 