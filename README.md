# LedSaber - Spada Laser Smart con Firmware Avanzato

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Questo progetto nasce dall'idea di creare una semplice lampada e si è evoluto in una spada laser a LED completamente funzionale e altamente personalizzabile. È basata su un ESP32-CAM e offre un'ampia gamma di effetti visivi reattivi, aggiornamenti firmware via Bluetooth (BLE OTA) e un sistema di rilevamento del movimento innovativo basato su sensore ottico.

**Guarda il video dimostrativo:** [Link al tuo video su YouTube]

## Funzionalità Principali

*   **Effetti Visivi Avanzati:** Engine personalizzato con effetti reattivi come `flicker`, `unstable`, `pulse`, e una simulazione fisica per l'effetto `dual_pulse` (pong).
*   **Rilevamento del Movimento con Optical Flow:** Utilizza la camera dell'ESP32-CAM per un rilevamento del movimento preciso e a basso costo, un'alternativa innovativa ai classici accelerometri.
*   **Firmware Over-The-Air (OTA):** Aggiorna il firmware della spada laser direttamente dal tuo smartphone o PC tramite Bluetooth Low Energy, senza bisogno di cavi.
*   **Controllo Completo via BLE:** Un'app client può controllare ogni aspetto della spada: colore, luminosità, effetto, velocità e parametri specifici.
*   **Gestione della Configurazione:** Le impostazioni vengono salvate sulla memoria flash (LittleFS) e ricaricate all'avvio.
*   **Architettura Robusta:** Il codice è organizzato in classi e manager (es. `LedEffectEngine`, `OTAManager`, `CameraManager`), seguendo i principi di un'ingegneria del software solida e manutenibile.

## Tecnologie Utilizzate

*   **Hardware:** ESP32-CAM
*   **Framework:** Arduino
*   **Librerie Principali:**
    *   `FastLED` per la gestione della striscia LED (WS2812B).
    *   `ArduinoJson` per la comunicazione BLE.
    *   ESP32 BLE Stack per i servizi GATT.
*   **Sistema Operativo:** FreeRTOS per la gestione di task paralleli (es. cattura video).

## Installazione e Uso

1.  **Clona il repository:** `git clone [URL del tuo repository]`
2.  **Apri con PlatformIO:** Il progetto è configurato per PlatformIO. Apri la cartella con VSCode e l'estensione PlatformIO.
3.  **Compila e Carica:** Connetti il tuo ESP32-CAM e usa il comando "Upload" di PlatformIO.
4.  **Connettiti via BLE:** Usa un'app come "nRF Connect for Mobile" per connetterti al device "LedSaber-BLE" ed esplorare i servizi e le caratteristiche, oppure installa l app flutter ancora in test.

## Licenza

Questo progetto è rilasciato con una **doppia licenza**.

### 1. Licenza Open Source (GNU General Public License v3.0)

Sei libero di usare, modificare e distribuire questo software per progetti personali e open source, a condizione di rispettare i termini della licenza GPLv3. In breve, qualsiasi lavoro derivato e distribuito deve essere anch'esso rilasciato sotto licenza GPLv3.

Il testo completo della licenza è disponibile nel file LICENSE.

### 2. Licenza Commerciale

Se desideri utilizzare questo software (o parti di esso) all'interno di un prodotto commerciale a codice chiuso, senza essere vincolato dagli obblighi della GPLv3, è necessario acquistare una licenza commerciale.

Per informazioni sulla licenza commerciale, per favore contattami a: **rederyk88@gmail.com**.
