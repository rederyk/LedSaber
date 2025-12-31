import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../models/effect.dart';
import '../../providers/led_provider.dart';

/// Tab per controllo effetti orologio (chrono_hybrid)
class ClockTab extends StatefulWidget {
  const ClockTab({super.key});

  @override
  State<ClockTab> createState() => _ClockTabState();
}

class _ClockTabState extends State<ClockTab> {
  int _hourTheme = 0;
  int _secondTheme = 0;
  String _lastSyncLabel = 'Not synced';
  bool _isSyncing = false;
  bool _wellnessMode = false;
  double _breathingRate = 5.0;

  static const List<String> _defaultHourThemes = [
    'Classic',
    'Neon',
    'Plasma',
    'Digital',
    'Inferno',
    'Storm',
    'Circadian',
    'Forest',
    'Ocean',
    'Ember',
    'Moon',
    'Aurora',
  ];

  static const List<String> _defaultSecondThemes = [
    'Classic',
    'Spiral',
    'Fire',
    'Lightning',
    'Particle',
    'Quantum',
  ];

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final ledProvider = Provider.of<LedProvider>(context, listen: false);
      final state = ledProvider.currentState;
      if (state != null) {
        setState(() {
          _hourTheme = state.chronoHourTheme;
          _secondTheme = state.chronoSecondTheme;
        });
      }
    });
  }

  List<String> _resolveThemes(Effect? effect, String key, List<String> fallback) {
    final themes = effect?.themes?[key];
    if (themes != null && themes.isNotEmpty) {
      return themes;
    }
    return fallback;
  }

  Effect? _getChronoEffect(LedProvider ledProvider) {
    return ledProvider.effectsList?.findById('chrono_hybrid');
  }

  Future<void> _syncTime() async {
    if (_isSyncing) return;
    setState(() {
      _isSyncing = true;
    });

    final ledProvider = Provider.of<LedProvider>(context, listen: false);
    await ledProvider.syncTime();

    final now = TimeOfDay.now();
    if (mounted) {
      setState(() {
        _lastSyncLabel = 'Synced at ${now.format(context)}';
        _isSyncing = false;
      });
    }
  }

  void _applyChronoThemes() {
    final ledProvider = Provider.of<LedProvider>(context, listen: false);
    ledProvider.setEffect(
      'chrono_hybrid',
      chronoHourTheme: _hourTheme,
      chronoSecondTheme: _secondTheme,
      chronoWellnessMode: _wellnessMode,
      breathingRate: _breathingRate.toInt(),
    );
  }

  @override
  Widget build(BuildContext context) {
    final ledProvider = Provider.of<LedProvider>(context);
    final chronoEffect = _getChronoEffect(ledProvider);
    final hourThemes =
        _resolveThemes(chronoEffect, 'hour', _defaultHourThemes);
    final secondThemes =
        _resolveThemes(chronoEffect, 'second', _defaultSecondThemes);

    return SingleChildScrollView(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Clock Effects',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 12,
            runSpacing: 8,
            crossAxisAlignment: WrapCrossAlignment.center,
            children: [
              ElevatedButton.icon(
                onPressed: _isSyncing ? null : _syncTime,
                icon: const Icon(Icons.sync, size: 16),
                label: const Text('Sync time'),
              ),
              Text(
                _lastSyncLabel,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: Colors.grey,
                    ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Text(
            'Hour theme',
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
          ),
          const SizedBox(height: 6),
          DropdownButton<int>(
            value: _hourTheme.clamp(0, hourThemes.length - 1),
            isExpanded: true,
            items: List.generate(hourThemes.length, (index) {
              return DropdownMenuItem(
                value: index,
                child: Text(hourThemes[index]),
              );
            }),
            onChanged: (value) {
              if (value == null) return;
              setState(() {
                _hourTheme = value;
              });
              _applyChronoThemes();
            },
          ),
          const SizedBox(height: 12),
          Text(
            'Second theme',
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
          ),
          const SizedBox(height: 6),
          DropdownButton<int>(
            value: _secondTheme.clamp(0, secondThemes.length - 1),
            isExpanded: true,
            items: List.generate(secondThemes.length, (index) {
              return DropdownMenuItem(
                value: index,
                child: Text(secondThemes[index]),
              );
            }),
            onChanged: (value) {
              if (value == null) return;
              setState(() {
                _secondTheme = value;
              });
              _applyChronoThemes();
            },
          ),
          const SizedBox(height: 16),
          const Divider(),
          const SizedBox(height: 8),
          Text(
            'Wellness Mode',
            style: Theme.of(context).textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
          ),
          const SizedBox(height: 8),
          SwitchListTile(
            title: const Text('Enable Wellness Mode'),
            subtitle: const Text('Slow transitions, breathing effect'),
            value: _wellnessMode,
            onChanged: (value) {
              setState(() {
                _wellnessMode = value;
                if (value && _hourTheme < 6) {
                  // Force Circadian theme quando wellness attivo
                  _hourTheme = 6;
                }
              });
              _applyChronoThemes();
            },
          ),
          if (_wellnessMode) ...[
            const SizedBox(height: 8),
            Text(
              'Breathing Rate: ${_breathingRate.toInt()} BPM',
              style: Theme.of(context).textTheme.bodySmall,
            ),
            Slider(
              value: _breathingRate,
              min: 2,
              max: 8,
              divisions: 6,
              label: '${_breathingRate.toInt()} BPM',
              onChanged: (value) {
                setState(() {
                  _breathingRate = value;
                });
              },
              onChangeEnd: (value) {
                _applyChronoThemes();
              },
            ),
            Text(
              _breathingRate <= 4
                  ? 'Deep meditation'
                  : (_breathingRate <= 6 ? 'Relaxation' : 'Normal'),
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: Colors.grey,
                  ),
            ),
          ],
          const SizedBox(height: 16),
          OutlinedButton.icon(
            onPressed: _applyChronoThemes,
            icon: const Icon(Icons.access_time, size: 16),
            label: const Text('Activate clock'),
          ),
        ],
      ),
    );
  }
}
