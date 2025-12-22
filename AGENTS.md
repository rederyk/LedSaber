# AGENTS

## Modifiche applicate (OpticalFlowDetector)
- Riutilizzo del buffer edge in PSRAM (`_edgeFrame`) per evitare alloc/free a ogni frame.

## File toccati
- src/OpticalFlowDetector.h
- src/OpticalFlowDetector.cpp
