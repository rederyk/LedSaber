/// Modello stato LED sincronizzato con il firmware
/// Corrisponde al JSON ricevuto dalla characteristic LED State
class LedState {
  final int r;
  final int g;
  final int b;
  final int brightness;
  final String effect;
  final int speed;
  final bool enabled;
  final String bladeState; // "off" | "igniting" | "on" | "retracting"
  final bool statusLedEnabled;
  final int statusLedBrightness;

  // Chrono themes
  final int chronoHourTheme;
  final int chronoSecondTheme;

  // Wellness mode
  final bool chronoWellnessMode;
  final int breathingRate;

  LedState({
    required this.r,
    required this.g,
    required this.b,
    required this.brightness,
    required this.effect,
    required this.speed,
    required this.enabled,
    required this.bladeState,
    required this.statusLedEnabled,
    required this.statusLedBrightness,
    required this.chronoHourTheme,
    required this.chronoSecondTheme,
    required this.chronoWellnessMode,
    required this.breathingRate,
  });

  /// Crea un LedState da JSON ricevuto via BLE
  factory LedState.fromJson(Map<String, dynamic> json) {
    return LedState(
      r: json['r'] ?? 255,
      g: json['g'] ?? 0,
      b: json['b'] ?? 0,
      brightness: json['brightness'] ?? 255,
      effect: json['effect'] ?? 'solid',
      speed: json['speed'] ?? 50,
      enabled: json['enabled'] ?? false,
      bladeState: json['bladeState'] ?? 'off',
      statusLedEnabled: json['statusLedEnabled'] ?? true,
      statusLedBrightness: json['statusLedBrightness'] ?? 32,
      chronoHourTheme: json['chronoHourTheme'] ?? 0,
      chronoSecondTheme: json['chronoSecondTheme'] ?? 0,
      chronoWellnessMode: json['chronoWellnessMode'] ?? false,
      breathingRate: json['breathingRate'] ?? 5,
    );
  }

  /// Converte il LedState in JSON
  Map<String, dynamic> toJson() {
    return {
      'r': r,
      'g': g,
      'b': b,
      'brightness': brightness,
      'effect': effect,
      'speed': speed,
      'enabled': enabled,
      'bladeState': bladeState,
      'statusLedEnabled': statusLedEnabled,
      'statusLedBrightness': statusLedBrightness,
      'chronoHourTheme': chronoHourTheme,
      'chronoSecondTheme': chronoSecondTheme,
      'chronoWellnessMode': chronoWellnessMode,
      'breathingRate': breathingRate,
    };
  }

  /// Copia del LedState con modifiche
  LedState copyWith({
    int? r,
    int? g,
    int? b,
    int? brightness,
    String? effect,
    int? speed,
    bool? enabled,
    String? bladeState,
    bool? statusLedEnabled,
    int? statusLedBrightness,
    int? chronoHourTheme,
    int? chronoSecondTheme,
    bool? chronoWellnessMode,
    int? breathingRate,
  }) {
    return LedState(
      r: r ?? this.r,
      g: g ?? this.g,
      b: b ?? this.b,
      brightness: brightness ?? this.brightness,
      effect: effect ?? this.effect,
      speed: speed ?? this.speed,
      enabled: enabled ?? this.enabled,
      bladeState: bladeState ?? this.bladeState,
      statusLedEnabled: statusLedEnabled ?? this.statusLedEnabled,
      statusLedBrightness: statusLedBrightness ?? this.statusLedBrightness,
      chronoHourTheme: chronoHourTheme ?? this.chronoHourTheme,
      chronoSecondTheme: chronoSecondTheme ?? this.chronoSecondTheme,
      chronoWellnessMode: chronoWellnessMode ?? this.chronoWellnessMode,
      breathingRate: breathingRate ?? this.breathingRate,
    );
  }

  @override
  String toString() {
    return 'LedState(r: $r, g: $g, b: $b, brightness: $brightness, '
        'effect: $effect, speed: $speed, enabled: $enabled, '
        'bladeState: $bladeState)';
  }
}
