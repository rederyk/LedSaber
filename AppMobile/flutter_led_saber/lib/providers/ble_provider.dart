import 'dart:async';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../models/ble_device_info.dart';
import '../services/ble_service.dart';
import '../services/led_service.dart';

/// Provider per gestire lo stato della connessione BLE
class BleProvider extends ChangeNotifier {
  final BleService _bleService = BleService();

  List<BleDeviceInfo> _discoveredDevices = [];
  BluetoothConnectionState _connectionState = BluetoothConnectionState.disconnected;
  bool _isScanning = false;
  String? _errorMessage;
  LedService? _ledService;

  StreamSubscription<List<BleDeviceInfo>>? _scanSubscription;
  StreamSubscription<BluetoothConnectionState>? _connectionSubscription;

  BleProvider() {
    _initialize();
  }

  // Getters
  List<BleDeviceInfo> get discoveredDevices => _discoveredDevices;
  BluetoothConnectionState get connectionState => _connectionState;
  bool get isScanning => _isScanning;
  bool get isConnected => _connectionState == BluetoothConnectionState.connected;
  String? get errorMessage => _errorMessage;
  LedService? get ledService => _ledService;
  BluetoothDevice? get connectedDevice => _bleService.connectedDevice;

  /// Inizializza il servizio BLE
  Future<void> _initialize() async {
    try {
      await _bleService.initialize();
      _errorMessage = null;
    } catch (e) {
      _errorMessage = e.toString();
    }
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

  /// Connette a un dispositivo
  Future<void> connectToDevice(BleDeviceInfo deviceInfo) async {
    try {
      _errorMessage = null;
      notifyListeners();

      debugPrint('[BleProvider] Connessione a ${deviceInfo.name}...');
      await _bleService.connectToDevice(deviceInfo.device);
      debugPrint('[BleProvider] Connesso a ${deviceInfo.name}');

      // Ascolta i cambiamenti di stato della connessione
      _connectionSubscription?.cancel();
      _connectionSubscription = _bleService.connectionState.listen((state) {
        debugPrint('[BleProvider] Stato connessione: $state');
        _connectionState = state;

        // Se disconnesso, pulisci il servizio LED
        if (state == BluetoothConnectionState.disconnected) {
          _ledService?.dispose();
          _ledService = null;
        }

        notifyListeners();
      });

      // Aggiorna immediatamente lo stato della connessione
      _connectionState = BluetoothConnectionState.connected;
      debugPrint('[BleProvider] Stato connessione aggiornato: $_connectionState');

      // Scopri i servizi e inizializza il LED Service
      debugPrint('[BleProvider] Scoperta servizi...');
      await _discoverServices();
      debugPrint('[BleProvider] Servizi scoperti con successo');

      notifyListeners();
    } catch (e) {
      _errorMessage = 'Errore durante la connessione: $e';
      debugPrint('[BleProvider] ERRORE: $_errorMessage');
      notifyListeners();
      rethrow;
    }
  }

  /// Scopre i servizi BLE e inizializza il LED Service
  Future<void> _discoverServices() async {
    try {
      debugPrint('[BleProvider] Ricerca servizio LED (UUID: ${BleService.ledServiceUuid})...');
      final ledServiceBle = _bleService.findService(BleService.ledServiceUuid);

      if (ledServiceBle == null) {
        // Stampa tutti i servizi disponibili per debug
        final services = _bleService.services ?? [];
        debugPrint('[BleProvider] Servizi disponibili:');
        for (final service in services) {
          debugPrint('  - ${service.uuid}');
        }
        throw Exception('Servizio LED non trovato sul dispositivo');
      }

      debugPrint('[BleProvider] Servizio LED trovato!');

      // Crea il LED Service
      _ledService = LedService(ledServiceBle);

      // Abilita le notifiche per lo stato LED
      debugPrint('[BleProvider] Abilitazione notifiche stato LED...');
      await _ledService!.enableStateNotifications();
      debugPrint('[BleProvider] Notifiche abilitate');

      notifyListeners();
    } catch (e) {
      _errorMessage = 'Errore scoprendo i servizi: $e';
      debugPrint('[BleProvider] ERRORE scoperta servizi: $e');
      notifyListeners();
      rethrow;
    }
  }

  /// Disconnette dal dispositivo corrente
  Future<void> disconnect() async {
    try {
      _ledService?.dispose();
      _ledService = null;
      await _bleService.disconnect();
      _connectionState = BluetoothConnectionState.disconnected;
      _errorMessage = null;
      notifyListeners();
    } catch (e) {
      _errorMessage = 'Errore durante la disconnessione: $e';
      notifyListeners();
    }
  }

  /// Riconnette all'ultimo dispositivo
  Future<void> reconnect() async {
    if (_bleService.connectedDevice != null) {
      final deviceInfo = BleDeviceInfo.fromDevice(_bleService.connectedDevice!);
      await connectToDevice(deviceInfo);
    }
  }

  /// Pulisce l'errore
  void clearError() {
    _errorMessage = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _scanSubscription?.cancel();
    _connectionSubscription?.cancel();
    _ledService?.dispose();
    _bleService.dispose();
    super.dispose();
  }
}
