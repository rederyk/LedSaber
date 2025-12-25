#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// UUID del servizio OTA (diverso dal servizio LED)
#define OTA_SERVICE_UUID         "4fafc202-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_OTA_DATA_UUID       "beb5483f-36e1-4688-b7f5-ea07361b26a8"  // WRITE (512 bytes max)
#define CHAR_OTA_STATUS_UUID     "d1e5a4c4-eb10-4a3e-8a4c-1234567890ab"  // READ + NOTIFY
#define CHAR_OTA_CONTROL_UUID    "e2f6b5d5-fc21-5b4f-9b5d-2345678901bc"  // WRITE
#define CHAR_OTA_PROGRESS_UUID   "f3e7c6e6-0d32-4c5a-ac6e-3456789012cd"  // READ + NOTIFY
#define CHAR_FW_VERSION_UUID     "a4b8d7fa-1e43-6c7d-ad8f-456789abcdef"  // READ

// Dimensione massima chunk (allineata con MTU massimo ESP32)
#define OTA_CHUNK_SIZE 512

// Timeout (millisecondi)
#define OTA_GLOBAL_TIMEOUT_MS   (5 * 60 * 1000)  // 5 minuti
#define OTA_CHUNK_TIMEOUT_MS    (10 * 1000)      // 10 secondi tra chunk
#define OTA_WAITING_TIMEOUT_MS  (30 * 1000)      // 30 secondi per primo chunk

// Versione firmware (definita in platformio.ini)
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"
#endif

// Stati OTA
enum class OTAState {
    IDLE,        // Pronto per aggiornamento
    WAITING,     // In attesa del primo chunk
    RECEIVING,   // Ricezione in corso
    VERIFYING,   // Verifica firmware
    READY,       // Pronto al reboot
    ERROR,       // Errore durante aggiornamento
    RECOVERY     // Rollback in corso
};

// Comandi OTA (via CONTROL characteristic)
enum class OTACommand {
    START = 0x01,
    ABORT = 0x02,
    VERIFY = 0x03,
    REBOOT = 0x04
};

// Struttura stato OTA
struct OTAStatus {
    OTAState state = OTAState::IDLE;
    uint32_t totalBytes = 0;        // Dimensione totale firmware (ricevuta nel comando START)
    uint32_t receivedBytes = 0;     // Byte ricevuti finora
    uint32_t crc32 = 0;             // CRC32 calcolato
    uint8_t progressPercent = 0;    // Percentuale progresso
    uint32_t lastChunkTime = 0;     // Timestamp ultimo chunk ricevuto
    uint32_t startTime = 0;         // Timestamp inizio OTA
    String errorMessage = "";       // Messaggio di errore dettagliato
};

// Comandi pendenti (da eseguire fuori dal callback BLE)
struct OTAPendingCommand {
    bool startPending = false;
    uint32_t startFirmwareSize = 0;
    bool abortPending = false;
    bool verifyPending = false;
    bool rebootPending = false;
};

// Callback for OTA events
typedef void (*ota_event_callback_t)(void);

class OTAManager {
private:
    BLEServer* pServer;
    BLECharacteristic* pCharOTAData;
    BLECharacteristic* pCharOTAStatus;
    BLECharacteristic* pCharOTAControl;
    BLECharacteristic* pCharOTAProgress;
    BLECharacteristic* pCharFWVersion;

    OTAStatus otaStatus;
    OTAPendingCommand pendingCmd;
    esp_ota_handle_t otaHandle;
    const esp_partition_t* updatePartition;

    // Evita operazioni lente (flash write) nel callback BLE: accoda i chunk e processali nel loop principale
    static constexpr size_t OTA_RX_QUEUE_DEPTH = 64; // 64 * 512 = 32KB buffer
    struct OTAQueuedChunk {
        uint16_t len;
        uint8_t data[OTA_CHUNK_SIZE];
    };
    QueueHandle_t rxQueue = nullptr;
    volatile uint8_t rxQueueError = 0; // 0=ok, 1=full, 2=oversize

    ota_event_callback_t preOtaCallback = nullptr;
    ota_event_callback_t postOtaCallback = nullptr;

    uint32_t crc32Calculate(const uint8_t* data, size_t length);
    void setState(OTAState newState);
    void setError(const String& errorMsg);
    void notifyStatus();
    void notifyProgress();
    void resetOTAState();
    bool validateFirmwareSize(uint32_t size);
    bool checkTimeout();
    void processRxQueue();

public:
    OTAManager();
    void begin(BLEServer* server);
    bool isOTAInProgress();
    OTAState getState() { return otaStatus.state; }
    uint8_t getProgress() { return otaStatus.progressPercent; }

    // Callbacks for freeing/restoring resources
    void setPreOtaCallback(ota_event_callback_t callback) { preOtaCallback = callback; }
    void setPostOtaCallback(ota_event_callback_t callback) { postOtaCallback = callback; }

    // Handler comandi (chiamati dal callback BLE - schedula solo)
    void scheduleStartCommand(uint32_t firmwareSize);
    void scheduleAbortCommand();

    // Esecuzione reale comandi (chiamati da update() nel loop principale)
    void executeStartCommand(uint32_t firmwareSize);
    void executeAbortCommand();
    void handleVerifyCommand();
    void handleRebootCommand();
    void processPendingCommands();

    // Handler dati
    void handleDataChunk(const uint8_t* data, size_t length);
    bool enqueueDataChunk(const uint8_t* data, size_t length);

    // Update loop (chiamato da main loop)
    void update();

    // Callback classes (friend)
    friend class OTADataCallbacks;
    friend class OTAControlCallbacks;
};

#endif
