# ⚔️ LedSaber Live Dashboard

**BTTop-Style Real-Time Monitor - Resistance Cyberpunk Edition**

Dashboard interattiva live per il controllo e monitoraggio della spada laser LED tramite BLE.

```
┌──────────────────────────────────────────────────────────────────────┐
│ ⚔️  LEDSABER DASHBOARD                               Resistance Edition │
│ BLE: ◉ LedSaber_BT (A4:CF:12:XX:XX:XX)                RSSI: -55 dBm     │
└──────────────────────────────────────────────────────────────────────┘
┌──────────────┬──────────────┬────────────────────────────────────────┐
│ ⚡ LED STATUS │ 📸 CAMERA    │ ⚡ STATUS │ 📊 INTENSITY │ 🧭 DIRECTION │
│ ◉ ON  │ FX   │ FPS + spark  │ mini-card │ mini-card   │ mini-card    │
│ 📡 RSSI  ✨ FX│ 🎬 Frames    │ (stack or span 3 cols based on width)  │
└──────────────┴──────────────┴────────────────────────────────────────┘
┌──────────────────────────────────────────────────────────────────────┐
│ 🔍 OPTICAL FLOW GRID (8x6 @ 40px) — double-head box + legend          │
└──────────────────────────────────────────────────────────────────────┘
┌──────────────────────────────────────────────────────────────────────┐
│ Console log (5 righe) + prompt compatto                              │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 🚀 Quick Start

```bash
./start_dashboard.sh
```

**Oppure manualmente:**
```bash
source venv/bin/activate
python saber_dashboard.py
```

---

## ✨ Features Principali

### 📊 Dashboard Real-Time Live

**Differenze dalla vecchia TUI:**
- ✅ **Visualizzazione massimizzata** - Optical flow grid centrale e grande
- ✅ **Auto-resize responsive** - Si adatta automaticamente al terminale
- ✅ **Console minimale** - Solo 5 righe di log visibili (massimo 100 salvate)
- ✅ **Comandi camera funzionanti** - F2=init, F3=start, F4=stop
- ✅ **Aggiornamenti live istantanei** - Griglia motion si aggiorna in tempo reale
- ✅ **Stile btop/htop** - Dashboard-first, comandi secondari

## 🧩 Layout Responsive a Griglia

- **Fila 1 – HeaderWidget**: pannello Rich con due colonne (titolo/modalità e stato BLE) che adatta automaticamente il padding e rimuove l'ASCII art rigido.
- **Fila 2 – `#stats_grid`**: tre colonne responsive (LED/BLE, Camera, Motion summary). Il codice `on_resize` del Dashboard applica classi CSS diverse (`cols-3`, `cols-2`, `cols-1`) per collassare le colonne automaticamente in base alla larghezza (≥160 → 3, 110-159 → 2, <110 → 1).
- **Fila 3 – OpticalFlowGrid**: tabella full-width con `box.MINIMAL_DOUBLE_HEAD`, legenda dedicata e padding dinamico per valorizzare i terminali larghi.
- **Fila 4 – Console**: contenitore compatto con bordo dedicato, linee orizzontali ridimensionate runtime e prompt che non occupa colonne inutili.

Le mini-card KPI (`📡 BLE RSSI`, `✨ Active FX`, `🎬 Camera Frames`) popolano automaticamente gli spazi vuoti delle colonne per enfatizzare il carattere “dashboard-first” quando il terminale è molto ampio e si impilano ordinatamente su schermi stretti.

> **Tip**: ridimensiona il terminale (es. 80 → 120 → 160 colonne) per verificare il comportamento della griglia senza riavviare l’app; le classi responsive vengono aggiornate live e Textual ricompone le colonne (`1fr`, `1fr 1fr`, `1fr 1fr 1fr`) in tempo reale.

### 📺 Panels

#### 1. **LED Column** (LED Panel + mini KPI)
```
┌─ ⚡ LED STATUS ──────────────────────────────────────────┐
│   ◉ ON  │  R:255 G:  0 B:  0  │  Bright:200 ████░░  │  FX:rainbow   │
└──────────────────────────────────────────────────────────┘
┌─ 📡 RSSI ─────┐┌─ ✨ FX ───────┐
│  -55 dBm     ││  RAINBOW     │
│  (live)      ││  150 ms      │
└──────────────┘└──────────────┘
```
Il container `#led_column` ospita il pannello principale e due mini-card (`BLERSSICard`, `ActiveFXCard`) che mostrano RSSI live (ricavato dallo scan/connection) e l’effetto LED attuale.

#### 2. **Camera Column** (pannello + Frames KPI)
```
┌─ 📸 CAMERA ─────────────────────────────────────────────┐
│   ◉ ACTIVE  │  Init:✓  │  FPS: 25.3  │  ▁▂▃▄▅▆▇█▇▆  │  │
└─────────────────────────────────────────────────────────┘
┌─ 🎬 Frames ─────────────────┐
│   12,345                    │
│   live counter + activity   │
└─────────────────────────────┘
```
Il pannello camera resta compatto mentre `CameraFramesCard` offre un contatore dedicato che riempie la colonna quando il terminale è ampio.

#### 3. **Motion Summary Cards**
```
┌─ ⚡ STATUS ─────────────┐┌─ 📊 INTENSITY ────────────┐┌─ 🧭 DIRECTION ─────┐
│ ◉ ENABLED               ││ Intensity: 156            ││   ↗ SWING UP        │
│ ⚡ MOTION               ││ ████████░░░░░░░░          ││ GESTURE: SLASH 87%  │
│ ⚠ SHAKE                ││ History: ▁▂▃▄▅▆▇█         ││                     │
└────────────────────────┘└───────────────────────────┘└────────────────────┘
```
`MotionSection` distribuisce tre sotto-widget reattivi (status, intensity, direction) che possono disporsi in tre colonne sui terminali larghi o impilarsi verticalmente sui terminali compatti.

#### 4. **Optical Flow Grid full-width**
```
┌──────────────────────────────────────────────────────────────────────┐
│ 🔍 OPTICAL FLOW GRID (8x6 @ 40px)                                    │
│ ┌╥════════════════════════════════════════════════════════════════╥┐ │
│ │║ > > > . . . . .                                                ║│ │
│ │║ > > > . . . . .                                                ║│ │
│ │║ > > . . . . . .                                                ║│ │
│ │║ . . . . . . . .                                                ║│ │
│ │╚════════════════════════════════════════════════════════════════╝│ │
│ │Legend: . idle   ^v up/down   <> left/right   A B C D diagonals  │ │
└──────────────────────────────────────────────────────────────────────┘
```
La griglia sfrutta `box.MINIMAL_DOUBLE_HEAD`, padding adattivo e una legenda separata per essere leggibile tanto su 90 quanto su 200 colonne.

#### 5. **Console Widget** (5 righe visibili)
```
CONSOLE LOG (last 5 lines)
───────────────────────────────────────────────
14:32:25 LED → RGB(255,0,0)
14:32:30 ⚡ MOTION_STARTED (I:156)
14:32:31 ⚔ GESTURE: SLASH (87%)
───────────────────────────────────────────────
> _
```
Le linee divisorie si adattano automaticamente alla larghezza del contenitore (`max(40, width-4)`) così il prompt non occupa più colonne del necessario.

---

## ⌨️ Keyboard Shortcuts

### Shortcuts Principali (Function Keys)

| Tasto | Azione | Descrizione |
|-------|--------|-------------|
| `Ctrl+S` | **Scan & Connect** | Scansiona e connette automaticamente al dispositivo |
| `Ctrl+D` | **Disconnect** | Disconnette dal dispositivo BLE |
| `Ctrl+C` | **Quit** | Esci dalla dashboard |
| `F1` | **Help** | Mostra lista comandi in console |
| `F2` | **Camera Init** | Inizializza camera |
| `F3` | **Camera Start** | Avvia cattura continua |
| `F4` | **Camera Stop** | Ferma cattura |
| `F5` | **Motion Toggle** | Abilita/disabilita motion detection |

### Comandi Console

Digita nella console in basso:

#### 🔗 BLE Commands
```bash
scan              # Cerca dispositivi
scan auto         # Scansiona e connette automaticamente
connect <addr>    # Connetti a indirizzo specifico
disconnect        # Disconnetti
```

#### 💡 LED Commands
```bash
color <r> <g> <b> # Imposta colore RGB (0-255)
effect <mode> [speed]  # Imposta effetto (solid, rainbow, breathe, etc.)
brightness <val>  # Imposta luminosità (0-255)
on                # Accendi LED
off               # Spegni LED
```

**Effetti disponibili:**
- `solid` - Colore solido
- `rainbow` - Arcobaleno animato
- `breathe` - Respirazione
- `ignition` - Accensione progressiva
- `retraction` - Spegnimento progressivo
- `flicker` - Instabilità plasma
- `unstable` - Kylo Ren style
- `pulse` - Onda energia
- `dual_pulse` - Due onde opposte
- `clash` - Flash impatto
- `rainbow_blade` - Arcobaleno lineare

#### 📸 Camera Commands
```bash
cam init          # Inizializza camera
cam start         # Avvia cattura continua
cam stop          # Ferma cattura
cam status        # Mostra stato camera
```

#### 🔍 Motion Detection Commands
```bash
motion enable     # Abilita motion detection
motion disable    # Disabilita motion detection
motion status     # Mostra stato dettagliato
motion quality <0-255>  # Imposta qualita'
```

#### 🆘 Other
```bash
help              # Mostra lista comandi
quit / exit / q   # Esci dalla dashboard
```

---

## 📐 Requisiti Terminale

### Dimensioni Minime
- **Minimo assoluto:** 80x24 caratteri
- **Raccomandato:** 100x30 caratteri
- **Ottimale:** 120x35+ caratteri (per vedere tutta la griglia motion)

### Terminal Compatibility
Testato su:
- ✅ GNOME Terminal
- ✅ Konsole (KDE)
- ✅ Alacritty
- ✅ Kitty
- ✅ Terminator
- ✅ Tilix

---

## 🎨 Color Scheme

**Cyberpunk Resistance Theme:**

| Elemento | Colore | Uso |
|----------|--------|-----|
| **Cyan** | #00FFFF | Bordi principali, header, LED panel |
| **Magenta** | #FF00FF | Motion detection, gesture |
| **Green** | #00FF00 | Camera status, success messages |
| **Yellow** | #FFFF00 | Warnings, intensity high |
| **Red** | #FF0000 | Errors, critical alerts, shake |
| **White** | #FFFFFF | Testo generale |
| **Dim** | #808080 | Timestamps, secondary info |

### Simboli Optical Flow Grid

| Simbolo | Colore | Significato |
|---------|--------|-------------|
| `.` | Dim White | Nessun movimento |
| `^` / `v` | Cyan | Su / Giù |
| `<` / `>` | Yellow | Sinistra / Destra |
| `A` | Green | Diagonale su-destra ↗ |
| `B` | Green | Diagonale su-sinistra ↖ |
| `C` | Magenta | Diagonale giù-destra ↘ |
| `D` | Magenta | Diagonale giù-sinistra ↙ |

---

## 🔧 Technical Details

### Architecture
```
saber_dashboard.py
├── HeaderWidget              # Header con status BLE
├── LEDPanelWidget           # Panel LED compatto (3 righe)
│   ├── BLERSSICard          # Mini-card KPI
│   └── ActiveFXCard         # Mini-card effetto/speed
├── CameraPanelWidget        # Panel camera + FPS sparkline (3 righe)
│   └── CameraFramesCard     # KPI dedicato ai frame
├── MotionSection            # Container responsivo per motion summary
│   ├── MotionStatusCard     # Stato + shake
│   ├── MotionIntensityCard  # Barra + sparkline + pixels
│   └── MotionDirectionCard  # Direzione + gesture confidence
├── OpticalFlowGridWidget    # Griglia full-width con legenda
├── ConsoleWidget            # Log console (5 righe visibili, 100 salvate)
└── CommandInputWidget       # Input comandi
```

### Callbacks Real-Time
I callback GATT sono collegati direttamente ai widget:
- `state_callback` → `LEDPanelWidget.led_state` (reactive update)
- `camera_callback` → `CameraPanelWidget.camera_state` (reactive update)
- `motion_callback` → `MotionSection.motion_state` + `OpticalFlowGridWidget.motion_state`
- `motion_event_callback` → `ConsoleWidget.add_log()`

**Reactive Updates:**
Textual usa il sistema `reactive` per aggiornamenti automatici. Quando cambia `led_state`, `camera_state` o `motion_state`, i widget si ri-renderizzano automaticamente senza lag.

### Auto-Resize
Il layout sfrutta `grid-columns` dinamici controllati programmaticamente da `_update_responsive_layout()`:
- **Header:** padding adattivo (1 → 2 spazi) basato sulla larghezza runtime.
- **Stats grid:** classi `cols-3/cols-2/cols-1` applicate on-the-fly per distribuire 3, 2 o 1 colonne.
- **Motion summary:** `MotionSection` riceve la classe `cols-3` quando il terminale supera ~140 colonne, altrimenti impila i card verticalmente.
- **Optical Flow:** pannello full-width con padding calcolato (`pad_x` variabile) per sfruttare l’ampiezza laterale.
- **Console:** contenitore con `max-height` 8 e righe divisorie calcolate (`max(40, width-4)`).

Ridimensionando il terminale puoi quindi verificare la ricomposizione live senza riavviare l’app.

---

## 🆚 Confronto con Vecchia TUI

| Feature | Vecchia TUI (`saber_tui.py`) | Nuova Dashboard (`saber_dashboard.py`) |
|---------|------------------------------|----------------------------------------|
| **Log visibili** | 8 righe fisse | 5 righe (100 salvate) |
| **Auto-resize** | ❌ No | ✅ Si |
| **Motion grid** | Piccola, in panel | **MASSIVA, centrale, live** |
| **Comandi camera** | ⚠️ Non funzionanti | ✅ Funzionanti (F2/F3/F4) |
| **Stile** | Command-first | **Dashboard-first (btop style)** |
| **Layout** | Fisso | Responsive, adattivo |
| **Console** | Input principale | Minimale, secondaria |
| **Optical flow** | Piccola visualizzazione | **Griglia 8x6 centrata, colorata** |
| **Sparklines** | FPS solo | FPS + Motion intensity (60 samples) |
| **Hotkeys** | Poche | **F1-F5 + Ctrl+S/D** |

---

## 🐛 Troubleshooting

### Dashboard non si avvia
```bash
# Verifica dipendenze
source venv/bin/activate
pip install --upgrade textual rich bleak

# Verifica Python version (richiede 3.8+)
python --version
```

### Terminale troppo piccolo
**Soluzione:** Ridimensiona il terminale a minimo 80x24.

Il launcher (`start_dashboard.sh`) controlla automaticamente le dimensioni e ti avvisa.

### Comandi camera non funzionano
**Possibili cause:**
1. Camera non inizializzata → Premi `F2` prima
2. Dispositivo non connesso → Premi `Ctrl+S`
3. Firmware ESP32 non supporta camera service → Verifica UUID in [ledsaber_control.py](ledsaber_control.py)

**Debug:**
```bash
# Controlla status camera
cam status

# Output dovrebbe mostrare:
# Cam: init=True active=False fps=0.0
```

### Motion grid non si aggiorna
**Verifica:**
1. Motion detection abilitata → `F5` o `motion enable`
2. Device connesso → Controlla header `◉ CONNECTED`
3. Notifiche BLE abilitate → Verifica log: `"✓ Notifiche Motion abilitate"`

### Permessi Bluetooth
```bash
# Aggiungi utente al gruppo bluetooth
sudo usermod -a -G bluetooth $USER

# Riavvia sessione (logout/login)

# Verifica
groups | grep bluetooth
```

---

## 🎯 Workflow Tipico

### Sessione Completa (dalla connessione al monitoring)

```bash
# 1. Avvia dashboard
./start_dashboard.sh

# 2. Scan & Connect (hotkey)
[Premi Ctrl+S]

# 3. Inizializza Camera (hotkey)
[Premi F2]
# Aspetta 1-2 secondi

# 4. Avvia Camera (hotkey)
[Premi F3]

# 5. Abilita Motion Detection (hotkey)
[Premi F5]

# 6. Cambia colore LED (console)
color 0 255 0    # Verde

# 7. Applica effetto (console)
effect rainbow 100

# 8. Monitora in tempo reale
# - Guarda la griglia optical flow aggiornarsi live
# - Osserva i grafici FPS e intensity
# - Leggi eventi gesture in console

# 9. Disconnetti (hotkey)
[Premi Ctrl+D]

# 10. Esci (hotkey)
[Premi Ctrl+C]
```

---

## 🚧 Known Limitations

1. **Griglia optical flow dimensione fissa:** 8x6 blocchi @ 40px (configurata dal firmware ESP32)
2. **Console scroll:** Non implementato, mostra solo ultime 5 righe (100 salvate in background)
3. **Multi-device:** Supporta un solo dispositivo connesso alla volta
4. **Theme switching:** Non implementato, solo theme cyberpunk

---

## 📝 Development Notes

### Aggiungere Nuovi Comandi

Modifica `SaberDashboard._execute_command()`:

```python
elif cmd == "my_command":
    # Handler
    await self.client.my_method()
    self.console_widget.add_log("My command executed", "cyan")
```

### Aggiungere Hotkey

1. Aggiungi binding in `BINDINGS`:
```python
Binding("f6", "my_action", "My Action"),
```

2. Implementa action:
```python
def action_my_action(self) -> None:
    """F6: My action"""
    self.run_worker(self._execute_command("my_command"))
```

### Modificare Layout

CSS in `SaberDashboard.CSS`:
```css
#motion_panel {
    height: auto;
    min-height: 25;  /* Aumenta min-height per panel più grandi */
}
```

---

## 📄 License

Parte del progetto **LedSaber BLE Control System**.

**Resistance is Futile. Join the Cyberpunk Underground.** ⚔️

---

## 🙏 Credits

- **Textual** - Framework TUI moderno by Textualize
- **Rich** - Beautiful terminal rendering
- **Bleak** - Bluetooth Low Energy library
- **Design inspiration:** btop++, htop, Hollywood terminal, Cyberpunk 2077

---

**Made with ⚡ and 🎨 in the resistance underground**

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
