import 'dart:async';
import 'dart:convert';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../models/led_state.dart';
import '../models/effect.dart';

/// Servizio per controllare i LED via BLE
class LedService {
  // UUID del servizio LED
  static const String serviceUuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";

  // Characteristic UUIDs (dal firmware BLELedController.h)
  static const String charLedState = "beb5483e-36e1-4688-b7f5-ea07361b26a8"; // READ + NOTIFY
  static const String charLedColor = "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab"; // WRITE
  static const String charLedEffect = "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc"; // WRITE
  static const String charLedBrightness = "f3e7c6e5-0d32-4c5a-ac6e-3456789012cd"; // WRITE
  static const String charStatusLed = "a4b8d7f9-1e43-6c7d-ad8f-456789abcdef"; // WRITE
  static const String charTimeSync = "d6e1a0b8-4a76-9f0c-dc1a-789abcdef012"; // WRITE
  static const String charDeviceControl = "c7f8e0d9-5b87-1a2b-be9d-7890abcdef23"; // WRITE
  static const String charEffectsList = "d8f9e1ea-6c98-2b3c-cf0e-890abcdef234"; // READ

  final BluetoothService _service;
  final StreamController<LedState> _ledStateController =
      StreamController<LedState>.broadcast();

  BluetoothCharacteristic? _stateChar;
  BluetoothCharacteristic? _colorChar;
  BluetoothCharacteristic? _effectChar;
  BluetoothCharacteristic? _brightnessChar;
  BluetoothCharacteristic? _statusLedChar;
  BluetoothCharacteristic? _timeSyncChar;
  BluetoothCharacteristic? _deviceControlChar;
  BluetoothCharacteristic? _effectsListChar;

  StreamSubscription<List<int>>? _stateSubscription;

  LedService(this._service) {
    _initializeCharacteristics();
  }

  /// Stream dello stato LED
  Stream<LedState> get ledStateStream => _ledStateController.stream;

  /// Inizializza le characteristic
  void _initializeCharacteristics() {
    for (final char in _service.characteristics) {
      final uuid = char.uuid.toString().toLowerCase();

      if (uuid == charLedState.toLowerCase()) {
        _stateChar = char;
      } else if (uuid == charLedColor.toLowerCase()) {
        _colorChar = char;
      } else if (uuid == charLedEffect.toLowerCase()) {
        _effectChar = char;
      } else if (uuid == charLedBrightness.toLowerCase()) {
        _brightnessChar = char;
      } else if (uuid == charStatusLed.toLowerCase()) {
        _statusLedChar = char;
      } else if (uuid == charTimeSync.toLowerCase()) {
        _timeSyncChar = char;
      } else if (uuid == charDeviceControl.toLowerCase()) {
        _deviceControlChar = char;
      } else if (uuid == charEffectsList.toLowerCase()) {
        _effectsListChar = char;
      }
    }
  }

  /// Abilita le notifiche per lo stato LED
  Future<void> enableStateNotifications() async {
    if (_stateChar == null) {
      throw Exception('LED State characteristic non trovata');
    }

    try {
      // Abilita le notifiche
      await _stateChar!.setNotifyValue(true);

      // Ascolta i cambiamenti
      _stateSubscription?.cancel();
      _stateSubscription = _stateChar!.lastValueStream.listen((value) {
        if (value.isNotEmpty) {
          try {
            final json = jsonDecode(utf8.decode(value));
            final state = LedState.fromJson(json);
            _ledStateController.add(state);
          } catch (e) {
            // Ignora errori di parsing - il valore potrebbe non essere JSON valido
          }
        }
      });

      // Leggi il valore iniziale
      final initialValue = await _stateChar!.read();
      if (initialValue.isNotEmpty) {
        final json = jsonDecode(utf8.decode(initialValue));
        final state = LedState.fromJson(json);
        _ledStateController.add(state);
      }
    } catch (e) {
      throw Exception('Errore abilitando notifiche stato LED: $e');
    }
  }

  /// Legge la lista degli effetti disponibili
  Future<EffectsList?> getEffectsList() async {
    if (_effectsListChar == null) {
      throw Exception('Effects List characteristic non trovata');
    }

    try {
      final value = await _effectsListChar!.read();
      if (value.isEmpty) return null;

      final json = jsonDecode(utf8.decode(value));
      return EffectsList.fromJson(json);
    } catch (e) {
      throw Exception('Errore leggendo lista effetti: $e');
    }
  }

  /// Imposta il colore RGB
  Future<void> setColor(int r, int g, int b) async {
    if (_colorChar == null) {
      throw Exception('LED Color characteristic non trovata');
    }

    try {
      final json = jsonEncode({'r': r, 'g': g, 'b': b});
      await _colorChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore impostando colore: $e');
    }
  }

  /// Imposta l'effetto
  Future<void> setEffect(
    String mode, {
    int? speed,
    int? chronoHourTheme,
    int? chronoSecondTheme,
  }) async {
    if (_effectChar == null) {
      throw Exception('LED Effect characteristic non trovata');
    }

    try {
      final Map<String, dynamic> payload = {'mode': mode};

      if (speed != null) payload['speed'] = speed;
      if (chronoHourTheme != null) payload['chronoHourTheme'] = chronoHourTheme;
      if (chronoSecondTheme != null) payload['chronoSecondTheme'] = chronoSecondTheme;

      final json = jsonEncode(payload);
      await _effectChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore impostando effetto: $e');
    }
  }

  /// Imposta brightness e on/off
  Future<void> setBrightness(int brightness, bool enabled) async {
    if (_brightnessChar == null) {
      throw Exception('LED Brightness characteristic non trovata');
    }

    try {
      final json = jsonEncode({
        'brightness': brightness,
        'enabled': enabled,
      });
      await _brightnessChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore impostando brightness: $e');
    }
  }

  /// Imposta Status LED (pin 4)
  Future<void> setStatusLed(int brightness, bool enabled) async {
    if (_statusLedChar == null) {
      throw Exception('Status LED characteristic non trovata');
    }

    try {
      final json = jsonEncode({
        'brightness': brightness,
        'enabled': enabled,
      });
      await _statusLedChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore impostando status LED: $e');
    }
  }

  /// Sincronizza il tempo con l'orologio
  Future<void> syncTime(int epochTimestamp) async {
    if (_timeSyncChar == null) {
      throw Exception('Time Sync characteristic non trovata');
    }

    try {
      final json = jsonEncode({'epoch': epochTimestamp});
      await _timeSyncChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore sincronizzando tempo: $e');
    }
  }

  /// Accende la lama (ignition)
  Future<void> ignite() async {
    if (_deviceControlChar == null) {
      throw Exception('Device Control characteristic non trovata');
    }

    try {
      final json = jsonEncode({'command': 'ignition'});
      await _deviceControlChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore durante ignition: $e');
    }
  }

  /// Spegne la lama (retract)
  Future<void> retract() async {
    if (_deviceControlChar == null) {
      throw Exception('Device Control characteristic non trovata');
    }

    try {
      final json = jsonEncode({'command': 'retract'});
      await _deviceControlChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore durante retract: $e');
    }
  }

  /// Imposta la configurazione di boot
  Future<void> setBootConfig({
    bool? autoIgnitionOnBoot,
    int? autoIgnitionDelayMs,
    bool? motionEnabled,
  }) async {
    if (_deviceControlChar == null) {
      throw Exception('Device Control characteristic non trovata');
    }

    try {
      final Map<String, dynamic> payload = {'command': 'boot_config'};

      if (autoIgnitionOnBoot != null) {
        payload['autoIgnitionOnBoot'] = autoIgnitionOnBoot;
      }
      if (autoIgnitionDelayMs != null) {
        payload['autoIgnitionDelayMs'] = autoIgnitionDelayMs;
      }
      if (motionEnabled != null) {
        payload['motionEnabled'] = motionEnabled;
      }

      final json = jsonEncode(payload);
      await _deviceControlChar!.write(
        utf8.encode(json),
        withoutResponse: false,
      );
    } catch (e) {
      throw Exception('Errore impostando boot config: $e');
    }
  }

  /// Pulizia
  void dispose() {
    _stateSubscription?.cancel();
    _ledStateController.close();
  }
}
