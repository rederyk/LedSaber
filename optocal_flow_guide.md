# Optical Flow Guide

Questa guida descrive il funzionamento del motion detector e dei nuovi setting
per le gesture, con esempi pratici su come tararli.

## Panoramica pipeline

1) Camera VGA 640x480 in grayscale.
2) Crop centrale 480x480.
3) Subsampling 2x verso 240x240.
4) Optical flow a griglia 6x6 (block size 40).
5) Estrazione direzione dominante, velocita media e intensita (0-255).
6) MotionProcessor classifica le gesture in base a direzione e intensita.

## Regole gesture attuali

- RETRACT: direzione "down" ampia (DOWN/DOWN_LEFT/DOWN_RIGHT + LEFT/RIGHT).
- CLASH: direzione "left/right" (LEFT/RIGHT/UP_LEFT/UP_RIGHT).
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
   - Movimenti in "down ampia" -> RETRACT.
   - Movimenti in "left/right" -> CLASH.
   - Niente gesture su movimenti casuali o micro-vibrazioni.

## Esempi pratici

Setup base consigliato:

- retractmin 18
- clashmin 14

Se il clash e troppo sensibile:
- alza clashmin a 18-22

Se il retract scatta troppo facilmente:
- alza retractmin a 20-25
