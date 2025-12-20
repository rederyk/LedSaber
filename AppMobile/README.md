# AppMobile (Flutter) — Note rapide

Questa cartella contiene una guida minima per creare una app mobile (Flutter) per controllare LedSaber via BLE, riusando lo stesso “contratto” GATT già usato dagli script Python (`ledsaber_control.py`, `update_saber.py`).

## 1) Prerequisiti (PC)

- Flutter SDK installato e nel `PATH`
- Android SDK + `adb` (anche senza Android Studio)
- Un telefono Android con “USB debugging” attivo

Verifica:

```bash
flutter doctor
adb devices
```

## 2) Creare il progetto Flutter

Da root repo:

```bash
mkdir -p AppMobile
cd AppMobile
flutter create ledsaber_mobile
```

Apri `AppMobile/ledsaber_mobile` in VS Code e lavora da lì.

## 3) Plugin BLE consigliato

Per Flutter, una scelta comune è:

- `flutter_blue_plus` (scan/connect/read/write/notify)

In `pubspec.yaml` (nel progetto Flutter) aggiungi:

```yaml
dependencies:
  flutter_blue_plus: ^1.0.0
```

Poi:

```bash
flutter pub get
```

## 4) “Backend BLE” dell’app: come organizzarlo

L’idea pratica è avere un layer unico (es. `lib/ble/ledsaber_ble.dart`) che:

- esegue scan filtrando per nome (`LedSaber...`) e/o UUID servizi
- connette e fa discovery servizi/characteristic
- abilita le notify su:
  - `CHAR_LED_STATE_UUID` (stato LED in JSON)
  - `CHAR_CAMERA_STATUS_UUID` (status camera in JSON, opzionale)
  - `CHAR_MOTION_STATUS_UUID` e `CHAR_MOTION_EVENTS_UUID` (JSON, opzionale)
  - `CHAR_OTA_STATUS_UUID` e `CHAR_OTA_PROGRESS_UUID` (stringhe, se fai OTA)
- espone metodi “alti” che corrispondono ai comandi del firmware:
  - `setColor(r,g,b)` → write JSON su `CHAR_LED_COLOR_UUID`
  - `setEffect(mode, speed, chronoThemes...)` → write JSON su `CHAR_LED_EFFECT_UUID`
  - `setBrightness(brightness, enabled)` → write JSON su `CHAR_LED_BRIGHTNESS_UUID`
  - `setStatusLed(enabled, brightness)` → write JSON su `CHAR_STATUS_LED_UUID`
  - `setFoldPoint(foldPoint)` → write JSON su `CHAR_FOLD_POINT_UUID`
  - `syncTime(epoch)` → write JSON su `CHAR_TIME_SYNC_UUID`
  - camera: write stringhe su `CHAR_CAMERA_CONTROL_UUID` (es. `init/start/stop`)
  - motion: write stringhe su `CHAR_MOTION_CONTROL_UUID` (es. `enable/disable/reset`)

Il layer UI (schermate) dovrebbe parlare solo con questi metodi, senza conoscere dettagli GATT.

## 5) UUID GATT (dal firmware)

Servizio LED:

- Service: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- State (READ+NOTIFY): `beb5483e-36e1-4688-b7f5-ea07361b26a8` (JSON)
- Color (WRITE): `d1e5a4c3-eb10-4a3e-8a4c-1234567890ab` (JSON)
- Effect (WRITE): `e2f6b5d4-fc21-5b4f-9b5d-2345678901bc` (JSON)
- Brightness (WRITE): `f3e7c6e5-0d32-4c5a-ac6e-3456789012cd` (JSON)
- Status LED pin4 (READ+WRITE): `a4b8d7f9-1e43-6c7d-ad8f-456789abcdef` (JSON)
- Fold point (READ+WRITE): `a5b0f9a7-3c65-8e9f-cf0c-6789abcdef01` (JSON)
- Time sync (WRITE): `d6e1a0b8-4a76-9f0c-dc1a-789abcdef012` (JSON)

Servizio OTA:

- Service: `4fafc202-1fb5-459e-8fcc-c5c9c331914b`
- Control (WRITE): `e2f6b5d5-fc21-5b4f-9b5d-2345678901bc` (binario)
- Data (WRITE): `beb5483f-36e1-4688-b7f5-ea07361b26a8` (raw chunk)
- Status (READ+NOTIFY): `d1e5a4c4-eb10-4a3e-8a4c-1234567890ab` (`STATE:ERR`)
- Progress (READ+NOTIFY): `f3e7c6e6-0d32-4c5a-ac6e-3456789012cd` (`PERCENT:RECEIVED:TOTAL`)
- FW version (READ): `a4b8d7fa-1e43-6c7d-ad8f-456789abcdef`

## 6) OTA in Flutter (nota pratica)

L’OTA usa un protocollo “a chunk” (vedi `update_saber.py`):

1. write CONTROL: `0x01 + <firmware_size uint32 little-endian>`
2. invia tanti chunk raw su DATA
3. ascolta status/progress via notify

Su mobile è importante:

- richiedere MTU alto (es. 517) e connection priority alta
- gestire rate limiting (se invii troppo veloce, Android può saturare la coda GATT)

Se vuoi OTA “molto” affidabile/veloce, spesso conviene una app Kotlin nativa; Flutter può andare bene ma richiede più tuning.

