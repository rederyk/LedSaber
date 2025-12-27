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
          content: Text('Configuration not loaded yet'),
          duration: Duration(seconds: 2),
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
        content: Text('Motion configuration applied!'),
        duration: Duration(seconds: 2),
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
      padding: const EdgeInsets.all(8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Titolo sezione
          Text(
            'Motion Detection',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.bold,
            ),
          ),
          const SizedBox(height: 8),

          // Motion Detection Toggle
          _buildMotionToggle(state, config, motionProvider),

          const SizedBox(height: 12),

          // Live Motion Data
          if (state?.enabled ?? false) ...[
            _buildLiveMotionData(state!),
            const SizedBox(height: 12),
          ],

          // Gestures Enable Toggle
          _buildGesturesToggle(config, motionProvider),

          const SizedBox(height: 12),

          // Motion Tuning Sliders
          _buildMotionTuning(),

          const SizedBox(height: 12),

          // Advanced Settings (Collapsable)
          _buildAdvancedSettings(),

          const SizedBox(height: 16),

          // Apply Button
          _buildApplyButton(motionProvider),

          // Error message
          if (motionProvider.errorMessage != null) ...[
            const SizedBox(height: 8),
            Text(
              motionProvider.errorMessage!,
              style: const TextStyle(color: Colors.red, fontSize: 12),
            ),
          ],
        ],
      ),
    );
  }

  /// Motion Detection Toggle
  Widget _buildMotionToggle(MotionState? state, MotionConfig? config, MotionProvider provider) {
    final isEnabled = state?.enabled ?? config?.enabled ?? false;

    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                Icon(
                  isEnabled ? Icons.motion_photos_on : Icons.motion_photos_off,
                  color: isEnabled ? Colors.green : Colors.grey,
                  size: 20,
                ),
                const SizedBox(width: 8),
                Text(
                  'Motion Detection',
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
            Switch(
              value: isEnabled,
              onChanged: (value) => _toggleMotionDetection(value, provider),
            ),
          ],
        ),
      ),
    );
  }

  /// Gestures Enable Toggle
  Widget _buildGesturesToggle(MotionConfig? config, MotionProvider provider) {
    final isEnabled = config?.gesturesEnabled ?? true;

    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                Icon(
                  isEnabled ? Icons.gesture : Icons.block,
                  color: isEnabled ? Colors.blue : Colors.grey,
                  size: 20,
                ),
                const SizedBox(width: 8),
                Text(
                  'Gesture Recognition',
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
            Switch(
              value: isEnabled,
              onChanged: (value) => _toggleGestures(value, provider),
            ),
          ],
        ),
      ),
    );
  }

  /// Live Motion Data Display
  Widget _buildLiveMotionData(MotionState state) {
    return Card(
      color: Theme.of(context).cardColor.withValues(alpha: 0.5),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Live Motion',
              style: Theme.of(context).textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 8),

            // Direction
            _buildLiveDataRow(
              'Direction:',
              state.direction,
              _getDirectionIcon(state.direction),
            ),
            const SizedBox(height: 4),

            // Speed
            _buildLiveDataRow(
              'Speed:',
              state.speed.toStringAsFixed(1),
              Icons.speed,
            ),
            const SizedBox(height: 4),

            // Confidence
            _buildLiveDataRow(
              'Confidence:',
              '${state.confidence}%',
              Icons.percent,
            ),

            const Divider(height: 16),

            // Last Gesture
            Text(
              'Last Gesture',
              style: Theme.of(context).textTheme.titleSmall?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 8),

            _buildLiveDataRow(
              'Type:',
              state.lastGesture,
              _getGestureIcon(state.lastGesture),
            ),
            const SizedBox(height: 4),

            _buildLiveDataRow(
              'Confidence:',
              '${state.gestureConfidence}%',
              Icons.percent,
            ),
          ],
        ),
      ),
    );
  }

  /// Live Data Row
  Widget _buildLiveDataRow(String label, String value, IconData icon) {
    return Row(
      children: [
        Icon(icon, size: 16, color: Colors.grey),
        const SizedBox(width: 6),
        Text(
          label,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            color: Colors.grey,
          ),
        ),
        const SizedBox(width: 8),
        Text(
          value,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
            fontWeight: FontWeight.bold,
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

  /// Motion Tuning Sliders
  Widget _buildMotionTuning() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          'Motion Tuning',
          style: Theme.of(context).textTheme.titleSmall?.copyWith(
            fontWeight: FontWeight.bold,
          ),
        ),
        const SizedBox(height: 8),

        // Quality
        _buildSlider(
          'Quality',
          _quality,
          0,
          255,
          (value) => setState(() => _quality = value),
          Icons.high_quality,
        ),

        const SizedBox(height: 8),

        // Motion Intensity Min
        _buildSlider(
          'Motion Min',
          _motionIntensityMin,
          0,
          50,
          (value) => setState(() => _motionIntensityMin = value),
          Icons.vibration,
        ),

        const SizedBox(height: 8),

        // Motion Speed Min
        _buildSlider(
          'Speed Min',
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

  /// Advanced Settings (Collapsable)
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
                size: 20,
              ),
              const SizedBox(width: 4),
              Text(
                'Gesture Thresholds',
                style: Theme.of(context).textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
              ),
            ],
          ),
        ),

        // Contenuto collapsabile
        if (_showAdvanced) ...[
          const SizedBox(height: 8),

          // Ignition Threshold
          _buildSlider(
            'Ignition',
            _gestureIgnitionIntensity,
            0,
            255,
            (value) => setState(() => _gestureIgnitionIntensity = value),
            Icons.flash_on,
          ),

          const SizedBox(height: 8),

          // Retract Threshold
          _buildSlider(
            'Retract',
            _gestureRetractIntensity,
            0,
            255,
            (value) => setState(() => _gestureRetractIntensity = value),
            Icons.flash_off,
          ),

          const SizedBox(height: 8),

          // Clash Threshold
          _buildSlider(
            'Clash',
            _gestureClashIntensity,
            0,
            255,
            (value) => setState(() => _gestureClashIntensity = value),
            Icons.warning,
          ),

          const SizedBox(height: 12),

          // Debug Logs Toggle
          Row(
            children: [
              const Icon(Icons.bug_report, size: 16, color: Colors.grey),
              const SizedBox(width: 6),
              Text(
                'Debug Logs',
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const Spacer(),
              Switch(
                value: _debugLogs,
                onChanged: (value) {
                  setState(() {
                    _debugLogs = value;
                  });
                },
              ),
            ],
          ),
        ],
      ],
    );
  }

  /// Generic Slider Builder
  Widget _buildSlider(
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
            Icon(icon, size: 16, color: Colors.grey),
            const SizedBox(width: 6),
            Text(
              label,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                fontWeight: FontWeight.bold,
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
              ),
            ),
          ],
        ),
        Slider(
          value: value,
          min: min,
          max: max,
          divisions: divisions ?? (max - min).round(),
          onChanged: onChanged,
        ),
      ],
    );
  }

  /// Apply Configuration Button
  Widget _buildApplyButton(MotionProvider provider) {
    return SizedBox(
      width: double.infinity,
      child: ElevatedButton.icon(
        onPressed: () => _applyMotionConfig(provider),
        icon: const Icon(Icons.check),
        label: const Text('Apply Configuration'),
        style: ElevatedButton.styleFrom(
          padding: const EdgeInsets.symmetric(vertical: 12),
        ),
      ),
    );
  }
}
