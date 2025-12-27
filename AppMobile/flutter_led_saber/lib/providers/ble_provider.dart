import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../models/ble_device_info.dart';
import '../models/connected_device.dart';
import '../services/ble_service.dart';
import '../services/led_service.dart';
import '../services/motion_service.dart';
import '../services/multi_device_manager.dart';

/// Provider per gestire lo stato della connessione BLE multi-device
class BleProvider extends ChangeNotifier {
  final BleService _bleService = BleService();
  final MultiDeviceManager _deviceManager = MultiDeviceManager();

  List<BleDeviceInfo> _discoveredDevices = [];
  bool _isScanning = false;
  String? _errorMessage;

  StreamSubscription<List<BleDeviceInfo>>? _scanSubscription;

  BleProvider() {
    _initialize();
  }

  // Getters
  List<BleDeviceInfo> get discoveredDevices => _discoveredDevices;
  bool get isScanning => _isScanning;
  bool get isConnected => _deviceManager.hasConnectedDevices;
  String? get errorMessage => _errorMessage;

  // Multi-device getters
  MultiDeviceManager get deviceManager => _deviceManager;
  List<ConnectedDevice> get connectedDevices => _deviceManager.connectedDevices;
  ConnectedDevice? get activeDevice => _deviceManager.activeDevice;
  LedService? get ledService => _deviceManager.activeDevice?.ledService;
  MotionService? get motionService => _deviceManager.activeDevice?.motionService;
  BluetoothDevice? get connectedDevice => _deviceManager.activeDevice?.device;
  List<BluetoothService>? get allServices => _deviceManager.activeDevice?.allServices;
  int get deviceCount => _deviceManager.deviceCount;

  /// Inizializza il servizio BLE e carica devices salvati
  Future<void> _initialize() async {
    try {
      await _bleService.initialize();

      // Inizializza il device manager e tenta auto-reconnect
      await _deviceManager.initialize();

      // Ascolta cambiamenti nel device manager
      _deviceManager.addListener(_onDeviceManagerChanged);

      _errorMessage = null;
    } catch (e) {
      _errorMessage = e.toString();
      debugPrint('[BleProvider] Errore inizializzazione: $e');
    }
    notifyListeners();
  }

  /// Callback quando il device manager cambia
  void _onDeviceManagerChanged() {
    notifyListeners();
  }

  /// Avvia la scansione dei dispositivi
  Future<void> startScan() async {
    try {
      _isScanning = true;
      _errorMessage = null;
      _discoveredDevices = [];
      notifyListeners();

      await _bleService.startScan();

      // Ascolta i risultati della scansione
      _scanSubscription?.cancel();
      _scanSubscription = _bleService.scanResults.listen((devices) {
        _discoveredDevices = devices;
        notifyListeners();
      });
    } catch (e) {
      _errorMessage = 'Errore durante la scansione: $e';
      _isScanning = false;
      notifyListeners();
    }
  }

  /// Ferma la scansione
  Future<void> stopScan() async {
    await _bleService.stopScan();
    _isScanning = false;
    _scanSubscription?.cancel();
    notifyListeners();
  }

  /// Connette a un dispositivo e lo aggiunge al manager
  Future<void> connectToDevice(BleDeviceInfo deviceInfo) async {
    try {
      _errorMessage = null;
      notifyListeners();

      debugPrint('[BleProvider] Connessione a ${deviceInfo.name}...');

      // Connetti al dispositivo
      await _bleService.connectToDevice(deviceInfo.device);
      debugPrint('[BleProvider] Connesso a ${deviceInfo.name}');

      // Aggiungi al device manager
      await _deviceManager.addDevice(deviceInfo.device);
      debugPrint('[BleProvider] Device ${deviceInfo.name} aggiunto al manager');

      notifyListeners();
    } catch (e) {
      _errorMessage = 'Errore durante la connessione: $e';
      debugPrint('[BleProvider] ERRORE: $_errorMessage');
      notifyListeners();
      rethrow;
    }
  }

  /// Disconnette da un dispositivo specifico
  Future<void> disconnectDevice(String deviceId) async {
    try {
      await _deviceManager.removeDevice(deviceId);
      _errorMessage = null;
      notifyListeners();
    } catch (e) {
      _errorMessage = 'Errore durante la disconnessione: $e';
      notifyListeners();
    }
  }

  /// Disconnette da tutti i dispositivi
  Future<void> disconnectAll() async {
    try {
      await _deviceManager.clearAll();
      _errorMessage = null;
      notifyListeners();
    } catch (e) {
      _errorMessage = 'Errore durante la disconnessione: $e';
      notifyListeners();
    }
  }

  /// Imposta il device attivo
  void setActiveDevice(String deviceId) {
    _deviceManager.setActiveDevice(deviceId);
    notifyListeners();
  }

  /// Pulisce l'errore
  void clearError() {
    _errorMessage = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _scanSubscription?.cancel();
    _deviceManager.removeListener(_onDeviceManagerChanged);
    _deviceManager.dispose();
    _bleService.dispose();
    super.dispose();
  }
}
