# Guida LedSaber

## Indice

1. Panoramica
2. Hardware e requisiti
3. Funzioni firmware e architettura
4. Effetti LED disponibili
5. Build e primo flash via USB (PlatformIO)
6. Aggiornamento firmware via BLE (OTA)
7. Dashboard: avvio e comandi
8. Struttura progetto e file utili
9. Licenza

## 1. Panoramica

LedSaber e' un firmware Arduino per ESP32-CAM che pilota una striscia WS2812B (nel progetto sono previsti 144 LED su GPIO 13) e comunica via BLE. Il controllo include colore, luminosita', effetto e parametri specifici, con salvataggio delle impostazioni in LittleFS. La parte motion sfrutta la camera per stimare il movimento (optical flow) e fornisce eventi/telemetria via BLE. Il firmware integra anche un servizio OTA via BLE per aggiornare il binario senza cavo. Sul lato PC e' disponibile una dashboard Python (Textual + Bleak) per monitoraggio live e comandi.

## 2. Hardware e requisiti

- Scheda: ESP32-CAM.
- LED: striscia WS2812B (144 LED, GPIO 13).
- Alimentazione: 5V con adeguata corrente per la striscia LED (il firmware imposta un limite dinamico con FastLED).
- PC: PlatformIO (VSCode) per build/flash USB.
- BLE su PC: stack Bluetooth funzionante (es. Linux con BlueZ).
- Python 3 per dashboard e strumenti BLE (dipendenze: `textual`, `rich`, `bleak`).

## 3. Funzioni firmware e architettura

- BLE GATT per controllo LED: colore, luminosita', effetto e parametri specifici.
- Servizio camera e motion: la camera fornisce metriche e motion detection (optical flow).
- OTAManager: servizio BLE dedicato all'aggiornamento firmware.
- Configurazione persistente: valori salvati in LittleFS e ricaricati all'avvio.
- Gestione task: FreeRTOS per camera/motion e render LED.

## 4. Effetti LED disponibili

Gli effetti esposti via BLE (lista dal firmware) includono:

- `solid` (colore fisso)
- `rainbow` (arcobaleno)
- `pulse` (onda)
- `breathe` (respirazione)
- `flicker` (flicker stile Kylo)
- `unstable` (instabile avanzato)
- `dual_pulse` (doppio impulso)
- `dual_pulse_simple` (doppio impulso semplice)
- `rainbow_blade` (arcobaleno lungo la lama)
- `chrono_hybrid` (orologio con temi ore/secondi)

Nota: esistono anche effetti manuali per comando diretto (`ignition`, `retraction`, `clash`) usati come gesture/azione.

## 5. Build e primo flash via USB (PlatformIO)

1. Apri il progetto con VSCode + PlatformIO.
2. Verifica la board in `platformio.ini` (`env:esp32cam`).
3. Collega l'ESP32-CAM via USB.
4. Compila e carica:

```bash
pio run
pio run -t upload
```

Le impostazioni di upload seriale sono in `platformio.ini` (`upload_port`, `upload_speed`).

## 6. Aggiornamento firmware via BLE (OTA)

Il firmware espone un servizio BLE OTA dedicato. Per aggiornare:

1. Compila il firmware con PlatformIO (produce `firmware.bin`).
2. Esegui lo script OTA:

```bash
./venv/bin/python ./update_saber.py ./.pio/build/esp32cam/firmware.bin
```

Opzionale: `-YY` per autoconferma upload e riavvio.

Lo script scansiona i dispositivi LedSaber, si connette, invia il firmware in chunk BLE, verifica e chiede il riavvio finale. Se conosci l'indirizzo BLE, puoi passarlo come secondo argomento per connessione diretta.

## 7. Dashboard: avvio e comandi

Per la dashboard TUI (Textual):

```bash
./start_dashboard.sh
```

Il launcher crea/attiva il venv, installa dipendenze e avvia `saber_dashboard.py`.

Shortcut principali:
- `Ctrl+S`: scan & connect
- `Ctrl+D`: disconnect
- `F2`: camera init
- `F3`: camera start
- `F4`: camera stop
- `F5`: motion toggle
- `F8`: ignition
- `F9`: retract

Comandi console (esempi):
```
scan
connect <addr>
color <r> <g> <b>
effect <mode> [speed]
brightness <val>
cam <init|start|stop|status>
motion <enable|disable|status|sensitivity N>
effects
```

La dashboard mostra stato LED, camera, motion, optical flow grid, RSSI e log eventi.

## 8. Struttura progetto e file utili

- `src/main.cpp`: bootstrap firmware, BLE server, task e inizializzazione.
- `src/LedEffectEngine.cpp`: rendering effetti LED.
- `src/OTAManager.cpp`: servizio OTA BLE.
- `saber_dashboard.py`: dashboard Textual.
- `ledsaber_control.py`: client BLE base usato anche dalla dashboard.
- `update_saber.py`: updater OTA BLE.
- `start_dashboard.sh`: launcher della dashboard.
- `platformio.ini`: configurazione PlatformIO.

## 9. Licenza

Il progetto e' rilasciato con doppia licenza:
- GPLv3 per uso open source.
- Licenza commerciale disponibile su richiesta.
