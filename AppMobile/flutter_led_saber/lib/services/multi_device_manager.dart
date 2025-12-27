import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/connected_device.dart';
import '../services/ble_service.dart';
import '../services/led_service.dart';

/// Gestisce connessioni multiple a dispositivi LED Saber
class MultiDeviceManager extends ChangeNotifier {
  static const String _prefsKey = 'connected_devices';
  static const String _activeDeviceKey = 'active_device_id';

  final Map<String, ConnectedDevice> _connectedDevices = {};
  String? _activeDeviceId;
  final Map<String, StreamSubscription<BluetoothConnectionState>>
      _connectionSubscriptions = {};

  // Getters
  List<ConnectedDevice> get connectedDevices => _connectedDevices.values.toList()
    ..sort((a, b) => b.connectedAt.compareTo(a.connectedAt));

  ConnectedDevice? get activeDevice =>
      _activeDeviceId != null ? _connectedDevices[_activeDeviceId] : null;

  int get deviceCount => _connectedDevices.length;

  bool get hasConnectedDevices => _connectedDevices.isNotEmpty;

  /// Inizializza e ricarica devices salvati
  Future<void> initialize() async {
    await _loadSavedDevices();
  }

  /// Carica i device salvati da SharedPreferences e tenta reconnect
  Future<void> _loadSavedDevices() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final devicesJson = prefs.getString(_prefsKey);
      final activeDeviceId = prefs.getString(_activeDeviceKey);

      if (devicesJson != null) {
        final List<dynamic> devicesList = jsonDecode(devicesJson);
        debugPrint(
            '[MultiDeviceManager] Trovati ${devicesList.length} device salvati');

        // Tenta reconnect per ogni device salvato
        for (final deviceData in devicesList) {
          try {
            await _attemptReconnect(deviceData as Map<String, dynamic>);
          } catch (e) {
            debugPrint(
                '[MultiDeviceManager] Errore reconnect ${deviceData['id']}: $e');
          }
        }

        // Ripristina active device
        if (activeDeviceId != null && _connectedDevices.containsKey(activeDeviceId)) {
          _activeDeviceId = activeDeviceId;
        } else if (_connectedDevices.isNotEmpty) {
          // Se active device non trovato, usa il primo disponibile
          _activeDeviceId = _connectedDevices.keys.first;
        }
      }

      notifyListeners();
    } catch (e) {
      debugPrint('[MultiDeviceManager] Errore caricando devices: $e');
    }
  }

  /// Tenta di riconnettersi a un device salvato
  Future<void> _attemptReconnect(Map<String, dynamic> deviceData) async {
    final deviceId = deviceData['id'] as String;
    debugPrint('[MultiDeviceManager] Tentativo reconnect a $deviceId...');

    try {
      // Cerca il device tramite remoteId
      final device = BluetoothDevice.fromId(deviceId);

      // Verifica se è già connesso
      final connectionState = await device.connectionState.first;
      if (connectionState == BluetoothConnectionState.connected) {
        debugPrint('[MultiDeviceManager] Device $deviceId già connesso');
        await _addConnectedDevice(device);
        return;
      }

      // Tenta connessione
      await device.connect(timeout: const Duration(seconds: 10));
      debugPrint('[MultiDeviceManager] Reconnect riuscito a $deviceId');

      await _addConnectedDevice(device);
    } catch (e) {
      debugPrint('[MultiDeviceManager] Reconnect fallito per $deviceId: $e');
      // Non rilanciare l'errore, continua con gli altri devices
    }
  }

  /// Aggiunge un device connesso al manager
  Future<void> addDevice(BluetoothDevice device) async {
    final deviceId = device.remoteId.toString();

    if (_connectedDevices.containsKey(deviceId)) {
      debugPrint('[MultiDeviceManager] Device $deviceId già presente');
      return;
    }

    await _addConnectedDevice(device);
    await _saveDevices();
    notifyListeners();
  }

  /// Helper interno per aggiungere device
  Future<void> _addConnectedDevice(BluetoothDevice device) async {
    final deviceId = device.remoteId.toString();

    try {
      // Scopri servizi
      debugPrint('[MultiDeviceManager] Discovery servizi per $deviceId...');
      final services = await device.discoverServices();

      // Trova LED service
      final ledServiceBle = services.firstWhere(
        (s) => s.uuid.toString().toLowerCase() == BleService.ledServiceUuid.toLowerCase(),
        orElse: () => throw Exception('LED Service non trovato'),
      );

      // Crea LED Service
      final ledService = LedService(ledServiceBle);
      await ledService.enableStateNotifications();

      // Crea ConnectedDevice
      final connectedDevice = ConnectedDevice(
        id: deviceId,
        name: device.platformName.isNotEmpty ? device.platformName : 'LedSaber',
        device: device,
        ledService: ledService,
        connectedAt: DateTime.now(),
        isActive: _connectedDevices.isEmpty, // Primo device = attivo
      );

      _connectedDevices[deviceId] = connectedDevice;

      // Se è il primo device, impostalo come attivo
      _activeDeviceId ??= deviceId;

      // Ascolta disconnessioni
      _connectionSubscriptions[deviceId] =
          device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _handleDeviceDisconnected(deviceId);
        }
      });

      debugPrint('[MultiDeviceManager] Device $deviceId aggiunto con successo');
    } catch (e) {
      debugPrint('[MultiDeviceManager] Errore aggiungendo device $deviceId: $e');
      rethrow;
    }
  }

  /// Rimuove un device
  Future<void> removeDevice(String deviceId) async {
    final device = _connectedDevices[deviceId];
    if (device == null) return;

    try {
      // Disconnetti
      await device.device.disconnect();

      // Cleanup
      device.ledService.dispose();
      _connectionSubscriptions[deviceId]?.cancel();
      _connectionSubscriptions.remove(deviceId);
      _connectedDevices.remove(deviceId);

      // Se era il device attivo, passa al primo disponibile
      if (_activeDeviceId == deviceId) {
        _activeDeviceId =
            _connectedDevices.isNotEmpty ? _connectedDevices.keys.first : null;
      }

      await _saveDevices();
      notifyListeners();

      debugPrint('[MultiDeviceManager] Device $deviceId rimosso');
    } catch (e) {
      debugPrint('[MultiDeviceManager] Errore rimuovendo device $deviceId: $e');
      rethrow;
    }
  }

  /// Gestisce disconnessione automatica
  void _handleDeviceDisconnected(String deviceId) {
    debugPrint('[MultiDeviceManager] Device $deviceId disconnesso');

    final device = _connectedDevices[deviceId];
    if (device != null) {
      device.ledService.dispose();
      _connectionSubscriptions[deviceId]?.cancel();
      _connectionSubscriptions.remove(deviceId);
      _connectedDevices.remove(deviceId);

      // Se era il device attivo, passa al primo disponibile
      if (_activeDeviceId == deviceId) {
        _activeDeviceId =
            _connectedDevices.isNotEmpty ? _connectedDevices.keys.first : null;
      }

      _saveDevices();
      notifyListeners();
    }
  }

  /// Imposta il device attivo
  void setActiveDevice(String deviceId) {
    if (_connectedDevices.containsKey(deviceId)) {
      // Aggiorna flag isActive
      for (final device in _connectedDevices.values) {
        device.isActive = device.id == deviceId;
      }

      _activeDeviceId = deviceId;
      _saveDevices();
      notifyListeners();

      debugPrint('[MultiDeviceManager] Device attivo: $deviceId');
    }
  }

  /// Salva i devices in SharedPreferences
  Future<void> _saveDevices() async {
    try {
      final prefs = await SharedPreferences.getInstance();

      // Salva lista devices
      final devicesList = _connectedDevices.values
          .map((d) => d.toJson())
          .toList();
      await prefs.setString(_prefsKey, jsonEncode(devicesList));

      // Salva active device
      if (_activeDeviceId != null) {
        await prefs.setString(_activeDeviceKey, _activeDeviceId!);
      } else {
        await prefs.remove(_activeDeviceKey);
      }

      debugPrint(
          '[MultiDeviceManager] Salvati ${devicesList.length} devices, active: $_activeDeviceId');
    } catch (e) {
      debugPrint('[MultiDeviceManager] Errore salvando devices: $e');
    }
  }

  /// Pulisce tutti i devices
  Future<void> clearAll() async {
    for (final deviceId in _connectedDevices.keys.toList()) {
      await removeDevice(deviceId);
    }

    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_prefsKey);
    await prefs.remove(_activeDeviceKey);

    _activeDeviceId = null;
    notifyListeners();
  }

  @override
  void dispose() {
    for (final subscription in _connectionSubscriptions.values) {
      subscription.cancel();
    }
    for (final device in _connectedDevices.values) {
      device.ledService.dispose();
    }
    super.dispose();
  }
}
