import 'package:flutter/material.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';
import 'package:provider/provider.dart';
import '../providers/ble_provider.dart';
import '../providers/led_provider.dart';

/// Schermata principale di controllo LED
class ControlScreen extends StatefulWidget {
  const ControlScreen({super.key});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  Color _currentColor = Colors.red;
  double _brightness = 255;
  bool _enabled = true;

  @override
  void initState() {
    super.initState();
    // Inizializza con lo stato corrente se disponibile
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final ledProvider = Provider.of<LedProvider>(context, listen: false);
      final state = ledProvider.currentState;
      if (state != null) {
        setState(() {
          _currentColor = Color.fromRGBO(state.r, state.g, state.b, 1.0);
          _brightness = state.brightness.toDouble();
          _enabled = state.enabled;
        });
      }
    });
  }

  /// Mostra il color picker
  void _showColorPicker() {
    showDialog(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title: const Text('Scegli Colore'),
          content: SingleChildScrollView(
            child: ColorPicker(
              pickerColor: _currentColor,
              onColorChanged: (Color color) {
                setState(() {
                  _currentColor = color;
                });
              },
              pickerAreaHeightPercent: 0.8,
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('Annulla'),
            ),
            ElevatedButton(
              onPressed: () {
                final ledProvider = Provider.of<LedProvider>(context, listen: false);
                final r = (_currentColor.r * 255.0).round().clamp(0, 255);
                final g = (_currentColor.g * 255.0).round().clamp(0, 255);
                final b = (_currentColor.b * 255.0).round().clamp(0, 255);
                ledProvider.setColor(r, g, b);
                Navigator.of(context).pop();
              },
              child: const Text('OK'),
            ),
          ],
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    final bleProvider = Provider.of<BleProvider>(context);
    final ledProvider = Provider.of<LedProvider>(context);

    // Se disconnesso, torna alla home
    if (!bleProvider.isConnected) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) {
          Navigator.of(context).pop();
        }
      });
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('Controllo LED Saber'),
        actions: [
          IconButton(
            icon: const Icon(Icons.bluetooth_connected),
            onPressed: () async {
              final navigator = Navigator.of(context);
              await bleProvider.disconnect();
              if (mounted) {
                navigator.pop();
              }
            },
            tooltip: 'Disconnetti',
          ),
        ],
      ),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // Blade Control
              _buildBladeControl(ledProvider),

              const SizedBox(height: 20),

              // Color & Brightness
              _buildColorControl(ledProvider),

              const SizedBox(height: 20),

              // Effects
              _buildEffectsControl(ledProvider),

              const SizedBox(height: 20),

              // Status LED (collapsable)
              _buildStatusLedControl(ledProvider),
            ],
          ),
        ),
      ),
    );
  }

  /// Widget Blade Control
  Widget _buildBladeControl(LedProvider ledProvider) {
    final state = ledProvider.currentState;
    final bladeState = state?.bladeState ?? 'off';
    final isAnimating = ledProvider.isBladeAnimating;

    String bladeStateText = 'OFF';
    Color bladeColor = Colors.grey;

    if (bladeState == 'on') {
      bladeStateText = 'ON';
      bladeColor = Colors.green;
    } else if (bladeState == 'igniting') {
      bladeStateText = 'ACCENSIONE...';
      bladeColor = Colors.orange;
    } else if (bladeState == 'retracting') {
      bladeStateText = 'SPEGNIMENTO...';
      bladeColor = Colors.orange;
    }

    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(Icons.flashlight_on, color: bladeColor, size: 32),
                const SizedBox(width: 12),
                Text(
                  'Lama: $bladeStateText',
                  style: TextStyle(
                    fontSize: 20,
                    fontWeight: FontWeight.bold,
                    color: bladeColor,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: isAnimating ? null : () => ledProvider.ignite(),
                    icon: const Icon(Icons.power_settings_new),
                    label: const Text('ACCENDI'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.green,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 16),
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: isAnimating ? null : () => ledProvider.retract(),
                    icon: const Icon(Icons.power_off),
                    label: const Text('SPEGNI'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red,
                      foregroundColor: Colors.white,
                      padding: const EdgeInsets.symmetric(vertical: 16),
                    ),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  /// Widget Color Control
  Widget _buildColorControl(LedProvider ledProvider) {
    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Colore e Luminosità',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),

            // Color preview e bottone picker
            Row(
              children: [
                Container(
                  width: 60,
                  height: 60,
                  decoration: BoxDecoration(
                    color: _currentColor,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: Colors.grey, width: 2),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'RGB: ${(_currentColor.r * 255.0).round()}, ${(_currentColor.g * 255.0).round()}, ${(_currentColor.b * 255.0).round()}',
                        style: const TextStyle(fontSize: 14),
                      ),
                      const SizedBox(height: 8),
                      ElevatedButton.icon(
                        onPressed: _showColorPicker,
                        icon: const Icon(Icons.palette),
                        label: const Text('Scegli Colore'),
                      ),
                    ],
                  ),
                ),
              ],
            ),

            const SizedBox(height: 20),

            // Brightness slider
            const Text('Luminosità', style: TextStyle(fontSize: 16)),
            Row(
              children: [
                Expanded(
                  child: Slider(
                    value: _brightness,
                    min: 0,
                    max: 255,
                    divisions: 255,
                    label: _brightness.round().toString(),
                    onChanged: (value) {
                      setState(() {
                        _brightness = value;
                      });
                    },
                    onChangeEnd: (value) {
                      ledProvider.setBrightness(value.round(), _enabled);
                    },
                  ),
                ),
                SizedBox(
                  width: 50,
                  child: Text(
                    _brightness.round().toString(),
                    style: const TextStyle(fontSize: 16),
                    textAlign: TextAlign.right,
                  ),
                ),
              ],
            ),

            // ON/OFF switch
            SwitchListTile(
              title: const Text('LED Attivi'),
              value: _enabled,
              onChanged: (value) {
                setState(() {
                  _enabled = value;
                });
                ledProvider.setBrightness(_brightness.round(), value);
              },
            ),
          ],
        ),
      ),
    );
  }

  /// Widget Effects Control
  Widget _buildEffectsControl(LedProvider ledProvider) {
    final state = ledProvider.currentState;
    final currentEffect = state?.effect ?? 'solid';

    return Card(
      elevation: 4,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Effetti',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                const Icon(Icons.auto_awesome),
                const SizedBox(width: 12),
                Expanded(
                  child: Text(
                    'Effetto corrente: $currentEffect',
                    style: const TextStyle(fontSize: 16),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            ElevatedButton.icon(
              onPressed: () {
                // Schermata effetti sarà implementata in seguito
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Schermata effetti in arrivo!')),
                );
              },
              icon: const Icon(Icons.edit),
              label: const Text('Cambia Effetto'),
              style: ElevatedButton.styleFrom(
                minimumSize: const Size(double.infinity, 48),
              ),
            ),
          ],
        ),
      ),
    );
  }

  /// Widget Status LED Control (collapsable)
  Widget _buildStatusLedControl(LedProvider ledProvider) {
    final state = ledProvider.currentState;
    final statusBrightness = state?.statusLedBrightness.toDouble() ?? 32;
    final statusEnabled = state?.statusLedEnabled ?? true;

    return Card(
      elevation: 4,
      child: ExpansionTile(
        title: const Text('Status LED (Pin 4)'),
        leading: const Icon(Icons.lightbulb_outline),
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                Row(
                  children: [
                    const Text('Luminosità'),
                    Expanded(
                      child: Slider(
                        value: statusBrightness,
                        min: 0,
                        max: 255,
                        divisions: 255,
                        label: statusBrightness.round().toString(),
                        onChangeEnd: (value) {
                          ledProvider.setStatusLed(value.round(), statusEnabled);
                        },
                        onChanged: (value) {},
                      ),
                    ),
                    SizedBox(
                      width: 40,
                      child: Text(statusBrightness.round().toString()),
                    ),
                  ],
                ),
                SwitchListTile(
                  title: const Text('Attivo'),
                  value: statusEnabled,
                  onChanged: (value) {
                    ledProvider.setStatusLed(statusBrightness.round(), value);
                  },
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
