#ifndef OVERLAY_RENDERER_H
#define OVERLAY_RENDERER_H

#include <Arduino.h>
#include "OpticalFlowDetector.h"

/**
 * @brief Renderer per overlay optical flow su frame JPEG
 *
 * Genera JSON con dati overlay per rendering client-side:
 * - Griglia blocchi optical flow
 * - Vettori movimento (frecce)
 * - Confidence (colore)
 * - Direzione globale
 * - Statistiche (intensity, speed)
 */
class OverlayRenderer {
public:
    OverlayRenderer();

    /**
     * @brief Genera JSON con dati overlay (per rendering client-side)
     * @param detector Puntatore al motion detector
     * @return JSON string
     */
    String generateOverlayJSON(const OpticalFlowDetector* detector);

private:
    // Configurazione griglia (deve corrispondere a OpticalFlowDetector)
    static constexpr uint8_t BLOCK_SIZE = 40;
    static constexpr uint8_t GRID_COLS = 6;
    static constexpr uint8_t GRID_ROWS = 6;
    static constexpr uint16_t FRAME_WIDTH = 320;
    static constexpr uint16_t FRAME_HEIGHT = 240;
};

#endif // OVERLAY_RENDERER_H
