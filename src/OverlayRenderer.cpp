#include "OverlayRenderer.h"
#include <ArduinoJson.h>

OverlayRenderer::OverlayRenderer() {
}

String OverlayRenderer::generateOverlayJSON(const OpticalFlowDetector* detector) {
    if (!detector) {
        return "{}";
    }

    JsonDocument doc;

    // Metriche globali
    auto metrics = detector->getMetrics();
    doc["intensity"] = metrics.currentIntensity;
    doc["speed"] = metrics.avgSpeed;
    doc["direction"] = OpticalFlowDetector::directionToString(metrics.dominantDirection);
    doc["active"] = metrics.motionActive;
    doc["activeBlocks"] = metrics.avgActiveBlocks;
    doc["confidence"] = (int)(metrics.avgConfidence * 100);

    // Frame info
    doc["frameWidth"] = FRAME_WIDTH;
    doc["frameHeight"] = FRAME_HEIGHT;
    doc["blockSize"] = BLOCK_SIZE;
    doc["gridCols"] = GRID_COLS;
    doc["gridRows"] = GRID_ROWS;

    // Blocchi con vettori movimento (solo blocchi validi per ridurre JSON)
    JsonArray blocks = doc["blocks"].to<JsonArray>();

    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        for (uint8_t col = 0; col < GRID_COLS; col++) {
            int8_t dx = 0, dy = 0;
            uint8_t confidence = 0;
            bool valid = detector->getBlockVector(row, col, &dx, &dy, &confidence);

            // Solo aggiungi blocchi validi con movimento significativo
            if (valid && (dx != 0 || dy != 0 || confidence > 0)) {
                JsonObject block = blocks.add<JsonObject>();
                block["row"] = row;
                block["col"] = col;
                block["x"] = col * BLOCK_SIZE;
                block["y"] = row * BLOCK_SIZE;
                block["dx"] = dx;
                block["dy"] = dy;
                block["confidence"] = confidence;
                block["valid"] = valid;
            }
        }
    }

    String jsonStr;
    serializeJson(doc, jsonStr);
    return jsonStr;
}
