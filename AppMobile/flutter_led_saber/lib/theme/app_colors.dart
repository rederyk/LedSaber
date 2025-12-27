import 'package:flutter/material.dart';

/// Palette colori centralizzata per l'app LED Saber
/// Basata sul design plan UI_DESIGN_PLAN.md
class AppColors {
  AppColors._();

  // ========== DARK MODE (Preferito) ==========

  /// Background principale - Nero profondo
  static const Color darkScaffoldBackground = Color(0xFF0D0D0D);

  /// Background card - Grigio scuro
  static const Color darkCardBackground = Color(0xFF1A1A1A);

  /// Primary accent - Rosso Sith
  static const Color darkPrimary = Color(0xFFFF3B30);

  /// Secondary accent - Azzurro elettrico
  static const Color darkSecondary = Color(0xFF00D4FF);

  /// Success - Verde neon
  static const Color darkSuccess = Color(0xFF00FF88);

  /// Warning - Giallo
  static const Color darkWarning = Color(0xFFFFCC00);

  /// Error - Rosso
  static const Color darkError = Color(0xFFFF3B30);

  /// Testo primario - Bianco
  static const Color darkTextPrimary = Color(0xFFFFFFFF);

  /// Testo secondario - Grigio chiaro
  static const Color darkTextSecondary = Color(0xFFAAAAAA);

  // ========== LIGHT MODE ==========

  /// Background principale - Grigio Apple
  static const Color lightScaffoldBackground = Color(0xFFF5F5F7);

  /// Background card - Bianco
  static const Color lightCardBackground = Color(0xFFFFFFFF);

  /// Primary accent - Rosso Sith (uguale a dark)
  static const Color lightPrimary = Color(0xFFFF3B30);

  /// Secondary accent - Blu Apple
  static const Color lightSecondary = Color(0xFF007AFF);

  /// Success - Verde
  static const Color lightSuccess = Color(0xFF34C759);

  /// Warning - Arancione
  static const Color lightWarning = Color(0xFFFF9500);

  /// Error - Rosso
  static const Color lightError = Color(0xFFFF3B30);

  /// Testo primario - Nero
  static const Color lightTextPrimary = Color(0xFF000000);

  /// Testo secondario - Grigio scuro
  static const Color lightTextSecondary = Color(0xFF666666);

  // ========== COLORI ELSA (Hilt) ==========

  /// Gradiente metallico - Base scura
  static const Color hiltBase = Color(0xFF2A2A2A);

  /// Gradiente metallico - Centro più chiaro
  static const Color hiltMid = Color(0xFF4A4A4A);

  // ========== COLORI POWER BUTTON ==========

  /// Bordo power button - OFF (rosso)
  static const Color powerButtonOff = Color(0xFFFF3B30);

  /// Bordo power button - ON (verde neon)
  static const Color powerButtonOn = Color(0xFF00FF88);

  /// Bordo power button - ANIMATING (arancione)
  static const Color powerButtonAnimating = Color(0xFFFFCC00);

  // ========== COLORI BLE CONNECTION ==========

  /// BLE connesso (verde)
  static const Color bleConnected = Color(0xFF00FF88);

  /// BLE disconnesso (rosso)
  static const Color bleDisconnected = Color(0xFFFF3B30);
}
