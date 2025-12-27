import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../models/motion_state.dart';

/// Servizio per Motion Detection & Gesture System via BLE
class MotionService {
  // UUID del servizio Motion (dal firmware BLEMotionService.h)
  static const String serviceUuid = "6fafc401-1fb5-459e-8fcc-c5c9c331914b";

  // Characteristic UUIDs
  static const String charMotionStatus = "7eb5583e-36e1-4688-b7f5-ea07361b26a9"; // READ + NOTIFY
  static const String charMotionControl = "8dc5b4c3-eb10-4a3e-8a4c-1234567890ac"; // WRITE
  static const String charMotionEvents = "9ef6c5d4-fc21-5b4f-9b5d-2345678901bd"; // NOTIFY
  static const String charMotionConfig = "aff7d6e5-0d32-4c5a-ac6e-3456789012ce"; // READ + WRITE

  // UUID del servizio Camera (necessario per sincronizzare motion+camera)
  static const String cameraServiceUuid = "5fafc301-1fb5-459e-8fcc-c5c9c331914b";
  static const String charCameraControl = "7dc5a4c3-eb10-4a3e-8a4c-1234567890ab"; // WRITE

  final BluetoothService _service;
  BluetoothCharacteristic? _cameraControlChar;
  final StreamController<MotionState> _motionStateController =
      StreamController<MotionState>.broadcast();
  final StreamController<MotionEvent> _motionEventController =
      StreamController<MotionEvent>.broadcast();

  BluetoothCharacteristic? _statusChar;
  BluetoothCharacteristic? _controlChar;
  BluetoothCharacteristic? _eventsChar;
  BluetoothCharacteristic? _configChar;

  StreamSubscription<List<int>>? _statusSubscription;
  StreamSubscription<List<int>>? _eventsSubscription;

  MotionService(this._service) {
    _initializeCharacteristics();
  }

  /// Stream dello stato Motion
  Stream<MotionState> get motionStateStream => _motionStateController.stream;

  /// Stream degli eventi Motion
  Stream<MotionEvent> get motionEventStream => _motionEventController.stream;

  /// Inizializza le characteristic
  void _initializeCharacteristics() {
    for (final char in _service.characteristics) {
      final uuid = char.uuid.toString().toLowerCase();

      if (uuid == charMotionStatus.toLowerCase()) {
        _statusChar = char;
        debugPrint('[MotionService] Found Motion Status characteristic');
      } else if (uuid == charMotionControl.toLowerCase()) {
        _controlChar = char;
        debugPrint('[MotionService] Found Motion Control characteristic');
      } else if (uuid == charMotionEvents.toLowerCase()) {
        _eventsChar = char;
        debugPrint('[MotionService] Found Motion Events characteristic');
      } else if (uuid == charMotionConfig.toLowerCase()) {
        _configChar = char;
        debugPrint('[MotionService] Found Motion Config characteristic');
      }
    }
  }

  /// Abilita le notifiche per lo stato Motion
  Future<void> enableStatusNotifications() async {
    if (_statusChar == null) {
      debugPrint('[MotionService] ERROR: Motion Status characteristic non trovata');
      throw Exception('Motion Status characteristic non trovata');
    }

    try {
      debugPrint('[MotionService] Abilitazione notifiche Motion Status...');

      // Abilita le notifiche
      await _statusChar!.setNotifyValue(true);

      // Ascolta i cambiamenti
      _statusSubscription?.cancel();
      _statusSubscription = _statusChar!.lastValueStream.listen((value) {
        if (value.isNotEmpty) {
          try {
            final decodedString = utf8.decode(value);
            debugPrint('[MotionService] Status notify ricevuto: ${decodedString.substring(0, decodedString.length > 100 ? 100 : decodedString.length)}');

            final json = jsonDecode(decodedString);
            final state = MotionState.fromJson(json);
            _motionStateController.add(state);
          } catch (e) {
            debugPrint('[MotionService] Errore parsing stato motion: $e');
          }
        }
      });

      // Leggi il valore iniziale
      final initialValue = await _statusChar!.read();
      if (initialValue.isNotEmpty) {
        try {
          final decodedString = utf8.decode(initialValue);
          debugPrint('[MotionService] Status iniziale: $decodedString');

          final json = jsonDecode(decodedString);
          final state = MotionState.fromJson(json);
          _motionStateController.add(state);
        } catch (e) {
          debugPrint('[MotionService] Errore parsing stato iniziale: $e');
        }
      }

      debugPrint('[MotionService] Notifiche Motion Status abilitate con successo');
    } catch (e) {
      debugPrint('[MotionService] ERROR abilitando notifiche stato motion: $e');
      throw Exception('Errore abilitando notifiche stato motion: $e');
    }
  }

  /// Abilita le notifiche per gli eventi Motion
  Future<void> enableEventsNotifications() async {
    if (_eventsChar == null) {
      debugPrint('[MotionService] WARNING: Motion Events characteristic non trovata');
      return; // Non è critico
    }

    try {
      debugPrint('[MotionService] Abilitazione notifiche Motion Events...');

      // Abilita le notifiche
      await _eventsChar!.setNotifyValue(true);

      // Ascolta gli eventi
      _eventsSubscription?.cancel();
      _eventsSubscription = _eventsChar!.lastValueStream.listen((value) {
        if (value.isNotEmpty) {
          try {
            final decodedString = utf8.decode(value);
            debugPrint('[MotionService] Event notify: $decodedString');

            final json = jsonDecode(decodedString);
            final event = MotionEvent.fromJson(json);
            _motionEventController.add(event);
          } catch (e) {
            debugPrint('[MotionService] Errore parsing evento motion: $e');
          }
        }
      });

      debugPrint('[MotionService] Notifiche Motion Events abilitate con successo');
    } catch (e) {
      debugPrint('[MotionService] ERROR abilitando notifiche eventi motion: $e');
    }
  }

  /// Legge la configurazione Motion corrente
  Future<MotionConfig?> getConfig() async {
    if (_configChar == null) {
      debugPrint('[MotionService] ERROR: Motion Config characteristic non trovata');
      throw Exception('Motion Config characteristic non trovata');
    }

    try {
      debugPrint('[MotionService] Lettura Motion Config...');
      final value = await _configChar!.read();

      if (value.isEmpty) {
        debugPrint('[MotionService] WARNING: Config vuota');
        return null;
      }

      final decodedString = utf8.decode(value);
      debugPrint('[MotionService] Config ricevuta: $decodedString');

      final json = jsonDecode(decodedString);
      final config = MotionConfig.fromJson(json);

      debugPrint('[MotionService] Config parsificata: $config');
      return config;
    } catch (e) {
      debugPrint('[MotionService] ERROR leggendo config motion: $e');
      throw Exception('Errore leggendo config motion: $e');
    }
  }

  /// Scrive la configurazione Motion
  Future<void> setConfig(MotionConfig config) async {
    if (_configChar == null) {
      debugPrint('[MotionService] ERROR: Motion Config characteristic non trovata');
      throw Exception('Motion Config characteristic non trovata');
    }

    try {
      final json = jsonEncode(config.toJson());
      debugPrint('[MotionService] Invio config: $json');

      await _configChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );

      debugPrint('[MotionService] Config inviata con successo');
    } catch (e) {
      debugPrint('[MotionService] ERROR scrivendo config motion: $e');
      throw Exception('Errore scrivendo config motion: $e');
    }
  }

  /// Invia comando Motion Control
  /// Comandi: "enable", "disable", "reset", "calibrate"
  Future<void> sendControlCommand(String command) async {
    if (_controlChar == null) {
      debugPrint('[MotionService] ERROR: Motion Control characteristic non trovata');
      throw Exception('Motion Control characteristic non trovata');
    }

    try {
      debugPrint('[MotionService] Invio comando: $command');

      await _controlChar!.write(
        utf8.encode(command),
        withoutResponse: false,
      );

      debugPrint('[MotionService] Comando "$command" inviato con successo');
    } catch (e) {
      debugPrint('[MotionService] ERROR inviando comando motion: $e');
      throw Exception('Errore inviando comando motion: $e');
    }
  }

  /// Abilita motion detection
  Future<void> enableMotion() async {
    await sendControlCommand('enable');
  }

  /// Disabilita motion detection
  Future<void> disableMotion() async {
    await sendControlCommand('disable');
  }

  /// Reset motion detector
  Future<void> resetMotion() async {
    await sendControlCommand('reset');
  }

  /// Calibra motion detector
  Future<void> calibrateMotion() async {
    await sendControlCommand('calibrate');
  }

  /// Inizializza la camera characteristic (opzionale, chiamato se serve camera sync)
  Future<void> initCameraControl(List<BluetoothService> allServices) async {
    try {
      // Cerca il servizio camera
      final cameraService = allServices.firstWhere(
        (s) => s.uuid.toString().toLowerCase() == cameraServiceUuid.toLowerCase(),
        orElse: () => throw Exception('Camera service non trovato'),
      );

      // Cerca la characteristic control
      _cameraControlChar = cameraService.characteristics.firstWhere(
        (c) => c.uuid.toString().toLowerCase() == charCameraControl.toLowerCase(),
        orElse: () => throw Exception('Camera Control characteristic non trovata'),
      );

      debugPrint('[MotionService] Camera Control characteristic inizializzata');
    } catch (e) {
      debugPrint('[MotionService] WARNING: Impossibile inizializzare camera control: $e');
      // Non blocchiamo se la camera non è disponibile
    }
  }

  /// Invia comando Camera Control (start, stop, init)
  Future<void> sendCameraCommand(String command) async {
    if (_cameraControlChar == null) {
      debugPrint('[MotionService] ERROR: Camera Control characteristic non disponibile');
      throw Exception('Camera Control characteristic non disponibile. Chiama initCameraControl prima.');
    }

    try {
      debugPrint('[MotionService] Invio comando camera: $command');

      await _cameraControlChar!.write(
        utf8.encode(command),
        withoutResponse: false,
      );

      debugPrint('[MotionService] Comando camera "$command" inviato con successo');
    } catch (e) {
      debugPrint('[MotionService] ERROR inviando comando camera: $e');
      throw Exception('Errore inviando comando camera: $e');
    }
  }

  /// Cleanup
  void dispose() {
    _statusSubscription?.cancel();
    _eventsSubscription?.cancel();
    _motionStateController.close();
    _motionEventController.close();
  }
}
