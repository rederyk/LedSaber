import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../providers/led_provider.dart';
import '../../models/effect.dart';

/// Tab per controllo effetti LED
class EffectsTab extends StatefulWidget {
  const EffectsTab({super.key});

  @override
  State<EffectsTab> createState() => _EffectsTabState();
}

class _EffectsTabState extends State<EffectsTab> {
  String _selectedEffectId = 'solid';
  double _speed = 50.0; // 1-100 (must stay in range to avoid assertion error)

  // Preset velocità
  static const Map<String, double> speedPresets = {
    'Slow': 25.0,
    'Medium': 50.0,
    'Fast': 75.0,
  };

  @override
  void initState() {
    super.initState();
    // Inizializza con effetto corrente e carica lista se necessario
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final ledProvider = Provider.of<LedProvider>(context, listen: false);
      final state = ledProvider.currentState;
      if (state != null) {
        setState(() {
          _selectedEffectId = state.effect;
          // Clamp speed to valid range (1-100) to avoid slider assertion error
          _speed = state.speed.toDouble().clamp(1.0, 100.0);
        });
      }

      // Se la lista effetti non è ancora caricata, richiedila
      if (ledProvider.effectsList == null || ledProvider.effectsList!.effects.isEmpty) {
        debugPrint('[EffectsTab] Lista effetti non disponibile, richiedo caricamento...');
        ledProvider.reloadEffectsList();
      }
    });
  }

  /// Applica effetto al LED
  void _applyEffect(String effectId) {
    final ledProvider = Provider.of<LedProvider>(context, listen: false);
    ledProvider.setEffect(effectId);
  }

  /// Applica velocità (se supportata dal firmware)
  void _applySpeed(double speed) {
    final ledProvider = Provider.of<LedProvider>(context, listen: false);
    ledProvider.setEffect(
      _selectedEffectId,
      speed: speed.round(),
    );
  }

  @override
  Widget build(BuildContext context) {
    final ledProvider = Provider.of<LedProvider>(context);
    final effectsList = ledProvider.effectsList;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Titolo sezione
          Text(
            'LED Effects',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 8),

          // Lista effetti
          if (effectsList == null || effectsList.effects.isEmpty)
            _buildLoadingOrEmpty()
          else
            _buildEffectsList(effectsList.effects),

          const SizedBox(height: 16),

          // Speed Control
          _buildSpeedControl(),
        ],
      ),
    );
  }

  /// Mostra caricamento o lista vuota
  Widget _buildLoadingOrEmpty() {
    return Container(
      padding: const EdgeInsets.all(16),
      child: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const SizedBox(
              width: 24,
              height: 24,
              child: CircularProgressIndicator(strokeWidth: 2),
            ),
            const SizedBox(height: 8),
            Text(
              'Loading effects...',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Colors.grey,
              ),
            ),
          ],
        ),
      ),
    );
  }

  /// Lista scrollabile effetti
  Widget _buildEffectsList(List<Effect> effects) {
    return ListView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      itemCount: effects.length,
      itemBuilder: (context, index) {
        final effect = effects[index];
        return _buildEffectCard(effect);
      },
    );
  }

  /// Card singolo effetto
  Widget _buildEffectCard(Effect effect) {
    final isSelected = _selectedEffectId == effect.id;

    return GestureDetector(
      onTap: () {
        setState(() {
          _selectedEffectId = effect.id;
        });
        _applyEffect(effect.id);
      },
      child: Container(
        margin: const EdgeInsets.only(bottom: 6),
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 8),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: isSelected
                ? Theme.of(context).colorScheme.primary
                : Colors.grey.withValues(alpha: 0.3),
            width: isSelected ? 2 : 1,
          ),
          color: Theme.of(context).cardColor,
        ),
        child: Row(
          children: [
            // Leading icon
            Container(
              width: 32,
              height: 32,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: isSelected
                    ? Theme.of(context).colorScheme.primary
                    : Colors.grey.withValues(alpha: 0.2),
              ),
              child: Center(
                child: Icon(
                  Icons.auto_awesome,
                  color: isSelected ? Colors.white : Colors.grey,
                  size: 16,
                ),
              ),
            ),
            const SizedBox(width: 8),
            // Title
            Expanded(
              child: Text(
                effect.name,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                  fontSize: 12,
                ),
                overflow: TextOverflow.ellipsis,
                maxLines: 1,
              ),
            ),
            // Trailing icon
            if (isSelected)
              const Icon(Icons.check_circle, color: Colors.green, size: 18),
          ],
        ),
      ),
    );
  }

  /// Controllo velocità
  Widget _buildSpeedControl() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          'Speed',
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
            fontWeight: FontWeight.bold,
          ),
        ),
        const SizedBox(height: 8),

        // Slider velocità
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(
              '${_speed.round()}%',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Colors.grey,
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        Slider(
          value: _speed,
          min: 1.0,
          max: 100.0,
          divisions: 99,
          onChanged: (value) {
            setState(() {
              _speed = value;
            });
            _applySpeed(value);
          },
        ),

        const SizedBox(height: 8),

        // Preset velocità
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: speedPresets.entries.map((entry) {
            return _buildSpeedPresetButton(entry.key, entry.value);
          }).toList(),
        ),
      ],
    );
  }

  /// Pulsante preset velocità
  Widget _buildSpeedPresetButton(String label, double speed) {
    final isSelected = (_speed - speed).abs() < 1.0;

    return Expanded(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 2),
        child: GestureDetector(
          onTap: () {
            setState(() {
              _speed = speed;
            });
            _applySpeed(speed);
          },
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 8),
            decoration: BoxDecoration(
              color: isSelected
                  ? Theme.of(context).colorScheme.primary
                  : Colors.grey.withValues(alpha: 0.2),
              borderRadius: BorderRadius.circular(6),
              border: Border.all(
                color: isSelected
                    ? Theme.of(context).colorScheme.primary
                    : Colors.grey.withValues(alpha: 0.3),
                width: 1,
              ),
            ),
            child: Center(
              child: Text(
                label,
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
                  color: isSelected ? Colors.white : Colors.black87,
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
