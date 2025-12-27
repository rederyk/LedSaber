import 'package:flutter/material.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';
import 'package:provider/provider.dart';
import '../../providers/led_provider.dart';
import 'dart:async';

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

  // Preset colori Star Wars
  static const Map<String, Color> starWarsPresets = {
    'Jedi Blue': Color(0xFF0080FF),
    'Sith Red': Color(0xFFFF0000),
    'Mace Windu': Color(0xFF8B00FF),
    'Yoda Green': Color(0xFF00FF00),
    'Ahsoka White': Color(0xFFFFFFFF),
    'Dark Saber': Color(0xFF1A1A1A),
    'Orange': Color(0xFFFF8000),
    'Yellow': Color(0xFFFFFF00),
  };

  @override
  void initState() {
    super.initState();
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

  @override
  void dispose() {
    _debounceTimer?.cancel();
    super.dispose();
  }

  /// Applica colore al LED con debounce
  void _applyColorToLED(Color color, double brightness) {
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
    });
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

          // Preset Colors
          Text(
            'Presets',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.bold,
            ),
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

  /// Grid preset colori - ottimizzato per spazio verticale
  Widget _buildPresetsGrid() {
    return GridView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 2,
        crossAxisSpacing: 8,
        mainAxisSpacing: 8,
        childAspectRatio: 3.2, // Più largo, meno alto
      ),
      itemCount: starWarsPresets.length,
      itemBuilder: (context, index) {
        final entry = starWarsPresets.entries.elementAt(index);
        return _buildPresetCard(entry.key, entry.value);
      },
    );
  }

  /// Card singolo preset - compatto
  Widget _buildPresetCard(String name, Color color) {
    final isSelected = _selectedColor == color;

    return GestureDetector(
      onTap: () {
        setState(() {
          _selectedColor = color;
        });
        _applyColorToLED(color, _brightness);
      },
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 6),
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
          children: [
            // Preview colore circolare - ridotto
            Container(
              width: 28,
              height: 28,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color,
                border: Border.all(
                  color: Colors.white.withValues(alpha: 0.3),
                  width: 1,
                ),
              ),
            ),
            const SizedBox(width: 8),
            // Nome preset
            Expanded(
              child: Text(
                name,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                  fontSize: 11,
                ),
                overflow: TextOverflow.ellipsis,
                maxLines: 1,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
