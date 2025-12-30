/// Definizione di un Sound Pack
class SoundPack {
  final String id;
  final String name;
  final String description;
  final String assetPath;       // Base path per gli asset
  final SoundPackType type;

  const SoundPack({
    required this.id,
    required this.name,
    required this.description,
    required this.assetPath,
    required this.type,
  });

  // Getters per file audio
  String get humBasePath => '$assetPath/hum_base.wav';
  String get ignitionPath => '$assetPath/ignition.wav';
  String get clashPath => '$assetPath/clash.wav';
  String get retractPath => '$assetPath/retract.wav';
}

enum SoundPackType {
  builtin,    // Predefiniti (jedi, sith)
  custom,     // Caricati dall'utente
}

/// Registry di tutti i sound pack disponibili
class SoundPackRegistry {
  static const List<SoundPack> availablePacks = [
    SoundPack(
      id: 'jedi',
      name: 'Jedi Classic',
      description: 'Suono tradizionale della lama Jedi',
      assetPath: 'assets/sounds/jedi',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'sith',
      name: 'Sith Dark Side',
      description: 'Suono più grave e minaccioso',
      assetPath: 'assets/sounds/sith',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'kyber_crystal',
      name: 'Kyber Crystal',
      description: 'Suono cristallino e puro',
      assetPath: 'assets/sounds/crystal',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'unstable',
      name: 'Unstable Blade',
      description: 'Stile Kylo Ren - crackling instabile',
      assetPath: 'assets/sounds/unstable',
      type: SoundPackType.builtin,
    ),
    SoundPack(
      id: 'custom_1',
      name: 'Custom Pack 1',
      description: 'Suoni personalizzati',
      assetPath: 'assets/sounds/custom_1',
      type: SoundPackType.custom,
    ),
  ];

  static SoundPack? getPackById(String id) {
    try {
      return availablePacks.firstWhere((pack) => pack.id == id);
    } catch (e) {
      return null;
    }
  }
}
