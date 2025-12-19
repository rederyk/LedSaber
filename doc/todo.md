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

  - l animazione di ignition durante l accensione si interrompe a meta e viene rispodotta due volte,
  mentre duranete le gesture viene interrotta ma solo una volta riprodotta

  -  pulse e troppo sfrafalloso diminuisci molto l ombra e di poco il pulse in luncghezza numero di led , mentre aumentane l intesita per renderlo piu vivace e definito  