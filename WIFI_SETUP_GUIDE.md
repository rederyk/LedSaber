# Guida Setup WiFi - LedSaber Camera Web Server

## Overview

Il LedSaber ESP32-CAM ora supporta WiFi e web server per streaming camera via BLE. Puoi configurare il WiFi direttamente dal controller Python senza dover riprogrammare il firmware!

## Prerequisiti

1. ESP32-CAM flashato con il firmware che include BLEWiFiService
2. Controller Python `ledsaber_control.py` aggiornato
3. Connessione BLE attiva all'ESP32-CAM

## Metodo 1: Setup Guidato (Raccomandato) 🎯

Il modo più semplice per configurare il WiFi:

```bash
python3 ledsaber_control.py
```

Nel prompt interattivo:

```
> scan auto             # Trova e connetti all'ESP32-CAM
> wifi setup            # Avvia setup guidato
```

Il setup guidato ti chiederà:
1. **SSID WiFi**: Nome della tua rete WiFi
2. **Password WiFi**: Password della rete

Poi automaticamente:
- Configura le credenziali
- Connette al WiFi
- Mostra l'URL del web server (es: `http://192.168.1.100`)

### Esempio Output

```
🛠️  SETUP GUIDATO WIFI

SSID WiFi: MiaReteWiFi
Password WiFi: ********

📡 Configurazione WiFi...
✓ Comando WiFi inviato: wifi_configure MiaReteWiFi password123

📶 Connessione in corso...
✓ Comando WiFi inviato: wifi_enable

⏳ Attendi connessione WiFi (fino a 15 secondi)...

✓ CONNESSO!

  IP: 192.168.1.100
  SSID: MiaReteWiFi
  RSSI: -45 dBm

  📹 Web Server Camera:
  http://192.168.1.100

  💡 Apri questo URL nel browser per vedere lo streaming!
```

## Metodo 2: Comandi Manuali

Se preferisci più controllo, puoi usare i comandi individuali:

### Configura Credenziali

```
> wifi configure MiaReteWiFi password123
```

### Abilita WiFi e Connetti

```
> wifi enable
```

### Controlla Stato

```
> wifi status
```

### Disabilita WiFi

```
> wifi disable
```

## Comandi WiFi Disponibili

| Comando | Descrizione |
|---------|-------------|
| `wifi setup` | Setup guidato interattivo (raccomandato) |
| `wifi enable` | Abilita WiFi e connetti con credenziali salvate |
| `wifi disable` | Disabilita WiFi |
| `wifi status` | Mostra stato connessione WiFi |
| `wifi configure <ssid> <password>` | Configura credenziali WiFi |

## Accesso al Web Server

Una volta connesso al WiFi, puoi accedere al web server camera:

1. Ottieni l'IP dall'output del setup o dal comando `wifi status`
2. Apri il browser a: `http://<IP_ADDRESS>`
3. Vedrai una dashboard con:
   - **Streaming video live** dalla camera
   - **Overlay optical flow** con vettori movimento
   - **Statistiche real-time**: intensity, speed, direction, active blocks
   - **Griglia blocchi** con visualizzazione movimento

### Features del Web Server

- **MJPEG Streaming**: `/stream` - Streaming video continuo
- **Overlay Data**: `/overlay` - JSON con dati optical flow (aggiornato ogni 100ms)
- **Snapshot**: `/snapshot` - Frame JPEG singolo
- **Metriche**: `/metrics` - Metriche complete del detector

## Notifiche Automatiche

Il controller Python riceve automaticamente notifiche quando:
- WiFi si connette/disconnette
- IP address cambia
- RSSI varia significativamente (>5 dBm)
- Web server diventa disponibile

Abilita/disabilita notifiche con:

```
> notify
```

## Troubleshooting

### WiFi non si connette

1. Verifica SSID e password corretti
2. Controlla che la rete WiFi sia a 2.4 GHz (ESP32 non supporta 5 GHz)
3. Verifica segnale WiFi nella zona ESP32-CAM
4. Riprova con `wifi setup`

### Web server non risponde

1. Verifica connessione WiFi con `wifi status`
2. Controlla IP address corretto
3. Assicurati che firewall non blocchi connessioni
4. Prova a pingare l'IP dell'ESP32-CAM

### Timeout connessione

Se il setup impiega più di 15 secondi:
- Segnale WiFi debole, avvicina ESP32-CAM al router
- Credenziali errate, riprova con `wifi setup`
- Troppi dispositivi sulla rete, riavvia router

## Architettura Tecnica

### Servizio BLE WiFi

- **Service UUID**: `8a7f1234-5678-90ab-cdef-1234567890ac`
- **Control Characteristic**: `8a7f1235-5678-90ab-cdef-1234567890ac` (Write)
- **Status Characteristic**: `8a7f1236-5678-90ab-cdef-1234567890ac` (Read + Notify)

### Comandi BLE (Formato JSON)

Il servizio WiFi accetta comandi in formato JSON via la characteristic CHAR_WIFI_CONTROL.

| Comando JSON | Descrizione |
|--------------|-------------|
| `{"cmd": "enable"}` | Abilita WiFi e connetti con credenziali salvate |
| `{"cmd": "disable"}` | Disabilita WiFi |
| `{"cmd": "configure", "ssid": "nome", "pass": "pwd"}` | Configura credenziali WiFi |
| `{"cmd": "status"}` | Richiedi update status immediato |

**Nota**: Il controller Python `ledsaber_control.py` gestisce automaticamente la conversione in JSON.

### Status JSON

La characteristic CHAR_WIFI_STATUS restituisce un JSON con le seguenti chiavi:

```json
{
  "enabled": true,
  "connected": true,
  "ssid": "MiaReteWiFi",
  "ip": "192.168.1.100",
  "rssi": -45,
  "url": "http://192.168.1.100"
}
```

Quando disconnesso:
```json
{
  "enabled": false,
  "connected": false,
  "ssid": "",
  "ip": "",
  "rssi": 0,
  "url": ""
}
```

## Note di Sicurezza

⚠️ **IMPORTANTE**:
- Le credenziali WiFi sono trasmesse via BLE (range limitato ~10m)
- Le credenziali sono salvate in memoria ESP32 (non persistenti al reboot)
- Il web server **NON** ha autenticazione - usa solo su reti fidate
- Per produzione, implementa HTTPS e autenticazione

## Performance

- **Web Server FPS**: ~10 FPS streaming (configurabile in platformio.ini)
- **Overlay Update**: 10 Hz (ogni 100ms)
- **RAM Usage**: ~20% con web server attivo
- **Flash Usage**: 91% (quasi pieno ma OK)

## Esempio Workflow Completo

```bash
# 1. Avvia controller Python
python3 ledsaber_control.py

# 2. Connetti a ESP32-CAM
> scan auto

# 3. Setup WiFi
> wifi setup
SSID WiFi: MiaReteWiFi
Password WiFi: miapassword

# 4. Attendi connessione (automatico)
✓ CONNESSO!
📹 Web Server: http://192.168.1.100

# 5. Apri browser a http://192.168.1.100

# 6. (Opzionale) Abilita motion detection
> motion enable

# 7. (Opzionale) Controlla stato
> wifi status
> cam status
> motion status

# 8. Disconnetti quando finito
> wifi disable
> quit
```

## Credits

Implementato seguendo la roadmap in `ROADMAP_CAMERA_WEB_SERVER.md`
