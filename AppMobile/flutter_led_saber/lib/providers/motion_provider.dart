import 'dart:async';
import 'package:flutter/foundation.dart';
import '../models/motion_state.dart';
import '../services/motion_service.dart';

/// Provider per gestire lo stato del Motion Detection & Gesture System
class MotionProvider extends ChangeNotifier {
  MotionService? _motionService;
  MotionState? _currentState;
  MotionConfig? _currentConfig;
  MotionEvent? _lastEvent;
  String? _errorMessage;

  StreamSubscription<MotionState>? _stateSubscription;
  StreamSubscription<MotionEvent>? _eventSubscription;

  // Getters
  MotionState? get currentState => _currentState;
  MotionConfig? get currentConfig => _currentConfig;
  MotionEvent? get lastEvent => _lastEvent;
  String? get errorMessage => _errorMessage;
  bool get isMotionEnabled => _currentState?.enabled ?? false;
  bool get isMotionDetected => _currentState?.motionDetected ?? false;

  /// Imposta il Motion Service e inizia ad ascoltare lo stato
  void setMotionService(MotionService? service) {
    // Se è lo stesso servizio, non fare nulla
    if (_motionService == service) {
      return;
    }

    // Cleanup del servizio precedente
    _stateSubscription?.cancel();
    _eventSubscription?.cancel();

    _motionService = service;

    if (_motionService != null) {
      debugPrint('[MotionProvider] Motion Service impostato, abilito notifiche...');

      // Ascolta i cambiamenti dello stato Motion
      _stateSubscription = _motionService!.motionStateStream.listen(
        (state) {
          debugPrint('[MotionProvider] Nuovo stato motion ricevuto: $state');
          _currentState = state;
          _errorMessage = null;
          notifyListeners();
        },
        onError: (error) {
          debugPrint('[MotionProvider] Errore stream stato: $error');
          _errorMessage = error.toString();
          notifyListeners();
        },
      );

      // Ascolta gli eventi Motion
      _eventSubscription = _motionService!.motionEventStream.listen(
        (event) {
          debugPrint('[MotionProvider] Nuovo evento motion: $event');
          _lastEvent = event;
          notifyListeners();
        },
        onError: (error) {
          debugPrint('[MotionProvider] Errore stream eventi: $error');
        },
      );

      // Abilita le notifiche
      _enableNotifications();

      // Carica la configurazione corrente
      _loadConfig();
    } else {
      // Cleanup dello stato
      _currentState = null;
      _currentConfig = null;
      _lastEvent = null;
    }

    notifyListeners();
  }

  /// Abilita le notifiche BLE per stato ed eventi
  Future<void> _enableNotifications() async {
    if (_motionService == null) return;

    try {
      await _motionService!.enableStatusNotifications();
      debugPrint('[MotionProvider] Notifiche stato abilitate');
    } catch (e) {
      debugPrint('[MotionProvider] Errore abilitando notifiche stato: $e');
      _errorMessage = 'Errore notifiche stato: $e';
      notifyListeners();
    }

    try {
      await _motionService!.enableEventsNotifications();
      debugPrint('[MotionProvider] Notifiche eventi abilitate');
    } catch (e) {
      debugPrint('[MotionProvider] Errore abilitando notifiche eventi: $e');
      // Non critico
    }
  }

  /// Carica la configurazione Motion corrente dal dispositivo
  Future<void> _loadConfig() async {
    if (_motionService == null) return;

    try {
      debugPrint('[MotionProvider] Caricamento configurazione motion...');
      final config = await _motionService!.getConfig();
      if (config != null) {
        _currentConfig = config;
        _errorMessage = null;
        debugPrint('[MotionProvider] Configurazione caricata: $config');
        notifyListeners();
      }
    } catch (e) {
      debugPrint('[MotionProvider] Errore caricando config: $e');
      _errorMessage = 'Errore caricando config: $e';
      notifyListeners();
    }
  }

  /// Ricarica manualmente la configurazione
  Future<void> reloadConfig() async {
    await _loadConfig();
  }

  /// Abilita motion detection
  Future<void> enableMotion() async {
    if (_motionService == null) {
      _errorMessage = 'Motion service non disponibile';
      notifyListeners();
      return;
    }

    try {
      debugPrint('[MotionProvider] Abilitazione motion detection...');
      await _motionService!.enableMotion();
      _errorMessage = null;
    } catch (e) {
      debugPrint('[MotionProvider] Errore abilitando motion: $e');
      _errorMessage = 'Errore abilitando motion: $e';
      notifyListeners();
    }
  }

  /// Disabilita motion detection
  Future<void> disableMotion() async {
    if (_motionService == null) {
      _errorMessage = 'Motion service non disponibile';
      notifyListeners();
      return;
    }

    try {
      debugPrint('[MotionProvider] Disabilitazione motion detection...');
      await _motionService!.disableMotion();
      _errorMessage = null;
    } catch (e) {
      debugPrint('[MotionProvider] Errore disabilitando motion: $e');
      _errorMessage = 'Errore disabilitando motion: $e';
      notifyListeners();
    }
  }

  /// Reset motion detector
  Future<void> resetMotion() async {
    if (_motionService == null) {
      _errorMessage = 'Motion service non disponibile';
      notifyListeners();
      return;
    }

    try {
      debugPrint('[MotionProvider] Reset motion detector...');
      await _motionService!.resetMotion();
      _errorMessage = null;
    } catch (e) {
      debugPrint('[MotionProvider] Errore reset motion: $e');
      _errorMessage = 'Errore reset motion: $e';
      notifyListeners();
    }
  }

  /// Applica una nuova configurazione Motion
  Future<void> applyConfig(MotionConfig config) async {
    if (_motionService == null) {
      _errorMessage = 'Motion service non disponibile';
      notifyListeners();
      return;
    }

    try {
      debugPrint('[MotionProvider] Applicazione configurazione: $config');
      await _motionService!.setConfig(config);
      _currentConfig = config;
      _errorMessage = null;
      notifyListeners();
    } catch (e) {
      debugPrint('[MotionProvider] Errore applicando config: $e');
      _errorMessage = 'Errore applicando config: $e';
      notifyListeners();
    }
  }

  /// Aggiorna un singolo parametro della configurazione
  Future<void> updateConfigParam({
    bool? enabled,
    bool? gesturesEnabled,
    int? quality,
    int? motionIntensityMin,
    double? motionSpeedMin,
    int? gestureIgnitionIntensity,
    int? gestureRetractIntensity,
    int? gestureClashIntensity,
    bool? debugLogs,
  }) async {
    if (_currentConfig == null) {
      debugPrint('[MotionProvider] Config non ancora caricata');
      return;
    }

    final newConfig = _currentConfig!.copyWith(
      enabled: enabled,
      gesturesEnabled: gesturesEnabled,
      quality: quality,
      motionIntensityMin: motionIntensityMin,
      motionSpeedMin: motionSpeedMin,
      gestureIgnitionIntensity: gestureIgnitionIntensity,
      gestureRetractIntensity: gestureRetractIntensity,
      gestureClashIntensity: gestureClashIntensity,
      debugLogs: debugLogs,
    );

    await applyConfig(newConfig);
  }

  @override
  void dispose() {
    _stateSubscription?.cancel();
    _eventSubscription?.cancel();
    super.dispose();
  }
}
