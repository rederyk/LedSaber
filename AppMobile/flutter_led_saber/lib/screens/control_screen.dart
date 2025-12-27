import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ble_provider.dart';
import '../providers/led_provider.dart';
import '../widgets/lightsaber_widget.dart';

/// Schermata principale di controllo LED
class ControlScreen extends StatefulWidget {
  const ControlScreen({super.key});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  @override
  Widget build(BuildContext context) {
    final bleProvider = Provider.of<BleProvider>(context);
    final ledProvider = Provider.of<LedProvider>(context);

    // Se disconnesso, torna alla home
    if (!bleProvider.isConnected) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) {
          ledProvider.setLedService(null);
          Navigator.of(context).pop();
        }
      });
    }

    // Determina orientamento
    final orientation = MediaQuery.of(context).orientation;
    final isPortrait = orientation == Orientation.portrait;

    return Scaffold(
      appBar: AppBar(
        title: const Text('LED Saber Control'),
        actions: [
          IconButton(
            icon: const Icon(Icons.bluetooth_connected),
            onPressed: () async {
              final navigator = Navigator.of(context);
              ledProvider.setLedService(null);
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
        child: isPortrait
            ? _buildPortraitLayout(bleProvider, ledProvider)
            : _buildLandscapeLayout(bleProvider, ledProvider),
      ),
    );
  }

  /// Layout Portrait: Menu a sinistra (25%), Spada a destra (75%)
  Widget _buildPortraitLayout(
      BleProvider bleProvider, LedProvider ledProvider) {
    final screenHeight = MediaQuery.of(context).size.height;

    return Row(
      children: [
        // Menu laterale sinistro
        Expanded(
          flex: 25,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(8),
              child: Column(
                children: [
                  _buildPlaceholderMenu('Sprint 2:\nMenu\nControlli'),
                ],
              ),
            ),
          ),
        ),

        // Spada a destra
        Expanded(
          flex: 75,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: _buildLightsaberSection(
                bleProvider, ledProvider, screenHeight * 0.8),
          ),
        ),
      ],
    );
  }

  /// Layout Landscape: Spada a sinistra (30%), Menu a destra (70%)
  Widget _buildLandscapeLayout(
      BleProvider bleProvider, LedProvider ledProvider) {
    final screenHeight = MediaQuery.of(context).size.height;

    return Row(
      children: [
        // Spada a sinistra
        Expanded(
          flex: 30,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: _buildLightsaberSection(
                bleProvider, ledProvider, screenHeight * 0.85),
          ),
        ),

        // Menu a destra
        Expanded(
          flex: 70,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  _buildPlaceholderMenu(
                      'Sprint 2: Pannelli Controllo\n\n• Color Panel\n• Effects Panel\n• Advanced Panel'),
                ],
              ),
            ),
          ),
        ),
      ],
    );
  }

  /// Placeholder per menu (Sprint 2)
  Widget _buildPlaceholderMenu(String text) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Theme.of(context).cardColor,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: Colors.grey.withValues(alpha: 0.3),
          width: 2,
          style: BorderStyle.solid,
        ),
      ),
      child: Center(
        child: Text(
          text,
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: Colors.grey,
              ),
          textAlign: TextAlign.center,
        ),
      ),
    );
  }

  /// Sezione Spada Laser
  Widget _buildLightsaberSection(
      BleProvider bleProvider, LedProvider ledProvider, double maxHeight) {
    final state = ledProvider.currentState;

    if (state == null) {
      return const Center(
        child: CircularProgressIndicator(),
      );
    }

    // Costruisci colore da RGB
    final bladeColor = Color.fromRGBO(state.r, state.g, state.b, 1.0);
    final brightness = state.brightness / 255.0;
    final bladeState = state.bladeState;
    final currentEffect = state.effect;

    return Center(
      child: LightsaberWidget(
        bladeColor: bladeColor,
        brightness: brightness,
        bladeState: bladeState,
        currentEffect: currentEffect,
        maxHeight: maxHeight,
        isConnected: bleProvider.isConnected,
        onPowerTap: () {
          if (bladeState == 'igniting' || bladeState == 'retracting') {
            return;
          }
          // Toggle ignite/retract
          if (bladeState == 'off' || bladeState == 'retracting') {
            ledProvider.ignite();
          } else if (bladeState == 'on' || bladeState == 'igniting') {
            ledProvider.retract();
          }
        },
      ),
    );
  }
}
