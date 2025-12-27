import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import 'dart:io';
import 'package:provider/provider.dart';
import 'package:permission_handler/permission_handler.dart';
import '../providers/ble_provider.dart';
import '../providers/led_provider.dart';
import '../models/ble_device_info.dart';
import 'control_screen.dart';

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

  @override
  Widget build(BuildContext context) {
    final bleProvider = Provider.of<BleProvider>(context);
    final ledProvider = Provider.of<LedProvider>(context, listen: false);

    return Scaffold(
      appBar: AppBar(
        title: const Text('LED Saber Controller'),
        centerTitle: true,
      ),
      body: SafeArea(
        child: Column(
          children: [
            // Stato connessione
            _buildConnectionStatus(bleProvider, ledProvider),

            const SizedBox(height: 20),

            // Bottone Scan
            _buildScanButton(bleProvider),

            const SizedBox(height: 20),

            // Lista dispositivi trovati
            Expanded(
              child: _buildDevicesList(bleProvider, ledProvider),
            ),
          ],
        ),
      ),
    );
  }

  /// Widget stato connessione
  Widget _buildConnectionStatus(BleProvider bleProvider, LedProvider ledProvider) {
    final isConnected = bleProvider.isConnected;
    final connectedDevice = bleProvider.connectedDevice;

    return Container(
      padding: const EdgeInsets.all(16),
      margin: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: isConnected ? Colors.green.shade100 : Colors.grey.shade200,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(
          color: isConnected ? Colors.green : Colors.grey,
          width: 2,
        ),
      ),
      child: Row(
        children: [
          Icon(
            isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
            color: isConnected ? Colors.green : Colors.grey,
            size: 32,
          ),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  isConnected ? 'Connesso' : 'Disconnesso',
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                    color: isConnected ? Colors.green.shade900 : Colors.grey.shade700,
                  ),
                ),
                if (isConnected && connectedDevice != null)
                  Text(
                    connectedDevice.platformName,
                    style: TextStyle(
                      fontSize: 14,
                      color: Colors.grey.shade700,
                    ),
                  ),
              ],
            ),
          ),
          if (isConnected)
            IconButton(
              icon: const Icon(Icons.close),
              onPressed: () {
                ledProvider.setLedService(null);
                bleProvider.disconnect();
              },
              tooltip: 'Disconnetti',
            ),
        ],
      ),
    );
  }

  /// Widget bottone scan
  Widget _buildScanButton(BleProvider bleProvider) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: SizedBox(
        width: double.infinity,
        height: 50,
        child: ElevatedButton.icon(
          onPressed: bleProvider.isScanning
              ? null
              : () => _startScan(bleProvider),
          icon: bleProvider.isScanning
              ? const SizedBox(
                  width: 20,
                  height: 20,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              : const Icon(Icons.search),
          label: Text(
            bleProvider.isScanning ? 'Scansione in corso...' : 'Cerca Dispositivi',
            style: const TextStyle(fontSize: 16),
          ),
          style: ElevatedButton.styleFrom(
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
            ),
          ),
        ),
      ),
    );
  }

  /// Widget lista dispositivi
  Widget _buildDevicesList(BleProvider bleProvider, LedProvider ledProvider) {
    final devices = bleProvider.discoveredDevices;

    if (devices.isEmpty && !bleProvider.isScanning) {
      return const Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.bluetooth_searching, size: 64, color: Colors.grey),
            SizedBox(height: 16),
            Text(
              'Nessun dispositivo trovato',
              style: TextStyle(fontSize: 16, color: Colors.grey),
            ),
            SizedBox(height: 8),
            Text(
              'Premi "Cerca Dispositivi" per iniziare',
              style: TextStyle(fontSize: 14, color: Colors.grey),
            ),
          ],
        ),
      );
    }

    return ListView.builder(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      itemCount: devices.length,
      itemBuilder: (context, index) {
        final device = devices[index];
        return _buildDeviceCard(device, bleProvider, ledProvider);
      },
    );
  }

  /// Widget card dispositivo
  Widget _buildDeviceCard(
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
            Text('ID: ${device.id}'),
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
