import 'package:flutter/material.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';
import 'package:provider/provider.dart';
import '../../providers/led_provider.dart';
import '../../models/color_preset.dart';
import '../../services/preset_storage_service.dart';
import 'dart:async';

class _ColorPreset {
  final String name;
  final Color color;
  final String effectId;

  const _ColorPreset(this.name, this.color, this.effectId);
}

/// Tab per controllo colore LED con HSV wheel e presets Star Wars
class ColorsTab extends StatefulWidget {
  const ColorsTab({super.key});

  @override
  State<ColorsTab> createState() => _ColorsTabState();
}

class _ColorsTabState extends State<ColorsTab> {
  Color _selectedColor = Colors.red;
  double _brightness = 1.0;
  Timer? _debounceTimer;
  List<ColorPreset> _customPresets = [];

  // Preset colori + effetto (ID effetti come da BLE list)
  static const List<_ColorPreset> saberPresets = [
    _ColorPreset('Jedi Blue', Color(0xFF0080FF), 'solid'),
    _ColorPreset('Obi-Wan Blue', Color(0xFF2F80FF), 'pulse'),
    _ColorPreset('Anakin Blue', Color(0xFF1E66FF), 'dual_pulse'),
    _ColorPreset('Luke Green', Color(0xFF00FF66), 'solid'),
    _ColorPreset('Yoda Green', Color(0xFF00FF00), 'sine_motion'),
    _ColorPreset('Ahsoka White', Color(0xFFFFFFFF), 'flicker'),
    _ColorPreset('Mace Windu', Color(0xFF8B00FF), 'pulse'),
    _ColorPreset('Rey Yellow', Color(0xFFFFFF00), 'sine_motion'),
    _ColorPreset('Temple Guard', Color(0xFFFFD000), 'solid'),
    _ColorPreset('Cal Kestis', Color(0xFFFF8000), 'pulse'),
    _ColorPreset('Sith Red', Color(0xFFFF0000), 'flicker'),
    _ColorPreset('Darth Vader', Color(0xFFB00000), 'unstable'),
    _ColorPreset('Kylo Ren', Color(0xFFFF2A2A), 'unstable'),
    _ColorPreset('Darth Maul', Color(0xFFE00000), 'sine_motion'),
    _ColorPreset('Darksaber', Color(0xFF1A1A1A), 'storm_lightning'),
  ];

  @override
  void initState() {
    super.initState();
    _loadCustomPresets();

    // Inizializza con colore corrente dal LED state
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final ledProvider = Provider.of<LedProvider>(context, listen: false);
      final state = ledProvider.currentState;
      if (state != null) {
        setState(() {
          _selectedColor = Color.fromRGBO(state.r, state.g, state.b, 1.0);
          _brightness = state.brightness / 255.0;
        });
      }
    });
  }

  /// Carica i preset personalizzati salvati
  Future<void> _loadCustomPresets() async {
    final presets = await PresetStorageService.loadPresets();
    setState(() {
      _customPresets = presets;
    });
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    // Sincronizza color picker con lo stato LED quando cambia
    final ledProvider = Provider.of<LedProvider>(context);
    final state = ledProvider.currentState;
    if (state != null) {
      final currentLedColor = Color.fromRGBO(state.r, state.g, state.b, 1.0);
      final currentBrightness = state.brightness / 255.0;

      // Aggiorna solo se diverso (evita loop infiniti)
      if (_selectedColor != currentLedColor || (_brightness - currentBrightness).abs() > 0.01) {
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (mounted) {
            setState(() {
              _selectedColor = currentLedColor;
              _brightness = currentBrightness;
            });
          }
        });
      }
    }
  }

  @override
  void dispose() {
    _debounceTimer?.cancel();
    super.dispose();
  }

  /// Applica colore al LED con debounce
  void _applyColorToLED(
    Color color,
    double brightness, {
    String? effectId,
  }) {
    _debounceTimer?.cancel();
    _debounceTimer = Timer(const Duration(milliseconds: 50), () {
      final ledProvider = Provider.of<LedProvider>(context, listen: false);

      // Invia colore (usa nuova API non deprecata)
      final r = (color.r * 255.0).round().clamp(0, 255);
      final g = (color.g * 255.0).round().clamp(0, 255);
      final b = (color.b * 255.0).round().clamp(0, 255);
      ledProvider.setColor(r, g, b);

      // Invia brightness (richiede brightness + enabled)
      final brightnessValue = (brightness * 255).round();
      ledProvider.setBrightness(brightnessValue, true);

      if (effectId != null) {
        final effectsList = ledProvider.effectsList;
        final effectExists =
            effectsList == null || effectsList.findById(effectId) != null;
        if (effectExists) {
          ledProvider.setEffect(effectId);
        } else {
          debugPrint('[ColorsTab] Effetto non trovato: $effectId');
        }
      }
    });
  }

  /// Mostra dialog per creare un nuovo preset
  Future<void> _showAddPresetDialog() async {
    final ledProvider = Provider.of<LedProvider>(context, listen: false);
    final currentEffect = ledProvider.currentState?.effect ?? 'solid';

    final nameController = TextEditingController();

    final result = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('New Preset'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameController,
              decoration: const InputDecoration(
                labelText: 'Preset Name',
                hintText: 'e.g., My Custom Saber',
              ),
              autofocus: true,
              maxLength: 30,
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                Container(
                  width: 40,
                  height: 40,
                  decoration: BoxDecoration(
                    color: _selectedColor,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: Colors.grey),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Effect: $currentEffect',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                      Text(
                        'Color: RGB(${(_selectedColor.r * 255).round()}, ${(_selectedColor.g * 255).round()}, ${(_selectedColor.b * 255).round()})',
                        style: Theme.of(context).textTheme.bodySmall?.copyWith(
                              color: Colors.grey,
                            ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () {
              if (nameController.text.trim().isEmpty) {
                return;
              }
              Navigator.of(context).pop(true);
            },
            child: const Text('Save'),
          ),
        ],
      ),
    );

    if (result == true && nameController.text.trim().isNotEmpty) {
      final newPreset = ColorPreset(
        id: DateTime.now().millisecondsSinceEpoch.toString(),
        name: nameController.text.trim(),
        color: _selectedColor,
        effectId: currentEffect,
        createdAt: DateTime.now(),
      );

      final success = await PresetStorageService.addPreset(newPreset);
      if (success && mounted) {
        await _loadCustomPresets();
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Preset "${newPreset.name}" saved!'),
            duration: const Duration(seconds: 2),
          ),
        );
      }
    }
  }

  /// Elimina un preset personalizzato
  Future<void> _deleteCustomPreset(ColorPreset preset) async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Delete Preset'),
        content: Text('Delete "${preset.name}"?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () => Navigator.of(context).pop(true),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
            ),
            child: const Text('Delete'),
          ),
        ],
      ),
    );

    if (confirm == true) {
      final success = await PresetStorageService.deletePreset(preset.id);
      if (success && mounted) {
        await _loadCustomPresets();
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Preset deleted'),
            duration: Duration(seconds: 2),
          ),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final screenSize = MediaQuery.of(context).size;
    final wheelSize = (screenSize.width * 0.45).clamp(140.0, 200.0);

    return SingleChildScrollView(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Titolo sezione compatto
          Text(
            'Color Picker',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 8),

          // HSV Color Wheel - dimensione ridotta per evitare overflow
          Center(
            child: ColorPicker(
              pickerColor: _selectedColor,
              onColorChanged: (Color color) {
                setState(() {
                  _selectedColor = color;
                });
                _applyColorToLED(color, _brightness);
              },
              colorPickerWidth: wheelSize,
              pickerAreaHeightPercent: 1.0,
              displayThumbColor: true,
              paletteType: PaletteType.hueWheel,
              labelTypes: const [],
              pickerAreaBorderRadius: BorderRadius.circular(12),
              enableAlpha: false, // Rimuove slider alpha
              portraitOnly: true,
            ),
          ),

          const SizedBox(height: 12),

          // Brightness Slider - spostato sotto il cerchio
          _buildBrightnessSlider(),

          const SizedBox(height: 12),

          // Custom Presets Section
          if (_customPresets.isNotEmpty) ...[
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  'My Presets',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.bold,
                  ),
                ),
                IconButton(
                  icon: const Icon(Icons.add_circle, size: 28),
                  onPressed: _showAddPresetDialog,
                  tooltip: 'Add new preset',
                ),
              ],
            ),
            const SizedBox(height: 8),
            _buildCustomPresetsGrid(),
            const SizedBox(height: 16),
          ],

          // Add Preset Button (if no custom presets yet)
          if (_customPresets.isEmpty)
            Container(
              width: double.infinity,
              margin: const EdgeInsets.only(bottom: 12),
              child: OutlinedButton.icon(
                onPressed: _showAddPresetDialog,
                icon: const Icon(Icons.add),
                label: const Text('Create Custom Preset'),
                style: OutlinedButton.styleFrom(
                  padding: const EdgeInsets.symmetric(vertical: 12),
                ),
              ),
            ),

          // Star Wars Presets
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                'Star Wars Presets',
                style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
              ),
              if (_customPresets.isNotEmpty)
                IconButton(
                  icon: const Icon(Icons.add_circle, size: 28),
                  onPressed: _showAddPresetDialog,
                  tooltip: 'Add new preset',
                ),
            ],
          ),
          const SizedBox(height: 8),

          _buildPresetsGrid(),
        ],
      ),
    );
  }

  /// Slider luminosità - compatto
  Widget _buildBrightnessSlider() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                const Icon(Icons.wb_sunny, size: 16, color: Colors.amber),
                const SizedBox(width: 6),
                Text(
                  'Brightness',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
            Text(
              '${(_brightness * 100).round()}%',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Colors.grey,
                fontWeight: FontWeight.w600,
              ),
            ),
          ],
        ),
        Slider(
          value: _brightness,
          min: 0.0,
          max: 1.0,
          divisions: 100,
          onChanged: (value) {
            setState(() {
              _brightness = value;
            });
            _applyColorToLED(_selectedColor, value);
          },
        ),
      ],
    );
  }

  /// Grid preset personalizzati
  Widget _buildCustomPresetsGrid() {
    return GridView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 1,
        mainAxisSpacing: 8,
        childAspectRatio: 3.0,
      ),
      itemCount: _customPresets.length,
      itemBuilder: (context, index) {
        final preset = _customPresets[index];
        return _buildCustomPresetCard(preset);
      },
    );
  }

  /// Grid preset colori - ottimizzato per spazio verticale
  Widget _buildPresetsGrid() {
    return GridView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 1,
        mainAxisSpacing: 8,
        childAspectRatio: 3.0, // Più alte per mostrare bene il testo
      ),
      itemCount: saberPresets.length,
      itemBuilder: (context, index) {
        final preset = saberPresets[index];
        return _buildPresetCard(preset);
      },
    );
  }

  /// Card preset personalizzato - con pulsante elimina
  Widget _buildCustomPresetCard(ColorPreset preset) {
    final name = preset.name;
    final color = preset.color;
    final isSelected = _selectedColor == color;

    return GestureDetector(
      onTap: () {
        setState(() {
          _selectedColor = color;
        });
        _applyColorToLED(
          color,
          _brightness,
          effectId: preset.effectId,
        );
      },
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: isSelected ? color : Colors.grey.withValues(alpha: 0.3),
            width: isSelected ? 2 : 1,
          ),
          color: Theme.of(context).cardColor,
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.start,
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            // Preview colore circolare
            Container(
              width: 24,
              height: 24,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color,
                border: Border.all(
                  color: Colors.white.withValues(alpha: 0.3),
                  width: 1,
                ),
              ),
            ),
            const SizedBox(width: 6),
            // Nome preset
            Expanded(
              child: Text(
                name,
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                  fontSize: 14,
                  color: Theme.of(context).colorScheme.onSurface,
                ),
                overflow: TextOverflow.ellipsis,
                maxLines: 2,
              ),
            ),
            // Pulsante elimina
            IconButton(
              icon: const Icon(Icons.delete_outline, size: 20),
              onPressed: () => _deleteCustomPreset(preset),
              padding: EdgeInsets.zero,
              constraints: const BoxConstraints(),
              color: Colors.red.withValues(alpha: 0.7),
            ),
          ],
        ),
      ),
    );
  }

  /// Card singolo preset - compatto
  Widget _buildPresetCard(_ColorPreset preset) {
    final name = preset.name;
    final color = preset.color;
    final isSelected = _selectedColor == color;

    return GestureDetector(
      onTap: () {
        setState(() {
          _selectedColor = color;
        });
        _applyColorToLED(
          color,
          _brightness,
          effectId: preset.effectId,
        );
      },
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: isSelected ? color : Colors.grey.withValues(alpha: 0.3),
            width: isSelected ? 2 : 1,
          ),
          color: Theme.of(context).cardColor,
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.start,
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            // Preview colore circolare - ridotto
            Container(
              width: 24,
              height: 24,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color,
                border: Border.all(
                  color: Colors.white.withValues(alpha: 0.3),
                  width: 1,
                ),
              ),
            ),
            const SizedBox(width: 6),
            // Nome preset
            Expanded(
              child: Text(
                name,
                style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                  fontSize: 14,
                  color: Theme.of(context).colorScheme.onSurface,
                ),
                overflow: TextOverflow.ellipsis,
                maxLines: 2,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
