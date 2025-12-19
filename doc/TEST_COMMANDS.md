# Test Comandi BLE - LedSaber

## Procedura di Test

### 1. Avvia il client Python
```bash
python3 ledsaber_control.py
```

### 2. Connetti al dispositivo
```
> scan auto
```

### 3. Test comandi base LED

#### Test colore
```
> color 255 0 0    # Rosso
> color 0 255 0    # Verde
> color 0 0 255    # Blu
```

#### Test luminosità
```
> brightness 100
> brightness 50
> on
> off
```

### 4. Test comandi specifici

#### Test Status LED (pin 4)
```
> statusled on
> statusled off
> statusled 128
> statusled brightness 200
```

Verifica: Il LED fisico sul pin 4 dovrebbe accendersi/spegnersi

#### Test Fold Point
```
> foldpoint 50
> foldpoint 72
> foldpoint 100
```

Verifica: Controlla che il valore cambi nello stato

#### Test Sincronizzazione Tempo **[NUOVO]**
```
> sync
```

Output atteso:
```
[DEBUG] Sending time sync: epoch=1734XXXXX, json={"epoch": 1734XXXXX}
✓ Tempo sincronizzato: 2024-12-19 XX:XX:XX
```

### 5. Verifica Log Seriale ESP32

Sul monitor seriale dell'ESP32 dovresti vedere:

```
[BLE] Color set to RGB(255,0,0)
[BLE] Brightness set: 100 (enabled: 1)
[BLE] Status LED (pin 4): enabled=1 brightness=128
[BLE] Fold point set to 72
[BLE TIME SYNC] Received: {"epoch": 1734XXXXX}
[BLE TIME SYNC] Parsed epoch: 1734XXXXX
[BLE] Time synced: epoch=1734XXXXX at millis=XXXXX
```

## Debug

Se i comandi NON funzionano:

1. **Verifica connessione BLE**
   ```
   > status
   ```
   Deve mostrare lo stato corrente

2. **Controlla il log Python**
   Cerca le righe `[DEBUG] Writing to ...`

3. **Controlla il log seriale ESP32**
   - Apri PlatformIO Serial Monitor
   - Baud rate: 115200
   - Cerca messaggi `[BLE]` o `[BLE ERROR]`

4. **Test caratteristiche disponibili**
   ```
   > debug services
   ```
   Verifica che tutti i servizi/characteristic siano presenti

## Comandi Aggiunti in questa Fix

✅ **`sync` / `time` / `timesync`** - Sincronizza orologio ESP32 con PC

## Logging Diagnostico Aggiunto

Ora ogni comando mostra:
- `[DEBUG] Writing to <UUID>: <JSON_DATA>` prima di inviare
- Messaggio di conferma dopo l'invio

Questo permette di tracciare esattamente cosa viene inviato via BLE.
