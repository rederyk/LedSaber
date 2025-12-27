import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ble_provider.dart';
import '../providers/led_provider.dart';
import '../providers/motion_provider.dart';
import '../widgets/lightsaber_widget.dart';
import 'device_list_screen.dart';
import 'tabs/colors_tab.dart';
import 'tabs/effects_tab.dart';
import 'tabs/motion_tab.dart';

/// Schermata principale di controllo LED
class ControlScreen extends StatefulWidget {
  const ControlScreen({super.key});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> with SingleTickerProviderStateMixin {
  late TabController _tabController;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 3, vsync: this);
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
  }
  @override
  Widget build(BuildContext context) {
    final bleProvider = Provider.of<BleProvider>(context);
    final ledProvider = Provider.of<LedProvider>(context);

    // Aggiorna il LED Service quando cambia il device attivo
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) {
        ledProvider.setLedService(bleProvider.ledService);

        // Aggiorna anche il Motion Service (con tutti i servizi per camera sync)
        final motionProvider = Provider.of<MotionProvider>(context, listen: false);
        motionProvider.setMotionService(
          bleProvider.motionService,
          allServices: bleProvider.allServices,
        );
      }
    });

    // Determina orientamento
    final orientation = MediaQuery.of(context).orientation;
    final isPortrait = orientation == Orientation.portrait;

    return Scaffold(
      appBar: AppBar(
        title: const Text('LED Saber Control'),
        actions: [
          // Badge con numero devices connessi
          Stack(
            children: [
              IconButton(
                icon: const Icon(Icons.bluetooth_connected),
                onPressed: () {
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (context) => const DeviceListScreen(),
                    ),
                  );
                },
                tooltip: 'Dispositivi connessi (${bleProvider.deviceCount})',
              ),
              if (bleProvider.deviceCount > 1)
                Positioned(
                  right: 8,
                  top: 8,
                  child: Container(
                    padding: const EdgeInsets.all(4),
                    decoration: BoxDecoration(
                      color: Theme.of(context).colorScheme.primary,
                      shape: BoxShape.circle,
                    ),
                    constraints: const BoxConstraints(
                      minWidth: 16,
                      minHeight: 16,
                    ),
                    child: Text(
                      '${bleProvider.deviceCount}',
                      style: const TextStyle(
                        color: Colors.white,
                        fontSize: 10,
                        fontWeight: FontWeight.bold,
                      ),
                      textAlign: TextAlign.center,
                    ),
                  ),
                ),
            ],
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

  /// Layout Portrait: Menu a sinistra (40%), Spada a destra (60%)
  Widget _buildPortraitLayout(
      BleProvider bleProvider, LedProvider ledProvider) {
    final screenHeight = MediaQuery.of(context).size.height;

    return Row(
      children: [
        // Menu laterale sinistro con TabBar
        Expanded(
          flex: 40,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: Column(
              children: [
                // TabBar con font più piccolo
                TabBar(
                  controller: _tabController,
                  labelStyle: const TextStyle(fontSize: 11),
                  tabs: const [
                    Tab(icon: Icon(Icons.palette, size: 18), text: 'Colors'),
                    Tab(icon: Icon(Icons.auto_awesome, size: 18), text: 'Effects'),
                    Tab(icon: Icon(Icons.motion_photos_on, size: 18), text: 'Motion'),
                  ],
                ),
                // TabBarView
                Expanded(
                  child: TabBarView(
                    controller: _tabController,
                    children: const [
                      ColorsTab(),
                      EffectsTab(),
                      MotionTab(),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),

        // Spada a destra
        Expanded(
          flex: 60,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: _buildLightsaberSection(
                bleProvider, ledProvider, screenHeight * 0.8),
          ),
        ),
      ],
    );
  }

  /// Layout Landscape: Spada a sinistra (35%), Menu a destra (65%)
  Widget _buildLandscapeLayout(
      BleProvider bleProvider, LedProvider ledProvider) {
    final screenHeight = MediaQuery.of(context).size.height;

    return Row(
      children: [
        // Spada a sinistra
        Expanded(
          flex: 35,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: _buildLightsaberSection(
                bleProvider, ledProvider, screenHeight * 0.85),
          ),
        ),

        // Menu a destra con TabBar
        Expanded(
          flex: 65,
          child: Container(
            color: Theme.of(context).scaffoldBackgroundColor,
            child: Column(
              children: [
                // TabBar
                TabBar(
                  controller: _tabController,
                  tabs: const [
                    Tab(icon: Icon(Icons.palette), text: 'Colors'),
                    Tab(icon: Icon(Icons.auto_awesome), text: 'Effects'),
                    Tab(icon: Icon(Icons.motion_photos_on), text: 'Motion'),
                  ],
                ),
                // TabBarView
                Expanded(
                  child: TabBarView(
                    controller: _tabController,
                    children: const [
                      ColorsTab(),
                      EffectsTab(),
                      MotionTab(),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),
      ],
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
