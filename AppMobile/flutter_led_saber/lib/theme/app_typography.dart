import 'package:flutter/material.dart';

/// Tipografia centralizzata per l'app LED Saber
/// Font custom: Orbitron (titoli), Inter (body), JetBrains Mono (codici)
class AppTypography {
  AppTypography._();

  // ========== FONT FAMILIES ==========

  static const String orbitron = 'Orbitron';
  static const String inter = 'Inter';
  static const String jetBrainsMono = 'JetBrainsMono';

  // ========== TITOLI (Orbitron - Sci-fi) ==========

  /// Titolo grande - 28px Bold
  static const TextStyle headlineLarge = TextStyle(
    fontFamily: orbitron,
    fontSize: 28,
    fontWeight: FontWeight.bold,
    letterSpacing: 0.5,
  );

  /// Titolo medio - 20px SemiBold
  static const TextStyle headlineMedium = TextStyle(
    fontFamily: orbitron,
    fontSize: 20,
    fontWeight: FontWeight.w600,
    letterSpacing: 0.3,
  );

  /// Titolo piccolo - 16px SemiBold
  static const TextStyle headlineSmall = TextStyle(
    fontFamily: orbitron,
    fontSize: 16,
    fontWeight: FontWeight.w600,
    letterSpacing: 0.2,
  );

  // ========== BODY (Inter - Leggibile) ==========

  /// Body grande - 16px Regular
  static const TextStyle bodyLarge = TextStyle(
    fontFamily: inter,
    fontSize: 16,
    fontWeight: FontWeight.normal,
    letterSpacing: 0,
  );

  /// Body medio - 14px Regular
  static const TextStyle bodyMedium = TextStyle(
    fontFamily: inter,
    fontSize: 14,
    fontWeight: FontWeight.normal,
    letterSpacing: 0,
  );

  /// Body piccolo - 12px Regular
  static const TextStyle bodySmall = TextStyle(
    fontFamily: inter,
    fontSize: 12,
    fontWeight: FontWeight.normal,
    letterSpacing: 0,
  );

  // ========== LABELS ==========

  /// Label grande - 14px SemiBold
  static const TextStyle labelLarge = TextStyle(
    fontFamily: inter,
    fontSize: 14,
    fontWeight: FontWeight.w600,
    letterSpacing: 0.1,
  );

  /// Label medio - 12px SemiBold
  static const TextStyle labelMedium = TextStyle(
    fontFamily: inter,
    fontSize: 12,
    fontWeight: FontWeight.w600,
    letterSpacing: 0.1,
  );

  /// Label piccolo - 10px SemiBold
  static const TextStyle labelSmall = TextStyle(
    fontFamily: inter,
    fontSize: 10,
    fontWeight: FontWeight.w600,
    letterSpacing: 0.1,
  );

  // ========== MONOSPACE (JetBrains Mono - Codici RGB/HEX) ==========

  /// Codice - 14px Regular
  static const TextStyle code = TextStyle(
    fontFamily: jetBrainsMono,
    fontSize: 14,
    fontWeight: FontWeight.normal,
    letterSpacing: 0,
  );

  /// Codice grande - 16px Regular
  static const TextStyle codeLarge = TextStyle(
    fontFamily: jetBrainsMono,
    fontSize: 16,
    fontWeight: FontWeight.normal,
    letterSpacing: 0,
  );

  // ========== BUTTON TEXT ==========

  /// Button text - 14px SemiBold (Inter)
  static const TextStyle button = TextStyle(
    fontFamily: inter,
    fontSize: 14,
    fontWeight: FontWeight.w600,
    letterSpacing: 0.2,
  );

  /// Button text large - 16px SemiBold (Inter)
  static const TextStyle buttonLarge = TextStyle(
    fontFamily: inter,
    fontSize: 16,
    fontWeight: FontWeight.w600,
    letterSpacing: 0.3,
  );
}
