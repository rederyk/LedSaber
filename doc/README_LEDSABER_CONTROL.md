# LedSaber BLE Controller

Script Python interattivo per controllare la spada laser LED via Bluetooth LE.

## Installazione

```bash
# Installa dipendenze
pip install -r requirements.txt

# Oppure manualmente
pip install bleak
```

## Utilizzo

```bash
# Avvia lo script
python3 ledsaber_control.py

# Oppure rendilo eseguibile
chmod +x ledsaber_control.py
./ledsaber_control.py
```

## Comandi Disponibili

### Connessione
- `scan` - Cerca dispositivi LedSaber nelle vicinanze
- `connect <indirizzo>` - Connetti al dispositivo (es: `connect 10:52:1C:75:58:56`)
- `disconnect` - Disconnetti dal dispositivo
- `status` - Mostra stato corrente LED

### Controllo LED
- `color <r> <g> <b>` - Imposta colore RGB (0-255)
  - Esempio: `color 255 0 0` (rosso)
- `effect <mode> [speed]` - Imposta effetto
  - Modalità: `solid`, `rainbow`, `breathe`
  - Speed: 0-100 (opzionale, default 50)
  - Esempio: `effect rainbow 80`
- `brightness <valore>` - Imposta luminosità (0-255)
  - Esempio: `brightness 128`
- `on` - Accendi LED (luminosità massima)
- `off` - Spegni LED

### Preset Colori
- `presets` - Mostra lista preset disponibili
- `preset <nome>` - Applica preset colore
  - Disponibili: red, green, blue, white, yellow, cyan, magenta, orange, purple, pink

### Altro
- `help` - Mostra menu comandi
- `quit` o `exit` - Esci dallo script

## Esempio Sessione

```
> scan
🔍 Scansione dispositivi BLE...
✓ Trovato: LedSaber-BLE (10:52:1C:75:58:56)

> connect 10:52:1C:75:58:56
📡 Connessione a 10:52:1C:75:58:56...
✓ Connesso!
✓ Notifiche abilitate

> preset blue
✓ Colore impostato: RGB(0,0,255)

> effect rainbow 60
✓ Effetto impostato: rainbow (speed: 60)

> status
📊 Stato LED:
  Stato: ON
  Colore: RGB(0, 0, 255)
  Effetto: rainbow (speed: 60)
  Luminosità: 255/255

> quit
📡 Disconnesso
👋 Arrivederci!
```

## Funzionalità

✅ Scansione automatica dispositivi LedSaber
✅ Connessione BLE GATT
✅ Controllo colore RGB
✅ Effetti LED (solid, rainbow, breathe)
✅ Controllo luminosità e on/off
✅ Ricezione notifiche stato in tempo reale
✅ Preset colori predefiniti
✅ Interfaccia CLI colorata e interattiva

## Troubleshooting

### Errore "No module named 'bleak'"
```bash
pip install bleak
```

### Errore permessi Bluetooth su Linux
```bash
# Aggiungi il tuo utente al gruppo bluetooth
sudo usermod -a -G bluetooth $USER

# Riavvia sessione o riavvia il PC
```

### Dispositivo non trovato
- Verifica che l'ESP32 sia acceso
- Verifica che il Bluetooth sia attivo sul PC
- Verifica che il dispositivo non sia già connesso da altro software
- Prova ad aumentare il timeout scan: modifica `timeout=5.0` nella funzione `scan()`

### Disconnessione improvvisa
- Normale con BlueZ/Linux per servizi GATT custom
- Lo script mantiene la connessione attiva tramite le notifiche
- Se si disconnette, usa `connect` per riconnettersi
