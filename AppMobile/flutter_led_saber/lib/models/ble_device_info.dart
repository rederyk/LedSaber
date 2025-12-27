import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// Modello per informazioni sul dispositivo BLE
class BleDeviceInfo {
  final BluetoothDevice device;
  final String name;
  final String id;
  final int rssi;

  BleDeviceInfo({
    required this.device,
    required this.name,
    required this.id,
    this.rssi = 0,
  });

  /// Crea un BleDeviceInfo da un BluetoothDevice
  factory BleDeviceInfo.fromDevice(BluetoothDevice device, {int rssi = 0}) {
    return BleDeviceInfo(
      device: device,
      name: device.platformName.isNotEmpty
          ? device.platformName
          : 'Unknown Device',
      id: device.remoteId.toString(),
      rssi: rssi,
    );
  }

  /// Verifica se è un dispositivo LedSaber
  bool get isLedSaber =>
      name.toLowerCase().contains('ledsaber') ||
      name.toLowerCase().contains('led_saber') ||
      name.toLowerCase().contains('led saber');

  /// Ottiene la qualità del segnale come stringa
  String get signalQuality {
    if (rssi >= -50) return 'Eccellente';
    if (rssi >= -60) return 'Buono';
    if (rssi >= -70) return 'Discreto';
    if (rssi >= -80) return 'Debole';
    return 'Molto debole';
  }

  @override
  String toString() {
    return 'BleDeviceInfo(name: $name, id: $id, rssi: $rssi dBm)';
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is BleDeviceInfo &&
          runtimeType == other.runtimeType &&
          id == other.id;

  @override
  int get hashCode => id.hashCode;
}
