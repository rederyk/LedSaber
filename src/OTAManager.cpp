#include "OTAManager.h"
#include <cstring>

// ============================================================================
// CALLBACK CLASSES
// ============================================================================

class OTADataCallbacks : public BLECharacteristicCallbacks {
    OTAManager* manager;
public:
    explicit OTADataCallbacks(OTAManager* mgr) : manager(mgr) {}

    void onWrite(BLECharacteristic* pChar) override {
        std::string value = pChar->getValue();
        if (value.length() > 0) {
            // Non scrivere in flash nel callback BLE (blocca il task BLE e crolla la velocità)
            manager->enqueueDataChunk((const uint8_t*)value.data(), value.length());
        }
    }
};

class OTAControlCallbacks : public BLECharacteristicCallbacks {
    OTAManager* manager;
public:
    explicit OTAControlCallbacks(OTAManager* mgr) : manager(mgr) {}

    void onWrite(BLECharacteristic* pChar) override {
        std::string value = pChar->getValue();
        if (value.length() < 1) return;

        uint8_t command = value[0];

        switch ((OTACommand)command) {
            case OTACommand::START: {
                if (value.length() >= 5) {
                    // Primi 4 byte dopo il comando: dimensione firmware (little-endian)
                    uint32_t firmwareSize =
                        ((uint32_t)value[1]) |
                        ((uint32_t)value[2] << 8) |
                        ((uint32_t)value[3] << 16) |
                        ((uint32_t)value[4] << 24);
                    // IMPORTANTE: Schedula solo, non eseguire nel callback BLE!
                    // esp_ota_begin() blocca troppo a lungo e causa disconnessione
                    manager->scheduleStartCommand(firmwareSize);
                }
                break;
            }
            case OTACommand::ABORT:
                manager->scheduleAbortCommand();
                break;
            case OTACommand::VERIFY:
                manager->handleVerifyCommand();
                break;
            case OTACommand::REBOOT:
                manager->handleRebootCommand();
                break;
            default:
                Serial.printf("[OTA ERROR] Unknown command: 0x%02X\n", command);
                break;
        }
    }
};

// ============================================================================
// COSTRUTTORE E INIZIALIZZAZIONE
// ============================================================================

OTAManager::OTAManager() {
    pServer = nullptr;
    pCharOTAData = nullptr;
    pCharOTAStatus = nullptr;
    pCharOTAControl = nullptr;
    pCharOTAProgress = nullptr;
    pCharFWVersion = nullptr;
    otaHandle = 0;
    updatePartition = nullptr;
    preOtaCallback = nullptr;
    postOtaCallback = nullptr;
}

void OTAManager::begin(BLEServer* server) {
    pServer = server;

    // Imposta MTU più grande possibile per ESP32 (512 bytes + overhead)
    // Questo migliora la velocità di trasferimento OTA
    BLEDevice::setMTU(517);  // MTU = 512 (payload) + 3 (header) + 2 (safety margin)
    Serial.println("[OTA] MTU set to 517 bytes");

    // Crea servizio OTA separato dal servizio LED
    BLEService* pService = pServer->createService(BLEUUID(OTA_SERVICE_UUID), 30);

    // Characteristic 1: FW_VERSION (READ)
    pCharFWVersion = pService->createCharacteristic(
        CHAR_FW_VERSION_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    pCharFWVersion->setValue(FIRMWARE_VERSION);
    BLEDescriptor* descFwVersion = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descFwVersion->setValue("Firmware Version");
    pCharFWVersion->addDescriptor(descFwVersion);

    // Characteristic 2: OTA_STATUS (READ + NOTIFY)
    pCharOTAStatus = pService->createCharacteristic(
        CHAR_OTA_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharOTAStatus->addDescriptor(new BLE2902());
    BLEDescriptor* descStatus = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descStatus->setValue("OTA Status");
    pCharOTAStatus->addDescriptor(descStatus);

    // Characteristic 3: OTA_PROGRESS (READ + NOTIFY)
    pCharOTAProgress = pService->createCharacteristic(
        CHAR_OTA_PROGRESS_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharOTAProgress->addDescriptor(new BLE2902());
    BLEDescriptor* descProgress = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descProgress->setValue("OTA Progress");
    pCharOTAProgress->addDescriptor(descProgress);

    // Characteristic 4: OTA_CONTROL (WRITE)
    pCharOTAControl = pService->createCharacteristic(
        CHAR_OTA_CONTROL_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharOTAControl->setCallbacks(new OTAControlCallbacks(this));
    BLEDescriptor* descControl = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descControl->setValue("OTA Control");
    pCharOTAControl->addDescriptor(descControl);

    // Characteristic 5: OTA_DATA (WRITE)
    pCharOTAData = pService->createCharacteristic(
        CHAR_OTA_DATA_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR // Aggiungi Write Without Response
    );
    pCharOTAData->setCallbacks(new OTADataCallbacks(this));
    BLEDescriptor* descData = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
    descData->setValue("OTA Data");
    pCharOTAData->addDescriptor(descData);

    // Avvia servizio OTA
    pService->start();

    // Ottimizzazione velocità OTA: Richiedi un connection interval più breve
    // Valori in multipli di 1.25ms. 7.5ms min, 15ms max.
    // Questo è il fattore più importante per la velocità di trasferimento.
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // per iOS: min conn interval: 7.5ms (6 * 1.25ms)
    pAdvertising->setMaxPreferred(0x0C); // per iOS: max conn interval: 15ms (12 * 1.25ms)
    // Non è necessario chiamare pAdvertising->start() qui, verrà gestito dal server BLE.

    Serial.println("[OTA OK] OTA Manager initialized");
    Serial.printf("[OTA] Firmware version: %s\n", FIRMWARE_VERSION);

    // Coda RX per chunk OTA (processati nel loop principale)
    if (!rxQueue) {
        rxQueue = xQueueCreate(OTA_RX_QUEUE_DEPTH, sizeof(OTAQueuedChunk));
        if (!rxQueue) {
            Serial.println("[OTA ERROR] Failed to create RX queue");
        }
    }

    // Verifica partizioni disponibili
    const esp_partition_t* runningPartition = esp_ota_get_running_partition();
    updatePartition = esp_ota_get_next_update_partition(nullptr);

    Serial.printf("[OTA] Running partition: %s (offset=0x%X, size=%u bytes)\n",
        runningPartition->label, runningPartition->address, runningPartition->size);

    if (updatePartition) {
        Serial.printf("[OTA] Update partition: %s (offset=0x%X, size=%u bytes)\n",
            updatePartition->label, updatePartition->address, updatePartition->size);
    } else {
        Serial.println("[OTA ERROR] No update partition available!");
    }

    // Notifica stato iniziale
    notifyStatus();
    notifyProgress();
}

// ============================================================================
// GESTIONE STATI
// ============================================================================

void OTAManager::setState(OTAState newState) {
    if (otaStatus.state != newState) {
        otaStatus.state = newState;

        const char* stateNames[] = {
            "IDLE", "WAITING", "RECEIVING", "VERIFYING", "READY", "ERROR", "RECOVERY"
        };
        Serial.printf("[OTA] State changed to: %s\n", stateNames[(int)newState]);

        notifyStatus();
    }
}

void OTAManager::setError(const String& errorMsg) {
    otaStatus.errorMessage = errorMsg;
    setState(OTAState::ERROR);
    Serial.printf("[OTA ERROR] %s\n", errorMsg.c_str());
}

void OTAManager::notifyStatus() {
    if (!pCharOTAStatus) return;

    // Formato: "STATE:ERROR_MSG"
    String statusStr = String((int)otaStatus.state);
    if (otaStatus.errorMessage.length() > 0) {
        statusStr += ":" + otaStatus.errorMessage;
    }

    pCharOTAStatus->setValue(statusStr.c_str());
    pCharOTAStatus->notify();
}

void OTAManager::notifyProgress() {
    if (!pCharOTAProgress) return;

    // Formato: "PERCENT:RECEIVED:TOTAL" (es. "45:230400:512000")
    char progressStr[64];
    snprintf(progressStr, sizeof(progressStr), "%u:%u:%u",
        otaStatus.progressPercent,
        otaStatus.receivedBytes,
        otaStatus.totalBytes);

    pCharOTAProgress->setValue(progressStr);
    pCharOTAProgress->notify();
}

void OTAManager::resetOTAState() {
    otaStatus.totalBytes = 0;
    otaStatus.receivedBytes = 0;
    otaStatus.crc32 = 0;
    otaStatus.progressPercent = 0;
    otaStatus.lastChunkTime = 0;
    otaStatus.startTime = 0;
    otaStatus.errorMessage = "";

    if (otaHandle) {
        esp_ota_end(otaHandle);
        otaHandle = 0;
    }

    if (rxQueue) {
        xQueueReset(rxQueue);
    }
    rxQueueError = 0;
}

bool OTAManager::enqueueDataChunk(const uint8_t* data, size_t length) {
    if (!rxQueue) {
        return false;
    }

    if (length == 0) {
        return true;
    }

    if (length > OTA_CHUNK_SIZE) {
        if (rxQueueError == 0) rxQueueError = 2;
        return false;
    }

    OTAQueuedChunk chunk;
    chunk.len = static_cast<uint16_t>(length);
    memcpy(chunk.data, data, length);

    if (xQueueSend(rxQueue, &chunk, 0) != pdTRUE) {
        if (rxQueueError == 0) rxQueueError = 1;
        return false;
    }

    return true;
}

void OTAManager::processRxQueue() {
    if (!rxQueue) return;

    if (rxQueueError != 0 && otaStatus.state != OTAState::ERROR) {
        if (rxQueueError == 1) {
            setError("BLE RX queue full");
        } else if (rxQueueError == 2) {
            setError("OTA chunk oversize");
        } else {
            setError("BLE RX queue error");
        }
        executeAbortCommand();
        return;
    }

    OTAQueuedChunk chunk;
    uint32_t startMs = millis();
    uint16_t processed = 0;

    while (xQueueReceive(rxQueue, &chunk, 0) == pdTRUE) {
        handleDataChunk(chunk.data, chunk.len);
        processed++;

        // Se lo stato cambia (errore/abort/verifica), svuota e smetti
        if (otaStatus.state == OTAState::IDLE || otaStatus.state == OTAState::ERROR) {
            xQueueReset(rxQueue);
            break;
        }

        // Evita di bloccare troppo il loop principale
        if (processed >= 32) break;
        if (millis() - startMs >= 10) break;
    }
}

// ============================================================================
// VALIDAZIONI E TIMEOUT
// ============================================================================

bool OTAManager::validateFirmwareSize(uint32_t size) {
    if (!updatePartition) {
        return false;
    }

    if (size == 0 || size > updatePartition->size) {
        return false;
    }

    return true;
}

bool OTAManager::checkTimeout() {
    uint32_t now = millis();

    // Timeout globale (5 minuti dall'inizio)
    if (otaStatus.state != OTAState::IDLE && otaStatus.state != OTAState::ERROR) {
        if (now - otaStatus.startTime > OTA_GLOBAL_TIMEOUT_MS) {
            setError("Global timeout (5min)");
            executeAbortCommand();
            return true;
        }
    }

    // Timeout in attesa del primo chunk (30 secondi)
    if (otaStatus.state == OTAState::WAITING) {
        if (now - otaStatus.startTime > OTA_WAITING_TIMEOUT_MS) {
            setError("No data received (30s timeout)");
            executeAbortCommand();
            return true;
        }
    }

    // Timeout tra chunk (10 secondi di inattività)
    if (otaStatus.state == OTAState::RECEIVING) {
        if (now - otaStatus.lastChunkTime > OTA_CHUNK_TIMEOUT_MS) {
            setError("Chunk timeout (10s inactivity)");
            executeAbortCommand();
            return true;
        }
    }

    return false;
}

// ============================================================================
// CRC32 CALCULATION
// ============================================================================

uint32_t OTAManager::crc32Calculate(const uint8_t* data, size_t length) {
    // Tabella CRC32 standard (IEEE 802.3)
    static const uint32_t crc32_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

// ============================================================================
// GESTIONE COMANDI - SCHEDULING (chiamati dal callback BLE)
// ============================================================================

void OTAManager::scheduleStartCommand(uint32_t firmwareSize) {
    Serial.printf("[OTA] START command scheduled (size=%u bytes, %.2f KB)\n",
        firmwareSize, firmwareSize / 1024.0f);

    // Validazioni rapide che possiamo fare nel callback
    if (otaStatus.state != OTAState::IDLE && otaStatus.state != OTAState::ERROR) {
        setError("OTA already in progress");
        return;
    }

    if (!validateFirmwareSize(firmwareSize)) {
        setError("Invalid firmware size");
        return;
    }

    // Schedula per esecuzione nel loop principale
    // NON impostare stato WAITING qui! Lo farà executeStartCommand dopo esp_ota_begin
    pendingCmd.startPending = true;
    pendingCmd.startFirmwareSize = firmwareSize;

    Serial.println("[OTA] Command queued, waiting for main loop to execute...");
}

void OTAManager::scheduleAbortCommand() {
    Serial.println("[OTA] ABORT command scheduled");
    pendingCmd.abortPending = true;
}

// ============================================================================
// GESTIONE COMANDI - ESECUZIONE (chiamati da update() nel loop principale)
// ============================================================================

void OTAManager::processPendingCommands() {
    // Processa START (operazione pesante - esp_ota_begin erasa flash)
    if (pendingCmd.startPending) {
        Serial.println("[OTA] Processing pending START command...");
        pendingCmd.startPending = false;
        executeStartCommand(pendingCmd.startFirmwareSize);
    }

    // Processa ABORT
    if (pendingCmd.abortPending) {
        Serial.println("[OTA] Processing pending ABORT command...");
        pendingCmd.abortPending = false;
        executeAbortCommand();
    }
}

void OTAManager::executeStartCommand(uint32_t firmwareSize) {
    Serial.printf("[OTA] Executing START (size=%u bytes)\n", firmwareSize);

    if (preOtaCallback) {
        Serial.println("[OTA] Executing pre-OTA callback...");
        preOtaCallback();
    }

    // Verifica heap disponibile (minimo 50KB per sicurezza)
    uint32_t freeHeap = ESP.getFreeHeap();
    Serial.printf("[OTA] Free heap: %u bytes\n", freeHeap);
    if (freeHeap < 50000) {
        setError("Insufficient memory");
        if (postOtaCallback) {
            Serial.println("[OTA] Executing post-OTA (error) callback...");
            postOtaCallback();
        }
        return;
    }

    // Ottieni partizione di aggiornamento
    updatePartition = esp_ota_get_next_update_partition(nullptr);
    if (!updatePartition) {
        setError("No update partition");
        return;
    }

    Serial.println("[OTA] Calling esp_ota_begin...");

    // Usa OTA_SIZE_UNKNOWN per evitare l'erase iniziale della flash
    // L'erase avverrà incrementalmente durante la scrittura dei chunk
    // Questo evita il blocco prolungato che causa la disconnessione BLE
    esp_err_t err = esp_ota_begin(updatePartition, OTA_SIZE_UNKNOWN, &otaHandle);
    if (err != ESP_OK) {
        setError("esp_ota_begin failed: " + String(esp_err_to_name(err)));
        return;
    }
    Serial.println("[OTA] esp_ota_begin completed successfully");

    // Reset contatori e imposta stato
    otaStatus.totalBytes = firmwareSize;
    otaStatus.receivedBytes = 0;
    otaStatus.crc32 = 0;
    otaStatus.progressPercent = 0;
    otaStatus.startTime = millis();
    otaStatus.lastChunkTime = millis();
    otaStatus.errorMessage = "";

    // ORA impostiamo WAITING - dopo che esp_ota_begin è completato con successo!
    setState(OTAState::WAITING);
    notifyProgress();

    Serial.println("[OTA] Ready to receive firmware data");
}

void OTAManager::executeAbortCommand() {
    Serial.println("[OTA] Executing ABORT");

    if (otaHandle) {
        esp_ota_end(otaHandle);
        otaHandle = 0;
    }

    resetOTAState();
    setState(OTAState::IDLE);

    if (postOtaCallback) {
        Serial.println("[OTA] Executing post-OTA (abort) callback...");
        postOtaCallback();
    }
}

void OTAManager::handleVerifyCommand() {
    Serial.println("[OTA] VERIFY command received");

    if (otaStatus.state != OTAState::RECEIVING) {
        setError("Not in receiving state");
        return;
    }

    setState(OTAState::VERIFYING);

    // Finalizza scrittura OTA
    esp_err_t err = esp_ota_end(otaHandle);
    otaHandle = 0;

    if (err != ESP_OK) {
        setError("OTA verification failed: " + String(esp_err_to_name(err)));
        return;
    }

    // Verifica integrità (ESP-IDF fa già controlli SHA256)
    Serial.println("[OTA] Firmware verification successful");

    // Imposta la nuova partizione come boot partition
    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK) {
        setError("Set boot partition failed: " + String(esp_err_to_name(err)));
        return;
    }

    setState(OTAState::READY);
    Serial.printf("[OTA] Firmware ready! Total: %u bytes, CRC32: 0x%08X\n",
        otaStatus.receivedBytes, otaStatus.crc32);
    Serial.println("[OTA] Send REBOOT command to apply update");
}

void OTAManager::handleRebootCommand() {
    Serial.println("[OTA] REBOOT command received");

    if (otaStatus.state != OTAState::READY) {
        Serial.println("[OTA WARNING] Reboot requested but firmware not ready");
    }

    Serial.println("[OTA] Rebooting in 2 seconds...");
    delay(2000);
    ESP.restart();
}

// ============================================================================
// GESTIONE DATI
// ============================================================================

void OTAManager::handleDataChunk(const uint8_t* data, size_t length) {
    // Ignora dati se non in stato WAITING o RECEIVING
    if (otaStatus.state != OTAState::WAITING && otaStatus.state != OTAState::RECEIVING) {
        Serial.printf("[OTA WARNING] Data received in wrong state: %d\n", (int)otaStatus.state);
        return;
    }

    // Primo chunk ricevuto
    if (otaStatus.state == OTAState::WAITING) {
        setState(OTAState::RECEIVING);
    }

    // Verifica overflow
    if (otaStatus.receivedBytes + length > otaStatus.totalBytes) {
        setError("Data overflow");
        executeAbortCommand();
        return;
    }

    // Scrivi chunk nella partizione
    esp_err_t err = esp_ota_write(otaHandle, data, length);
    if (err != ESP_OK) {
        setError("OTA write failed: " + String(esp_err_to_name(err)));
        executeAbortCommand();
        return;
    }

    // Aggiorna stato
    otaStatus.receivedBytes += length;
    otaStatus.lastChunkTime = millis();

    // Calcola CRC32 incrementale
    uint32_t chunkCrc = crc32Calculate(data, length);
    otaStatus.crc32 ^= chunkCrc;  // XOR per combinare CRC progressivi

    // Calcola percentuale (usa uint64_t per evitare overflow e ottenere precisione)
    otaStatus.progressPercent = ((uint64_t)otaStatus.receivedBytes * 100) / otaStatus.totalBytes;

    // Log solo ogni 50KB per ridurre drasticamente l'overhead (logging rallenta molto ESP32)
    static uint32_t lastLogBytes = 0;
    if (otaStatus.receivedBytes - lastLogBytes >= 51200 ||
        otaStatus.receivedBytes == otaStatus.totalBytes) {

        uint32_t elapsed = millis() - otaStatus.startTime;
        float speed = 0.0f;
        if (elapsed > 0) {
            speed = (otaStatus.receivedBytes / 1024.0f) / (elapsed / 1000.0f);  // KB/s
        }

        Serial.printf("[OTA] Total: %u/%u (%u%%) | Speed: %.2f KB/s\n",
            otaStatus.receivedBytes, otaStatus.totalBytes,
            otaStatus.progressPercent, speed);

        lastLogBytes = otaStatus.receivedBytes;
    }

    // Notifica progresso ogni 50KB (ridotto overhead per velocizzare trasferimento)
    static uint32_t lastNotifyBytes = 0;
    if (otaStatus.receivedBytes - lastNotifyBytes >= 51200 ||
        otaStatus.receivedBytes == otaStatus.totalBytes) {
        notifyProgress();
        lastNotifyBytes = otaStatus.receivedBytes;
    }

    // Se ricezione completa, passa automaticamente a verifica
    if (otaStatus.receivedBytes >= otaStatus.totalBytes) {
        Serial.println("[OTA] All data received! Verifying...");
        handleVerifyCommand();
    }
}

// ============================================================================
// UPDATE LOOP
// ============================================================================

void OTAManager::update() {
    // Processa comandi pendenti (eseguiti fuori dal callback BLE)
    processPendingCommands();

    // Processa dati OTA ricevuti via BLE
    processRxQueue();

    // Controlla timeout
    checkTimeout();
}

// ============================================================================
// STATO PUBBLICO
// ============================================================================

bool OTAManager::isOTAInProgress() {
    return (otaStatus.state == OTAState::WAITING ||
            otaStatus.state == OTAState::RECEIVING ||
            otaStatus.state == OTAState::VERIFYING);
}
