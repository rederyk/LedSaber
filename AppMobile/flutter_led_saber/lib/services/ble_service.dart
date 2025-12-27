import 'dart:async';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../models/ble_device_info.dart';

/// Servizio BLE principale per gestire la connessione al dispositivo
class BleService {
  // UUID del servizio LED dal firmware
  static const String ledServiceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";

  BluetoothDevice? _connectedDevice;
  List<BluetoothService>? _services;

  final StreamController<List<BleDeviceInfo>> _scanResultsController =
      StreamController<List<BleDeviceInfo>>.broadcast();
  final StreamController<BluetoothConnectionState> _connectionStateController =
      StreamController<BluetoothConnectionState>.broadcast();

  final List<BleDeviceInfo> _discoveredDevices = [];
  StreamSubscription<List<ScanResult>>? _scanSubscription;
  StreamSubscription<BluetoothConnectionState>? _connectionSubscription;

  /// Stream dei dispositivi trovati durante la scansione
  Stream<List<BleDeviceInfo>> get scanResults => _scanResultsController.stream;

  /// Stream dello stato di connessione
  Stream<BluetoothConnectionState> get connectionState =>
      _connectionStateController.stream;

  /// Dispositivo attualmente connesso
  BluetoothDevice? get connectedDevice => _connectedDevice;

  /// Servizi BLE scoperti
  List<BluetoothService>? get services => _services;

  /// Verifica se c'è un dispositivo connesso
  bool get isConnected =>
      _connectedDevice != null &&
      _connectedDevice!.isConnected;

  /// Inizializza il servizio BLE
  Future<void> initialize() async {
    // Verifica se il Bluetooth è disponibile
    if (!await FlutterBluePlus.isSupported) {
      throw Exception('Bluetooth non disponibile su questo dispositivo');
    }

    // Verifica se il Bluetooth è acceso
    final adapterState = await FlutterBluePlus.adapterState.first;
    if (adapterState != BluetoothAdapterState.on) {
      throw Exception('Bluetooth è spento. Accendilo per continuare.');
    }
  }

  /// Avvia la scansione dei dispositivi BLE
  Future<void> startScan({Duration timeout = const Duration(seconds: 10)}) async {
    try {
      _discoveredDevices.clear();
      _scanResultsController.add([]);

      // Ferma eventuali scansioni in corso
      await FlutterBluePlus.stopScan();

      // Avvia la scansione
      await FlutterBluePlus.startScan(
        timeout: timeout,
        androidUsesFineLocation: true,
      );

      // Ascolta i risultati della scansione
      _scanSubscription?.cancel();
      _scanSubscription = FlutterBluePlus.scanResults.listen((results) {
        for (final result in results) {
          final deviceInfo = BleDeviceInfo.fromDevice(
            result.device,
            rssi: result.rssi,
          );

          // Aggiungi solo se non è già nella lista
          if (!_discoveredDevices.any((d) => d.id == deviceInfo.id)) {
            _discoveredDevices.add(deviceInfo);
            _scanResultsController.add(List.from(_discoveredDevices));
          }
        }
      });
    } catch (e) {
      throw Exception('Errore durante la scansione: $e');
    }
  }

  /// Ferma la scansione
  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
    _scanSubscription?.cancel();
  }

  /// Connette a un dispositivo BLE (Multi-device safe)
  Future<void> connectToDevice(BluetoothDevice device) async {
    try {
      // Ferma la scansione se in corso
      await stopScan();

      // NON disconnettere dispositivi precedenti (gestito da MultiDeviceManager)
      // Ogni device mantiene la propria connessione

      // Connetti al dispositivo
      await device.connect(
        timeout: const Duration(seconds: 15),
        autoConnect: false, // Non usare auto-connect (incompatibile con alcune operazioni)
      );

      // Ascolta i cambiamenti di stato della connessione
      _connectionSubscription?.cancel();
      _connectionSubscription =
          device.connectionState.listen(_connectionStateController.add);

      // Scopri i servizi disponibili
      final services = await device.discoverServices();

      // Aggiorna i riferimenti legacy (mantenuti per compatibilità)
      _connectedDevice = device;
      _services = services;
    } catch (e) {
      throw Exception('Errore durante la connessione: $e');
    }
  }

  /// Disconnette dal dispositivo corrente
  Future<void> disconnect() async {
    if (_connectedDevice != null) {
      try {
        await _connectedDevice!.disconnect();
      } catch (e) {
        // Ignora errori durante la disconnessione
      }
      _connectedDevice = null;
      _services = null;
    }
  }

  /// Trova un servizio per UUID
  BluetoothService? findService(String uuid) {
    if (_services == null) return null;
    try {
      return _services!.firstWhere(
        (service) => service.uuid.toString().toLowerCase() == uuid.toLowerCase(),
      );
    } catch (_) {
      return null;
    }
  }

  /// Trova una characteristic in un servizio
  BluetoothCharacteristic? findCharacteristic(
    BluetoothService service,
    String characteristicUuid,
  ) {
    try {
      return service.characteristics.firstWhere(
        (char) => char.uuid.toString().toLowerCase() ==
            characteristicUuid.toLowerCase(),
      );
    } catch (_) {
      return null;
    }
  }

  /// Pulizia e chiusura
  void dispose() {
    _scanSubscription?.cancel();
    _connectionSubscription?.cancel();
    _scanResultsController.close();
    _connectionStateController.close();
    disconnect();
  }
}
