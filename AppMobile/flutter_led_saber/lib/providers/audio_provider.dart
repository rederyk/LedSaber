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

  // Swing parameters
  double _swingMinIntensity = 10.0;

  // Tracking dello stato della lama per gestire l'audio
  String? _lastBladeState;

  // Getters
  bool get soundsEnabled => _soundsEnabled;
  double get masterVolume => _masterVolume;
  String get currentSoundPackId => _currentSoundPackId;
  List<SoundPack> get availableSoundPacks => SoundPackRegistry.availablePacks;
  SoundPack? get currentSoundPack => SoundPackRegistry.getPackById(_currentSoundPackId);

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

  void setSwingMinIntensity(double intensity) {
    _swingMinIntensity = intensity.clamp(0.0, 255.0);
    notifyListeners();
  }

  // ══════════════════════════════════════════════════════════
  // BLADE STATE SYNC - L'audio segue lo stato della lama
  // ══════════════════════════════════════════════════════════

  /// Sincronizza l'audio con lo stato della lama
  /// Questo metodo viene chiamato ogni volta che bladeState cambia
  Future<void> syncWithBladeState(String? bladeState) async {
    if (!_soundsEnabled || bladeState == null) return;

    // Se lo stato non è cambiato, non fare nulla
    if (bladeState == _lastBladeState) return;

    print('[AudioProvider] bladeState cambiato: $_lastBladeState -> $bladeState');

    switch (bladeState) {
      case 'igniting':
        // Lama sta accendendosi - suona ignition
        if (_lastBladeState == 'off' || _lastBladeState == null) {
          print('[AudioProvider] Avvio ignition audio');
          _audioService.playIgnition();

          // Aspetta un attimo poi avvia il loop
          await Future.delayed(const Duration(milliseconds: 50));
          _audioService.startHum();
        }
        break;

      case 'on':
        // Lama accesa - assicurati che il loop sia attivo
        if (!_audioService.isHumPlaying) {
          print('[AudioProvider] Loop hum non attivo, lo avvio');
          _audioService.startHum();
        }
        break;

      case 'retracting':
        // Lama sta spegnendosi - suona retract e ferma il loop
        if (_lastBladeState == 'on' || _lastBladeState == 'igniting') {
          print('[AudioProvider] Avvio retract audio');
          _audioService.playRetract();

          // Aspetta un attimo per far partire il retract, poi ferma il loop
          await Future.delayed(const Duration(milliseconds: 100));
          await _audioService.stopHum();
        }
        break;

      case 'off':
        // Lama spenta - assicurati che il loop sia fermato
        if (_audioService.isHumPlaying) {
          print('[AudioProvider] Lama spenta ma loop ancora attivo - lo fermo');
          await _audioService.stopHum();
        }
        break;
    }

    _lastBladeState = bladeState;
  }

  // ══════════════════════════════════════════════════════════
  // GESTURE HANDLERS (solo per clash dal firmware)
  // ══════════════════════════════════════════════════════════

  Future<void> handleGesture(String gesture) async {
    if (!_soundsEnabled) return;

    final gestureType = gesture.toLowerCase();

    // Gestisci solo clash - ignition/retract sono ora gestiti da syncWithBladeState
    if (gestureType == 'clash') {
      print('[AudioProvider] Clash gesture ricevuto');
      _audioService.playClash();
    }
  }

  // ══════════════════════════════════════════════════════════
  // MOTION-REACTIVE SWING
  // ══════════════════════════════════════════════════════════

  Future<void> updateSwing(MotionState state) async {
    // Usa lo stato effettivo del loop hum invece di _bladeOn
    // Questo è più affidabile perché riflette lo stato reale dell'audio
    if (!_soundsEnabled || !_audioService.isHumPlaying) return;

    // Solo se c'è movimento sopra threshold
    if (!state.motionDetected || state.intensity < _swingMinIntensity) {
      return;
    }

    // Usa perturbationGrid se disponibile, altrimenti fallback a griglia generata da intensity
    final grid = state.perturbationGrid ?? _generateGridFromIntensity(state.intensity);

    // Non await - i swing devono essere reattivi e non bloccare
    _audioService.playSwing(
      perturbationGrid: grid,
      speed: state.speed,
      direction: state.direction,
    );
  }

  /// Genera griglia 8x8 di fallback basata su intensity
  /// Usato quando firmware non invia perturbationGrid
  List<int> _generateGridFromIntensity(double intensity) {
    // Crea griglia uniforme con valore proporzionale a intensity
    final value = (intensity * 2.55).toInt().clamp(0, 255);
    return List<int>.filled(64, value);
  }

  @override
  void dispose() {
    _audioService.dispose();
    super.dispose();
  }
}
