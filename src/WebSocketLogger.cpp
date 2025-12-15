#include "WebSocketLogger.h"
#include <cstdarg>

WebSocketLogger::WebSocketLogger(AsyncWebSocket* websocket) {
    ws = websocket;
    logIndex = 0;
    logCount = 0;
}

void WebSocketLogger::begin() {
    // Niente da fare qui, la gestione eventi viene fatta esternamente
}

void WebSocketLogger::addToBuffer(const String& message) {
    logBuffer[logIndex] = message;
    logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
    if (logCount < LOG_BUFFER_SIZE) {
        logCount++;
    }
}

void WebSocketLogger::log(const String& message) {
    // Scrivi su Serial per debug USB
    Serial.println(message);

    // Verifica che il WebSocket sia valido prima di usarlo
    if (!ws) {
        return;
    }

    // Aggiungi timestamp
    unsigned long ms = millis();
    String timestamped = "[" + String(ms) + "ms] " + message;

    // Aggiungi al buffer
    addToBuffer(timestamped);

    // Invia a tutti i client WebSocket connessi SOLO se ci sono client
    // e facciamo una copia locale per evitare problemi di concorrenza
    uint32_t clientCount = ws->count();
    if (clientCount > 0) {
        // Invia in modo sicuro - textAll gestisce internamente la coda
        ws->textAll(timestamped.c_str());
    }
}

void WebSocketLogger::logf(const char* format, ...) {
    char buffer[MAX_LOG_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MAX_LOG_LENGTH, format, args);
    va_end(args);

    log(String(buffer));
}

void WebSocketLogger::sendHistory(AsyncWebSocketClient* client) {
    // Verifica che il client sia valido
    if (!client || !client->canSend()) {
        return;
    }

    if (logCount == 0) {
        client->text("[System] No logs in history");
        return;
    }

    // Invia messaggio di inizio storico
    client->text("[System] === Log History ===");

    // Calcola l'indice di partenza nel buffer circolare
    int startIndex = (logCount < LOG_BUFFER_SIZE) ? 0 : logIndex;

    // Invia tutti i log nello storico in modo sicuro
    for (int i = 0; i < logCount; i++) {
        int idx = (startIndex + i) % LOG_BUFFER_SIZE;
        if (client->canSend()) {
            client->text(logBuffer[idx].c_str());
        } else {
            // Se il client non può ricevere, fermiamoci
            break;
        }
    }

    if (client->canSend()) {
        client->text("[System] === End History ===");
    }
}

void WebSocketLogger::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                          AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            // Invia lo storico al nuovo client
            sendHistory(client);
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;

        case WS_EVT_DATA:
            // Gestisci comandi dal client
            {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info && info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    // Verifica che len sia valido e ci sia spazio per il null terminator
                    if (len > 0 && len < 128) {
                        // Crea un buffer locale sicuro per evitare corruzione memoria
                        char buffer[128];
                        memcpy(buffer, data, len);
                        buffer[len] = 0; // Null terminate
                        String message = buffer;

                        if (message == "clear") {
                            clearBuffer();
                            if (ws && ws->count() > 0) {
                                ws->textAll("[System] Logs cleared");
                            }
                        } else if (message == "ping") {
                            if (client && client->canSend()) {
                                client->text("[System] pong");
                            }
                        }
                    }
                }
            }
            break;

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebSocketLogger::clearBuffer() {
    logIndex = 0;
    logCount = 0;
    Serial.println("Log buffer cleared");
}
