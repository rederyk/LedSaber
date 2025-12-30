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

  // Event state tracking (per gestire precedenze e ducking)
  bool _ignitionPlaying = false;
  bool _retractPlaying = false;

  // Swing modulation tracking (per interpolazione smooth)
  double _currentPitch = 1.0;
  double _currentSwingVolume = 0.0;

  // Ducking configuration
  static const double _duckingVolume = 0.15;  // Volume ridotto durante eventi
  static const Duration _duckingFadeMs = Duration(milliseconds: 150);

  bool get isHumPlaying => _humPlaying;
  bool get isEventPlaying => _ignitionPlaying || _retractPlaying;

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
    if (_humPlaying) {
      // Applica ducking se c'è un evento in corso
      final targetVolume = isEventPlaying ? _masterVolume * _duckingVolume : _masterVolume;
      _humPlayer.setVolume(targetVolume);
    }
  }

  // ════════════════════════════════════════════════════════════
  // DUCKING - Abbassa hum durante eventi importanti
  // ════════════════════════════════════════════════════════════

  Future<void> _applyDucking() async {
    if (!_humPlaying) return;

    try {
      final duckVolume = _masterVolume * _duckingVolume;
      await _humPlayer.setVolume(duckVolume);
      print('[AudioService] 🔉 Ducking applicato: ${(duckVolume * 100).toInt()}%');
    } catch (e) {
      print('[AudioService] Errore ducking: $e');
    }
  }

  Future<void> _restoreDucking() async {
    if (!_humPlaying || isEventPlaying) return;

    try {
      await _humPlayer.setVolume(_masterVolume);
      print('[AudioService] 🔊 Volume hum ripristinato: ${(_masterVolume * 100).toInt()}%');

      // Ripristina anche swing se era attivo prima
      if (_swingPlaying) {
        // Il volume verrà impostato dalla prossima chiamata a playSwing
        print('[AudioService] 🔊 Swing riabilitato');
      }
    } catch (e) {
      print('[AudioService] Errore restore ducking: $e');
    }
  }

  // ════════════════════════════════════════════════════════════
  // BLADE STATE SOUNDS
  // ════════════════════════════════════════════════════════════

  Future<void> playIgnition() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      // PRIORITÀ: Ferma retract se in corso (ignition ha precedenza su retract incompleto)
      if (_retractPlaying) {
        await _retractPlayer.stop();
        _retractPlaying = false;
        print('[AudioService] ⚠️ Retract interrotto da Ignition');
      }

      // Imposta stato PRIMA di suonare (evita race condition)
      _ignitionPlaying = true;

      // Abbassa hum se già in esecuzione (ducking)
      await _applyDucking();

      // Suona ignition
      await _eventPlayer.setAsset(_currentPack!.ignitionPath);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      await _eventPlayer.play();

      // Calcola durata approssimativa dell'ignition (per ripristinare volume dopo)
      final duration = _eventPlayer.duration ?? const Duration(milliseconds: 1500);

      // Ripristina volume hum dopo la fine dell'ignition
      Future.delayed(duration + _duckingFadeMs, () {
        _ignitionPlaying = false;
        _restoreDucking();
      });

      print('[AudioService] 🔥 Ignition avviato (${duration.inMilliseconds}ms)');
    } catch (e) {
      print('[AudioService] Errore playIgnition: $e');
      _ignitionPlaying = false;
    }
  }

  Future<void> startHum() async {
    if (!_soundsEnabled || _currentPack == null || _humPlaying) return;

    try {
      await _humPlayer.setAsset(_currentPack!.humBasePath);

      // Reset pitch a normale all'avvio
      _currentPitch = 1.0;
      _currentSwingVolume = 0.0;
      await _humPlayer.setSpeed(1.0);

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

      // Pulisci anche gli stati degli eventi quando fermi tutto
      _ignitionPlaying = false;
      _retractPlaying = false;

      // Reset interpolazione per il prossimo avvio
      _currentPitch = 1.0;
      _currentSwingVolume = 0.0;
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
      // PRIORITÀ MASSIMA: Retract interrompe qualsiasi evento in corso
      if (_ignitionPlaying) {
        await _eventPlayer.stop();
        _ignitionPlaying = false;
        print('[AudioService] ⚠️ Ignition interrotto da Retract (priorità alta)');
      }

      // Imposta stato PRIMA di suonare
      _retractPlaying = true;

      // Abbassa hum se in esecuzione (ducking più aggressivo per retract)
      if (_humPlaying) {
        final retractDuckVolume = _masterVolume * (_duckingVolume * 0.5); // Ancora più basso
        await _humPlayer.setVolume(retractDuckVolume);
        print('[AudioService] 🔉 Ducking retract applicato: ${(retractDuckVolume * 100).toInt()}%');
      }

      // Abbassa anche swing se attivo
      if (_swingPlaying) {
        await _swingPlayer.setVolume(0.0);
      }

      // Suona retract
      await _retractPlayer.stop();
      await _retractPlayer.setAsset(_currentPack!.retractPath);
      await _retractPlayer.setVolume(_masterVolume);
      await _retractPlayer.seek(Duration.zero);
      await _retractPlayer.play();

      // Calcola durata retract
      final duration = _retractPlayer.duration ?? const Duration(milliseconds: 1500);

      // Ripristina volumi dopo retract (ma solo se hum ancora attivo)
      Future.delayed(duration + _duckingFadeMs, () {
        _retractPlaying = false;
        _restoreDucking();
      });

      print('[AudioService] 🔴 Retract avviato (${duration.inMilliseconds}ms)');
    } catch (e) {
      print('[AudioService] Errore playRetract: $e');
      _retractPlaying = false;
    }
  }

  Future<void> playClash() async {
    if (!_soundsEnabled || _currentPack == null) return;

    try {
      // PRIORITÀ: Clash NON interrompe ignition/retract (hanno priorità più alta)
      // ma può suonare in parallelo se _eventPlayer è libero

      // Se ignition è in corso, usa un approccio più delicato
      if (_ignitionPlaying) {
        print('[AudioService] ⚠️ Clash ritardato: ignition in corso');
        return; // Ignora clash durante ignition
      }

      // Retract ha priorità assoluta, non interrompere
      if (_retractPlaying) {
        print('[AudioService] ⚠️ Clash ignorato: retract in corso');
        return;
      }

      await _eventPlayer.setAsset(_currentPack!.clashPath);
      await _eventPlayer.setVolume(_masterVolume);
      await _eventPlayer.seek(Duration.zero);
      await _eventPlayer.play();

      print('[AudioService] ⚔️ Clash suonato');
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
      final targetVolume = _calculateVolumeFromGrid(perturbationGrid);
      final targetPitch = _calculatePitchFromSpeed(speed);

      // PRIORITÀ: Abbassa swing durante eventi importanti (ignition/retract)
      if (isEventPlaying) {
        return; // Non modificare l'hum durante eventi, rimane stabile
      }

      // Modula solo l'HUM player esistente invece di usare un player separato
      if (!_humPlaying) {
        return; // Se l'hum non sta suonando, non fare nulla
      }

      // Interpolazione smooth per evitare salti bruschi nel pitch
      final speedDiff = (targetPitch - _currentPitch).abs();

      // Se il cambio è molto piccolo, usa il valore target direttamente
      // Se è grande, interpola gradualmente (25% del delta per smoothness)
      if (speedDiff < 0.05) {
        _currentPitch = targetPitch;
      } else {
        _currentPitch += (targetPitch - _currentPitch) * 0.25;
      }

      // Interpolazione smooth anche per il volume
      final volumeDiff = (targetVolume - _currentSwingVolume).abs();
      if (volumeDiff < 0.05) {
        _currentSwingVolume = targetVolume;
      } else {
        _currentSwingVolume += (targetVolume - _currentSwingVolume) * 0.3;
      }

      // Applica modulazione all'HUM player
      await _humPlayer.setSpeed(_currentPitch.clamp(0.7, 1.5));

      // Volume basato su movimento (fade smooth)
      final baseVolume = _masterVolume * 0.6; // Volume base hum
      final swingBoost = _currentSwingVolume * _masterVolume * 0.4; // Boost da movimento
      await _humPlayer.setVolume(baseVolume + swingBoost);

    } catch (e) {
      print('[AudioService] Errore playSwing: $e');
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
