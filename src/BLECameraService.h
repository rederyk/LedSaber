#ifndef BLE_CAMERA_SERVICE_H
#define BLE_CAMERA_SERVICE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "CameraManager.h"

// UUIDs per Camera Service
#define CAMERA_SERVICE_UUID        "5fafc301-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_CAMERA_STATUS_UUID    "6eb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_CAMERA_CONTROL_UUID   "7dc5a4c3-eb10-4a3e-8a4c-1234567890ab"
#define CHAR_CAMERA_METRICS_UUID   "8ef6b5d4-fc21-5b4f-9b5d-2345678901bc"
#define CHAR_CAMERA_FLASH_UUID     "9fe7c6e5-0d32-4c5a-ac6e-3456789012cd"

/**
 * @brief Servizio BLE per testare e controllare la camera ESP32-CAM
 *
 * Caratteristiche:
 * - STATUS (Read, Notify): Stato camera (inizializzato, frame rate, errori)
 * - CONTROL (Write): Comandi (init, capture, stop, reset_metrics)
 * - METRICS (Read): Metriche dettagliate (fps, frame count, memoria)
 * - FLASH (Read/Write): Controllo flash LED
 */
class BLECameraService {
public:
    BLECameraService(CameraManager* cameraManager);

    /**
     * @brief Inizializza servizio BLE
     * @param pServer BLE Server principale
     */
    void begin(BLEServer* pServer);

    /**
     * @brief Notifica stato camera ai client connessi
     */
    void notifyStatus();

    /**
     * @brief Aggiorna metriche camera
     */
    void updateMetrics();

    /**
     * @brief Verifica se camera è attiva per cattura continua
     */
    bool isCameraActive() const { return _cameraActive; }

private:
    CameraManager* _camera;
    BLEService* _pService;
    BLECharacteristic* _pCharStatus;
    BLECharacteristic* _pCharControl;
    BLECharacteristic* _pCharMetrics;
    BLECharacteristic* _pCharFlash;

    bool _statusNotifyEnabled;
    bool _cameraActive;

    /**
     * @brief Callback per ricezione comandi
     */
    class ControlCallbacks : public BLECharacteristicCallbacks {
    public:
        ControlCallbacks(BLECameraService* service) : _service(service) {}

        void onWrite(BLECharacteristic* pCharacteristic) override;

    private:
        BLECameraService* _service;
    };

    /**
     * @brief Callback per controllo flash
     */
    class FlashCallbacks : public BLECharacteristicCallbacks {
    public:
        FlashCallbacks(BLECameraService* service) : _service(service) {}

        void onWrite(BLECharacteristic* pCharacteristic) override;

    private:
        BLECameraService* _service;
    };

    /**
     * @brief Callback per gestione sottoscrizione notifiche
     */
    class StatusDescriptorCallbacks : public BLEDescriptorCallbacks {
    public:
        StatusDescriptorCallbacks(BLECameraService* service) : _service(service) {}

        void onWrite(BLEDescriptor* pDescriptor) override {
            uint8_t* data = pDescriptor->getValue();
            if (pDescriptor->getLength() > 0) {
                uint16_t value = data[0] | (data[1] << 8);
                _service->_statusNotifyEnabled = (value == 0x0001);
                Serial.printf("[CAM BLE] Notifications %s\n",
                    _service->_statusNotifyEnabled ? "enabled" : "disabled");
            }
        }

    private:
        BLECameraService* _service;
    };

    /**
     * @brief Esegue comando ricevuto via BLE
     */
    void _executeCommand(const String& command);

    /**
     * @brief Serializza stato camera in JSON
     */
    String _getStatusJson();

    /**
     * @brief Serializza metriche camera in JSON
     */
    String _getMetricsJson();
};

#endif // BLE_CAMERA_SERVICE_H
