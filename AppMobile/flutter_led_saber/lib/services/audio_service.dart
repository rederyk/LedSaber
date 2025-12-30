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

  // Public getter per sapere se il loop è attivo
  bool get isHumPlaying => _humPlaying;

  // Swing debouncing
  DateTime? _lastSwingTime;
  static const Duration swingCooldown = Duration(milliseconds: 100);

  AudioService() {
    _initializePlayers();
  }

  /// Inizializza i player audio
  Future<void> _initializePlayers() async {
    _humPlayer = AudioPlayer();
    _swingPlayer = AudioPlayer();
    _eventPlayer = AudioPlayer();
    _retractPlayer = AudioPlayer();

    // Setup hum loop infinito
    _humPlayer.setLoopMode(LoopMode.one);
  }

  /// Imposta il sound pack corrente
  Future<void> setSoundPack(String packId) async {
    print('[AudioService] setSoundPack chiamato - packId: $packId');

    final pack = SoundPackRegistry.getPackById(packId);
    if (pack == null) {
      print('[AudioService] Sound pack non trovato: $packId');
      throw Exception('Sound pack non trovato: $packId');
    }

    _currentPack = pack;
    print('[AudioService] Sound pack caricato: ${pack.name} - path: ${pack.assetPath}');

    // Ricarica hum se già attivo
    if (_humPlaying) {
      print('[AudioService] Ricarico hum con nuovo sound pack...');
      await stopHum();
      await startHum();
    }
  }

  /// Abilita/disabilita suoni
  void setSoundsEnabled(bool enabled) {
    _soundsEnabled = enabled;
    if (!enabled) {
      stopHum();
    }
  }

  /// Imposta volume master
  void setMasterVolume(double volume) {
    _masterVolume = volume.clamp(0.0, 1.0);
    _updateHumVolume();
  }

  // ════════════════════════════════════════════════════════════
  // BLADE STATE SOUNDS
  // ════════════════════════════════════════════════════════════

  /// Suona accensione lama
  Future<void> playIgnition() async {
    print('[AudioService] playIgnition chiamato - enabled: $_soundsEnabled, pack: ${_currentPack?.id}');

    if (!_soundsEnabled) {
      print('[AudioService] playIgnition skipped - sounds disabled');
      return;
    }

    if (_currentPack == null) {
      print('[AudioService] playIgnition skipped - no sound pack loaded');
      return;
    }

    try {
      final path = _currentPack!.ignitionPath;
      print('[AudioService] playIgnition loading: $path');
      await _eventPlayer.setAsset(path);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      print('[AudioService] playIgnition playing...');
      await _eventPlayer.play();
      print('[AudioService] playIgnition started successfully');
    } catch (e) {
      print('[AudioService] Errore playIgnition: $e');
    }
  }

  /// Avvia hum continuo
  Future<void> startHum() async {
    print('[AudioService] startHum chiamato - enabled: $_soundsEnabled, pack: ${_currentPack?.id}, already playing: $_humPlaying');

    if (!_soundsEnabled) {
      print('[AudioService] startHum skipped - sounds disabled');
      return;
    }

    if (_currentPack == null) {
      print('[AudioService] startHum skipped - no sound pack loaded');
      return;
    }

    if (_humPlaying) {
      print('[AudioService] startHum skipped - already playing');
      return;
    }

    try {
      final path = _currentPack!.humBasePath;
      print('[AudioService] startHum loading: $path');
      await _humPlayer.setAsset(path);
      await _humPlayer.setVolume(_masterVolume);
      print('[AudioService] startHum playing loop...');
      await _humPlayer.play();
      _humPlaying = true;
      print('[AudioService] startHum started successfully - loop active');
    } catch (e) {
      print('[AudioService] Errore startHum: $e');
      _humPlaying = false;
    }
  }

  /// Ferma hum continuo
  Future<void> stopHum() async {
    print('[AudioService] stopHum chiamato - currently playing: $_humPlaying');

    if (!_humPlaying) {
      print('[AudioService] stopHum skipped - not playing');
      return;
    }

    try {
      await _humPlayer.stop();
      _humPlaying = false;
      print('[AudioService] stopHum - hum stopped successfully');
    } catch (e) {
      print('[AudioService] Errore stopHum: $e');
      _humPlaying = false;
    }
  }

  /// Suona spegnimento lama
  Future<void> playRetract() async {
    print('[AudioService] playRetract chiamato - enabled: $_soundsEnabled, pack: ${_currentPack?.id}');

    if (!_soundsEnabled || _currentPack == null) return;

    try {
      final path = _currentPack!.retractPath;
      print('[AudioService] playRetract loading: $path');

      // Usa player dedicato per retract (non condiviso con ignition)
      await _retractPlayer.stop(); // Ferma eventuali retract precedenti
      await _retractPlayer.setAsset(path);
      await _retractPlayer.setVolume(_masterVolume);
      await _retractPlayer.seek(Duration.zero);
      await _retractPlayer.play();
      print('[AudioService] playRetract started successfully');
    } catch (e) {
      print('[AudioService] Errore playRetract: $e');
    }
  }

  /// Suona colpo
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
  // SWING MODULATO (STESSA TRACCIA HUM DISTORTA)
  // ════════════════════════════════════════════════════════════

  /// Suona swing modulato
  /// IMPORTANTE: usa hum_base.wav con distorsione parametrica
  Future<void> playSwing({
    required List<int> perturbationGrid,
    required double speed,
    required String direction,
  }) async {
    if (!_soundsEnabled || _currentPack == null) return;

    // Debouncing per evitare spam
    final now = DateTime.now();
    if (_lastSwingTime != null &&
        now.difference(_lastSwingTime!) < swingCooldown) {
      return;
    }
    _lastSwingTime = now;

    try {
      // Calcola parametri modulazione
      final volume = _calculateVolumeFromGrid(perturbationGrid);
      final pitch = _calculatePitchFromSpeed(speed);
      // Note: Pan stereo non implementato (just_audio non supporta setBalance)
      // final pan = _calculatePanFromDirection(direction);

      // Skip se volume troppo basso
      if (volume < 0.05) return;

      // Setup swing player (STESSA TRACCIA HUM)
      await _swingPlayer.setAsset(_currentPack!.humBasePath);
      await _swingPlayer.setVolume(volume * _masterVolume);
      await _swingPlayer.setSpeed(pitch);  // Pitch shift

      // Play one-shot
      await _swingPlayer.seek(Duration.zero);
      await _swingPlayer.play();

    } catch (e) {
      print('[AudioService] Errore playSwing: $e');
    }
  }

  // ════════════════════════════════════════════════════════════
  // CALCOLI MODULAZIONE
  // ════════════════════════════════════════════════════════════

  double _calculateVolumeFromGrid(List<int> grid) {
    if (grid.isEmpty) return 0.0;

    // Somma energia totale griglia 8x8
    int totalEnergy = grid.reduce((a, b) => a + b);

    // Normalizza: max = 64 celle × 255 = 16320
    double normalized = totalEnergy / 16320.0;

    // Gamma correction (0.7) per boost valori medi
    double curved = pow(normalized, 0.7).toDouble();

    return curved.clamp(0.0, 1.0);
  }

  double _calculatePitchFromSpeed(double speed) {
    const double minPitch = 0.7;  // Lento = grave
    const double maxPitch = 1.5;  // Veloce = acuto

    // Mappa lineare da speed (0-20 px/frame)
    double normalized = (speed / 20.0).clamp(0.0, 1.0);

    return minPitch + (normalized * (maxPitch - minPitch));
  }

  void _updateHumVolume() {
    if (_humPlaying) {
      _humPlayer.setVolume(_masterVolume);
    }
  }

  /// Cleanup
  void dispose() {
    _humPlayer.dispose();
    _swingPlayer.dispose();
    _eventPlayer.dispose();
    _retractPlayer.dispose();
  }
}
