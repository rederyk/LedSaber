import 'dart:async';
import 'package:flutter/foundation.dart';
import '../services/audio_service.dart';
import '../models/sound_pack.dart';
import '../models/motion_state.dart';

class AudioProvider extends ChangeNotifier {
  final AudioService _audioService = AudioService();

  // Settings
  bool _soundsEnabled = true;
  double _masterVolume = 0.8;
  String _currentSoundPackId = 'jedi';

  // Animation timing - MUST match LightsaberWidget animation durations
  static const _ignitionDuration = Duration(milliseconds: 1500);
  static const _retractDuration = Duration(milliseconds: 1000);

  // State tracking
  String? _lastBladeState;
  String? _targetBladeState; // Stato finale desiderato
  Timer? _animationTimer; // Timer per seguire l'animazione UI
  bool _isAnimating = false;

  // Getters
  bool get soundsEnabled => _soundsEnabled;
  double get masterVolume => _masterVolume;
  String get currentSoundPackId => _currentSoundPackId;
  List<SoundPack> get availableSoundPacks => SoundPackRegistry.availablePacks;
  SoundPack? get currentSoundPack => SoundPackRegistry.getPackById(_currentSoundPackId);
  bool get isAnimating => _isAnimating;

  AudioProvider() {
    _initialize();
  }

  Future<void> _initialize() async {
    await _audioService.setSoundPack(_currentSoundPackId);
  }

  // ══════════════════════════════════════════════════════════
  // SETTINGS
  // ══════════════════════════════════════════════════════════

  Future<void> setSoundsEnabled(bool enabled) async {
    _soundsEnabled = enabled;
    _audioService.setSoundsEnabled(enabled);

    if (!enabled && _audioService.isHumPlaying) {
      await _audioService.stopHum();
    }

    notifyListeners();
  }

  Future<void> setMasterVolume(double volume) async {
    _masterVolume = volume.clamp(0.0, 1.0);
    _audioService.setMasterVolume(_masterVolume);
    notifyListeners();
  }

  Future<void> setSoundPack(String packId) async {
    _currentSoundPackId = packId;
    await _audioService.setSoundPack(packId);
    notifyListeners();
  }

  // ══════════════════════════════════════════════════════════
  // BLADE STATE SYNC - Sincronizzazione IDENTICA all'animazione del widget
  // ══════════════════════════════════════════════════════════

  /// Sincronizza l'audio con lo stato della lama seguendo ESATTAMENTE il timing
  /// dell'animazione del LightsaberWidget (1500ms ignition, 1000ms retract)
  void syncWithBladeState(String? bladeState) {
    if (!_soundsEnabled) {
      print('[AudioProvider] ⚠️ Suoni disabilitati, ignoro sync');
      return;
    }

    if (bladeState == null) {
      bladeState = 'off';
    }

    final previousBladeState = _lastBladeState;
    final isFirstSync = previousBladeState == null;

    // Determina l'azione basandosi sulla transizione di stato
    // Logica IDENTICA a LightsaberWidget._updateAnimationState()
    _handleStateTransition(previousBladeState, bladeState, isFirstSync);

    _lastBladeState = bladeState;
  }

  void _handleStateTransition(String? from, String to, bool isFirstSync) {
    print('[AudioProvider] Transizione: $from -> $to ${_isAnimating ? "(animating)" : "(idle)"}');

    switch (to) {
      case 'igniting':
        // Analogo a: if (!_ignitionController.isAnimating && !_ignitionController.isCompleted)
        if (!_isAnimating && from != 'on') {
          _startIgnitionAnimation();
        } else {
          print('[AudioProvider] 🕒 Ignition ignorato: già in corso o completato');
        }
        break;

      case 'retracting':
        // Analogo a: if (!_ignitionController.isAnimating && !_ignitionController.isDismissed)
        if (!_isAnimating && from != 'off') {
          _startRetractAnimation();
        } else {
          print('[AudioProvider] 🕒 Retract ignorato: già in corso o completato');
        }
        break;

      case 'on':
        // Stato stabile 'on'
        if (from == 'igniting') {
          // Firmware dice che ignition è completata
          // Ma se stiamo ancora animando, ignoriamo (il timer gestirà)
          if (_isAnimating && _targetBladeState == 'on') {
            print('[AudioProvider] 🕒 On ricevuto ma ignition ancora in corso, continuo animazione');
          } else if (!_isAnimating && !_audioService.isHumPlaying) {
            // Animazione già finita ma hum non parte (edge case)
            print('[AudioProvider] ✅ On stabile → startHum diretto');
            _audioService.startHum();
          }
        } else if (from == 'off' || from == 'retracting' || from == null) {
          // Transizione diretta off->on (prima sincronizzazione o caso speciale)
          if (isFirstSync) {
            print('[AudioProvider] 🔁 Primo sync con lama ON → avvio hum senza ignition');
            _audioService.startHum();
          } else {
            // Avvia animazione ignition
            _startIgnitionAnimation();
          }
        }
        break;

      case 'off':
        // Stato stabile 'off'
        if (from == 'retracting') {
          // Firmware dice che retract è completata
          // Ma se stiamo ancora animando, ignoriamo (il timer gestirà)
          if (_isAnimating && _targetBladeState == 'off') {
            print('[AudioProvider] 🕒 Off ricevuto ma retract ancora in corso, continuo animazione');
          } else if (!_isAnimating && _audioService.isHumPlaying) {
            // Animazione già finita ma hum ancora attivo (edge case)
            print('[AudioProvider] ⭕ Off stabile → stopHum diretto');
            _audioService.stopHum();
          }
        } else if (from == 'on' || from == 'igniting') {
          // Transizione diretta on->off
          _startRetractAnimation();
        } else {
          // Già off
          print('[AudioProvider] ⭕ Off confermato');
          _ensureAudioStopped();
        }
        break;
    }
  }

  /// Avvia l'animazione di ignition con timing identico al widget (1500ms)
  void _startIgnitionAnimation() {
    print('[AudioProvider] 🔥 Ignition START (1500ms)');

    _isAnimating = true;
    _targetBladeState = 'on';
    _animationTimer?.cancel();

    // Audio: ignition sound + hum start immediato (come nel widget)
    _audioService.playIgnition();
    Future.delayed(const Duration(milliseconds: 50), () {
      if (_targetBladeState == 'on') {
        _audioService.startHum();
      }
    });

    // Timer che simula la durata dell'animazione del widget
    _animationTimer = Timer(_ignitionDuration, () {
      print('[AudioProvider] 🔥 Ignition COMPLETE');
      _isAnimating = false;
      // Verifica che lo stato finale sia ancora 'on'
      if (_targetBladeState == 'on' && !_audioService.isHumPlaying) {
        print('[AudioProvider] ⚠️ Hum non attivo a fine ignition, lo avvio');
        _audioService.startHum();
      }
    });
  }

  /// Avvia l'animazione di retract con timing identico al widget (1000ms)
  void _startRetractAnimation() {
    print('[AudioProvider] 🔴 Retract START (1000ms)');

    _isAnimating = true;
    _targetBladeState = 'off';
    _animationTimer?.cancel();

    // Audio: stop hum + retract sound (come nel widget)
    _audioService.stopHum();
    _audioService.playRetract();

    // Timer che simula la durata dell'animazione del widget
    _animationTimer = Timer(_retractDuration, () {
      print('[AudioProvider] 🔴 Retract COMPLETE');
      _isAnimating = false;
      // Assicura che tutto sia spento
      _ensureAudioStopped();
    });
  }

  /// Assicura che tutti i suoni continui siano fermati
  void _ensureAudioStopped() {
    if (_audioService.isHumPlaying) {
      print('[AudioProvider] 🛑 Force stop hum');
      _audioService.stopHum();
    }
  }

  // ══════════════════════════════════════════════════════════
  // GESTURE HANDLERS (clash dal firmware)
  // ══════════════════════════════════════════════════════════

  Future<void> handleGesture(String gesture) async {
    if (!_soundsEnabled) return;

    if (gesture.toLowerCase() == 'clash') {
      _audioService.playClash();
    }
  }

  // ══════════════════════════════════════════════════════════
  // MOTION-REACTIVE SWING (solo se lama accesa)
  // ══════════════════════════════════════════════════════════

  Future<void> updateSwing(MotionState state) async {
    if (!_soundsEnabled) return;

    // CRITICAL: Swing solo se la lama DOVREBBE essere accesa
    // Verifica sia stato target che stato corrente
    final shouldBeOn = _targetBladeState == 'on' || _lastBladeState == 'on';
    if (!shouldBeOn) {
      return;
    }

    // Controllo secondario: verifica che hum sia effettivamente attivo
    if (!_audioService.isHumPlaying) return;

    final grid = state.perturbationGrid ?? _generateGridFromIntensity(state.intensity);

    _audioService.playSwing(
      perturbationGrid: grid,
      speed: state.speed,
      direction: state.direction,
    );
  }

  List<int> _generateGridFromIntensity(double intensity) {
    final value = (intensity * 2.55).toInt().clamp(0, 255);
    return List<int>.filled(64, value);
  }

  @override
  void dispose() {
    _animationTimer?.cancel();
    _audioService.dispose();
    super.dispose();
  }
}
