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

  // ══════════════════════════════════════════════════════════
  // BLADE STATE SYNC - L'audio segue ESATTAMENTE lo stato della lama
  // ══════════════════════════════════════════════════════════

  /// Sincronizza l'audio con lo stato della lama (chiamato dalla UI)
  void syncWithBladeState(String? bladeState) {
    if (!_soundsEnabled || bladeState == null) return;

    switch (bladeState) {
      case 'igniting':
        // Lama si accende -> suona ignition + avvia loop
        _audioService.playIgnition();
        Future.delayed(const Duration(milliseconds: 50), () {
          _audioService.startHum();
        });
        break;

      case 'on':
        // Lama accesa -> assicura loop attivo
        _audioService.startHum();
        break;

      case 'retracting':
        // Lama si spegne -> ferma tutto + suona retract
        _audioService.stopHum();
        _audioService.playRetract();
        break;

      case 'off':
        // Lama spenta -> ferma tutto
        _audioService.stopHum();
        break;
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
    if (!_soundsEnabled || !_audioService.isHumPlaying) return;

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
    _audioService.dispose();
    super.dispose();
  }
}
