/// Modello stato Motion Detection sincronizzato con il firmware
/// Corrisponde al JSON ricevuto dalla characteristic Motion Status
class MotionState {
  final bool enabled;
  final bool motionDetected;
  final double intensity;
  final double speed;
  final String direction; // "UP", "DOWN", "LEFT", "RIGHT", "NONE"
  final int confidence;

  // Gesture info
  final String lastGesture; // "IGNITION", "RETRACT", "CLASH", "NONE"
  final int gestureConfidence;
  final int lastGestureTime;

  // Metrics
  final int totalFrames;
  final int fps;

  MotionState({
    required this.enabled,
    required this.motionDetected,
    required this.intensity,
    required this.speed,
    required this.direction,
    required this.confidence,
    required this.lastGesture,
    required this.gestureConfidence,
    required this.lastGestureTime,
    required this.totalFrames,
    required this.fps,
  });

  /// Crea un MotionState da JSON ricevuto via BLE
  factory MotionState.fromJson(Map<String, dynamic> json) {
    return MotionState(
      enabled: json['enabled'] ?? false,
      motionDetected: json['motionDetected'] ?? false,
      intensity: (json['intensity'] ?? 0.0).toDouble(),
      speed: (json['speed'] ?? 0.0).toDouble(),
      direction: json['direction'] ?? 'NONE',
      confidence: json['confidence'] ?? 0,
      lastGesture: json['lastGesture'] ?? 'NONE',
      gestureConfidence: json['gestureConfidence'] ?? 0,
      lastGestureTime: json['lastGestureTime'] ?? 0,
      totalFrames: json['totalFrames'] ?? 0,
      fps: json['fps'] ?? 0,
    );
  }

  /// Converte il MotionState in JSON
  Map<String, dynamic> toJson() {
    return {
      'enabled': enabled,
      'motionDetected': motionDetected,
      'intensity': intensity,
      'speed': speed,
      'direction': direction,
      'confidence': confidence,
      'lastGesture': lastGesture,
      'gestureConfidence': gestureConfidence,
      'lastGestureTime': lastGestureTime,
      'totalFrames': totalFrames,
      'fps': fps,
    };
  }

  @override
  String toString() {
    return 'MotionState(enabled: $enabled, motionDetected: $motionDetected, '
        'intensity: $intensity, speed: $speed, direction: $direction, '
        'lastGesture: $lastGesture)';
  }
}

/// Modello configurazione Motion Detection
class MotionConfig {
  final bool enabled;
  final bool gesturesEnabled;
  final int quality;
  final int motionIntensityMin;
  final double motionSpeedMin;
  final int gestureIgnitionIntensity;
  final int gestureRetractIntensity;
  final int gestureClashIntensity;
  final String effectMapUp;
  final String effectMapDown;
  final String effectMapLeft;
  final String effectMapRight;
  final bool debugLogs;

  MotionConfig({
    required this.enabled,
    required this.gesturesEnabled,
    required this.quality,
    required this.motionIntensityMin,
    required this.motionSpeedMin,
    required this.gestureIgnitionIntensity,
    required this.gestureRetractIntensity,
    required this.gestureClashIntensity,
    required this.effectMapUp,
    required this.effectMapDown,
    required this.effectMapLeft,
    required this.effectMapRight,
    required this.debugLogs,
  });

  /// Crea un MotionConfig da JSON ricevuto via BLE
  factory MotionConfig.fromJson(Map<String, dynamic> json) {
    return MotionConfig(
      enabled: json['enabled'] ?? false,
      gesturesEnabled: json['gesturesEnabled'] ?? true,
      quality: json['quality'] ?? 160,
      motionIntensityMin: json['motionIntensityMin'] ?? 6,
      motionSpeedMin: (json['motionSpeedMin'] ?? 0.4).toDouble(),
      gestureIgnitionIntensity: json['gestureIgnitionIntensity'] ?? 15,
      gestureRetractIntensity: json['gestureRetractIntensity'] ?? 15,
      gestureClashIntensity: json['gestureClashIntensity'] ?? 15,
      effectMapUp: json['effectMapUp'] ?? '',
      effectMapDown: json['effectMapDown'] ?? '',
      effectMapLeft: json['effectMapLeft'] ?? '',
      effectMapRight: json['effectMapRight'] ?? '',
      debugLogs: json['debugLogs'] ?? false,
    );
  }

  /// Converte il MotionConfig in JSON per invio via BLE
  Map<String, dynamic> toJson() {
    return {
      'enabled': enabled,
      'gesturesEnabled': gesturesEnabled,
      'quality': quality,
      'motionIntensityMin': motionIntensityMin,
      'motionSpeedMin': motionSpeedMin,
      'gestureIgnitionIntensity': gestureIgnitionIntensity,
      'gestureRetractIntensity': gestureRetractIntensity,
      'gestureClashIntensity': gestureClashIntensity,
      'effectMapUp': effectMapUp,
      'effectMapDown': effectMapDown,
      'effectMapLeft': effectMapLeft,
      'effectMapRight': effectMapRight,
      'debugLogs': debugLogs,
    };
  }

  /// Copia del MotionConfig con modifiche
  MotionConfig copyWith({
    bool? enabled,
    bool? gesturesEnabled,
    int? quality,
    int? motionIntensityMin,
    double? motionSpeedMin,
    int? gestureIgnitionIntensity,
    int? gestureRetractIntensity,
    int? gestureClashIntensity,
    String? effectMapUp,
    String? effectMapDown,
    String? effectMapLeft,
    String? effectMapRight,
    bool? debugLogs,
  }) {
    return MotionConfig(
      enabled: enabled ?? this.enabled,
      gesturesEnabled: gesturesEnabled ?? this.gesturesEnabled,
      quality: quality ?? this.quality,
      motionIntensityMin: motionIntensityMin ?? this.motionIntensityMin,
      motionSpeedMin: motionSpeedMin ?? this.motionSpeedMin,
      gestureIgnitionIntensity: gestureIgnitionIntensity ?? this.gestureIgnitionIntensity,
      gestureRetractIntensity: gestureRetractIntensity ?? this.gestureRetractIntensity,
      gestureClashIntensity: gestureClashIntensity ?? this.gestureClashIntensity,
      effectMapUp: effectMapUp ?? this.effectMapUp,
      effectMapDown: effectMapDown ?? this.effectMapDown,
      effectMapLeft: effectMapLeft ?? this.effectMapLeft,
      effectMapRight: effectMapRight ?? this.effectMapRight,
      debugLogs: debugLogs ?? this.debugLogs,
    );
  }

  @override
  String toString() {
    return 'MotionConfig(enabled: $enabled, gesturesEnabled: $gesturesEnabled, '
        'quality: $quality, motionIntensityMin: $motionIntensityMin, '
        'motionSpeedMin: $motionSpeedMin)';
  }
}

/// Modello evento Motion
class MotionEvent {
  final String eventType; // "motion_started", "motion_ended", "shake_detected", "still_detected"
  final String? gesture; // "IGNITION", "RETRACT", "CLASH"
  final int? gestureConfidence;
  final int timestamp;

  MotionEvent({
    required this.eventType,
    this.gesture,
    this.gestureConfidence,
    required this.timestamp,
  });

  /// Crea un MotionEvent da JSON ricevuto via BLE
  factory MotionEvent.fromJson(Map<String, dynamic> json) {
    return MotionEvent(
      eventType: json['eventType'] ?? '',
      gesture: json['gesture'],
      gestureConfidence: json['gestureConfidence'],
      timestamp: json['timestamp'] ?? DateTime.now().millisecondsSinceEpoch,
    );
  }

  @override
  String toString() {
    return 'MotionEvent(type: $eventType, gesture: $gesture, '
        'confidence: $gestureConfidence)';
  }
}
