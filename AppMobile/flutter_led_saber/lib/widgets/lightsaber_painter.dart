import 'package:flutter/material.dart';
import 'dart:ui' as ui;

/// CustomPainter per renderizzare la spada laser
/// Disegna: lama con glow triplo, elsa metallica, decorazioni
class LightsaberPainter extends CustomPainter {
  final Color bladeColor;
  final double brightness;
  final double bladeHeightFactor; // 0.0-1.0 per animazione
  final double pulseOpacity; // 0.8-1.0 per pulse effect
  final String bladeState;
  final bool isConnected;

  LightsaberPainter({
    required this.bladeColor,
    required this.brightness,
    required this.bladeHeightFactor,
    required this.pulseOpacity,
    required this.bladeState,
    required this.isConnected,
  });

  // Dimensioni componenti
  static const double hiltWidth = 70;
  static const double hiltHeight = 150;
  static const double bladeWidth = 60;
  static const double bladeTopRadius = 30;
  static const double bladeBottomRadius = 5;

  @override
  void paint(Canvas canvas, Size size) {
    final centerX = size.width / 2;

    // Calcola posizioni Y (dal basso verso l'alto)
    final hiltBottom = size.height;
    final hiltTop = hiltBottom - hiltHeight;
    final bladeMaxHeight = size.height * 0.7; // 70% dello spazio
    final bladeCurrentHeight = bladeMaxHeight * bladeHeightFactor;

    // 1. Disegna lama (se bladeHeightFactor > 0)
    if (bladeHeightFactor > 0.01) {
      _drawBlade(
        canvas,
        centerX,
        hiltTop,
        bladeCurrentHeight,
      );
    }

    // 2. Disegna elsa (hilt)
    _drawHilt(canvas, centerX, hiltTop, hiltBottom);

    // 3. Disegna decorazioni elsa
    _drawHiltDecorations(canvas, centerX, hiltTop, hiltBottom);
  }

  /// Disegna la lama con glow effect triplo
  void _drawBlade(Canvas canvas, double centerX, double hiltTop, double currentHeight) {
    final bladeBottom = hiltTop;
    final bladeTop = bladeBottom - currentHeight;

    // Calcola colore effettivo con brightness
    final effectiveColor = Color.lerp(
      Colors.black,
      bladeColor,
      brightness,
    )!;

    // Layer 1: Outer Glow (più esterno)
    _drawGlowLayer(
      canvas,
      centerX,
      bladeTop,
      bladeBottom,
      effectiveColor,
      width: 80,
      opacity: 0.3 * pulseOpacity,
      blurRadius: 20,
    );

    // Layer 2: Mid Glow
    _drawGlowLayer(
      canvas,
      centerX,
      bladeTop,
      bladeBottom,
      effectiveColor,
      width: 40,
      opacity: 0.7 * pulseOpacity,
      blurRadius: 10,
    );

    // Layer 3: Inner Core (bianco brillante)
    _drawGlowLayer(
      canvas,
      centerX,
      bladeTop,
      bladeBottom,
      Colors.white,
      width: 20,
      opacity: 1.0,
      blurRadius: 0,
    );

    // Layer 4: Lama principale (sopra tutti i glow)
    _drawMainBlade(canvas, centerX, bladeTop, bladeBottom, effectiveColor);
  }

  /// Disegna un layer di glow
  void _drawGlowLayer(
    Canvas canvas,
    double centerX,
    double top,
    double bottom,
    Color color,
    {
    required double width,
    required double opacity,
    required double blurRadius,
  }) {
    final rect = RRect.fromRectAndCorners(
      Rect.fromCenter(
        center: Offset(centerX, (top + bottom) / 2),
        width: width,
        height: bottom - top,
      ),
      topLeft: Radius.circular(bladeTopRadius),
      topRight: Radius.circular(bladeTopRadius),
      bottomLeft: Radius.circular(bladeBottomRadius),
      bottomRight: Radius.circular(bladeBottomRadius),
    );

    final paint = Paint()
      ..color = color.withValues(alpha: opacity)
      ..style = PaintingStyle.fill;

    if (blurRadius > 0) {
      paint.maskFilter = ui.MaskFilter.blur(ui.BlurStyle.normal, blurRadius);
    }

    canvas.drawRRect(rect, paint);
  }

  /// Disegna la lama principale con gradiente
  void _drawMainBlade(Canvas canvas, double centerX, double top, double bottom, Color color) {
    final rect = RRect.fromRectAndCorners(
      Rect.fromCenter(
        center: Offset(centerX, (top + bottom) / 2),
        width: bladeWidth,
        height: bottom - top,
      ),
      topLeft: Radius.circular(bladeTopRadius),
      topRight: Radius.circular(bladeTopRadius),
      bottomLeft: Radius.circular(bladeBottomRadius),
      bottomRight: Radius.circular(bladeBottomRadius),
    );

    // Gradiente verticale (bottom → top)
    // Base: colore pieno, Centro: bianco brillante, Punta: fade-out
    final gradient = ui.Gradient.linear(
      Offset(centerX, bottom), // Start (base)
      Offset(centerX, top), // End (punta)
      [
        color, // Base: colore pieno
        Color.lerp(color, Colors.white, 0.7)!, // Centro: blend con bianco
        color.withValues(alpha: 0.3), // Punta: fade-out
      ],
      [0.0, 0.5, 1.0], // Stop positions
    );

    final paint = Paint()
      ..shader = gradient
      ..style = PaintingStyle.fill;

    canvas.drawRRect(rect, paint);
  }

  /// Disegna l'elsa (hilt) con gradiente metallico
  void _drawHilt(Canvas canvas, double centerX, double top, double bottom) {
    final rect = RRect.fromRectAndRadius(
      Rect.fromCenter(
        center: Offset(centerX, (top + bottom) / 2),
        width: hiltWidth,
        height: hiltHeight,
      ),
      const Radius.circular(8),
    );

    // Gradiente metallico verticale
    final gradient = ui.Gradient.linear(
      Offset(centerX, top),
      Offset(centerX, bottom),
      const [
        Color(0xFF2A2A2A), // Base scura
        Color(0xFF4A4A4A), // Centro chiaro
        Color(0xFF2A2A2A), // Bottom scuro
      ],
      [0.0, 0.5, 1.0],
    );

    final paint = Paint()
      ..shader = gradient
      ..style = PaintingStyle.fill;

    // Shadow per effetto 3D
    final shadowPaint = Paint()
      ..color = Colors.black.withValues(alpha: 0.5)
      ..maskFilter = const ui.MaskFilter.blur(ui.BlurStyle.normal, 8);

    canvas.drawRRect(rect.shift(const Offset(0, 4)), shadowPaint);
    canvas.drawRRect(rect, paint);

    // Bordo sottile
    final borderPaint = Paint()
      ..color = const Color(0xFF1A1A1A)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2;

    canvas.drawRRect(rect, borderPaint);
  }

  /// Disegna decorazioni dell'elsa
  void _drawHiltDecorations(Canvas canvas, double centerX, double top, double bottom) {
    final hiltCenter = (top + bottom) / 2;

    // 1. Texture grip (linee orizzontali)
    _drawGripLines(canvas, centerX, top, bottom);

    // 2. Badge BLE connessione (in alto a destra)
    _drawBLEBadge(canvas, centerX, top);

    // 3. Decorazioni verticali centrali (│││)
    _drawCenterDecorations(canvas, centerX, hiltCenter);

    // 4. Separator lines (top e bottom)
    _drawSeparators(canvas, centerX, top, bottom);
  }

  /// Disegna linee grip texture
  void _drawGripLines(Canvas canvas, double centerX, double top, double bottom) {
    final paint = Paint()
      ..color = const Color(0xFF1A1A1A)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1;

    const spacing = 10.0;
    final startY = top + 20;
    final endY = bottom - 20;

    for (double y = startY; y < endY; y += spacing) {
      canvas.drawLine(
        Offset(centerX - hiltWidth / 2 + 10, y),
        Offset(centerX + hiltWidth / 2 - 10, y),
        paint,
      );
    }
  }

  /// Disegna badge connessione BLE
  void _drawBLEBadge(Canvas canvas, double centerX, double top) {
    final badgeColor = isConnected
        ? const Color(0xFF00FF88) // Verde connesso
        : const Color(0xFFFF3B30); // Rosso disconnesso

    final badgeCenter = Offset(centerX + hiltWidth / 2 - 10, top + 10);

    final paint = Paint()
      ..color = badgeColor
      ..style = PaintingStyle.fill;

    canvas.drawCircle(badgeCenter, 4, paint);

    // Glow sul badge
    if (isConnected) {
      final glowPaint = Paint()
        ..color = badgeColor.withValues(alpha: 0.5)
        ..maskFilter = const ui.MaskFilter.blur(ui.BlurStyle.normal, 4);
      canvas.drawCircle(badgeCenter, 6, glowPaint);
    }
  }

  /// Disegna decorazioni verticali centrali
  void _drawCenterDecorations(Canvas canvas, double centerX, double centerY) {
    final paint = Paint()
      ..color = const Color(0xFF666666)
      ..style = PaintingStyle.fill;

    const rectWidth = 3.0;
    const rectHeight = 30.0;
    const spacing = 8.0;

    for (int i = -1; i <= 1; i++) {
      final rect = RRect.fromRectAndRadius(
        Rect.fromCenter(
          center: Offset(centerX + i * spacing, centerY),
          width: rectWidth,
          height: rectHeight,
        ),
        const Radius.circular(1.5),
      );
      canvas.drawRRect(rect, paint);
    }
  }

  /// Disegna linee separator top/bottom
  void _drawSeparators(Canvas canvas, double centerX, double top, double bottom) {
    final paint = Paint()
      ..color = const Color(0xFF666666)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.5;

    // Top separator
    canvas.drawLine(
      Offset(centerX - hiltWidth / 2 + 5, top),
      Offset(centerX + hiltWidth / 2 - 5, top),
      paint,
    );

    // Bottom separator
    canvas.drawLine(
      Offset(centerX - hiltWidth / 2 + 5, bottom),
      Offset(centerX + hiltWidth / 2 - 5, bottom),
      paint,
    );
  }

  @override
  bool shouldRepaint(LightsaberPainter oldDelegate) {
    return oldDelegate.bladeColor != bladeColor ||
        oldDelegate.brightness != brightness ||
        oldDelegate.bladeHeightFactor != bladeHeightFactor ||
        oldDelegate.pulseOpacity != pulseOpacity ||
        oldDelegate.bladeState != bladeState ||
        oldDelegate.isConnected != isConnected;
  }
}
