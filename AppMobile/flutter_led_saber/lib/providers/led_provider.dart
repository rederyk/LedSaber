import 'dart:async';
import 'package:flutter/foundation.dart';
import '../models/led_state.dart';
import '../models/effect.dart';
import '../services/led_service.dart';

/// Provider per gestire lo stato dei LED
class LedProvider extends ChangeNotifier {
  LedService? _ledService;
  LedState? _currentState;
  EffectsList? _effectsList;
  String? _errorMessage;

  StreamSubscription<LedState>? _stateSubscription;

  // Getters
  LedState? get currentState => _currentState;
  EffectsList? get effectsList => _effectsList;
  String? get errorMessage => _errorMessage;
  bool get isBladeOn => _currentState?.bladeState == 'on';
  bool get isBladeAnimating =>
      _currentState?.bladeState == 'igniting' ||
      _currentState?.bladeState == 'retracting';

  /// Imposta il LED Service e inizia ad ascoltare lo stato
  void setLedService(LedService? service) {
    _ledService = service;

    if (_ledService != null) {
      // Ascolta i cambiamenti dello stato LED
      _stateSubscription?.cancel();
      _stateSubscription = _ledService!.ledStateStream.listen((state) {
        _currentState = state;
        notifyListeners();
      });

      // Carica la lista degli effetti
      _loadEffectsList();
    } else {
      _stateSubscription?.cancel();
      _currentState = null;
      _effectsList = null;
    }

    notifyListeners();
  }

  /// Carica la lista degli effetti dal dispositivo
  Future<void> _loadEffectsList() async {
    try {
      _effectsList = await _ledService?.getEffectsList();
      _errorMessage = null;
      notifyListeners();
    } catch (e) {
      _errorMessage = 'Errore caricando lista effetti: $e';
      notifyListeners();
    }
  }

  /// Imposta il colore RGB
  Future<void> setColor(int r, int g, int b) async {
    try {
      await _ledService?.setColor(r, g, b);
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore impostando colore: $e';
      notifyListeners();
    }
  }

  /// Imposta l'effetto
  Future<void> setEffect(
    String mode, {
    int? speed,
    int? chronoHourTheme,
    int? chronoSecondTheme,
  }) async {
    try {
      await _ledService?.setEffect(
        mode,
        speed: speed,
        chronoHourTheme: chronoHourTheme,
        chronoSecondTheme: chronoSecondTheme,
      );
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore impostando effetto: $e';
      notifyListeners();
    }
  }

  /// Imposta brightness e on/off
  Future<void> setBrightness(int brightness, bool enabled) async {
    try {
      await _ledService?.setBrightness(brightness, enabled);
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore impostando brightness: $e';
      notifyListeners();
    }
  }

  /// Imposta Status LED
  Future<void> setStatusLed(int brightness, bool enabled) async {
    try {
      await _ledService?.setStatusLed(brightness, enabled);
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore impostando status LED: $e';
      notifyListeners();
    }
  }

  /// Sincronizza il tempo
  Future<void> syncTime() async {
    try {
      final now = DateTime.now();
      final epochSeconds = now.millisecondsSinceEpoch ~/ 1000;
      await _ledService?.syncTime(epochSeconds);
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore sincronizzando tempo: $e';
      notifyListeners();
    }
  }

  /// Accende la lama (ignition)
  Future<void> ignite() async {
    try {
      await _ledService?.ignite();
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore durante ignition: $e';
      notifyListeners();
    }
  }

  /// Spegne la lama (retract)
  Future<void> retract() async {
    try {
      await _ledService?.retract();
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore durante retract: $e';
      notifyListeners();
    }
  }

  /// Imposta la configurazione di boot
  Future<void> setBootConfig({
    bool? autoIgnitionOnBoot,
    int? autoIgnitionDelayMs,
    bool? motionEnabled,
  }) async {
    try {
      await _ledService?.setBootConfig(
        autoIgnitionOnBoot: autoIgnitionOnBoot,
        autoIgnitionDelayMs: autoIgnitionDelayMs,
        motionEnabled: motionEnabled,
      );
      _errorMessage = null;
    } catch (e) {
      _errorMessage = 'Errore impostando boot config: $e';
      notifyListeners();
    }
  }

  /// Pulisce l'errore
  void clearError() {
    _errorMessage = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _stateSubscription?.cancel();
    super.dispose();
  }
}
