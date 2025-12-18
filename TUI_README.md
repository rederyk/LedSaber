# 🗡️ LedSaber TUI - Cyberpunk Edition

**Terminal User Interface avanzata per il controllo LedSaber BLE**

Interfaccia grafica terminale in stile resistenza cyberpunk, ispirata a btop e matrix.

---

## ✨ Features

### 📊 Dashboard Real-Time
- **LED Control Panel** - Stato RGB, luminosità, effetti, fold point
- **Camera Metrics** - FPS con sparkline, frame capture, failed frames
- **Motion Detection** - Intensity graphs, direzione movimento, shake detection
- **Optical Flow Grid** - Visualizzazione 8x6 blocchi con colori per direzioni
- **Event Log** - Log eventi in tempo reale (gestures, motion, camera)
- **Connection Status** - Stato BLE con device info

### 🎨 Stile Cyberpunk
- **Colori neon** - Cyan, magenta, verde acido
- **Bordi decorativi** - Box double-edge, rounded panels
- **Sparklines animate** - Grafici FPS e motion intensity
- **Indicators dinamici** - ◉ ONLINE, ⚡ MOTION, ⚔ GESTURE
- **Theme scuro** - Ottimizzato per terminali moderni

### ⚡ Real-Time Updates
- Callbacks GATT integrate per aggiornamenti istantanei
- LED state changes
- Camera FPS monitoring
- Motion detection events
- Gesture recognition
- Shake detection

### ⌨️ Comandi & Shortcuts

**Keyboard Shortcuts:**
- `Ctrl+C` - Quit
- `Ctrl+S` - Scan & Auto-Connect
- `Ctrl+D` - Disconnect
- `F1` - Help

**Comandi disponibili:**
```bash
# BLE Connection
scan              # Cerca dispositivi LedSaber
scan auto         # Scansiona e collega automaticamente
connect <addr>    # Connetti a indirizzo specifico
disconnect        # Disconnetti

# LED Control
color <r> <g> <b> # Imposta colore RGB (0-255)
effect <mode>     # Imposta effetto (solid, rainbow, breathe, etc.)
brightness <val>  # Imposta luminosità (0-255)
on                # Accendi LED
off               # Spegni LED

# System
help              # Mostra comandi
quit              # Esci
```

---

## 🚀 Installazione & Avvio

### Metodo 1: Script automatico (Raccomandato)

```bash
./start_tui.sh
```

Lo script gestisce automaticamente:
- ✓ Creazione virtual environment
- ✓ Installazione dipendenze (textual, rich, bleak)
- ✓ Verifica permessi Bluetooth
- ✓ Avvio TUI

### Metodo 2: Manuale

```bash
# Crea virtual environment
python3 -m venv venv
source venv/bin/activate

# Installa dipendenze
pip install -r requirements.txt

# Avvia TUI
python3 saber_tui.py
```

---

## 📦 Dipendenze

- **textual** >= 0.47.0 - Framework TUI moderno
- **rich** >= 13.7.0 - Rendering terminale avanzato
- **bleak** >= 0.21.0 - Bluetooth Low Energy

---

## 🎮 Utilizzo

### 1. Avvia la TUI
```bash
./start_tui.sh
```

### 2. Connetti al dispositivo
Premi `Ctrl+S` o digita:
```bash
scan auto
```

### 3. Controlla il LED
```bash
color 255 0 0      # Rosso
effect rainbow     # Effetto arcobaleno
brightness 200     # Luminosità 200/255
```

### 4. Monitora in Real-Time
- **LED Panel** - Mostra stato corrente RGB/effetto/brightness
- **Camera Panel** - FPS sparkline, frame counter
- **Motion Panel** - Optical flow grid aggiornata in tempo reale
- **Event Log** - Eventi gesture/shake/motion

---

## 🔍 Widget Details

### LED Control Panel
```
┌─ ⚡ LED CONTROL ─────────────┐
│ STATUS      ● ONLINE         │
│ RGB         255  0    0      │
│ BRIGHTNESS  200/255 ████░░   │
│ EFFECT      RAINBOW          │
│ SPEED       150 ms           │
│ FOLD POINT  72/143           │
└──────────────────────────────┘
```

### Camera Metrics Panel
```
┌─ 📸 CAMERA METRICS ──────────┐
│ STATUS      ◉ ACTIVE         │
│ INITIALIZED ✓                │
│ FPS         25.34 fps        │
│ FPS GRAPH   ▁▂▃▄▅▆▇█▇▆      │
│ FRAMES      1,234            │
│ FAILED      0                │
└──────────────────────────────┘
```

### Motion Detection Panel
```
┌─ 🔍 MOTION DETECTOR ─────────────┐
│ STATUS       ◉ ENABLED           │
│ MOTION       ⚡ MOTION            │
│ INTENSITY    156 ████████░░░     │
│ HISTORY      ▁▂▃▄▅▆▇█▇▆▅▄▃▂▁   │
│ DIRECTION    → RIGHT             │
│ PIXELS       1,245               │
│                                  │
│ Optical Flow Grid (8x6 @ 40px): │
│   > > > . . . . .                │
│   > > > . . . . .                │
│   > > . . . . . .                │
│   . . . . . . . .                │
│   . . . . . . . .                │
│   . . . . . . . .                │
│                                  │
│ ⚔ GESTURE: SLASH (87%)          │
└──────────────────────────────────┘
```

### Event Log
```
┌─ 📜 EVENT LOG ───────────────────┐
│ 14:32:15 SYSTEM TUI initialized  │
│ 14:32:20 BLE Connected to device │
│ 14:32:25 LED Color → RGB(255,0,0)│
│ 14:32:30 MOTION ⚡ Motion STARTED│
│ 14:32:31 GESTURE ⚔ SLASH (87%)  │
│ 14:32:35 MOTION ⚠ SHAKE!        │
│ 14:32:40 CAMERA FPS → 28.50     │
└──────────────────────────────────┘
```

---

## 🎨 Theme Customization

Il tema cyberpunk usa:
- **Primary colors**: Cyan (#00FFFF), Magenta (#FF00FF)
- **Accent colors**: Green (#00FF00), Yellow (#FFFF00)
- **Borders**: DOUBLE_EDGE, ROUNDED boxes
- **Text styles**: Bold headers, dim timestamps

Per personalizzare, modifica la sezione `CSS` in [saber_tui.py](saber_tui.py:262).

---

## 🐛 Troubleshooting

### Permessi Bluetooth
Se ottieni errori di permessi:
```bash
sudo usermod -a -G bluetooth $USER
# Poi riavvia la sessione
```

### Dipendenze mancanti
```bash
pip install --upgrade textual rich bleak
```

### Textual non trovato
```bash
source venv/bin/activate
pip install textual>=0.47.0
```

### Terminal troppo piccolo
La TUI richiede almeno **80x24** caratteri. Espandi il terminale.

---

## 🔧 Development

### Struttura Codice

```
saber_tui.py
├── LEDStatusWidget          # Panel LED control
├── CameraMetricsWidget      # Panel camera FPS/metrics
├── MotionDetectionWidget    # Panel motion + optical flow
├── ConnectionStatusWidget   # BLE connection status
├── EventLogWidget          # Real-time event log
└── SaberTUI               # Main app + command handler
```

### Callbacks GATT
I callback del `LedSaberClient` sono collegati ai widget:
- `state_callback` → `LEDStatusWidget.led_state`
- `camera_callback` → `CameraMetricsWidget.camera_state`
- `motion_callback` → `MotionDetectionWidget.motion_state`
- `motion_event_callback` → `EventLogWidget.add_event`

### Aggiungere Comandi
Aggiungi comandi in `SaberTUI._execute_command()`:
```python
elif cmd == "my_command":
    # Handler per comando custom
    await self.client.my_custom_method()
    self.event_log.add_event("CMD", "Custom command executed", "cyan")
```

---

## 🎯 Roadmap

- [ ] Pannello gesture training/config
- [ ] Export log eventi su file
- [ ] Supporto preset custom (salvabili)
- [ ] Controllo camera flash da TUI
- [ ] Configurazione motion sensitivity
- [ ] Theme switcher (cyberpunk/matrix/terminal)
- [ ] Animazioni ASCII art per gestures

---

## 📝 Credits

**Framework:**
- [Textual](https://textual.textualize.io/) - Modern TUI framework by Textualize
- [Rich](https://rich.readthedocs.io/) - Beautiful terminal output
- [Bleak](https://github.com/hbldh/bleak) - Bluetooth Low Energy library

**Design inspiration:**
- btop++ (system monitor)
- Matrix digital rain
- Cyberpunk 2077 UI

**Developed for:** LedSaber BLE Control System

---

## 📄 License

Parte del progetto LedSaber. Usa con parsimonia la resistenza. 🗡️

---

**Made with ⚡ and 🎨 in the cyberpunk underground**
