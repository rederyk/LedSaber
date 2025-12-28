import 'package:flutter/material.dart';

/// Preset colore personalizzato salvato dall'utente
class ColorPreset {
  final String id; // UUID univoco
  final String name; // Nome dato dall'utente
  final Color color; // Colore RGB
  final String effectId; // ID effetto associato
  final DateTime createdAt; // Data creazione

  const ColorPreset({
    required this.id,
    required this.name,
    required this.color,
    required this.effectId,
    required this.createdAt,
  });

  /// Crea un preset da JSON
  factory ColorPreset.fromJson(Map<String, dynamic> json) {
    return ColorPreset(
      id: json['id'] as String,
      name: json['name'] as String,
      color: Color(json['color'] as int),
      effectId: json['effectId'] as String,
      createdAt: DateTime.parse(json['createdAt'] as String),
    );
  }

  /// Converte il preset in JSON
  Map<String, dynamic> toJson() {
    final a = (color.a * 255).round();
    final r = (color.r * 255).round();
    final g = (color.g * 255).round();
    final b = (color.b * 255).round();

    return {
      'id': id,
      'name': name,
      'color': (a << 24) | (r << 16) | (g << 8) | b,
      'effectId': effectId,
      'createdAt': createdAt.toIso8601String(),
    };
  }

  /// Crea copia con modifiche
  ColorPreset copyWith({
    String? id,
    String? name,
    Color? color,
    String? effectId,
    DateTime? createdAt,
  }) {
    return ColorPreset(
      id: id ?? this.id,
      name: name ?? this.name,
      color: color ?? this.color,
      effectId: effectId ?? this.effectId,
      createdAt: createdAt ?? this.createdAt,
    );
  }

  @override
  String toString() {
    return 'ColorPreset(id: $id, name: $name, color: $color, effectId: $effectId)';
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is ColorPreset && other.id == id;
  }

  @override
  int get hashCode => id.hashCode;
}
