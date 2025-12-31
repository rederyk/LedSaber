import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import '../models/led_state.dart';
import '../models/effect.dart';
import '../services/led_service.dart';
import 'audio_provider.dart';

/// Modalità del color picker sulla lama
enum BladeColorPickerMode {
  saturation, // Variazione saturazione/luminosità
  hue, // Colori vicini nell'arcobaleno
  brightness, // Solo variazione di luminosità
}

/// Provider per gestire lo stato dei LED
class LedProvider extends ChangeNotifier {
  LedService? _ledService;
  LedState? _currentState;
  EffectsList? _effectsList;
  String? _errorMessage;

  StreamSubscription<LedState>? _stateSubscription;

  // Audio integration
  AudioProvider? _audioProvider;

  /// Timer per debounce delle notifiche durante animazioni
  Timer? _debounceTimer;

  /// Ultimo stato bladeState per rilevare cambi significativi
  String? _lastBladeState;
  String? _lastStateSignature;

  /// Modalità color picker avanzata sulla lama
  bool _isPreviewMode = false;
  BladeColorPickerMode _pickerMode = BladeColorPickerMode.saturation;

  // Getters
  LedState? get currentState => _currentState;
  EffectsList? get effectsList => _effectsList;
  LedService? get ledService => _ledService;
  String? get errorMessage => _errorMessage;
  bool get isBladeOn => _currentState?.bladeState == 'on';
  bool get isBladeAnimating =>
      _currentState?.bladeState == 'igniting' ||
      _currentState?.bladeState == 'retracting';

  // Getters per modalità preview color picker
  bool get isPreviewMode => _isPreviewMode;
  BladeColorPickerMode get pickerMode => _pickerMode;

  /// Collega AudioProvider per sincronizzare audio con bladeState
  void setAudioProvider(AudioProvider? audioProvider) {
    _audioProvider = audioProvider;
    debugPrint('[LedProvider] AudioProvider collegato: ${audioProvider != null}');
  }

  /// Imposta il LED Service e inizia ad ascoltare lo stato
  void setLedService(LedService? service) {
    // Se è lo stesso servizio, non fare nulla
    if (_ledService == service) {
      return;
    }

    // Cleanup del servizio precedente
    _stateSubscription?.cancel();
    _debounceTimer?.cancel();

    _ledService = service;

    if (_ledService != null) {
      // Ascolta i cambiamenti dello stato LED
      _stateSubscription = _ledService!.ledStateStream.listen((state) {
        _handleStateUpdate(state);
      });

      // Carica la lista degli effetti
      _loadEffectsList();
    } else {
      // Cleanup dello stato
      _currentState = null;
      _effectsList = null;
      _lastBladeState = null;
      _lastStateSignature = null;
    }

    notifyListeners();
  }

  /// Gestisce gli aggiornamenti di stato con debounce intelligente
  void _handleStateUpdate(LedState state) {
    final stateSignature = _buildStateSignature(state);
    if (stateSignature == _lastStateSignature) {
      return;
    }

    final currentBladeState = state.bladeState;
    final isAnimating = currentBladeState == 'igniting' || currentBladeState == 'retracting';

    // Verifica se c'è un cambio significativo di bladeState
    final bladeStateChanged = _lastBladeState != currentBladeState;

    if (bladeStateChanged) {
      // Cambio di stato significativo - notifica immediatamente
      _lastBladeState = currentBladeState;
      _lastStateSignature = stateSignature;
      _currentState = state;
      _debounceTimer?.cancel();

      // SYNC AUDIO: Sincronizza audio quando bladeState cambia
      if (_audioProvider != null) {
        debugPrint('[LedProvider] Sincronizzazione audio con bladeState: $currentBladeState');
        _audioProvider!.syncWithBladeState(currentBladeState);
      }

      notifyListeners();
    } else if (isAnimating) {
      // Durante animazione - applica debounce (solo ogni 100ms)
      _lastStateSignature = stateSignature;
      _currentState = state;

      if (_debounceTimer == null || !_debounceTimer!.isActive) {
        _debounceTimer = Timer(const Duration(milliseconds: 100), () {
          notifyListeners();
        });
      }
    } else {
      // Stato stabile (on/off) - notifica normalmente
      _lastStateSignature = stateSignature;
      _currentState = state;
      _debounceTimer?.cancel();
      notifyListeners();
    }
  }

  String _buildStateSignature(LedState state) {
    return jsonEncode(state.toJson());
  }

  /// Ricarica manualmente la lista degli effetti (metodo pubblico)
  Future<void> reloadEffectsList() async {
    await _loadEffectsList();
  }

  /// Carica la lista degli effetti dal dispositivo con retry automatico
  Future<void> _loadEffectsList() async {
    const int maxRetries = 3;
    const int delayMs = 500; // Delay iniziale prima del primo tentativo
    const int retryDelayMs = 1000; // Delay tra i retry

    // Aspetta un po' per permettere alla connessione BLE di stabilizzarsi
    await Future.delayed(const Duration(milliseconds: delayMs));

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
      try {
        debugPrint('[LedProvider] Tentativo $attempt/$maxRetries di caricamento lista effetti...');
        _effectsList = await _ledService?.getEffectsList();

        if (_effectsList != null && _effectsList!.effects.isNotEmpty) {
          debugPrint('[LedProvider] Lista effetti caricata con successo: ${_effectsList!.effects.length} effetti');
          _errorMessage = null;
          notifyListeners();
          return; // Successo, esci dal loop
        } else {
          debugPrint('[LedProvider] Lista effetti vuota o null al tentativo $attempt');
        }
      } catch (e) {
        debugPrint('[LedProvider] Errore al tentativo $attempt: $e');

        if (attempt == maxRetries) {
          // Ultimo tentativo fallito
          _errorMessage = 'Errore caricando lista effetti dopo $maxRetries tentativi: $e';
          debugPrint('[LedProvider] Tutti i tentativi falliti');
          notifyListeners();
          return;
        }
      }

      // Aspetta prima del prossimo retry
      if (attempt < maxRetries) {
        debugPrint('[LedProvider] Attendo ${retryDelayMs}ms prima del prossimo tentativo...');
        await Future.delayed(const Duration(milliseconds: retryDelayMs));
      }
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
    bool? chronoWellnessMode,
    int? breathingRate,
  }) async {
    try {
      await _ledService?.setEffect(
        mode,
        speed: speed,
        chronoHourTheme: chronoHourTheme,
        chronoSecondTheme: chronoSecondTheme,
        chronoWellnessMode: chronoWellnessMode,
        breathingRate: breathingRate,
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

  /// Attiva la modalità color picker avanzata sulla lama
  void setPreviewColor(Color? color, {bool isPreviewMode = false}) {
    _isPreviewMode = isPreviewMode;
    notifyListeners();
  }

  /// Cambia la modalità del color picker
  void cyclePickerMode() {
    switch (_pickerMode) {
      case BladeColorPickerMode.saturation:
        _pickerMode = BladeColorPickerMode.hue;
        break;
      case BladeColorPickerMode.hue:
        _pickerMode = BladeColorPickerMode.brightness;
        break;
      case BladeColorPickerMode.brightness:
        _pickerMode = BladeColorPickerMode.saturation;
        break;
    }
    notifyListeners();
  }

  /// Resetta la modalità preview
  void clearPreview() {
    _isPreviewMode = false;
    _pickerMode = BladeColorPickerMode.saturation;
    notifyListeners();
  }

  @override
  void dispose() {
    _stateSubscription?.cancel();
    _debounceTimer?.cancel();
    super.dispose();
  }
}
