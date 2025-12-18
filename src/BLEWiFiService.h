#ifndef BLE_WIFI_SERVICE_H
#define BLE_WIFI_SERVICE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <WiFi.h>

/**
 * @brief Servizio BLE per controllo WiFi da remoto
 *
 * Permette di:
 * - Attivare/disattivare WiFi STA mode
 * - Configurare SSID e password
 * - Ricevere notifiche stato connessione
 * - Ottenere IP address assegnato
 */
class BLEWiFiService {
public:
    BLEWiFiService();

    /**
     * @brief Inizializza servizio BLE WiFi
     * @param pServer Puntatore al BLEServer esistente
     */
    void begin(BLEServer* pServer);

    /**
     * @brief Aggiorna stato connessione WiFi (chiamare in loop)
     */
    void update();

    /**
     * @brief Ottieni stato WiFi
     */
    bool isWiFiEnabled() const { return _wifiEnabled; }
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    String getIPAddress() const;

    // Per accesso interno ai callbacks
    void _handleControlCommand(const std::string& value);

private:
    // UUIDs (generare con uuidgen)
    static constexpr const char* SERVICE_UUID = "8a7f1234-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_CONTROL_UUID = "8a7f1235-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_STATUS_UUID = "8a7f1236-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_SSID_UUID = "8a7f1237-5678-90ab-cdef-1234567890ac";
    static constexpr const char* CHAR_WIFI_PASSWORD_UUID = "8a7f1238-5678-90ab-cdef-1234567890ac";

    BLECharacteristic* _pCharControl;
    BLECharacteristic* _pCharStatus;
    BLECharacteristic* _pCharSSID;
    BLECharacteristic* _pCharPassword;

    bool _wifiEnabled;
    bool _wasConnected;
    String _ssid;
    String _password;
    unsigned long _lastStatusUpdate;

    void _notifyStatus();
    void _connectWiFi();
    void _disconnectWiFi();
};

#endif // BLE_WIFI_SERVICE_H
