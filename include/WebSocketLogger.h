#ifndef WEBSOCKET_LOGGER_H
#define WEBSOCKET_LOGGER_H

#include <Arduino.h>
#include <AsyncWebSocket.h>

// Dimensione massima del buffer circolare per i log
#define LOG_BUFFER_SIZE 100
#define MAX_LOG_LENGTH 256

class WebSocketLogger {
private:
    AsyncWebSocket* ws;

    // Buffer circolare per storico log
    String logBuffer[LOG_BUFFER_SIZE];
    int logIndex;
    int logCount;

    // Metodo helper per aggiungere al buffer
    void addToBuffer(const String& message);

public:
    WebSocketLogger(AsyncWebSocket* websocket);

    // Inizializza il WebSocket e gestisce eventi
    void begin();

    // Invia un messaggio di log a tutti i client connessi
    void log(const String& message);

    // Helper per log formattati (stile printf)
    void logf(const char* format, ...);

    // Invia lo storico dei log a un client appena connesso
    void sendHistory(AsyncWebSocketClient* client);

    // Handler per eventi WebSocket
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                            AwsEventType type, void* arg, uint8_t* data, size_t len);

    // Pulisce il buffer dei log
    void clearBuffer();
};

#endif
