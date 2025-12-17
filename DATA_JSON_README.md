# Directory Data - LittleFS Configuration

Questa directory contiene file di esempio per il filesystem LittleFS.

## File di Configurazione

Il file `/config.json` viene creato automaticamente da ConfigManager sul filesystem LittleFS dell'ESP32 (non in questa directory).

### Struttura config.json

```json
{
  "brightness": 50,
  "r": 0,
  "g": 255,
  "b": 128,
  "effect": "rainbow",
  "speed": 80,
  "enabled": true,
  "statusLedEnabled": false
}
```

### Campi Disponibili

| Campo | Tipo | Range | Default | Descrizione |
|-------|------|-------|---------|-------------|
| `brightness` | uint8 | 0-255 | 30 | Luminosità LED strip |
| `r` | uint8 | 0-255 | 255 | Componente rosso RGB |
| `g` | uint8 | 0-255 | 255 | Componente verde RGB |
| `b` | uint8 | 0-255 | 255 | Componente blu RGB |
| `effect` | string | - | "solid" | Effetto: "solid", "rainbow", "breathe" |
| `speed` | uint8 | 0-255 | 50 | Velocità effetto animato |
| `enabled` | bool | true/false | true | LED strip on/off |
| `statusLedEnabled` | bool | true/false | true | LED integrato pin 4 on/off |

### Comportamento

**Salvataggio Selettivo**:
- Il sistema salva SOLO i valori diversi dai default
- Se tutti i valori sono uguali ai default, il file viene eliminato
- Questo riduce l'usura della flash e lo spazio occupato

**Esempio 1** - Solo brightness modificato:
```json
{
  "brightness": 50
}
```

**Esempio 2** - Colore e effetto personalizzati:
```json
{
  "r": 255,
  "g": 0,
  "b": 0,
  "effect": "breathe",
  "speed": 100
}
```

**Esempio 3** - Tutti valori = default:
Il file `/config.json` non esiste (viene eliminato)

### Gestione Errori

- **File mancante**: Normale al primo avvio, usa defaults
- **JSON malformato**: File corrotto eliminato automaticamente, usa defaults
- **Filesystem corrotto**: Sistema continua con defaults in RAM

### Debug

Per vedere lo stato del filesystem e il contenuto della config:
```cpp
configManager.printDebugInfo();
```

Output esempio:
```
=== CONFIG DEBUG INFO ===
Filesystem: 192000/192000 bytes (48 used)

Files in root:
  - /config.json (48 bytes)

Config file content:
{"brightness":50,"effect":"rainbow"}
=========================
```

### Reset Configurazione

Per ripristinare tutti i default:
1. Via codice: `configManager.resetToDefaults()`
2. Via hardware: Elimina manualmente il file LittleFS

### Note Implementative

- **Partizione**: 192KB SPIFFS a offset 0x3D0000 (definita in `partitions_ota.csv`)
- **Path**: `/config.json` sul filesystem LittleFS
- **Salvataggio ritardato**: 5 secondi dopo l'ultima modifica
- **Protezione OTA**: Salvataggio bloccato durante aggiornamento firmware
