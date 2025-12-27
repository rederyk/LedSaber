import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../providers/ble_provider.dart';
import '../models/connected_device.dart';
import 'package:intl/intl.dart';

/// Schermata che mostra i dispositivi connessi e permette di switchare tra loro
class DeviceListScreen extends StatelessWidget {
  const DeviceListScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final bleProvider = Provider.of<BleProvider>(context);
    final connectedDevices = bleProvider.connectedDevices;

    return Scaffold(
      appBar: AppBar(
        title: Text('Dispositivi Connessi (${connectedDevices.length})'),
        actions: [
          if (connectedDevices.isNotEmpty)
            IconButton(
              icon: const Icon(Icons.delete_sweep),
              onPressed: () => _showDisconnectAllDialog(context, bleProvider),
              tooltip: 'Disconnetti tutti',
            ),
        ],
      ),
      body: connectedDevices.isEmpty
          ? _buildEmptyState(context)
          : _buildDeviceList(context, bleProvider, connectedDevices),
    );
  }

  /// Stato vuoto quando non ci sono devices connessi
  Widget _buildEmptyState(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(
            Icons.bluetooth_disabled,
            size: 64,
            color: Colors.grey.withValues(alpha: 0.5),
          ),
          const SizedBox(height: 16),
          Text(
            'Nessun dispositivo connesso',
            style: Theme.of(context).textTheme.titleLarge?.copyWith(
                  color: Colors.grey,
                ),
          ),
          const SizedBox(height: 8),
          Text(
            'Torna alla home per connettere\nun LED Saber',
            textAlign: TextAlign.center,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: Colors.grey,
                ),
          ),
          const SizedBox(height: 24),
          ElevatedButton.icon(
            onPressed: () => Navigator.pop(context),
            icon: const Icon(Icons.arrow_back),
            label: const Text('Torna alla Home'),
          ),
        ],
      ),
    );
  }

  /// Lista dei dispositivi connessi
  Widget _buildDeviceList(
    BuildContext context,
    BleProvider bleProvider,
    List<ConnectedDevice> devices,
  ) {
    return ListView.builder(
      padding: const EdgeInsets.all(16),
      itemCount: devices.length,
      itemBuilder: (context, index) {
        final device = devices[index];
        return _buildDeviceCard(context, bleProvider, device);
      },
    );
  }

  /// Card per un singolo dispositivo
  Widget _buildDeviceCard(
    BuildContext context,
    BleProvider bleProvider,
    ConnectedDevice device,
  ) {
    final isActive = device.isActive;
    final timeFormatter = DateFormat('HH:mm:ss');
    final connectedTime = timeFormatter.format(device.connectedAt);

    return Card(
      elevation: isActive ? 4 : 1,
      margin: const EdgeInsets.only(bottom: 12),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: BorderSide(
          color: isActive
              ? Theme.of(context).colorScheme.primary
              : Colors.grey.withValues(alpha: 0.2),
          width: isActive ? 2 : 1,
        ),
      ),
      child: InkWell(
        onTap: isActive
            ? null
            : () {
                bleProvider.setActiveDevice(device.id);
                Navigator.pop(context);
              },
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  // Icona device
                  Container(
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: isActive
                          ? Theme.of(context).colorScheme.primary.withValues(alpha: 0.1)
                          : Colors.grey.withValues(alpha: 0.1),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Icon(
                      Icons.bluetooth_connected,
                      color: isActive
                          ? Theme.of(context).colorScheme.primary
                          : Colors.grey,
                      size: 28,
                    ),
                  ),
                  const SizedBox(width: 16),

                  // Nome e info device
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Expanded(
                              child: Text(
                                device.name,
                                style: Theme.of(context).textTheme.titleMedium?.copyWith(
                                      fontWeight: isActive ? FontWeight.bold : FontWeight.normal,
                                    ),
                              ),
                            ),
                            if (isActive)
                              Container(
                                padding: const EdgeInsets.symmetric(
                                  horizontal: 8,
                                  vertical: 4,
                                ),
                                decoration: BoxDecoration(
                                  color: Theme.of(context).colorScheme.primary,
                                  borderRadius: BorderRadius.circular(12),
                                ),
                                child: Text(
                                  'ATTIVO',
                                  style: Theme.of(context).textTheme.labelSmall?.copyWith(
                                        color: Colors.white,
                                        fontWeight: FontWeight.bold,
                                      ),
                                ),
                              ),
                          ],
                        ),
                        const SizedBox(height: 4),
                        Text(
                          'ID: ${device.id.substring(0, 17)}...',
                          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                                color: Colors.grey,
                                fontFamily: 'JetBrainsMono',
                              ),
                        ),
                        const SizedBox(height: 2),
                        Text(
                          'Connesso alle $connectedTime',
                          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                                color: Colors.grey,
                              ),
                        ),
                      ],
                    ),
                  ),

                  // Bottone disconnect
                  IconButton(
                    icon: const Icon(Icons.close),
                    color: Colors.red,
                    onPressed: () => _showDisconnectDialog(
                      context,
                      bleProvider,
                      device,
                    ),
                    tooltip: 'Disconnetti',
                  ),
                ],
              ),

              // Azioni rapide (solo se attivo)
              if (isActive) ...[
                const SizedBox(height: 12),
                const Divider(),
                const SizedBox(height: 8),
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _buildQuickAction(
                      context,
                      Icons.lightbulb_outline,
                      'Controlli',
                      () {
                        Navigator.pop(context);
                      },
                    ),
                    _buildQuickAction(
                      context,
                      Icons.info_outline,
                      'Info',
                      () {
                        _showDeviceInfo(context, device);
                      },
                    ),
                  ],
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }

  /// Bottone azione rapida
  Widget _buildQuickAction(
    BuildContext context,
    IconData icon,
    String label,
    VoidCallback onTap,
  ) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(8),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        child: Column(
          children: [
            Icon(icon, size: 24),
            const SizedBox(height: 4),
            Text(
              label,
              style: Theme.of(context).textTheme.labelSmall,
            ),
          ],
        ),
      ),
    );
  }

  /// Dialog conferma disconnessione singolo device
  void _showDisconnectDialog(
    BuildContext context,
    BleProvider bleProvider,
    ConnectedDevice device,
  ) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Disconnetti dispositivo'),
        content: Text(
          'Vuoi disconnettere "${device.name}"?\n\nPotrai riconnetterti in seguito dalla schermata principale.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Annulla'),
          ),
          ElevatedButton(
            onPressed: () async {
              Navigator.pop(context);
              await bleProvider.disconnectDevice(device.id);

              if (context.mounted) {
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(
                    content: Text('${device.name} disconnesso'),
                  ),
                );

                // Se non ci sono più devices, torna alla home
                if (bleProvider.deviceCount == 0 && context.mounted) {
                  Navigator.pop(context);
                }
              }
            },
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
            ),
            child: const Text('Disconnetti'),
          ),
        ],
      ),
    );
  }

  /// Dialog conferma disconnessione tutti i devices
  void _showDisconnectAllDialog(
    BuildContext context,
    BleProvider bleProvider,
  ) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Disconnetti tutti'),
        content: Text(
          'Vuoi disconnettere tutti i ${bleProvider.deviceCount} dispositivi connessi?\n\nPotrai riconnetterti in seguito dalla schermata principale.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Annulla'),
          ),
          ElevatedButton(
            onPressed: () async {
              Navigator.pop(context);
              await bleProvider.disconnectAll();

              if (context.mounted) {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(
                    content: Text('Tutti i dispositivi disconnessi'),
                  ),
                );

                // Torna alla home
                Navigator.pop(context);
              }
            },
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
            ),
            child: const Text('Disconnetti tutti'),
          ),
        ],
      ),
    );
  }

  /// Mostra informazioni dettagliate del device
  void _showDeviceInfo(BuildContext context, ConnectedDevice device) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(device.name),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _buildInfoRow('Device ID', device.id),
            const SizedBox(height: 8),
            _buildInfoRow(
              'Connesso',
              DateFormat('dd/MM/yyyy HH:mm:ss').format(device.connectedAt),
            ),
            const SizedBox(height: 8),
            _buildInfoRow(
              'Stato',
              device.isActive ? 'Attivo' : 'In background',
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Chiudi'),
          ),
        ],
      ),
    );
  }

  /// Riga informazione
  Widget _buildInfoRow(String label, String value) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        SizedBox(
          width: 100,
          child: Text(
            '$label:',
            style: const TextStyle(
              fontWeight: FontWeight.bold,
            ),
          ),
        ),
        Expanded(
          child: Text(
            value,
            style: const TextStyle(
              fontFamily: 'JetBrainsMono',
              fontSize: 12,
            ),
          ),
        ),
      ],
    );
  }
}
