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

  /// Determina se lo stato è cambiato in modo significativo
  bool _shouldUpdateState(MotionState newState) {
    if (_currentState == null) return true;

    // Aggiorna sempre se:
    // - Enabled/motionDetected cambia
    // - C'è un nuovo gesture
    // - Intensity cambia significativamente (>5)
    // - Speed cambia significativamente (>0.5)
    // - Direction cambia

    if (_currentState!.enabled != newState.enabled) return true;
    if (_currentState!.motionDetected != newState.motionDetected) return true;
    if (_currentState!.direction != newState.direction) return true;
    if (_currentState!.lastGesture != newState.lastGesture) return true;

    // Cambiamenti significativi nei valori numerici
    if ((_currentState!.intensity - newState.intensity).abs() > 5) return true;
    if ((_currentState!.speed - newState.speed).abs() > 0.5) return true;
    if ((_currentState!.confidence - newState.confidence).abs() > 10) return true;

    // Altrimenti ignora (è quasi identico)
    return false;
  }

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
          // Filtra aggiornamenti duplicati per ridurre spam
          if (_shouldUpdateState(state)) {
            debugPrint('[MotionProvider] Nuovo stato motion ricevuto: $state');
            _currentState = state;
            _errorMessage = null;
            notifyListeners();
          }
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
    debugPrint('[MotionProvider] enableMotion chiamato');
    debugPrint('[MotionProvider] _motionService: ${_motionService != null ? "disponibile" : "NULL"}');
    debugPrint('[MotionProvider] Stato corrente: enabled=${_currentState?.enabled}');

    if (_motionService == null) {
      _errorMessage = 'Motion service non disponibile';
      debugPrint('[MotionProvider] ERRORE: Motion service è NULL!');
      notifyListeners();
      return;
    }

    try {
      debugPrint('[MotionProvider] Invio comando enable...');
      await _motionService!.enableMotion();
      debugPrint('[MotionProvider] Comando enable inviato con successo');
      _errorMessage = null;
    } catch (e) {
      debugPrint('[MotionProvider] ERRORE abilitando motion: $e');
      _errorMessage = 'Errore abilitando motion: $e';
      notifyListeners();
    }
  }

  /// Disabilita motion detection
  Future<void> disableMotion() async {
    debugPrint('[MotionProvider] disableMotion chiamato');
    debugPrint('[MotionProvider] _motionService: ${_motionService != null ? "disponibile" : "NULL"}');
    debugPrint('[MotionProvider] Stato corrente: enabled=${_currentState?.enabled}');

    if (_motionService == null) {
      _errorMessage = 'Motion service non disponibile';
      debugPrint('[MotionProvider] ERRORE: Motion service è NULL!');
      notifyListeners();
      return;
    }

    try {
      debugPrint('[MotionProvider] Invio comando disable...');
      await _motionService!.disableMotion();
      debugPrint('[MotionProvider] Comando disable inviato con successo');
      _errorMessage = null;
    } catch (e) {
      debugPrint('[MotionProvider] ERRORE disabilitando motion: $e');
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

    debugPrint('[MotionProvider] updateConfigParam chiamato: '
        'enabled=$enabled, gesturesEnabled=$gesturesEnabled');
    debugPrint('[MotionProvider] Config attuale: '
        'enabled=${_currentConfig!.enabled}, gesturesEnabled=${_currentConfig!.gesturesEnabled}');

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

    debugPrint('[MotionProvider] Nuova config da applicare: '
        'enabled=${newConfig.enabled}, gesturesEnabled=${newConfig.gesturesEnabled}');

    await applyConfig(newConfig);
  }

  @override
  void dispose() {
    _stateSubscription?.cancel();
    _eventSubscription?.cancel();
    super.dispose();
  }
}
