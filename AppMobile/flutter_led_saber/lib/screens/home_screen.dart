import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'dart:io';
import 'package:provider/provider.dart';
import 'package:permission_handler/permission_handler.dart';
import '../providers/ble_provider.dart';
import '../providers/led_provider.dart';
import '../models/ble_device_info.dart';
import '../models/connected_device.dart';
import 'control_screen.dart';
import 'package:intl/intl.dart';

/// Schermata principale per scansione e connessione BLE
class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  @override
  void initState() {
    super.initState();
    _checkPermissions();
  }

  /// Verifica e richiede i permessi necessari
  Future<void> _checkPermissions() async {
    // I permessi sono necessari solo su Android/iOS
    if (kIsWeb || (!Platform.isAndroid && !Platform.isIOS)) {
      return;
    }

    try {
      // Richiedi permessi Bluetooth (solo Android/iOS)
      if (await Permission.bluetoothScan.isDenied) {
        await Permission.bluetoothScan.request();
      }
      if (await Permission.bluetoothConnect.isDenied) {
        await Permission.bluetoothConnect.request();
      }
      if (await Permission.location.isDenied) {
        await Permission.location.request();
      }
    } catch (e) {
      // Ignora errori su piattaforme non supportate
      debugPrint('Permission check not supported on this platform: $e');
    }
  }

  /// Avvia la scansione
  Future<void> _startScan(BleProvider bleProvider) async {
    try {
      await bleProvider.startScan();
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Errore: $e')),
        );
      }
    }
  }

  /// Ferma la scansione
  Future<void> _stopScan(BleProvider bleProvider) async {
    await bleProvider.stopScan();
  }

  /// Connette a un dispositivo
  Future<void> _connectToDevice(
    BleProvider bleProvider,
    LedProvider ledProvider,
    BleDeviceInfo deviceInfo,
  ) async {
    // Mostra dialog di caricamento
    if (mounted) {
      showDialog(
        context: context,
        barrierDismissible: false,
        builder: (BuildContext context) {
          return const AlertDialog(
            content: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                CircularProgressIndicator(),
                SizedBox(height: 16),
                Text('Connessione in corso...'),
              ],
            ),
          );
        },
      );
    }

    try {
      await bleProvider.connectToDevice(deviceInfo);

      // Imposta il LED Service nel LedProvider
      if (bleProvider.ledService != null) {
        ledProvider.setLedService(bleProvider.ledService);
      }

      // Chiudi il dialog di caricamento
      if (mounted) {
        Navigator.of(context).pop();
      }

      // Naviga alla schermata di controllo
      if (mounted && bleProvider.isConnected) {
        Navigator.of(context).push(
          MaterialPageRoute(
            builder: (context) => const ControlScreen(),
          ),
        );
      } else if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Connessione fallita: dispositivo non connesso')),
        );
      }
    } catch (e) {
      // Chiudi il dialog di caricamento
      if (mounted) {
        Navigator.of(context).pop();
      }

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Errore connessione: $e'),
            duration: const Duration(seconds: 5),
            action: SnackBarAction(
              label: 'OK',
              onPressed: () {},
            ),
          ),
        );
      }
    }
  }

  /// Imposta un device come attivo e naviga al control screen
  void _setActiveAndNavigate(BleProvider bleProvider, ConnectedDevice device) {
    bleProvider.setActiveDevice(device.id);
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (context) => const ControlScreen(),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final bleProvider = Provider.of<BleProvider>(context);
    final ledProvider = Provider.of<LedProvider>(context, listen: false);

    return Scaffold(
      appBar: AppBar(
        title: const Text('LED Saber Controller'),
        centerTitle: true,
        actions: [
          // Mostra numero dispositivi connessi
          if (bleProvider.deviceCount > 0)
            Center(
              child: Padding(
                padding: const EdgeInsets.only(right: 16),
                child: Container(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
                  decoration: BoxDecoration(
                    color: Colors.green.shade100,
                    borderRadius: BorderRadius.circular(16),
                    border: Border.all(color: Colors.green),
                  ),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const Icon(Icons.bluetooth_connected, color: Colors.green, size: 16),
                      const SizedBox(width: 4),
                      Text(
                        '${bleProvider.deviceCount}',
                        style: const TextStyle(
                          color: Colors.green,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
        ],
      ),
      body: SafeArea(
        child: Column(
          children: [
            // Sezione dispositivi connessi
            if (bleProvider.deviceCount > 0) ...[
              _buildConnectedDevicesSection(bleProvider, ledProvider),
              const Divider(height: 1, thickness: 2),
            ],

            // Sezione scansione
            _buildScanSection(bleProvider),

            // Lista dispositivi trovati
            Expanded(
              child: _buildDiscoveredDevicesList(bleProvider, ledProvider),
            ),
          ],
        ),
      ),
    );
  }

  /// Sezione dispositivi connessi
  Widget _buildConnectedDevicesSection(BleProvider bleProvider, LedProvider ledProvider) {
    final connectedDevices = bleProvider.connectedDevices;

    return Container(
      color: Colors.green.shade50,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
            child: Row(
              children: [
                const Icon(Icons.check_circle, color: Colors.green, size: 20),
                const SizedBox(width: 8),
                Text(
                  'Dispositivi Connessi (${connectedDevices.length})',
                  style: const TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.bold,
                    color: Colors.green,
                  ),
                ),
                const Spacer(),
                TextButton.icon(
                  onPressed: () async {
                    final confirm = await showDialog<bool>(
                      context: context,
                      builder: (context) => AlertDialog(
                        title: const Text('Disconnetti tutti'),
                        content: const Text('Vuoi disconnettere tutti i dispositivi?'),
                        actions: [
                          TextButton(
                            onPressed: () => Navigator.pop(context, false),
                            child: const Text('Annulla'),
                          ),
                          ElevatedButton(
                            onPressed: () => Navigator.pop(context, true),
                            style: ElevatedButton.styleFrom(
                              backgroundColor: Colors.red,
                              foregroundColor: Colors.white,
                            ),
                            child: const Text('Disconnetti'),
                          ),
                        ],
                      ),
                    );

                    if (confirm == true) {
                      ledProvider.setLedService(null);
                      await bleProvider.disconnectAll();
                    }
                  },
                  icon: const Icon(Icons.close, size: 16),
                  label: const Text('Disconnetti tutti'),
                  style: TextButton.styleFrom(
                    foregroundColor: Colors.red,
                  ),
                ),
              ],
            ),
          ),
          SizedBox(
            height: 120,
            child: ListView.builder(
              scrollDirection: Axis.horizontal,
              padding: const EdgeInsets.symmetric(horizontal: 12),
              itemCount: connectedDevices.length,
              itemBuilder: (context, index) {
                final device = connectedDevices[index];
                return _buildConnectedDeviceCard(bleProvider, device);
              },
            ),
          ),
          const SizedBox(height: 12),
        ],
      ),
    );
  }

  /// Card dispositivo connesso (orizzontale)
  Widget _buildConnectedDeviceCard(BleProvider bleProvider, ConnectedDevice device) {
    final isActive = device.isActive;
    final timeFormatter = DateFormat('HH:mm');

    return GestureDetector(
      onTap: () => _setActiveAndNavigate(bleProvider, device),
      child: Container(
        width: 160,
        margin: const EdgeInsets.symmetric(horizontal: 4),
        child: Card(
          elevation: isActive ? 4 : 2,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
            side: BorderSide(
              color: isActive ? Colors.green : Colors.grey.shade300,
              width: isActive ? 2 : 1,
            ),
          ),
          child: Padding(
            padding: const EdgeInsets.all(12),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Icon(
                      Icons.bluetooth_connected,
                      color: isActive ? Colors.green : Colors.grey,
                      size: 20,
                    ),
                    const Spacer(),
                    if (isActive)
                      Container(
                        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                        decoration: BoxDecoration(
                          color: Colors.green,
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: const Text(
                          'ATTIVO',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 8,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                      ),
                  ],
                ),
                const SizedBox(height: 8),
                Text(
                  device.name,
                  style: TextStyle(
                    fontWeight: isActive ? FontWeight.bold : FontWeight.normal,
                    fontSize: 14,
                  ),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                const SizedBox(height: 4),
                Text(
                  timeFormatter.format(device.connectedAt),
                  style: TextStyle(
                    fontSize: 11,
                    color: Colors.grey.shade600,
                  ),
                ),
                const Spacer(),
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton(
                    onPressed: () async {
                      await bleProvider.disconnectDevice(device.id);
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red.shade50,
                      foregroundColor: Colors.red,
                      padding: const EdgeInsets.symmetric(vertical: 4),
                      minimumSize: const Size(0, 28),
                    ),
                    child: const Text(
                      'Disconnetti',
                      style: TextStyle(fontSize: 11),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  /// Sezione scansione
  Widget _buildScanSection(BleProvider bleProvider) {
    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.bluetooth_searching, size: 20),
              const SizedBox(width: 8),
              const Text(
                'Aggiungi Nuovo Dispositivo',
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          SizedBox(
            width: double.infinity,
            height: 50,
            child: ElevatedButton.icon(
              onPressed: bleProvider.isScanning
                  ? () => _stopScan(bleProvider)
                  : () => _startScan(bleProvider),
              icon: bleProvider.isScanning
                  ? const SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(
                        strokeWidth: 2,
                        valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
                      ),
                    )
                  : const Icon(Icons.refresh),
              label: Text(
                bleProvider.isScanning ? 'Ferma Scansione' : 'Scansiona Dispositivi',
                style: const TextStyle(fontSize: 16),
              ),
              style: ElevatedButton.styleFrom(
                backgroundColor: bleProvider.isScanning ? Colors.orange : null,
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  /// Lista dispositivi scoperti
  Widget _buildDiscoveredDevicesList(BleProvider bleProvider, LedProvider ledProvider) {
    final devices = bleProvider.discoveredDevices;
    final connectedIds = bleProvider.connectedDevices.map((d) => d.id).toSet();

    // Filtra i dispositivi già connessi
    final availableDevices = devices.where((d) => !connectedIds.contains(d.id)).toList();

    if (availableDevices.isEmpty && !bleProvider.isScanning) {
      return const Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.bluetooth_searching, size: 64, color: Colors.grey),
            SizedBox(height: 16),
            Text(
              'Nessun nuovo dispositivo trovato',
              style: TextStyle(fontSize: 16, color: Colors.grey),
            ),
            SizedBox(height: 8),
            Text(
              'Premi "Scansiona Dispositivi" per cercare',
              style: TextStyle(fontSize: 14, color: Colors.grey),
            ),
          ],
        ),
      );
    }

    return ListView.builder(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      itemCount: availableDevices.length,
      itemBuilder: (context, index) {
        final device = availableDevices[index];
        return _buildDiscoveredDeviceCard(device, bleProvider, ledProvider);
      },
    );
  }

  /// Card dispositivo scoperto
  Widget _buildDiscoveredDeviceCard(
    BleDeviceInfo device,
    BleProvider bleProvider,
    LedProvider ledProvider,
  ) {
    final isLedSaber = device.isLedSaber;

    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      elevation: isLedSaber ? 4 : 1,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: isLedSaber
            ? BorderSide(color: Colors.red.shade300, width: 2)
            : BorderSide.none,
      ),
      child: ListTile(
        contentPadding: const EdgeInsets.all(12),
        leading: Icon(
          isLedSaber ? Icons.flashlight_on : Icons.bluetooth,
          color: isLedSaber ? Colors.red : Colors.blue,
          size: 32,
        ),
        title: Text(
          device.name,
          style: TextStyle(
            fontWeight: isLedSaber ? FontWeight.bold : FontWeight.normal,
            fontSize: 16,
          ),
        ),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const SizedBox(height: 4),
            Text('ID: ${device.id.substring(0, 17)}...'),
            Text('RSSI: ${device.rssi} dBm (${device.signalQuality})'),
          ],
        ),
        trailing: ElevatedButton(
          onPressed: () => _connectToDevice(bleProvider, ledProvider, device),
          style: ElevatedButton.styleFrom(
            backgroundColor: isLedSaber ? Colors.red : null,
            foregroundColor: isLedSaber ? Colors.white : null,
          ),
          child: const Text('Connetti'),
        ),
      ),
    );
  }
}
