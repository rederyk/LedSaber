# AGENTS

## Modifiche applicate (OpticalFlowDetector)
- Riutilizzo del buffer edge in PSRAM (`_edgeFrame`) per evitare alloc/free a ogni frame.
- Fast-path quando `frameDiffAvg` è basso e non c’è motion attiva: salta optical flow.
- Skip dei blocchi con poca texture via somma edge campionata (riduce SAD inutili).
- Finestra di ricerca adattiva attorno al vettore precedente quando `frameDiff` è basso.
- Precheck SAD campionato (1/4 dei pixel) per scartare match pessimi prima del SAD completo.
- Fix overflow/ottimizzazione indici nel SAD con pointer arithmetic.

## File toccati
- src/OpticalFlowDetector.h
- src/OpticalFlowDetector.cpp
