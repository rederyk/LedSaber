# LedSaber OTA Update via BLE GATT

Sistema di aggiornamento Over-The-Air (OTA) per ledSaber tramite Bluetooth Low Energy GATT.

## 📋 Caratteristiche

- ✅ **Partizioni dual A/B** (app0/app1) per rollback automatico
- ✅ **Chunk da 512 bytes** con negoziazione MTU
- ✅ **CRC32 incrementale** per verifica integrità
- ✅ **Timeout multi-livello** (globale, waiting, chunk)
- ✅ **Progress tracking** in tempo reale
- ✅ **LED status indicator** (blink veloce durante OTA)
- ✅ **Sicurezza**: LED strip disabilitato durante OTA per risparmiare RAM

## 🔧 Servizio GATT OTA

### UUID Servizio
```
OTA_SERVICE_UUID: 4fafc202-1fb5-459e-8fcc-c5c9c331914b
```

### Caratteristiche

| Caratteristica | UUID | Proprietà | Descrizione |
|---------------|------|-----------|-------------|
| **FW_VERSION** | `a4b8d7fa-1e43-6c7d-ad8f-456789abcdef` | READ | Versione firmware corrente |
| **OTA_STATUS** | `d1e5a4c4-eb10-4a3e-8a4c-1234567890ab` | READ + NOTIFY | Stato aggiornamento |
| **OTA_PROGRESS** | `f3e7c6e6-0d32-4c5a-ac6e-3456789012cd` | READ + NOTIFY | Progresso upload |
| **OTA_CONTROL** | `e2f6b5d5-fc21-5b4f-9b5d-2345678901bc` | WRITE | Comandi di controllo |
| **OTA_DATA** | `beb5483f-36e1-4688-b7f5-ea07361b26a8` | WRITE | Dati firmware (chunk) |

## 📊 Stati OTA

```cpp
enum OTAState {
    IDLE = 0,        // Pronto per aggiornamento
    WAITING = 1,     // In attesa del primo chunk (timeout 30s)
    RECEIVING = 2,   // Ricezione in corso
    VERIFYING = 3,   // Verifica firmware
    READY = 4,       // Pronto al reboot
    ERROR = 5,       // Errore (vedi messaggio)
    RECOVERY = 6     // Rollback in corso
};
```

## 🎮 Comandi OTA

| Comando | Valore | Payload | Descrizione |
|---------|--------|---------|-------------|
| **START** | `0x01` | uint32_t size | Inizia OTA (4 bytes little-endian) |
| **ABORT** | `0x02` | - | Annulla aggiornamento |
| **VERIFY** | `0x03` | - | Verifica firmware (auto al 100%) |
| **REBOOT** | `0x04` | - | Riavvia con nuovo firmware |

## 🚀 Utilizzo

### 1. Compila il firmware
```bash
platformio run
```

Il binario sarà in `.pio/build/esp32cam/firmware.bin`

### 2. Aggiorna via OTA

#### Metodo automatico (scansione)
```bash
./update_saber.py .pio/build/esp32cam/firmware.bin
```

#### Metodo con indirizzo BLE
```bash
./update_saber.py .pio/build/esp32cam/firmware.bin AA:BB:CC:DD:EE:FF
```

### 3. Sequenza aggiornamento

1. **Scansione/Connessione** al dispositivo
2. **Lettura versione** firmware corrente
3. **Conferma utente** (ATTENZIONE: non spegnere!)
4. **Invio comando START** con dimensione firmware
5. **Upload chunk** (512 bytes ciascuno)
   - Progress bar in tempo reale
   - Notifiche ogni 5KB
6. **Verifica automatica** (CRC32 + SHA256 ESP-IDF)
7. **Impostazione boot partition** (app0 ↔ app1)
8. **Reboot** (opzionale, richiede conferma)

## 📡 Formato Dati

### OTA_STATUS (notify)
```
Formato: "STATE:ERROR_MESSAGE"
Esempio: "2:"              (RECEIVING, no error)
         "5:CRC mismatch"  (ERROR, con messaggio)
```

### OTA_PROGRESS (notify)
```
Formato: "PERCENT:RECEIVED:TOTAL"
Esempio: "45:230400:512000"  (45%, 230KB/500KB)
```

### OTA_DATA (write)
```
Formato: raw bytes (max 512 bytes per chunk)
```

### OTA_CONTROL (write)
```
Formato: [command_byte] + [payload]

START:  0x01 + [size_uint32_le]
        Esempio: 01 00 00 08 00  (START, 512KB)

ABORT:  0x02
VERIFY: 0x03
REBOOT: 0x04
```

## ⚙️ Parametri di Sicurezza

| Parametro | Valore | Descrizione |
|-----------|--------|-------------|
| **Timeout globale** | 5 minuti | Abort automatico se supera |
| **Timeout waiting** | 30 secondi | No primo chunk |
| **Timeout chunk** | 10 secondi | Inattività tra chunk |
| **MTU negoziato** | 512+ bytes | Per chunk ottimali |
| **Min heap** | 50KB | Verifica RAM disponibile |

## 🛡️ Sicurezza e Recovery

### Durante OTA
- ✅ **LED strip disabilitato** (risparmio RAM ~35KB)
- ✅ **Status LED blink 100ms** (segnale "NON TOCCARE")
- ✅ **Comandi LED bloccati** (scartati)
- ✅ **Log seriale dettagliato** (chunk, CRC, velocità)

### Rollback Automatico
- ESP32 bootloader verifica il nuovo firmware al boot
- Se corrotto/invalido → rollback automatico alla partizione precedente
- `esp_ota_set_boot_partition()` aggiorna solo se `esp_ota_end()` ha successo

### Verifica Integrità
1. **CRC32 incrementale** durante ricezione
2. **SHA256** di partizione (ESP-IDF nativo)
3. **Validazione dimensione** vs partizione disponibile

## 📈 Performance Attese

| Metrica | Valore |
|---------|--------|
| **Velocità upload** | 15-20 KB/s (BLE) |
| **Tempo firmware 1.2MB** | ~2-3 minuti |
| **RAM usata durante OTA** | ~40-50KB |
| **Flash writes** | Progressivi (no buffer completo) |

## 🐛 Troubleshooting

### Errore: "No update partition"
- Verifica `partitions_ota.csv` configurato correttamente
- Controlla che app0/app1 siano definite

### Errore: "Insufficient memory"
- Heap < 50KB
- Riduci NUM_LEDS o disabilita servizi non essenziali

### Timeout durante upload
- Verifica connessione BLE stabile
- Riduci distanza dal dispositivo
- Evita interferenze (WiFi 2.4GHz, microonde)

### Firmware corrotto dopo reboot
- ESP32 fa rollback automatico
- Verifica che il binario sia corretto
- Riprova upload

### Device disconnesso durante OTA
- Riconnetti e usa comando `ABORT`
- Device torna in stato `IDLE` dopo timeout

## 📝 Esempio Serial Log

```
[OTA] START command received (size=1186845 bytes, 1159.02 KB)
[OTA] Free heap: 184532 bytes
[OTA] Ready to receive firmware data
[OTA] Chunk: 512 bytes | Total: 512/1186845 (0.0%) | Speed: 25.60 KB/s | CRC: 0x12345678
[OTA] Chunk: 512 bytes | Total: 1024/1186845 (0.1%) | Speed: 25.60 KB/s | CRC: 0x23456789
...
[OTA] All data received! Verifying...
[OTA] Firmware verification successful
[OTA] Firmware ready! Total: 1186845 bytes, CRC32: 0xABCDEF12
[OTA] Send REBOOT command to apply update
[OTA] REBOOT command received
[OTA] Rebooting in 2 seconds...
```

## 🔗 Integrazione Custom

Se vuoi integrare l'OTA nel tuo client BLE custom:

```python
from bleak import BleakClient

# 1. Connetti al dispositivo
client = BleakClient(address)
await client.connect()

# 2. Leggi versione corrente
fw_version = await client.read_gatt_char(CHAR_FW_VERSION_UUID)
print(f"Current FW: {fw_version.decode()}")

# 3. Abilita notifiche
await client.start_notify(CHAR_OTA_STATUS_UUID, status_handler)
await client.start_notify(CHAR_OTA_PROGRESS_UUID, progress_handler)

# 4. Invia comando START
firmware_size = 1186845
cmd_start = bytes([0x01]) + struct.pack('<I', firmware_size)
await client.write_gatt_char(CHAR_OTA_CONTROL_UUID, cmd_start)

# 5. Invia chunk (512 bytes)
with open('firmware.bin', 'rb') as f:
    while chunk := f.read(512):
        await client.write_gatt_char(CHAR_OTA_DATA_UUID, chunk)

# 6. Attendi verifica (notifica OTA_STATUS → READY)

# 7. Reboot
cmd_reboot = bytes([0x04])
await client.write_gatt_char(CHAR_OTA_CONTROL_UUID, cmd_reboot)
```

## 📦 Dimensioni Firmware

Verifica dimensioni con:
```bash
platformio run -t size
```

Output esempio:
```
   text	   data	    bss	    dec	    hex	filename
1005570	 187704	  17593	1210867	 1279f3	firmware.elf

Flash: 60.4% (1186845 bytes / 1966080 bytes)
```

Max firmware per partizione: **1.9MB** (0x1E0000)

## 🎯 Note Finali

- **Non disconnettere** durante OTA
- **Non spegnere** il dispositivo
- **LED blink veloce** = OTA in corso
- **Verifica sempre** il serial log per debug
- **Test consigliato** su partizione di sviluppo prima del deploy

---

**Versione documento**: 1.0.0
**Compatibile con**: ESP32-CAM, Arduino Framework, PlatformIO
