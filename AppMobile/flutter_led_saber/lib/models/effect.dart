/// Modello per un singolo effetto LED
class Effect {
  final String id;
  final String name;
  final List<String> params; // es. ["speed", "color"]
  final Map<String, List<String>>? themes; // Per chrono_hybrid

  Effect({
    required this.id,
    required this.name,
    required this.params,
    this.themes,
  });

  /// Crea un Effect da JSON ricevuto via BLE
  factory Effect.fromJson(Map<String, dynamic> json) {
    return Effect(
      id: json['id'] as String,
      name: json['name'] as String,
      params: (json['params'] as List<dynamic>?)
              ?.map((e) => e.toString())
              .toList() ??
          [],
      themes: json['themes'] != null
          ? (json['themes'] as Map<String, dynamic>).map(
              (k, v) => MapEntry(
                k,
                (v as List<dynamic>).map((e) => e.toString()).toList(),
              ),
            )
          : null,
    );
  }

  /// Converte l'Effect in JSON
  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'name': name,
      'params': params,
      if (themes != null) 'themes': themes,
    };
  }

  /// Verifica se l'effetto ha un certo parametro
  bool hasParam(String param) => params.contains(param);

  /// Verifica se è l'effetto chrono con temi
  bool get isChrono => id == 'chrono_hybrid' && themes != null;

  @override
  String toString() {
    return 'Effect(id: $id, name: $name, params: $params, themes: $themes)';
  }
}

/// Lista completa degli effetti disponibili
class EffectsList {
  final String version;
  final List<Effect> effects;

  EffectsList({
    required this.version,
    required this.effects,
  });

  /// Crea una EffectsList da JSON ricevuto via BLE
  factory EffectsList.fromJson(Map<String, dynamic> json) {
    return EffectsList(
      version: json['version'] as String,
      effects: (json['effects'] as List<dynamic>)
          .map((e) => Effect.fromJson(e as Map<String, dynamic>))
          .toList(),
    );
  }

  /// Converte l'EffectsList in JSON
  Map<String, dynamic> toJson() {
    return {
      'version': version,
      'effects': effects.map((e) => e.toJson()).toList(),
    };
  }

  /// Trova un effetto per ID
  Effect? findById(String id) {
    try {
      return effects.firstWhere((e) => e.id == id);
    } catch (_) {
      return null;
    }
  }

  @override
  String toString() {
    return 'EffectsList(version: $version, effects: ${effects.length} items)';
  }
}
