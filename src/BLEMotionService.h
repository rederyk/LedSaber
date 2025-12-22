#ifndef BLE_MOTION_SERVICE_H
#define BLE_MOTION_SERVICE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "OpticalFlowDetector.h"
#include "MotionProcessor.h"

// UUIDs per Motion Service
#define MOTION_SERVICE_UUID        "6fafc401-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_MOTION_STATUS_UUID    "7eb5583e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_MOTION_CONTROL_UUID   "8dc5b4c3-eb10-4a3e-8a4c-1234567890ac"
#define CHAR_MOTION_EVENTS_UUID    "9ef6c5d4-fc21-5b4f-9b5d-2345678901bd"
#define CHAR_MOTION_CONFIG_UUID    "aff7d6e5-0d32-4c5a-ac6e-3456789012ce"

/**
 * @brief Servizio BLE per motion detection e gesture recognition
 *
 * Caratteristiche:
 * - STATUS (Read, Notify): Stato motion detector (intensità, movimento)
 * - CONTROL (Write): Comandi (enable, disable, reset, calibrate)
 * - EVENTS (Notify): Eventi motion (shake_detected, motion_started, motion_ended)
 * - CONFIG (Read/Write): Configurazione (quality, motionIntensityMin, motionSpeedMin)
 */
class BLEMotionService {
public:
    BLEMotionService(OpticalFlowDetector* motionDetector, MotionProcessor* motionProcessor = nullptr);

    /**
     * @brief Inizializza servizio BLE
     * @param pServer BLE Server principale
     */
    void begin(BLEServer* pServer);

    /**
     * @brief Notifica stato motion ai client connessi
     */
    void notifyStatus();

    /**
     * @brief Notifica evento motion ai client connessi
     * @param eventType Tipo evento (shake_detected, motion_started, motion_ended, still_detected)
     */
    void notifyEvent(const String& eventType, bool includeGesture = false);

    /**
     * @brief Verifica se motion detection è abilitato
     */
    bool isMotionEnabled() const { return _motionEnabled; }

    /**
     * @brief Abilita/disabilita motion detection
     */
    void setMotionEnabled(bool enabled) { _motionEnabled = enabled; }

    /**
     * @brief Aggiorna stato e rileva eventi
     * @param motionDetected Movimento rilevato nel frame corrente
     * @param shakeDetected Shake rilevato
     * @param processed MotionProcessor output (gesture, ecc.) opzionale
     */
    void update(bool motionDetected, bool shakeDetected, const MotionProcessor::ProcessedMotion* processed = nullptr);

private:
    OpticalFlowDetector* _motion;
    MotionProcessor* _processor;
    BLEService* _pService;
    BLECharacteristic* _pCharStatus;
    BLECharacteristic* _pCharControl;
    BLECharacteristic* _pCharEvents;
    BLECharacteristic* _pCharConfig;

    bool _statusNotifyEnabled;
    bool _eventsNotifyEnabled;
    bool _motionEnabled;

    // Event tracking
    bool _wasMotionActive;
    bool _wasShakeDetected;
    unsigned long _lastEventTime;
    MotionProcessor::GestureType _lastGesture;
    uint8_t _lastGestureConfidence;
    unsigned long _lastGestureTime;

    // Motion event hysteresis (reduce chatter)
    unsigned long _motionCandidateSince;
    unsigned long _stillCandidateSince;

    /**
     * @brief Callback per ricezione comandi
     */
    class ControlCallbacks : public BLECharacteristicCallbacks {
    public:
        ControlCallbacks(BLEMotionService* service) : _service(service) {}

        void onWrite(BLECharacteristic* pCharacteristic) override;

    private:
        BLEMotionService* _service;
    };

    /**
     * @brief Callback per configurazione
     */
    class ConfigCallbacks : public BLECharacteristicCallbacks {
    public:
        ConfigCallbacks(BLEMotionService* service) : _service(service) {}

        void onWrite(BLECharacteristic* pCharacteristic) override;

    private:
        BLEMotionService* _service;
    };

    /**
     * @brief Callback per gestione sottoscrizione notifiche STATUS
     */
    class StatusDescriptorCallbacks : public BLEDescriptorCallbacks {
    public:
        StatusDescriptorCallbacks(BLEMotionService* service) : _service(service) {}

        void onWrite(BLEDescriptor* pDescriptor) override {
            uint8_t* data = pDescriptor->getValue();
            if (pDescriptor->getLength() > 0) {
                uint16_t value = data[0] | (data[1] << 8);
                _service->_statusNotifyEnabled = (value == 0x0001);
                Serial.printf("[MOTION BLE] Status notifications %s\n",
                    _service->_statusNotifyEnabled ? "enabled" : "disabled");
            }
        }

    private:
        BLEMotionService* _service;
    };

    /**
     * @brief Callback per gestione sottoscrizione notifiche EVENTS
     */
    class EventsDescriptorCallbacks : public BLEDescriptorCallbacks {
    public:
        EventsDescriptorCallbacks(BLEMotionService* service) : _service(service) {}

        void onWrite(BLEDescriptor* pDescriptor) override {
            uint8_t* data = pDescriptor->getValue();
            if (pDescriptor->getLength() > 0) {
                uint16_t value = data[0] | (data[1] << 8);
                _service->_eventsNotifyEnabled = (value == 0x0001);
                Serial.printf("[MOTION BLE] Event notifications %s\n",
                    _service->_eventsNotifyEnabled ? "enabled" : "disabled");
            }
        }

    private:
        BLEMotionService* _service;
    };

    /**
     * @brief Esegue comando ricevuto via BLE
     */
    void _executeCommand(const String& command);

    /**
     * @brief Serializza stato motion in JSON
     */
    String _getStatusJson();

    /**
     * @brief Serializza configurazione in JSON
     */
    String _getConfigJson();
};

#endif // BLE_MOTION_SERVICE_H
