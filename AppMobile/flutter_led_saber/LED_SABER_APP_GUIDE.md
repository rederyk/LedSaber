# LedSaber App Guide - Effetti, Temi, Motion

Scopo: dare a chi crea l'app una descrizione chiara delle funzioni UI e del
comportamento reale di effetti, colori, temi orologio e motion detection.

Fonti principali: firmware BLE (LED/Motion/Camera) e lista effetti BLE.

---

## 1) Effetti LED (effects list)

La lista effetti si legge da BLE con la characteristic:

- Service: LED (`4fafc201-1fb5-459e-8fcc-c5c9c331914b`)
- Effects List: `d8f9e1ea-6c98-2b3c-cf0e-890abcdef234` (READ)

Risposta JSON (esempio ridotto):

```json
{
  "version": "1.0.0",
  "effects": [
    {"id":"solid","name":"Solid Color","params":["color"]},
    {"id":"rainbow","name":"Rainbow","params":["speed"]},
    {"id":"pulse","name":"Pulse Wave","params":["speed","color"]},
    {"id":"chrono_hybrid","name":"Chrono Clock","params":["chronoHourTheme","chronoSecondTheme"],
     "themes":{"hour":["Classic","Neon","Plasma","Digital","Inferno","Storm"],
               "second":["Classic","Spiral","Fire","Lightning","Particle","Quantum"]}}
  ]
}
```

UI consigliata:

- Lista effetti con nome e icona (se presente).
- Quando si seleziona un effetto, mostra SOLO i parametri indicati in `params`.
- Non hardcodare gli effetti: leggere sempre la lista dal device.

Invio effetto:

```
Characteristic: LED Effect
UUID: e2f6b5d4-fc21-5b4f-9b5d-2345678901bc
Payload JSON: {"mode":"pulse","speed":100}
```

Parametri supportati:

- speed: 0..255 (slider)
- color: RGB (usare LED Color separato)
- chronoHourTheme: 0..N-1 (indice tema)
- chronoSecondTheme: 0..N-1 (indice tema)

---

## 2) Colore e Brightness

Colore (RGB):

- Characteristic: LED Color
- UUID: d1e5a4c3-eb10-4a3e-8a4c-1234567890ab
- Payload: {"r":255,"g":0,"b":0}

Brightness + on/off:

- Characteristic: LED Brightness
- UUID: f3e7c6e5-0d32-4c5a-ac6e-3456789012cd
- Payload: {"brightness":128,"enabled":true}

UI consigliata:

- Color picker RGB.
- Slider brightness con toggle ON/OFF.
- Mostrare sempre lo stato reale usando LED State notify.

---

## 3) Blade state + accensione/spegnimento

La lama ha stato reale (non solo enabled):

LED State JSON include:

- bladeState: off | igniting | on | retracting

Accensione/spegnimento con animazione:

- Characteristic: Device Control
- UUID: c7f8e0d9-5b87-1a2b-be9d-7890abcdef23
- Payload: {"command":"ignition"} oppure {"command":"retract"}

UI consigliata:

- Bottone "Ignite" e "Retract" con feedback dello stato bladeState.
- Disabilitare i bottoni durante igniting/retracting.

---

## 4) Chrono Effects (effetti orologio)

Effetto:

- id: chrono_hybrid (nome "Chrono Clock" da effects list)
- Parametri: chronoHourTheme + chronoSecondTheme

Significato:

- chronoHourTheme: tema grafico degli "marker ore"
- chronoSecondTheme: tema grafico dei cursori secondi/minuti

UI consigliata:

- 2 selector separati (Hour Theme / Second Theme).
- Etichette dal JSON `themes` in effects list.
- Se il tema non e' presente, non mostrare il selector.

Time Sync (per mantenere l'orologio coerente):

- Characteristic: Time Sync
- UUID: d6e1a0b8-4a76-9f0c-dc1a-789abcdef012
- Payload: {"epoch":1703107200}

UI consigliata:

- Bottone "Sync Time" (usa ora locale device).

---

## 5) Motion Detection (feature e UI)

Service Motion:

- Status: JSON con metriche (notify)
- Events: JSON con eventi (notify)
- Control: comandi stringa (write)
- Config: JSON (read/write)

Comandi Control:

- enable / disable
- reset
- quality <0..255>
- motionmin <0..255> (soglia intensita)
- speedmin <0..20> (soglia velocita)

Config JSON (write):

```json
{
  "enabled": true,
  "quality": 128,
  "motionIntensityMin": 12,
  "motionSpeedMin": 1.2,
  "gestureIgnitionIntensity": 140,
  "gestureRetractIntensity": 120,
  "gestureClashIntensity": 200,
  "debugLogs": false
}
```

Status JSON (campi chiave):

- enabled, motionDetected
- direction (stringa), speed, confidence
- gesture, gestureConfidence (con TTL)
- grid[] + trail[] per visualizzazione semplice

Eventi:

- motion_started / motion_ended
- shake_detected
- gesture_detected (quando disponibile)

UI consigliata:

- Toggle motion enable.
- Sezione "Live Motion" con direction + speed + confidence.
- Sezione "Gestures" con ultimo gesto e confidenza.
- Sezione "Tuning" con slider: quality, motionIntensityMin, motionSpeedMin.
- Avanzato: slider gestureIgnition/gestureRetract/gestureClash + debug logs toggle.

---

## 6) Status LED (pin 4)

Status LED e' separato dalla lama.

- Characteristic: Status LED
- UUID: a4b8d7f9-1e43-6c7d-ad8f-456789abcdef
- Payload: {"enabled":true,"brightness":32}

UI consigliata:

- Toggle + slider brightness.

---

## 7) Boot behavior (accensione automatica)

Comando:

- Characteristic: Device Control
- Payload:

```json
{
  "command": "boot_config",
  "autoIgnitionOnBoot": true,
  "autoIgnitionDelayMs": 3000,
  "motionEnabled": true
}
```

UI consigliata:

- Toggle "Auto ignition on boot".
- Slider "Delay ms".
- Toggle "Motion on boot".

