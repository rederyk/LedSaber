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

  // CRITICAL: Source of truth - stato desiderato dalla UI
  String? _desiredBladeState;

  // Getters
  bool get soundsEnabled => _soundsEnabled;
  double get masterVolume => _masterVolume;
  String get currentSoundPackId => _currentSoundPackId;
  List<SoundPack> get availableSoundPacks => SoundPackRegistry.availablePacks;
  SoundPack? get currentSoundPack => SoundPackRegistry.getPackById(_currentSoundPackId);

  // Getter per verificare se la lama DOVREBBE essere spenta secondo UI
  bool get _shouldBeOff => _desiredBladeState == 'off' || _desiredBladeState == null;

  AudioProvider() {
    _initialize();
    _startSyncWatchdog();
  }

  Future<void> _initialize() async {
    await _audioService.setSoundPack(_currentSoundPackId);
  }

  /// Watchdog che verifica periodicamente la sincronizzazione audio-UI
  void _startSyncWatchdog() {
    // Controlla ogni 500ms se c'è desincronizzazione
    Stream.periodic(const Duration(milliseconds: 500)).listen((_) {
      if (!_soundsEnabled) return;

      // Se UI dice OFF ma audio sta suonando -> ERRORE CRITICO
      if (_shouldBeOff && _audioService.isHumPlaying) {
        print('[AudioProvider] 🚨 WATCHDOG: Desincronizzazione rilevata! UI=OFF ma Hum=ON');
        print('[AudioProvider] 🔧 WATCHDOG: Correggo forzando stopHum()');
        _audioService.stopHum();
      }

      // Se UI dice ON ma audio non suona -> Avviso (può essere normale se in pausa)
      if (!_shouldBeOff && !_audioService.isHumPlaying) {
        // Questo è normale durante ignition o se appena acceso, quindi solo log debug
        // print('[AudioProvider] ℹ️ WATCHDOG: UI=ON ma Hum=OFF (normale durante transizioni)');
      }
    });
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
    // CRITICAL: Salva SEMPRE lo stato desiderato dalla UI
    _desiredBladeState = bladeState;

    print('[AudioProvider] 🎯 UI richiede bladeState: $bladeState');

    if (!_soundsEnabled) {
      print('[AudioProvider] ⚠️ Suoni disabilitati, ignoro sync');
      return;
    }

    if (bladeState == null) {
      print('[AudioProvider] ⚠️ bladeState null, assumo OFF');
      _audioService.stopHum();
      return;
    }

    switch (bladeState) {
      case 'igniting':
        // Lama si accende -> suona ignition + avvia loop
        print('[AudioProvider] 🔥 Igniting -> playIgnition + startHum');
        _audioService.playIgnition();
        Future.delayed(const Duration(milliseconds: 50), () {
          // Controllo di sicurezza: verifica che la UI non abbia cambiato idea
          if (_desiredBladeState == 'igniting' || _desiredBladeState == 'on') {
            _audioService.startHum();
          } else {
            print('[AudioProvider] ⚠️ StartHum annullato: UI cambiata a $_desiredBladeState');
          }
        });
        break;

      case 'on':
        // Lama accesa -> assicura loop attivo
        print('[AudioProvider] ✅ On -> startHum');
        _audioService.startHum();
        break;

      case 'retracting':
        // Lama si spegne -> ferma tutto + suona retract
        print('[AudioProvider] 🔴 Retracting -> stopHum + playRetract');
        // CRITICAL: stopHum DEVE essere chiamato SEMPRE quando UI dice retracting
        _audioService.stopHum();
        _audioService.playRetract();
        break;

      case 'off':
        // Lama spenta -> ferma tutto IMMEDIATAMENTE
        print('[AudioProvider] ⭕ Off -> stopHum FORZATO');
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
    if (!_soundsEnabled) return;

    // CRITICAL: Controllo di sicurezza PRIORITARIO basato su UI
    // Se la UI dice che la lama è spenta, NON permettere swing
    if (_shouldBeOff) {
      // Se hum sta ancora suonando ma UI dice OFF, fermalo immediatamente
      if (_audioService.isHumPlaying) {
        print('[AudioProvider] 🚨 SICUREZZA: Hum attivo ma UI dice OFF! Fermo tutto.');
        await _audioService.stopHum();
      }
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
    _audioService.dispose();
    super.dispose();
  }
}
