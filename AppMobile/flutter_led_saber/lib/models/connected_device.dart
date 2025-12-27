import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../services/led_service.dart';

/// Stato della connessione del dispositivo
enum ConnectionStatus {
  connected,    // Verde: Connesso e funzionante
  unstable,     // Giallo: Connesso ma segnale debole
  disconnected, // Rosso: Disconnesso
}

/// Rappresenta un dispositivo LED Saber connesso
class ConnectedDevice {
  final String id; // remoteId del device
  final String name;
  final BluetoothDevice device;
  final LedService ledService;
  final DateTime connectedAt;
  bool isActive; // Device correntemente visualizzato
  ConnectionStatus connectionStatus; // Stato connessione
  DateTime? lastSeen; // Ultimo heartbeat

  ConnectedDevice({
    required this.id,
    required this.name,
    required this.device,
    required this.ledService,
    required this.connectedAt,
    this.isActive = false,
    this.connectionStatus = ConnectionStatus.connected,
    DateTime? lastSeen,
  }) : lastSeen = lastSeen ?? DateTime.now();

  /// Converte in Map per salvataggio in SharedPreferences
  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'name': name,
      'connectedAt': connectedAt.toIso8601String(),
      'isActive': isActive,
    };
  }

  /// Crea da Map (per reload da SharedPreferences)
  factory ConnectedDevice.fromJson(
    Map<String, dynamic> json,
    BluetoothDevice device,
    LedService ledService,
  ) {
    return ConnectedDevice(
      id: json['id'] as String,
      name: json['name'] as String,
      device: device,
      ledService: ledService,
      connectedAt: DateTime.parse(json['connectedAt'] as String),
      isActive: json['isActive'] as bool? ?? false,
    );
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      other is ConnectedDevice &&
          runtimeType == other.runtimeType &&
          id == other.id;

  @override
  int get hashCode => id.hashCode;
}
