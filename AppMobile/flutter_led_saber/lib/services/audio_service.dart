import 'dart:math';
import 'package:just_audio/just_audio.dart';
import '../models/sound_pack.dart';

class AudioService {
  // Audio players pool
  late AudioPlayer _humPlayer;       // Loop continuo
  late AudioPlayer _swingPlayer;     // Swing modulato (stessa traccia hum)
  late AudioPlayer _eventPlayer;     // One-shot events (ignition, clash)
  late AudioPlayer _retractPlayer;   // Player dedicato per retract (priorità alta)

  // State
  SoundPack? _currentPack;
  bool _soundsEnabled = true;
  double _masterVolume = 0.8;
  bool _humPlaying = false;
  bool _swingPlaying = false;

  bool get isHumPlaying => _humPlaying;

  AudioService() {
    _initializePlayers();
  }

  Future<void> _initializePlayers() async {
    _humPlayer = AudioPlayer();
    _swingPlayer = AudioPlayer();
    _eventPlayer = AudioPlayer();
    _retractPlayer = AudioPlayer();

    _humPlayer.setLoopMode(LoopMode.one);
    _swingPlayer.setLoopMode(LoopMode.one);
  }

  Future<void> setSoundPack(String packId) async {
    final pack = SoundPackRegistry.getPackById(packId);
    if (pack == null) throw Exception('Sound pack non trovato: $packId');

    _currentPack = pack;

    if (_humPlaying) {
      await stopHum();
      await startHum();
    }
  }

  void setSoundsEnabled(bool enabled) {
    _soundsEnabled = enabled;
    if (!enabled) stopHum();
  }

  void setMasterVolume(double volume) {
    _masterVolume = volume.clamp(0.0, 1.0);
    if (_humPlaying) _humPlayer.setVolume(_masterVolume);
  }

  // ════════════════════════════════════════════════════════════
  // BLADE STATE SOUNDS
  // ════════════════════════════════════════════════════════════

  Future<void> playIgnition() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      await _eventPlayer.setAsset(_currentPack!.ignitionPath);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      await _eventPlayer.play();
    } catch (e) {
      print('[AudioService] Errore playIgnition: $e');
    }
  }

  Future<void> startHum() async {
    if (!_soundsEnabled || _currentPack == null || _humPlaying) return;

    try {
      await _humPlayer.setAsset(_currentPack!.humBasePath);
      await _humPlayer.setVolume(_masterVolume);
      await _humPlayer.play();
      _humPlaying = true;
    } catch (e) {
      print('[AudioService] Errore startHum: $e');
      _humPlaying = false;
    }
  }

  Future<void> stopHum() async {
    if (!_humPlaying) return;

    // CRITICO: Imposta SUBITO lo stato a false per evitare race condition
    _humPlaying = false;

    try {
      await _humPlayer.stop();
      await stopSwing();
    } catch (e) {
      print('[AudioService] Errore stopHum: $e');
    }
  }

  Future<void> stopSwing() async {
    if (!_swingPlaying) return;

    _swingPlaying = false;
    try {
      await _swingPlayer.stop();
    } catch (e) {
      print('[AudioService] Errore stopSwing: $e');
    }
  }

  Future<void> playRetract() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      await _retractPlayer.stop();
      await _retractPlayer.setAsset(_currentPack!.retractPath);
      await _retractPlayer.setVolume(_masterVolume);
      await _retractPlayer.seek(Duration.zero);
      await _retractPlayer.play();
    } catch (e) {
      print('[AudioService] Errore playRetract: $e');
    }
  }

  Future<void> playClash() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      await _eventPlayer.setAsset(_currentPack!.clashPath);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      await _eventPlayer.play();
    } catch (e) {
      print('[AudioService] Errore playClash: $e');
    }
  }

  // ════════════════════════════════════════════════════════════
  // SWING MODULATO (EFFETTO THEREMIN)
  // ════════════════════════════════════════════════════════════

  Future<void> playSwing({
    required List<int> perturbationGrid,
    required double speed,
    required String direction,
  }) async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      final volume = _calculateVolumeFromGrid(perturbationGrid);
      final pitch = _calculatePitchFromSpeed(speed);

      if (volume < 0.05) {
        if (_swingPlaying) {
          _swingPlaying = false;
          await _swingPlayer.stop();
        }
        return;
      }

      if (!_swingPlaying) {
        await _swingPlayer.setAsset(_currentPack!.humBasePath);
        await _swingPlayer.play();
        _swingPlaying = true;
      }

      await _swingPlayer.setVolume(volume * _masterVolume);
      await _swingPlayer.setSpeed(pitch);
    } catch (e) {
      print('[AudioService] Errore playSwing: $e');
      _swingPlaying = false;
    }
  }

  // ════════════════════════════════════════════════════════════
  // CALCOLI MODULAZIONE
  // ════════════════════════════════════════════════════════════

  double _calculateVolumeFromGrid(List<int> grid) {
    if (grid.isEmpty) return 0.0;
    int totalEnergy = grid.reduce((a, b) => a + b);
    double normalized = totalEnergy / 16320.0;
    double curved = pow(normalized, 0.7).toDouble();
    return curved.clamp(0.0, 1.0);
  }

  double _calculatePitchFromSpeed(double speed) {
    const double minPitch = 0.7;
    const double maxPitch = 1.5;
    double normalized = (speed / 20.0).clamp(0.0, 1.0);
    return minPitch + (normalized * (maxPitch - minPitch));
  }

  void dispose() {
    _humPlayer.dispose();
    _swingPlayer.dispose();
    _eventPlayer.dispose();
    _retractPlayer.dispose();
  }
}
