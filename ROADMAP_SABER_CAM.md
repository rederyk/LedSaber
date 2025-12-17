# LedSaber Motion Detection - Roadmap Implementazione

## 🎯 Obiettivo
Implementare un sistema di motion detection leggero usando la camera ESP32-CAM per controllare gli effetti LED della spada tramite gesture e movimenti.

---

## 📋 Fase 1: Preparazione Hardware e Librerie

### 1.1 Verifica Hardware
- [ ] Confermare pinout ESP32-CAM (camera OV2640)
- [ ] Verificare connessione flash LED pin 4
- [ ] Testare PSRAM disponibile (4MB)
- [ ] Documentare GPIO utilizzati vs GPIO liberi

### 1.2 Dipendenze Software
- [ ] Aggiungere libreria `esp32-camera` a platformio.ini
- [ ] Configurare build flags per PSRAM
- [ ] Verificare compatibilità con FastLED e BLE stack
- [ ] Testare heap disponibile dopo inclusione camera

**Deliverable**: platformio.ini aggiornato, test compilazione

---

## 📋 Fase 2: Implementazione Base Camera

### 2.1 Driver Camera
- [ ] Creare `CameraManager.h` / `CameraManager.cpp`
- [ ] Implementare init camera QVGA grayscale
- [ ] Configurare 2 frame buffer circolari
- [ ] Testare cattura frame singolo
- [ ] Implementare gestione errori camera

### 2.2 Frame Buffer Management
- [ ] Allocare buffer PSRAM per frame precedente/corrente
- [ ] Implementare frame capture senza blocco
- [ ] Testare frame rate effettivo (target: 30 fps)
- [ ] Monitorare consumo memoria e CPU

**Deliverable**: Camera funzionante con cattura frame stabile

---

## 📋 Fase 3: Motion Detection Core

### 3.1 Frame Differencing
- [ ] Implementare algoritmo frame differencing
- [ ] Configurare soglia movimento (threshold pixel)
- [ ] Implementare conteggio pixel cambiati
- [ ] Testare sensibilità in condizioni diverse

### 3.2 Motion Intensity Calculation
- [ ] Calcolare intensità movimento (0-255)
- [ ] Implementare media delta pixel significativi
- [ ] Testare mapping intensità vs movimento reale
- [ ] Ottimizzare performance calcolo

**Deliverable**: Motion detection funzionante con metriche precise

---

## 📋 Fase 4: Shake Detection

### 4.1 History Buffer
- [ ] Implementare buffer circolare intensità (10 frame)
- [ ] Calcolare min/max intensità recente
- [ ] Implementare rilevamento swing intensità

### 4.2 Shake Classifier
- [ ] Definire soglie shake (intensitySwing > 150)
- [ ] Implementare debouncing shake events
- [ ] Testare con movimenti reali della spada
- [ ] Calibrare sensibilità ottimale

**Deliverable**: Riconoscimento affidabile di scuotimenti

---

## 📋 Fase 5: Integrazione BLE

### 5.1 Servizio BLE Motion
- [ ] Creare `MotionDetector.h` / `MotionDetector.cpp`
- [ ] Definire UUID servizio motion
- [ ] Implementare characteristic notify per eventi
- [ ] Creare payload JSON eventi motion

### 5.2 Eventi Motion
- [ ] Evento `motion_detected` (pixels, intensity)
- [ ] Evento `motion_ended` (timeout based)
- [ ] Evento `shake_detected` (shake specifico)
- [ ] Implementare debouncing notifiche BLE

**Deliverable**: Servizio BLE motion funzionante

---

## 📋 Fase 6: Gesture Recognition (Avanzato)

### 6.1 Gesture Semplici
- [ ] **Hand Over**: rileva mano sopra camera
- [ ] **Quick Shake**: scuotimento rapido
- [ ] **Slow Wave**: movimento lento continuo
- [ ] **Still**: nessun movimento per N secondi

### 6.2 Direction Detection
- [ ] Dividere frame in zone (top/bottom/left/right)
- [ ] Calcolare movimento per zona
- [ ] Rilevare direzione predominante
- [ ] Implementare gesture direzionali

**Deliverable**: Riconoscimento gesture base

---

## 📋 Fase 7: Trigger Effetti LED

### 7.1 Mapping Motion → Effect
- [ ] Shake → attiva effetto "clash"
- [ ] Hand over → cambia colore random
- [ ] Movimento rapido → "ignition"
- [ ] Still timeout → "retraction" + off
- [ ] Wave lento → "pulse" o "breathe"

### 7.2 Configurazione Trigger
- [ ] Creare struct mapping motion-to-effect
- [ ] Implementare enable/disable motion control
- [ ] Salvare preferenze in ConfigManager
- [ ] Testare tutti i trigger

**Deliverable**: Controllo LED tramite movimento completo

---

## 📋 Fase 8: Flash LED Integration

### 8.1 Flash Control
- [ ] Implementare PWM control flash pin 4
- [ ] Configurare intensità flash (0-255)
- [ ] Implementare flash pulse per cattura frame
- [ ] Testare consumo energetico flash

### 8.2 Adaptive Lighting
- [ ] Rilevare condizioni luce ambiente
- [ ] Attivare flash solo se necessario
- [ ] Implementare auto-exposure rudimentale
- [ ] Ottimizzare durata flash (500µs)

**Deliverable**: Flash automatico per visione notturna

---

## 📋 Fase 9: Python Controller Extension

### 9.1 BLE Motion Service Client
- [ ] Estendere `ledsaber_control.py`
- [ ] Aggiungere UUID motion service
- [ ] Implementare handler notifiche motion
- [ ] Parsare JSON payload eventi

### 9.2 Comandi CLI Motion
- [ ] `motion status` - mostra stato motion detection
- [ ] `motion enable/disable` - toggle motion control
- [ ] `motion sensitivity <0-255>` - imposta sensibilità
- [ ] `motion triggers` - mostra mapping motion→effect
- [ ] `motion calibrate` - calibrazione interattiva

**Deliverable**: Controller Python con supporto motion completo

---

## 📋 Fase 10: Ottimizzazione & Testing

### 10.1 Performance Optimization
- [ ] Profilare CPU usage (target: <30% CPU)
- [ ] Ottimizzare allocazioni memoria
- [ ] Ridurre latenza motion→LED (target: <100ms)
- [ ] Testare stabilità long-running (24h+)

### 10.2 Power Management
- [ ] Implementare sleep camera quando inattiva
- [ ] Ridurre frame rate in idle
- [ ] Gestire consumo batteria ottimale
- [ ] Testare autonomia con motion attivo

### 10.3 Edge Cases & Robustness
- [ ] Gestire camera init failure
- [ ] Recovery automatico da errori camera
- [ ] Testare con luce variabile
- [ ] Testare con movimenti estremi

**Deliverable**: Sistema stabile e ottimizzato

---

## 📋 Fase 11: Documentazione

### 11.1 Documentazione Tecnica
- [ ] Documentare API MotionDetector
- [ ] Documentare algoritmi motion detection
- [ ] Creare schema pin assignment
- [ ] Documentare configurazione ottimale

### 11.2 User Guide
- [ ] Guida setup hardware
- [ ] Guida calibrazione sensibilità
- [ ] Esempi gesture supportate
- [ ] Troubleshooting comune

**Deliverable**: Documentazione completa

---

## 🎯 Funzionalità Future (Post-MVP)

### Advanced Features
- [ ] **Machine Learning**: TinyML per gesture recognition avanzate
- [ ] **Face Detection**: riconoscimento volto per auto-on
- [ ] **Color Tracking**: seguire oggetto colorato con LED
- [ ] **Audio Sync**: combinare motion + microfono per effetti sincronizzati
- [ ] **Multi-Saber**: sincronizzazione movimento tra più spade

### Integration Ideas
- [ ] **Accelerometro/Giroscopio**: complementare camera con IMU
- [ ] **WiFi Streaming**: stream camera per debug/demo
- [ ] **Mobile App**: app dedicata iOS/Android
- [ ] **Game Mode**: integrazione con giochi/VR

---

## 📊 Metriche di Successo

### Performance Targets
- **Latency**: Motion detection → LED effect < 100ms
- **CPU Usage**: < 30% durante motion detection attivo
- **Memory**: Heap free > 100KB durante operazione
- **Frame Rate**: 30 fps QVGA grayscale consistente
- **Accuracy**: >95% detection rate per shake intenzionale
- **Battery Life**: > 2 ore uso continuo con motion attivo

### User Experience
- **Setup Time**: < 5 minuti prima utilizzo
- **Calibration**: < 1 minuto per calibrazione ottimale
- **Responsiveness**: Risposta LED immediata (<100ms percepito)
- **Reliability**: < 1 false positive ogni 10 minuti

---

## 🚀 Quick Start Implementation Order

**Sprint 1 (Settimana 1-2)**: Fasi 1-3
- Setup librerie + driver camera + motion detection base

**Sprint 2 (Settimana 3-4)**: Fasi 4-5
- Shake detection + integrazione BLE

**Sprint 3 (Settimana 5-6)**: Fasi 6-7
- Gesture recognition + trigger effetti LED

**Sprint 4 (Settimana 7-8)**: Fasi 8-10
- Flash control + Python extension + ottimizzazione

**Sprint 5 (Settimana 9)**: Fase 11
- Documentazione e polish finale

---

## 📝 Note Implementazione

### Constraints Tecnici
- **RAM limitata**: Usare PSRAM per frame buffer
- **CPU condivisa**: BLE + FastLED + Camera competono per cicli
- **Alimentazione**: Flash LED + 144 LED consumano significativamente
- **Latenza BLE**: MTU 251 bytes, connection interval 7.5-15ms

### Best Practices
- Sempre liberare frame buffer con `esp_camera_fb_return()`
- Non chiamare `FastLED.show()` durante OTA (già implementato)
- Usare `ps_malloc()` per allocazioni grandi (>10KB)
- Implementare watchdog per recovery automatico
- Loggare metriche performance per debug

### Safety Considerations
- Limitare brightness LED a MAX_SAFE_BRIGHTNESS (60/255)
- Non mantenere flash LED acceso continuamente (surriscaldamento)
- Verificare voltage drop durante flash + LED strip accesi
- Implementare thermal monitoring se possibile

---

## 🔗 Risorse

### Hardware
- [ESP32-CAM Pinout Reference](https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/)
- [OV2640 Datasheet](http://www.ovt.com/products/sensor.php?id=56)

### Software
- [esp32-camera GitHub](https://github.com/espressif/esp32-camera)
- [ESP-IDF Camera Examples](https://github.com/espressif/esp-idf/tree/master/examples/camera)
- [FastLED Documentation](https://github.com/FastLED/FastLED/wiki)

### Community
- [PlatformIO ESP32 Forum](https://community.platformio.org/c/platformio-ecosystem/esp32/)
- [ESP32 Reddit](https://reddit.com/r/esp32)

---

**Versione**: 1.0
**Ultima modifica**: 2025-12-17
**Autore**: LedSaber Team
**Status**: Planning Phase
