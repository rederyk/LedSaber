import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../providers/motion_provider.dart';
import '../../models/motion_state.dart';

/// Tab per Motion Detection & Gesture System
/// Basato sulla roadmap Sprint 3 - Sezione 4.2 Motion Screen
class MotionTab extends StatefulWidget {
  const MotionTab({super.key});

  @override
  State<MotionTab> createState() => _MotionTabState();
}

class _MotionTabState extends State<MotionTab> {
  // Motion Tuning Parameters (local state per sliders)
  double _quality = 160.0;
  double _motionIntensityMin = 6.0;
  double _motionSpeedMin = 0.4;

  // Gesture Thresholds (local state per sliders)
  double _gestureIgnitionIntensity = 15.0;
  double _gestureRetractIntensity = 15.0;
  double _gestureClashIntensity = 15.0;

  bool _debugLogs = false;
  bool _showAdvanced = false;

  @override
  void initState() {
    super.initState();
    // Inizializza con configurazione corrente dal provider
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final motionProvider = Provider.of<MotionProvider>(context, listen: false);
      final config = motionProvider.currentConfig;

      if (config != null) {
        setState(() {
          _quality = config.quality.toDouble();
          _motionIntensityMin = config.motionIntensityMin.toDouble();
          _motionSpeedMin = config.motionSpeedMin;
          _gestureIgnitionIntensity = config.gestureIgnitionIntensity.toDouble();
          _gestureRetractIntensity = config.gestureRetractIntensity.toDouble();
          _gestureClashIntensity = config.gestureClashIntensity.toDouble();
          _debugLogs = config.debugLogs;
        });
      } else {
        // Se config non disponibile, caricala
        motionProvider.reloadConfig();
      }
    });
  }

  /// Toggle Motion Detection
  void _toggleMotionDetection(bool enabled, MotionProvider provider) {
    if (enabled) {
      provider.enableMotion();
    } else {
      provider.disableMotion();
    }
  }

  /// Toggle Gestures
  void _toggleGestures(bool enabled, MotionProvider provider) {
    provider.updateConfigParam(gesturesEnabled: enabled);
  }

  /// Apply Motion Configuration
  void _applyMotionConfig(MotionProvider provider) {
    final currentConfig = provider.currentConfig;

    if (currentConfig == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Config not loaded'),
          duration: Duration(seconds: 1),
        ),
      );
      return;
    }

    final newConfig = currentConfig.copyWith(
      quality: _quality.round(),
      motionIntensityMin: _motionIntensityMin.round(),
      motionSpeedMin: _motionSpeedMin,
      gestureIgnitionIntensity: _gestureIgnitionIntensity.round(),
      gestureRetractIntensity: _gestureRetractIntensity.round(),
      gestureClashIntensity: _gestureClashIntensity.round(),
      debugLogs: _debugLogs,
    );

    provider.applyConfig(newConfig);

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('Applied!'),
        duration: Duration(seconds: 1),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final motionProvider = Provider.of<MotionProvider>(context);
    final state = motionProvider.currentState;
    final config = motionProvider.currentConfig;

    // Sincronizza local state con config se disponibile
    if (config != null && !_showAdvanced) {
      // Aggiorna solo quando advanced settings è chiuso
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) {
          setState(() {
            _quality = config.quality.toDouble();
            _motionIntensityMin = config.motionIntensityMin.toDouble();
            _motionSpeedMin = config.motionSpeedMin;
          });
        }
      });
    }

    return SingleChildScrollView(
      padding: const EdgeInsets.all(6),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Motion Detection Toggle (compatto)
          _buildMotionToggle(state, config, motionProvider),
          const SizedBox(height: 6),

          // Live Motion Data (solo se abilitato)
          if (state?.enabled ?? false) ...[
            _buildLiveMotionData(state!),
            const SizedBox(height: 6),
          ],

          // Gestures Enable Toggle (compatto)
          _buildGesturesToggle(config, motionProvider),
          const SizedBox(height: 6),

          // Motion Tuning Sliders (compatti)
          _buildMotionTuning(),
          const SizedBox(height: 6),

          // Advanced Settings (Collapsable)
          _buildAdvancedSettings(),
          const SizedBox(height: 8),

          // Apply Button (compatto)
          _buildApplyButton(motionProvider),

          // Error message
          if (motionProvider.errorMessage != null) ...[
            const SizedBox(height: 4),
            Text(
              motionProvider.errorMessage!,
              style: const TextStyle(color: Colors.red, fontSize: 10),
            ),
          ],
        ],
      ),
    );
  }

  /// Motion Detection Toggle (compatto)
  Widget _buildMotionToggle(MotionState? state, MotionConfig? config, MotionProvider provider) {
    final isEnabled = state?.enabled ?? config?.enabled ?? false;

    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                Icon(
                  isEnabled ? Icons.motion_photos_on : Icons.motion_photos_off,
                  color: isEnabled ? Colors.green : Colors.grey,
                  size: 16,
                ),
                const SizedBox(width: 6),
                Text(
                  'Motion',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.bold,
                    fontSize: 11,
                  ),
                ),
              ],
            ),
            Transform.scale(
              scale: 0.8,
              child: Switch(
                value: isEnabled,
                onChanged: (value) => _toggleMotionDetection(value, provider),
              ),
            ),
          ],
        ),
      ),
    );
  }

  /// Gestures Enable Toggle (compatto)
  Widget _buildGesturesToggle(MotionConfig? config, MotionProvider provider) {
    final isEnabled = config?.gesturesEnabled ?? true;

    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                Icon(
                  isEnabled ? Icons.gesture : Icons.block,
                  color: isEnabled ? Colors.blue : Colors.grey,
                  size: 16,
                ),
                const SizedBox(width: 6),
                Text(
                  'Gestures',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.bold,
                    fontSize: 11,
                  ),
                ),
              ],
            ),
            Transform.scale(
              scale: 0.8,
              child: Switch(
                value: isEnabled,
                onChanged: (value) => _toggleGestures(value, provider),
              ),
            ),
          ],
        ),
      ),
    );
  }

  /// Live Motion Data Display (ultra-compatto)
  Widget _buildLiveMotionData(MotionState state) {
    return Card(
      margin: EdgeInsets.zero,
      color: Theme.of(context).cardColor.withValues(alpha: 0.5),
      child: Padding(
        padding: const EdgeInsets.all(8),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Header compatto
            Row(
              children: [
                const Icon(Icons.sensors, size: 12, color: Colors.grey),
                const SizedBox(width: 4),
                Text(
                  'Live',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.bold,
                    fontSize: 10,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),

            // Dati in griglia compatta 2x2
            Row(
              children: [
                Expanded(
                  child: _buildCompactDataItem(
                    _getDirectionIcon(state.direction),
                    state.direction,
                  ),
                ),
                Expanded(
                  child: _buildCompactDataItem(
                    Icons.speed,
                    state.speed.toStringAsFixed(1),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),
            Row(
              children: [
                Expanded(
                  child: _buildCompactDataItem(
                    _getGestureIcon(state.lastGesture),
                    state.lastGesture,
                  ),
                ),
                Expanded(
                  child: _buildCompactDataItem(
                    Icons.percent,
                    '${state.confidence}%',
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  /// Item dati compatto
  Widget _buildCompactDataItem(IconData icon, String value) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, size: 12, color: Colors.grey),
        const SizedBox(width: 4),
        Flexible(
          child: Text(
            value,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
              fontSize: 10,
              fontWeight: FontWeight.w600,
            ),
            overflow: TextOverflow.ellipsis,
          ),
        ),
      ],
    );
  }

  /// Get Direction Icon
  IconData _getDirectionIcon(String direction) {
    switch (direction) {
      case 'UP':
        return Icons.arrow_upward;
      case 'DOWN':
        return Icons.arrow_downward;
      case 'LEFT':
        return Icons.arrow_back;
      case 'RIGHT':
        return Icons.arrow_forward;
      default:
        return Icons.compare_arrows;
    }
  }

  /// Get Gesture Icon
  IconData _getGestureIcon(String gesture) {
    switch (gesture) {
      case 'IGNITION':
        return Icons.flash_on;
      case 'RETRACT':
        return Icons.flash_off;
      case 'CLASH':
        return Icons.warning;
      default:
        return Icons.touch_app;
    }
  }

  /// Motion Tuning Sliders (compatti)
  Widget _buildMotionTuning() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Quality
        _buildCompactSlider(
          'Qual',
          _quality,
          0,
          255,
          (value) => setState(() => _quality = value),
          Icons.high_quality,
        ),

        // Motion Intensity Min
        _buildCompactSlider(
          'MinInt',
          _motionIntensityMin,
          0,
          50,
          (value) => setState(() => _motionIntensityMin = value),
          Icons.vibration,
        ),

        // Motion Speed Min
        _buildCompactSlider(
          'MinSpd',
          _motionSpeedMin,
          0.0,
          5.0,
          (value) => setState(() => _motionSpeedMin = value),
          Icons.speed,
          divisions: 50,
          decimals: 1,
        ),
      ],
    );
  }

  /// Advanced Settings (Collapsable, compatto)
  Widget _buildAdvancedSettings() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Header con chevron
        InkWell(
          onTap: () {
            setState(() {
              _showAdvanced = !_showAdvanced;
            });
          },
          child: Row(
            children: [
              Icon(
                _showAdvanced ? Icons.expand_less : Icons.expand_more,
                size: 16,
              ),
              const SizedBox(width: 4),
              Text(
                'Gesture Thresholds',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontWeight: FontWeight.bold,
                  fontSize: 10,
                ),
              ),
            ],
          ),
        ),

        // Contenuto collapsabile
        if (_showAdvanced) ...[
          const SizedBox(height: 4),

          // Ignition Threshold
          _buildCompactSlider(
            'Ignit',
            _gestureIgnitionIntensity,
            0,
            255,
            (value) => setState(() => _gestureIgnitionIntensity = value),
            Icons.flash_on,
          ),

          // Retract Threshold
          _buildCompactSlider(
            'Retrc',
            _gestureRetractIntensity,
            0,
            255,
            (value) => setState(() => _gestureRetractIntensity = value),
            Icons.flash_off,
          ),

          // Clash Threshold
          _buildCompactSlider(
            'Clash',
            _gestureClashIntensity,
            0,
            255,
            (value) => setState(() => _gestureClashIntensity = value),
            Icons.warning,
          ),

          const SizedBox(height: 4),

          // Debug Logs Toggle
          Row(
            children: [
              const Icon(Icons.bug_report, size: 12, color: Colors.grey),
              const SizedBox(width: 4),
              Text(
                'Debug',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontSize: 10,
                ),
              ),
              const Spacer(),
              Transform.scale(
                scale: 0.7,
                child: Switch(
                  value: _debugLogs,
                  onChanged: (value) {
                    setState(() {
                      _debugLogs = value;
                    });
                  },
                ),
              ),
            ],
          ),
        ],
      ],
    );
  }

  /// Compact Slider Builder
  Widget _buildCompactSlider(
    String label,
    double value,
    double min,
    double max,
    ValueChanged<double> onChanged,
    IconData icon, {
    int? divisions,
    int decimals = 0,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Icon(icon, size: 12, color: Colors.grey),
            const SizedBox(width: 4),
            Text(
              label,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                fontWeight: FontWeight.bold,
                fontSize: 10,
              ),
            ),
            const Spacer(),
            Text(
              decimals == 0
                  ? value.round().toString()
                  : value.toStringAsFixed(decimals),
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Colors.grey,
                fontWeight: FontWeight.w600,
                fontSize: 10,
              ),
            ),
          ],
        ),
        SizedBox(
          height: 24,
          child: Slider(
            value: value,
            min: min,
            max: max,
            divisions: divisions ?? (max - min).round(),
            onChanged: onChanged,
          ),
        ),
      ],
    );
  }

  /// Apply Configuration Button (compatto)
  Widget _buildApplyButton(MotionProvider provider) {
    return SizedBox(
      width: double.infinity,
      height: 32,
      child: ElevatedButton.icon(
        onPressed: () => _applyMotionConfig(provider),
        icon: const Icon(Icons.check, size: 14),
        label: const Text(
          'Apply',
          style: TextStyle(fontSize: 11),
        ),
        style: ElevatedButton.styleFrom(
          padding: const EdgeInsets.symmetric(horizontal: 12),
        ),
      ),
    );
  }
}
