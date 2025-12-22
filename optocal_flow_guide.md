# Optical Flow Guide

Questa guida descrive il funzionamento del motion detector e dei nuovi setting
per le gesture, con esempi pratici su come tararli.

## Panoramica pipeline

1) Camera VGA 640x480 in grayscale.
2) Crop centrale 480x480.
3) Subsampling 2x verso 240x240.
4) Optical flow a griglia 6x6 (block size 40).
5) Estrazione direzione dominante, velocita media e intensita (0-255).
6) MotionProcessor classifica le gesture usando un angolo medio in gradi
   calcolato dai vettori per-blocco (pesi normalizzati per FPS), poi applica
   soglie di intensita.

## Regole gesture attuali

- RETRACT: settore "down" in gradi (centrato a 90°).
- CLASH: settori "left/right" in gradi (centrati a 180° e 0°).
- IGNITION: gestito lato LED quando la lama e spenta (auto-ignition su direzione != none).

Nota: la durata non e usata per le gesture (filtriamo solo per direzione
e intensita, con cooldown).

## Setting rilevanti

### Soglie motion (detector)

- motionIntensityMin: intensita minima per considerare motion attivo.
- motionSpeedMin: velocita minima (px/frame) per motion attivo.

Queste soglie agiscono sul detector, non sulle gesture.

### Soglie gesture (intensita)

- gestureRetractIntensity: intensita minima per RETRACT.
- gestureClashIntensity: intensita minima per CLASH.
- gestureIgnitionIntensity: presente ma non usata per l'auto-ignition attuale.

## Comandi BLE / CLI

Per le intensita (0..255):

- motion retractmin <0-255>
- motion clashmin <0-255>

## Strategia di taratura

1) Imposta prima il detector:
   - Alza motionIntensityMin se hai false attivazioni.
   - Alza motionSpeedMin se il motion parte con micro-movimenti.

2) Taratura gesture:
   - Aumenta gestureRetractIntensity se il retract scatta troppo facile.
   - Aumenta gestureClashIntensity se il clash scatta troppo facile.
3) Verifica:
   - Movimenti nel settore "down" -> RETRACT.
   - Movimenti nei settori "left/right" -> CLASH.
   - Niente gesture su movimenti casuali o micro-vibrazioni.

## Esempi pratici

Setup base consigliato:

- retractmin 18
- clashmin 14

Se il clash e troppo sensibile:
- alza clashmin a 18-22

Se il retract scatta troppo facilmente:
- alza retractmin a 20-25

## Setting avanzati optical flow (finestra e algoritmo)

Questi parametri non sono esposti via BLE: vanno cambiati in codice
(`src/OpticalFlowDetector.cpp` e `src/OpticalFlowDetector.h`).

### Finestra/griglia

- BLOCK_SIZE: dimensione del blocco (default 40). Piu piccolo = piu dettaglio, piu CPU.
- GRID_ROWS/GRID_COLS: griglia 6x6 fissa a 240x240.

### Search window (SAD)

- _searchRange: raggio ricerca in pixel (default 10). Piu alto = cattura movimenti piu grandi, ma piu lento.
- _searchStep: passo campionamento (default 5). Piu basso = piu preciso, piu lento.

### Edge threshold

- computeEdgeImage threshold: soglia gradiente (default 50).
  Alzala per ridurre rumore, abbassala per aumentare sensibilita ai bordi deboli.

### Altre soglie interne

- _directionMagnitudeThreshold (default 2.0): sotto questa magnitudine la direzione e NONE.
- _minCentroidWeight (default 100.0): peso minimo per considerare il centroide valido.

### Quality (via BLE)

- quality mappa _minConfidence e _minActiveBlocks (vedi setQuality).
  Quality piu alta = piu permissivo, piu rumoroso.

### Motion thresholds (via BLE)

- motionIntensityMin e motionSpeedMin: soglie globali per motion active.
