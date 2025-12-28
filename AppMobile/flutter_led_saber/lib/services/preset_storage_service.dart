import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/color_preset.dart';

/// Servizio per gestire il salvataggio/caricamento dei preset personalizzati
class PresetStorageService {
  static const String _presetsKey = 'custom_color_presets';

  /// Carica tutti i preset salvati
  static Future<List<ColorPreset>> loadPresets() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final presetsJson = prefs.getString(_presetsKey);

      if (presetsJson == null || presetsJson.isEmpty) {
        return [];
      }

      final List<dynamic> decodedList = jsonDecode(presetsJson);
      return decodedList
          .map((json) => ColorPreset.fromJson(json as Map<String, dynamic>))
          .toList();
    } catch (e) {
      print('[PresetStorage] Errore caricamento preset: $e');
      return [];
    }
  }

  /// Salva la lista completa di preset
  static Future<bool> savePresets(List<ColorPreset> presets) async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final presetsJson = jsonEncode(presets.map((p) => p.toJson()).toList());
      return await prefs.setString(_presetsKey, presetsJson);
    } catch (e) {
      print('[PresetStorage] Errore salvataggio preset: $e');
      return false;
    }
  }

  /// Aggiunge un nuovo preset
  static Future<bool> addPreset(ColorPreset preset) async {
    final presets = await loadPresets();
    presets.add(preset);
    return await savePresets(presets);
  }

  /// Elimina un preset per ID
  static Future<bool> deletePreset(String id) async {
    final presets = await loadPresets();
    presets.removeWhere((p) => p.id == id);
    return await savePresets(presets);
  }

  /// Aggiorna un preset esistente
  static Future<bool> updatePreset(ColorPreset preset) async {
    final presets = await loadPresets();
    final index = presets.indexWhere((p) => p.id == preset.id);

    if (index != -1) {
      presets[index] = preset;
      return await savePresets(presets);
    }

    return false;
  }

  /// Elimina tutti i preset
  static Future<bool> clearAllPresets() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      return await prefs.remove(_presetsKey);
    } catch (e) {
      print('[PresetStorage] Errore eliminazione preset: $e');
      return false;
    }
  }
}
