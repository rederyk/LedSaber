# 📱 LedSaber BLE API - Specifica Completa per App Mobile

**Versione**: 2.0
**Data**: 2025-12-20
**Target**: App Mobile User-Friendly (Flutter/React Native/Kotlin)

---

## 📋 Indice

1. [Analisi Gap Attuali](#-analisi-gap-attuali)
2. [Caratteristiche Esistenti](#-caratteristiche-ble-esistenti)
3. [Caratteristiche Necessarie NUOVE](#-caratteristiche-ble-necessarie-nuove)
4. [Mapping UI/Bottoni](#-mapping-uibottoni-per-app-mobile)
5. [Formato Messaggi](#-formato-messaggi)
6. [Esempio Schermata App](#-esempio-schermata-app-mobile)

---

## 🔍 Analisi Gap Attuali

### ❌ **Endpoint Mancanti Critici**

Dopo aver analizzato `ledsaber_control.py` e `saber_dashboard.py`, ho identificato questi **gap critici**:

| Funzionalità | Attuale | Necessario per App |
|-------------|---------|-------------------|
| **Accensione/Spegnimento Lama** | Solo via `brightness + enabled` | ✅ Comando dedicato `IGNITION/RETRACT` |
| **Reboot Device** | ❌ Assente | ✅ Comando `REBOOT` |
| **Power Off / Deep Sleep** | ❌ Assente | ✅ Comando `SLEEP` |
| **Lista Effetti Dinamica** | ❌ Hardcoded nell'app | ✅ Characteristic `EFFECTS_LIST` (READ) |
| **Stato Lama** | ❌ Solo `enabled` booleano | ✅ Campo `bladeState` (off/igniting/on/retracting) |
| **Metadati Effetti** | ❌ App non sa quali parametri ha ogni effetto | ✅ JSON con parametri disponibili per effetto |

### ✅ **Endpoint Già Disponibili**

Gli script Python usano già questi endpoint che funzionano bene:

- ✅ **LED Control**: Color, Effect, Brightness, Status LED, Fold Point
- ✅ **Camera Control**: Init, Start, Stop, Status, Metrics, Flash
- ✅ **Motion Control**: Enable, Disable, Status, Config, Sensitivity
- ✅ **OTA**: Firmware update via BLE
- ✅ **Time Sync**: Sincronizzazione orologio per Chrono effects
- ✅ **Chrono Themes**: Selezione temi hour/second

---

## 📡 Caratteristiche BLE Esistenti

### **Service: LED Control** (`4fafc201-1fb5-459e-8fcc-c5c9c331914b`)

| Characteristic | UUID | Ops | Formato | Descrizione |
|---------------|------|-----|---------|-------------|
| **LED State** | `beb5483e-36e1-4688-b7f5-ea07361b26a8` | READ, NOTIFY | JSON | Stato completo LED (colore, effetto, brightness, enabled) |
| **LED Color** | `d1e5a4c3-eb10-4a3e-8a4c-1234567890ab` | WRITE | JSON | Imposta colore RGB `{"r":255,"g":0,"b":0}` |
| **LED Effect** | `e2f6b5d4-fc21-5b4f-9b5d-2345678901bc` | WRITE | JSON | Imposta effetto `{"mode":"pulse","speed":100}` |
| **LED Brightness** | `f3e7c6e5-0d32-4c5a-ac6e-3456789012cd` | WRITE | JSON | Luminosità + on/off `{"brightness":60,"enabled":true}` |
| **Status LED** | `a4b8d7f9-1e43-6c7d-ad8f-456789abcdef` | READ, WRITE | JSON | LED integrato pin 4 `{"enabled":true,"brightness":32}` |
| **Fold Point** | `a5b0f9a7-3c65-8e9f-cf0c-6789abcdef01` | READ, WRITE | JSON | Punto di piegatura `{"foldPoint":72}` (1-143) |
| **Time Sync** | `d6e1a0b8-4a76-9f0c-dc1a-789abcdef012` | WRITE | JSON | Sincronizza tempo `{"epoch":1703107200}` |

#### **LED State Notify** (ogni 500ms se connesso)
```json
{
  "r": 255,
  "g": 0,
  "b": 0,
  "brightness": 50,
  "effect": "pulse",
  "speed": 100,
  "enabled": true,
  "statusLedEnabled": true,
  "statusLedBrightness": 32,
  "foldPoint": 72
}
```

---

### **Service: OTA** (`4fafc202-1fb5-459e-8fcc-c5c9c331914b`)

| Characteristic | UUID | Ops | Formato | Descrizione |
|---------------|------|-----|---------|-------------|
| **FW Version** | `a4b8d7fa-1e43-6c7d-ad8f-456789abcdef` | READ | String | Versione firmware (es: "1.0.0") |
| **OTA Status** | `d1e5a4c4-eb10-4a3e-8a4c-1234567890ab` | READ, NOTIFY | String | Stato OTA (IDLE/WAITING/RECEIVING/VERIFYING/READY/ERROR/RECOVERY) |
| **OTA Progress** | `f3e7c6e6-0d32-4c5a-ac6e-3456789012cd` | READ, NOTIFY | String | Percentuale 0-100% |
| **OTA Control** | `e2f6b5d5-fc21-5b4f-9b5d-2345678901bc` | WRITE | Binary | Comandi: 0x01=START, 0x02=ABORT, 0x03=VERIFY, 0x04=REBOOT |
| **OTA Data** | `beb5483f-36e1-4688-b7f5-ea07361b26a8` | WRITE | Binary | Chunk firmware (max 512 bytes) |

#### **Protocollo OTA Completo** ⚠️ CRITICO

Il protocollo OTA ha diversi **"hack" critici** per gestire MTU, disconnessioni e rate limiting. Questi dettagli sono estratti da `update_saber.py`.

##### **Flusso Completo OTA**

```
1. CONNECT + MTU Acquisition
   ├─ Stabilizza connessione (sleep 1s)
   ├─ Acquisisci MTU reale (BlueZ hack)
   └─ Leggi FW version corrente

2. START Command
   ├─ Invia: 0x01 + <firmware_size uint32 little-endian>
   ├─ ⚠ HACK: Il firmware può disconnettere durante esp_ota_begin()
   ├─ Wait loop (max 10s): controlla stato ogni 0.5s
   ├─ Se disconnesso: riconnetti automaticamente
   └─ Aspetta stato = WAITING

3. UPLOAD CHUNKS
   ├─ Chunk size = min(512, MTU-3)
   ├─ ⚠ HACK: Se MTU=23 (default BlueZ), NON ridurre a 20!
   ├─ Usa write_gatt_char(..., response=False)  ← CRITICO per velocità
   ├─ Invia 100 chunk poi pausa minima (batch rate limiting)
   └─ Monitora OTA_STATUS per errori

4. VERIFY (automatica)
   ├─ Firmware fa auto-verifica MD5/SHA256
   ├─ Aspetta stato = READY o ERROR (max 30s)
   └─ Se READY → upload OK

5. REBOOT (opzionale)
   ├─ Invia: 0x04 (REBOOT command)
   ├─ Disconnetti
   ├─ Wait 3s
   └─ Riconnetti e leggi nuova FW version
```

##### **Hack 1: MTU Acquisition (BlueZ)**

**Problema**: Bleak su Linux/BlueZ riporta MTU=23 di default anche se il device supporta MTU più alto (es. 517).

**Soluzione** (da `update_saber.py:113-140`):

```python
async def _try_acquire_mtu(self) -> None:
    backend = getattr(self.client, "_backend", None)

    # Force services discovery
    if hasattr(self.client, "get_services"):
        await self.client.get_services()

    # Call internal BlueZ MTU acquire
    acquire_mtu = getattr(backend, "_acquire_mtu", None)
    if acquire_mtu is not None:
        await acquire_mtu()
        self.mtu_acquired = True
```

**Motivazione**: Senza questo hack, Bleak usa chunk da 20 bytes invece di 512, rendendo l'OTA 25x più lento.

**Per App Mobile**:
- Android: Usa `device.requestMtu(517)` dopo connessione
- iOS: MTU automatico (nessun workaround necessario)

---

##### **Hack 2: Gestione Disconnessione Durante START**

**Problema**: Il firmware ESP32 chiama `esp_ota_begin()` che può causare disconnessione BLE temporanea.

**Soluzione** (da `update_saber.py:300-328`):

```python
# Invia START command
await self.send_command(OTA_CMD_START, size_bytes)

# Wait loop con reconnect automatico
for i in range(max_wait * 2):
    await asyncio.sleep(0.5)

    # Check disconnessione
    if not self.client.is_connected:
        print("⚠ Disconnessione durante START. Riconnetto...")
        await asyncio.sleep(2.0)

        # Riconnetti
        if not await self.connect(address):
            return False  # FAIL
        break

    # Verifica stato
    await self.refresh_status()
    if self.ota_state == OTA_STATE_WAITING:
        print("✓ Device pronto (WAITING)")
        break
```

**Motivazione**: Se l'app non gestisce questa disconnessione, l'OTA fallisce subito dopo START.

**Per App Mobile**:
- Implementa reconnect automatico con retry (max 3 tentativi)
- Mostra loading spinner "Preparing device..."
- Non mostrare errore se disconnette per <5 secondi

---

##### **Hack 3: Chunk Size Basato su MTU**

**Problema**: Se MTU è basso, chunk da 512 bytes falliscono. Se MTU è alto, chunk piccoli sprecano bandwidth.

**Soluzione** (da `update_saber.py:335-339`):

```python
chunk_size_max = 512  # Default

if self.negotiated_mtu >= 64:
    # MTU affidabile: usa MTU-3 (header overhead)
    chunk_size_max = min(512, self.negotiated_mtu - 3)
elif self.negotiated_mtu > 0 and self.negotiated_mtu != 23:
    # MTU custom ma basso
    chunk_size_max = min(512, max(20, self.negotiated_mtu - 3))
# else: Se MTU=23 (default BlueZ), mantieni 512 (hack assume MTU reale > 512)
```

**Tabella MTU → Chunk Size**:

| MTU Negotiated | Chunk Size | Velocità Teorica |
|----------------|------------|------------------|
| 23 (default) | 512 bytes | ⚠️ **Assume MTU reale > 512** (hack) |
| 185 | 182 bytes | ~18 KB/s |
| 247 | 244 bytes | ~24 KB/s |
| 517 | 512 bytes | ~50 KB/s |

**Per App Mobile**:
- Dopo connessione: request MTU 517
- Se MTU < 64: usa chunk 20 bytes (safe fallback)
- Se MTU >= 64: usa `min(512, MTU-3)`

---

##### **Hack 4: Write Without Response (WWR)**

**Problema**: GATT Write con response aspetta ACK per ogni chunk → molto lento (1 chunk ogni 50ms).

**Soluzione** (da `update_saber.py:351`):

```python
# Usa Write Without Response (WWR) per massima velocità
await self.client.write_gatt_char(
    CHAR_OTA_DATA_UUID,
    chunk,
    response=False  # ← CRITICO!
)
```

**Differenza Prestazioni**:

| Modalità | Tempo per 1MB | Chunk/sec |
|----------|---------------|-----------|
| Write with Response | ~90 secondi | ~20 chunk/s |
| Write Without Response | ~20 secondi | ~100 chunk/s |

**Per App Mobile**:
- Android: `characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE`
- iOS: `peripheral.writeValue(data, for: characteristic, type: .withoutResponse)`

---

##### **Hack 5: Batch Rate Limiting**

**Problema**: Inviare chunk troppo velocemente satura la coda GATT del dispositivo → chunk persi.

**Soluzione** (da `update_saber.py:343`):

```python
CHUNKS_PER_BATCH = 100  # 50KB batch

while offset < firmware_size:
    chunk = firmware_data[offset:offset + chunk_size]
    await self.client.write_gatt_char(CHAR_OTA_DATA_UUID, chunk, response=False)
    offset += chunk_size
    chunk_count += 1

    # Micro-pausa ogni 100 chunk (implicita in await)
    # Il firmware ha buffer 64 chunk (32KB)
```

**Buffer Firmware**:
- Queue RX: 64 chunk × 512 bytes = **32KB buffer**
- Se superi 32KB senza pausa → chunk persi

**Per App Mobile**:
- Invia max 50 chunk consecutivi
- Pausa 10ms ogni 50 chunk
- Monitora `OTA_PROGRESS` notify per verificare ricezione

---

##### **OTA Status Notify Format**

```
Formato: "STATE_INT:ERROR_MESSAGE"

Esempi:
- "0:" → IDLE (nessun errore)
- "1:" → WAITING (pronto a ricevere)
- "2:" → RECEIVING (upload in corso)
- "3:" → VERIFYING (controllo MD5)
- "4:" → READY (OK, pronto per reboot)
- "5:MD5 mismatch" → ERROR con messaggio
- "6:" → RECOVERY (rollback automatico)
```

**Stati Enum** (C++):
```cpp
enum OTAState {
    IDLE = 0,
    WAITING = 1,
    RECEIVING = 2,
    VERIFYING = 3,
    READY = 4,
    ERROR = 5,
    RECOVERY = 6
};
```

---

##### **OTA Progress Notify Format**

```
Formato: "PERCENT:RECEIVED_BYTES:TOTAL_BYTES"

Esempio:
"45:923648:2048000" → 45% (923KB / 2000KB)
```

**Frequenza Notify**: Ogni 1% o ogni 10KB (quello che viene prima)

---

##### **Esempio Sequenza Completa (Timeline)**

```
T+0.0s  → CONNECT (address)
T+1.0s  → _try_acquire_mtu()
T+1.5s  → read FW_VERSION → "1.0.0"
T+2.0s  → subscribe OTA_STATUS notify
T+2.5s  → subscribe OTA_PROGRESS notify

T+3.0s  → write OTA_CONTROL: 0x01 + <size:1966080>
T+3.2s  → ⚠ DISCONNECTED (esp_ota_begin)
T+5.2s  → RECONNECT success
T+6.0s  → read OTA_STATUS → "1:" (WAITING)

T+6.5s  → write OTA_DATA chunk 0 (512 bytes)
T+6.5s  → write OTA_DATA chunk 1 (512 bytes)
...
T+26.0s → write OTA_DATA chunk 3839 (ultimo)
T+26.1s → OTA_PROGRESS notify: "100:1966080:1966080"

T+27.0s → OTA_STATUS notify: "3:" (VERIFYING)
T+28.0s → OTA_STATUS notify: "4:" (READY)

T+29.0s → write OTA_CONTROL: 0x04 (REBOOT)
T+30.0s → DISCONNECT
T+35.0s → RECONNECT
T+36.0s → read FW_VERSION → "1.1.0" ✓
```

**Tempo Totale**: ~36 secondi per firmware da 1.9MB

---

##### **Gestione Errori OTA**

| Errore | Causa | Soluzione App |
|--------|-------|---------------|
| **"Partition not found"** | OTA partition non configurata | Mostra "Firmware incompatibile" |
| **"MD5 mismatch"** | Chunk persi o corrotti | Retry upload (max 3 tentativi) |
| **"Not enough space"** | Firmware > 1.9MB | Mostra "File troppo grande" |
| **Timeout durante VERIFY** | MD5 check lento | Aspetta max 30s prima di fallback |
| **Disconnessione permanente** | BLE instabile | Mostra "Reconnect failed, retry?" |

---

##### **Codice Esempio Flutter (OTA Upload)**

```dart
class LedSaberOTA {
  static const MTU_TARGET = 517;
  static const CHUNK_SIZE = 512;
  static const BATCH_SIZE = 50; // chunk per batch

  BluetoothCharacteristic? _otaDataChar;
  BluetoothCharacteristic? _otaControlChar;

  Future<bool> uploadFirmware(Uint8List firmwareBytes) async {
    // 1. Request MTU
    await device.requestMtu(MTU_TARGET);
    final mtu = await device.mtu.first;
    final chunkSize = min(CHUNK_SIZE, mtu - 3);

    // 2. START command
    final sizeBytes = ByteData(4)..setUint32(0, firmwareBytes.length, Endian.little);
    await _otaControlChar!.write([0x01, ...sizeBytes.buffer.asUint8List()]);

    // 3. Wait for WAITING state (con reconnect se necessario)
    if (!await _waitForState(OTAState.WAITING, maxSeconds: 10)) {
      return false;
    }

    // 4. Upload chunks
    int offset = 0;
    int batchCount = 0;
    while (offset < firmwareBytes.length) {
      final chunk = firmwareBytes.sublist(offset, min(offset + chunkSize, firmwareBytes.length));

      // Write Without Response
      await _otaDataChar!.write(chunk, withoutResponse: true);

      offset += chunk.length;
      batchCount++;

      // Pausa ogni BATCH_SIZE chunk
      if (batchCount >= BATCH_SIZE) {
        await Future.delayed(Duration(milliseconds: 10));
        batchCount = 0;
      }
    }

    // 5. Wait for READY
    return await _waitForState(OTAState.READY, maxSeconds: 30);
  }

  Future<bool> _waitForState(OTAState targetState, {required int maxSeconds}) async {
    // Subscribe OTA_STATUS e aspetta target state
    // Gestisce disconnessioni con auto-reconnect
    // ...
  }
}
```

---

### **Service: Camera** (`5fafc301-1fb5-459e-8fcc-c5c9c331914b`)

| Characteristic | UUID | Ops | Formato | Descrizione |
|---------------|------|-----|---------|-------------|
| **Camera Status** | `6eb5483e-36e1-4688-b7f5-ea07361b26a8` | READ, NOTIFY | JSON | Stato camera (initialized, active, fps, frames) |
| **Camera Control** | `7dc5a4c3-eb10-4a3e-8a4c-1234567890ab` | WRITE | String | Comandi: `init`, `capture`, `start`, `stop`, `reset_metrics` |
| **Camera Metrics** | `8ef6b5d4-fc21-5b4f-9b5d-2345678901bc` | READ | JSON | Metriche dettagliate (FPS, frame count, memoria) |
| **Camera Flash** | `9fe7c6e5-0d32-4c5a-ac6e-3456789012cd` | READ, WRITE | JSON | Controllo flash `{"enabled":true,"brightness":255}` |

#### **Camera Status Notify**
```json
{
  "initialized": true,
  "active": true,
  "fps": 24.5,
  "totalFrames": 1234,
  "failedCaptures": 2
}
```

---

### **Service: Motion Detection** (`6fafc401-1fb5-459e-8fcc-c5c9c331914b`)

| Characteristic | UUID | Ops | Formato | Descrizione |
|---------------|------|-----|---------|-------------|
| **Motion Status** | `7eb5583e-36e1-4688-b7f5-ea07361b26a9` | READ, NOTIFY | JSON | Intensità, direzione, velocità, gesture |
| **Motion Control** | `8dc5b4c3-eb10-4a3e-8a4c-1234567890ac` | WRITE | String | Comandi: `enable`, `disable`, `reset`, `calibrate`, `quality <val>` |
| **Motion Events** | `9ef6c5d4-fc21-5b4f-9b5d-2345678901bd` | NOTIFY | JSON | Eventi gesturali (shake_detected, motion_started, motion_ended) |
| **Motion Config** | `aff7d6e5-0d32-4c5a-ac6e-3456789012ce` | READ, WRITE | JSON | Sensibilità, soglie gesture |

#### **Motion Status Notify**
```json
{
  "enabled": true,
  "motionDetected": true,
  "intensity": 150,
  "direction": "UP",
  "changedPixels": 1250,
  "shakeDetected": false,
  "gesture": "IGNITION",
  "gestureConfidence": 85,
  "totalFrames": 5000,
  "motionFrames": 320,
  "grid": ["^v^..", "..>.<", "^^^^^"],
  "gridRows": 6,
  "gridCols": 8,
  "blockSize": 40
}
```

#### **Motion Events Notify**
```json
{
  "event": "shake_detected",
  "timestamp": 12345678,
  "intensity": 200,
  "direction": "UP",
  "changedPixels": 1500,
  "gesture": "CLASH",
  "gestureConfidence": 95
}
```

---

## 🆕 Caratteristiche BLE NECESSARIE (NUOVE)

### **1. DEVICE_CONTROL** (Aggiungere al Service LED)

**UUID**: `c7f8e0d9-5b87-1a2b-be9d-7890abcdef23`
**Operazioni**: WRITE
**Formato**: JSON

#### Comandi Supportati

```json
// Accendi lama con animazione ignition
{"command": "ignition"}

// Spegni lama con animazione retract
{"command": "retract"}

// Riavvia ESP32
{"command": "reboot"}

// Deep sleep (risveglio con motion sensor)
{"command": "sleep"}

// Config boot: auto-ignition dopo delay (persistito in LittleFS)
{"command":"boot_config","autoIgnitionOnBoot":true,"autoIgnitionDelayMs":5000}
```

#### Motivazione
- L'app ha bisogno di un **Power Button** chiaro per accendere/spegnere la lama
- Attualmente si usa `brightness=0, enabled=false` che è contro-intuitivo
- L'animazione ignition/retract è già implementata nel firmware ma non esposta via BLE
- Reboot necessario per recovery da errori senza dover disconnettere fisicamente

---

### **2. EFFECTS_LIST** (Aggiungere al Service LED)

**UUID**: `d8f9e1ea-6c98-2b3c-cf0e-890abcdef234`
**Operazioni**: READ
**Formato**: JSON

#### Risposta

```json
{
  "version": "1.0.0",
  "effects": [
    {
      "id": "solid",
      "name": "Solid Color",
      "description": "Colore uniforme statico",
      "params": ["color"],
      "icon": "🟢"
    },
    {
      "id": "rainbow",
      "name": "Rainbow",
      "description": "Arcobaleno scorrevole",
      "params": ["speed"],
      "icon": "🌈"
    },
    {
      "id": "pulse",
      "name": "Pulse Wave",
      "description": "Onda di energia",
      "params": ["speed", "color"],
      "icon": "⚡"
    },
    {
      "id": "breathe",
      "name": "Breathing",
      "description": "Pulsazione sinusoidale",
      "params": ["speed"],
      "icon": "💨"
    },
    {
      "id": "flicker",
      "name": "Kylo Ren Flicker",
      "description": "Instabilità plasma",
      "params": ["speed"],
      "icon": "🔥"
    },
    {
      "id": "unstable",
      "name": "Kylo Ren Advanced",
      "description": "Flicker + sparks",
      "params": ["speed"],
      "icon": "💥"
    },
    {
      "id": "dual_pulse",
      "name": "Dual Pulse",
      "description": "Due onde opposte",
      "params": ["speed"],
      "icon": "⚔️"
    },
    {
      "id": "rainbow_blade",
      "name": "Rainbow Blade",
      "description": "Arcobaleno lineare",
      "params": ["speed"],
      "icon": "🌟"
    },
    {
      "id": "chrono_hybrid",
      "name": "Chrono Clock",
      "description": "Orologio con temi modulabili",
      "params": ["chronoHourTheme", "chronoSecondTheme"],
      "themes": {
        "hour": ["Classic", "Neon", "Plasma", "Digital", "Inferno", "Storm"],
        "second": ["Classic", "Spiral", "Fire", "Lightning", "Particle", "Quantum"]
      },
      "icon": "🕐"
    }
  ]
}
```

#### Motivazione
- **Problema critico**: Se aggiungi un nuovo effetto nel firmware, l'app mobile NON lo vedrà
- L'app deve richiedere la lista al connect e popolare il dropdown dinamicamente
- Permette di aggiungere metadati (icone, descrizioni) senza ricompilare l'app
- Indica quali parametri ha ogni effetto (speed, color, themes)

---

### **3. Modifica LED_STATE Notify** (Campo bladeState)

**UUID**: `beb5483e-36e1-4688-b7f5-ea07361b26a8` (esistente)
**Operazioni**: READ, NOTIFY
**Formato**: JSON aggiornato

#### Risposta Aggiornata

```json
{
  "r": 255,
  "g": 0,
  "b": 0,
  "brightness": 50,
  "effect": "pulse",
  "speed": 100,
  "enabled": true,
  "bladeState": "on",  // ← NUOVO: "off" | "igniting" | "on" | "retracting"
  "statusLedEnabled": true,
  "statusLedBrightness": 32,
  "foldPoint": 72
}
```

#### Valori `bladeState`

- `"off"` - Lama spenta
- `"igniting"` - Animazione accensione in corso
- `"on"` - Lama completamente accesa
- `"retracting"` - Animazione spegnimento in corso

#### Motivazione
- L'app deve mostrare feedback visivo durante le animazioni
- Permette di disabilitare il power button durante ignition/retract
- L'utente vede lo stato reale della lama, non solo `enabled: true/false`

---

## 🎨 Mapping UI/Bottoni per App Mobile

### **Schermata Principale**

```
┌─────────────────────────────────────┐
│  🗡️  LedSaber                      │
│  ┌─────────────────────────────┐   │
│  │  [●] LedSaber Mk1           │   │  ← Connessione BLE
│  │  v1.2.3 • 85% battery       │   │
│  └─────────────────────────────┘   │
│                                     │
│  ┌─────────────────────────────┐   │
│  │                             │   │
│  │    ┌────────────────┐       │   │
│  │    │   POWER ON     │       │   │  ← Grande Power Button
│  │    │   [  ON  ]     │       │   │    (ignition/retract)
│  │    └────────────────┘       │   │
│  │                             │   │
│  │  Current: Pulse Wave ⚡      │   │
│  │  RGB(255, 0, 0) • 80%       │   │
│  └─────────────────────────────┘   │
│                                     │
│  🎨 Color Picker                    │
│  ┌─────────────────────────────┐   │
│  │   [  RGB Color Wheel  ]     │   │
│  └─────────────────────────────┘   │
│                                     │
│  ✨ Effects                         │
│  ┌─────────────────────────────┐   │
│  │  [Dropdown: Pulse Wave ▼]   │   │  ← Popolato da EFFECTS_LIST
│  └─────────────────────────────┘   │
│                                     │
│  ⚡ Speed                           │
│  ┌─────────────────────────────┐   │
│  │  [────●───────────] 150     │   │
│  └─────────────────────────────┘   │
│                                     │
│  ☀️ Brightness                     │
│  ┌─────────────────────────────┐   │
│  │  [──────────●─────] 80%     │   │
│  └─────────────────────────────┘   │
│                                     │
│  ⚙️ Advanced                       │
│  ├─ 🔄 Reboot Device               │  ← DEVICE_CONTROL reboot
│  ├─ 💤 Sleep Mode                  │  ← DEVICE_CONTROL sleep
│  ├─ 📦 Firmware Update             │  ← OTA
│  └─ ⚙️  Settings                   │
│                                     │
└─────────────────────────────────────┘
```

### **Bottoni e Azioni**

| UI Element | BLE Endpoint | Payload | Note |
|-----------|--------------|---------|------|
| **Power Button** | `DEVICE_CONTROL` | `{"command":"ignition"}` / `{"command":"retract"}` | Toggle in base a `bladeState` |
| **Color Wheel** | `LED_COLOR` | `{"r":255,"g":0,"b":0}` | Real-time update |
| **Effect Dropdown** | `LED_EFFECT` | `{"mode":"pulse","speed":150}` | Lista da `EFFECTS_LIST` |
| **Speed Slider** | `LED_EFFECT` | `{"mode":"pulse","speed":100}` | Mostrato solo se effetto ha param `speed` |
| **Brightness Slider** | `LED_BRIGHTNESS` | `{"brightness":80,"enabled":true}` | 0-112 (max sicuro 58%) |
| **Reboot Button** | `DEVICE_CONTROL` | `{"command":"reboot"}` | Con conferma dialog |
| **Sleep Button** | `DEVICE_CONTROL` | `{"command":"sleep"}` | Con conferma dialog |
| **Firmware Update** | `OTA_CONTROL` + `OTA_DATA` | Binary protocol | Schermata dedicata |

---

### **Schermata Effects Selector**

Quando l'utente clicca sul dropdown effetti, l'app deve:

1. **Al Connect**: `READ EFFECTS_LIST` → salva in cache locale
2. **Popola Dropdown**: Mostra `effect.name` + `effect.icon`
3. **Selezione Effetto**: Mostra parametri dinamici in base a `effect.params`

**Esempio UI Dinamica**:

```
Effetto selezionato: Pulse Wave ⚡
┌─────────────────────────────────┐
│  Parametri:                     │
│                                 │
│  ⚡ Speed                       │
│  [────●───────────] 150         │  ← Mostrato perché params contiene "speed"
│                                 │
│  🎨 Color                       │
│  [  RGB Picker  ]               │  ← Mostrato perché params contiene "color"
│                                 │
│  [  APPLY  ]                    │
└─────────────────────────────────┘

Effetto selezionato: Chrono Clock 🕐
┌─────────────────────────────────┐
│  Parametri:                     │
│                                 │
│  ⏰ Hour Theme                  │
│  [Dropdown: Neon ▼]             │  ← Lista da effect.themes.hour
│                                 │
│  ⏱  Second Theme                │
│  [Dropdown: Fire ▼]             │  ← Lista da effect.themes.second
│                                 │
│  [  APPLY  ]                    │
└─────────────────────────────────┘
```

---

### **Schermata Camera (Tab separato)**

```
┌─────────────────────────────────┐
│  📸 Camera                      │
│                                 │
│  Status: ◉ ACTIVE               │
│  FPS: 24.5 █▇▆▅▄▃▂▁ (sparkline)│
│  Frames: 12,345                 │
│                                 │
│  [  Init  ] [  Start  ] [  Stop  ]  │
│                                 │
│  Flash: [●] ON  [─────●──] 80%  │
└─────────────────────────────────┘
```

| UI Element | BLE Endpoint | Payload |
|-----------|--------------|---------|
| **Init Button** | `CAMERA_CONTROL` | `"init"` (string) |
| **Start Button** | `CAMERA_CONTROL` | `"start"` |
| **Stop Button** | `CAMERA_CONTROL` | `"stop"` |
| **Flash Toggle** | `CAMERA_FLASH` | `{"enabled":true,"brightness":255}` |

---

### **Schermata Motion (Tab separato)**

```
┌─────────────────────────────────┐
│  🔍 Motion Detection            │
│                                 │
│  [●] ENABLED                    │
│  Intensity: 150  █████████░░░   │
│  Direction: ↑ UP                │
│                                 │
│  ⚔️  GESTURE: IGNITION (85%)    │
│                                 │
│  Optical Flow Grid (8x6):       │
│  ┌─────────────────────────┐   │
│  │  ^ v ^ . . < > A        │   │
│  │  . . > . < ^ ^ B        │   │
│  │  ^ ^ ^ ^ ^ v v C        │   │
│  └─────────────────────────┘   │
│                                 │
│  Sensitivity: [────●─────] 128  │
│                                 │
│  [  Enable  ] [  Disable  ] [  Reset  ]  │
└─────────────────────────────────┘
```

| UI Element | BLE Endpoint | Payload |
|-----------|--------------|---------|
| **Enable Button** | `MOTION_CONTROL` | `"enable"` (string) |
| **Disable Button** | `MOTION_CONTROL` | `"disable"` |
| **Reset Button** | `MOTION_CONTROL` | `"reset"` |
| **Quality Slider** | `MOTION_CONTROL` | `"quality 128"` (string con valore) |

---

## 📦 Formato Messaggi

### **JSON per Effetti con Temi Chrono**

```json
{
  "mode": "chrono_hybrid",
  "chronoHourTheme": 1,
  "chronoSecondTheme": 2
}
```

Inviato su `LED_EFFECT` (`e2f6b5d4-fc21-5b4f-9b5d-2345678901bc`)

---

### **JSON per Colore RGB**

```json
{
  "r": 255,
  "g": 128,
  "b": 64
}
```

Inviato su `LED_COLOR` (`d1e5a4c3-eb10-4a3e-8a4c-1234567890ab`)

---

### **JSON per Brightness + On/Off**

```json
{
  "brightness": 80,
  "enabled": true
}
```

Inviato su `LED_BRIGHTNESS` (`f3e7c6e5-0d32-4c5a-ac6e-3456789012cd`)

---

### **JSON per Sincronizzazione Tempo**

```json
{
  "epoch": 1703107200
}
```

Inviato su `TIME_SYNC` (`d6e1a0b8-4a76-9f0c-dc1a-789abcdef012`)

---

## 🎯 Riepilogo Implementazione Firmware

### **Modifiche Necessarie al Firmware**

#### 1. Aggiungere `DEVICE_CONTROL` Characteristic

**File**: `src/BLELedController.cpp` e `include/BLELedController.h`

```cpp
// In BLELedController.h
#define CHAR_DEVICE_CONTROL_UUID "c7f8e0d9-5b87-1a2b-be9d-7890abcdef23"

// In BLELedController.cpp setup()
pDeviceControlCharacteristic = pService->createCharacteristic(
    CHAR_DEVICE_CONTROL_UUID,
    BLECharacteristic::PROPERTY_WRITE
);
pDeviceControlCharacteristic->setCallbacks(new DeviceControlCallbacks());

// Callback handler
class DeviceControlCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        // Parse JSON
        DynamicJsonDocument doc(128);
        deserializeJson(doc, value);
        String command = doc["command"];

        if (command == "ignition") {
            ledEffectEngine.triggerIgnitionOneShot();
        } else if (command == "retract") {
            ledEffectEngine.triggerRetractionOneShot();
        } else if (command == "reboot") {
            ESP.restart();
        } else if (command == "sleep") {
            esp_deep_sleep_start();
        }
    }
};
```

---

#### 2. Aggiungere `EFFECTS_LIST` Characteristic

**File**: `src/BLELedController.cpp`

```cpp
// In BLELedController.h
#define CHAR_EFFECTS_LIST_UUID "d8f9e1ea-6c98-2b3c-cf0e-890abcdef234"

// In BLELedController.cpp setup()
pEffectsListCharacteristic = pService->createCharacteristic(
    CHAR_EFFECTS_LIST_UUID,
    BLECharacteristic::PROPERTY_READ
);
pEffectsListCharacteristic->setCallbacks(new EffectsListCallbacks());

// Callback handler
class EffectsListCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* pCharacteristic) {
        String effectsList = R"({
  "version": "1.0.0",
  "effects": [
    {"id":"solid","name":"Solid Color","params":["color"]},
    {"id":"rainbow","name":"Rainbow","params":["speed"]},
    {"id":"pulse","name":"Pulse Wave","params":["speed","color"]},
    {"id":"breathe","name":"Breathing","params":["speed"]},
    {"id":"flicker","name":"Kylo Ren","params":["speed"]},
    {"id":"unstable","name":"Kylo Advanced","params":["speed"]},
    {"id":"dual_pulse","name":"Dual Pulse","params":["speed"]},
    {"id":"rainbow_blade","name":"Rainbow Blade","params":["speed"]},
    {"id":"chrono_hybrid","name":"Chrono Clock","params":["chronoHourTheme","chronoSecondTheme"],"themes":{"hour":["Classic","Neon","Plasma","Digital","Inferno","Storm"],"second":["Classic","Spiral","Fire","Lightning","Particle","Quantum"]}}
  ]
})";
        pCharacteristic->setValue(effectsList.c_str());
    }
};
```

---

#### 3. Aggiungere campo `bladeState` al LED_STATE Notify

**File**: `src/BLELedController.cpp`

```cpp
// Nel metodo che invia LED_STATE notify
void BLELedController::notifyLEDState() {
    DynamicJsonDocument doc(512);

    doc["r"] = state.r;
    doc["g"] = state.g;
    doc["b"] = state.b;
    doc["brightness"] = state.brightness;
    doc["effect"] = effectToString(state.effect);
    doc["speed"] = state.speed;
    doc["enabled"] = state.enabled;

    // NUOVO: Aggiungi bladeState
    String bladeState = "off";
    if (ledEffectEngine.isIgnitionActive()) {
        bladeState = "igniting";
    } else if (ledEffectEngine.isRetractionActive()) {
        bladeState = "retracting";
    } else if (state.enabled) {
        bladeState = "on";
    }
    doc["bladeState"] = bladeState;

    doc["statusLedEnabled"] = statusLedEnabled;
    doc["statusLedBrightness"] = statusLedBrightness;
    doc["foldPoint"] = foldPoint;

    String output;
    serializeJson(doc, output);
    pLEDStateCharacteristic->setValue(output.c_str());
    pLEDStateCharacteristic->notify();
}
```

**Aggiungi metodi helper in `LedEffectEngine.h`**:

```cpp
// In LedEffectEngine.h
bool isIgnitionActive() const {
    return currentMode == Mode::IGNITION_ACTIVE;
}

bool isRetractionActive() const {
    return currentMode == Mode::RETRACT_ACTIVE;
}
```

---

## 📱 Esempio Codice App (Flutter)

### **Classe BLE Client**

```dart
class LedSaberBLE {
  static const SERVICE_LED = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  static const CHAR_DEVICE_CONTROL = "c7f8e0d9-5b87-1a2b-be9d-7890abcdef23";
  static const CHAR_EFFECTS_LIST = "d8f9e1ea-6c98-2b3c-cf0e-890abcdef234";
  static const CHAR_LED_STATE = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  static const CHAR_LED_COLOR = "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab";
  static const CHAR_LED_EFFECT = "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc";

  BluetoothCharacteristic? _deviceControlChar;
  BluetoothCharacteristic? _effectsListChar;

  // Power Button Handler
  Future<void> togglePower(String currentBladeState) async {
    String command = (currentBladeState == "on") ? "retract" : "ignition";
    await _deviceControlChar?.write(
      utf8.encode('{"command":"$command"}')
    );
  }

  // Get Available Effects
  Future<List<Effect>> getAvailableEffects() async {
    final data = await _effectsListChar?.read();
    if (data == null) return [];

    final json = jsonDecode(utf8.decode(data));
    return (json['effects'] as List)
      .map((e) => Effect.fromJson(e))
      .toList();
  }

  // Set Color
  Future<void> setColor(int r, int g, int b) async {
    await _ledColorChar?.write(
      utf8.encode('{"r":$r,"g":$g,"b":$b}')
    );
  }

  // Set Effect
  Future<void> setEffect(String mode, {int? speed, Map<String, dynamic>? extraParams}) async {
    Map<String, dynamic> payload = {"mode": mode};
    if (speed != null) payload["speed"] = speed;
    if (extraParams != null) payload.addAll(extraParams);

    await _ledEffectChar?.write(
      utf8.encode(jsonEncode(payload))
    );
  }

  // Reboot Device
  Future<void> rebootDevice() async {
    await _deviceControlChar?.write(
      utf8.encode('{"command":"reboot"}')
    );
  }
}

// Effect Model
class Effect {
  final String id;
  final String name;
  final List<String> params;
  final Map<String, List<String>>? themes;

  Effect({required this.id, required this.name, required this.params, this.themes});

  factory Effect.fromJson(Map<String, dynamic> json) {
    return Effect(
      id: json['id'],
      name: json['name'],
      params: List<String>.from(json['params']),
      themes: json['themes'] != null
        ? (json['themes'] as Map<String, dynamic>).map(
            (key, value) => MapEntry(key, List<String>.from(value))
          )
        : null,
    );
  }

  bool hasSpeed() => params.contains('speed');
  bool hasColor() => params.contains('color');
  bool hasThemes() => themes != null;
}
```

---

### **UI Widget Power Button**

```dart
class PowerButton extends StatelessWidget {
  final String bladeState;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    bool isOn = bladeState == "on";
    bool isTransitioning = bladeState == "igniting" || bladeState == "retracting";

    return ElevatedButton(
      onPressed: isTransitioning ? null : onPressed,
      style: ElevatedButton.styleFrom(
        backgroundColor: isOn ? Colors.red : Colors.green,
        padding: EdgeInsets.symmetric(vertical: 24, horizontal: 48),
      ),
      child: Column(
        children: [
          Icon(
            isOn ? Icons.power_off : Icons.power_settings_new,
            size: 48,
          ),
          SizedBox(height: 8),
          Text(
            isTransitioning
              ? (bladeState == "igniting" ? "IGNITING..." : "RETRACTING...")
              : (isOn ? "POWER OFF" : "POWER ON"),
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          ),
        ],
      ),
    );
  }
}
```

---

## 🎬 Conclusioni

### **Riassunto Modifiche Necessarie**

1. ✅ **Aggiungere `DEVICE_CONTROL` characteristic** con comandi `ignition`, `retract`, `reboot`, `sleep`
2. ✅ **Aggiungere `EFFECTS_LIST` characteristic** (READ) con lista effetti in JSON
3. ✅ **Aggiungere campo `bladeState`** al notify `LED_STATE`
4. ✅ **Esporre `triggerIgnitionOneShot()` e `triggerRetractionOneShot()`** via BLE

### **Benefici per App Mobile**

- ✅ **Power Button Intuitivo**: Accensione/spegnimento con animazioni
- ✅ **Lista Effetti Dinamica**: Nessun aggiornamento app quando aggiungi nuovi effetti
- ✅ **UI Dinamica**: L'app mostra parametri diversi per ogni effetto
- ✅ **Feedback Visivo**: L'utente vede lo stato reale della lama (igniting/retracting)
- ✅ **Manutenzione**: Reboot e sleep per recovery senza disconnessione
- ✅ **Zero Hardcoding**: Tutto è configurabile via BLE

### **Prossimi Passi**

1. Implementare le 3 modifiche al firmware (DEVICE_CONTROL, EFFECTS_LIST, bladeState)
2. Testare con `ledsaber_control.py` aggiornato
3. Sviluppare app Flutter con UI dinamica
4. Beta testing con utenti finali

---

**Fine Specifica BLE API v2.0**
