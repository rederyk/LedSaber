import 'package:flutter/material.dart';
import 'lightsaber_painter.dart';
import '../providers/led_provider.dart';

/// Widget custom della spada laser
/// Renderizza elsa, lama con glow effect triplo, power button e animazioni
class LightsaberWidget extends StatefulWidget {
  /// Colore RGB della lama
  final Color bladeColor;

  /// Luminosità (0.0-1.0)
  final double brightness;

  /// Stato della lama: "off" | "igniting" | "on" | "retracting"
  final String bladeState;

  /// Effetto corrente (per future implementazioni visive)
  final String currentEffect;

  /// Callback quando si preme il power button
  final VoidCallback? onPowerTap;

  /// Callback quando si tocca sulla lama
  final VoidCallback? onBladeTap;

  /// Callback quando si fa double tap sulla lama
  final VoidCallback? onBladeDoubleTap;

  /// Callback quando si fa pan sulla lama (con posizione verticale 0.0-1.0)
  final Function(double)? onBladePan;

  /// Altezza massima disponibile per il widget
  final double maxHeight;

  /// Se il dispositivo è connesso via BLE (per badge)
  final bool isConnected;

  /// Se è attiva la modalità preview
  final bool isPreviewMode;

  /// Modalità del color picker
  final BladeColorPickerMode pickerMode;

  const LightsaberWidget({
    super.key,
    required this.bladeColor,
    required this.brightness,
    required this.bladeState,
    required this.currentEffect,
    this.onPowerTap,
    this.onBladeTap,
    this.onBladeDoubleTap,
    this.onBladePan,
    required this.maxHeight,
    this.isConnected = false,
    this.isPreviewMode = false,
    this.pickerMode = BladeColorPickerMode.saturation,
  });

  @override
  State<LightsaberWidget> createState() => _LightsaberWidgetState();
}

class _LightsaberWidgetState extends State<LightsaberWidget>
    with TickerProviderStateMixin {
  /// Controller per animazione ignition/retract
  late AnimationController _ignitionController;

  /// Animazione altezza lama (0.0 → 1.0)
  late Animation<double> _bladeHeightAnimation;

  /// Controller per animazione pulse (quando lama è ON)
  late AnimationController _pulseController;

  /// Animazione pulse glow opacity
  late Animation<double> _pulseAnimation;

  /// Ultimo stato processato per evitare duplicati
  String _lastProcessedState = '';

  @override
  void initState() {
    super.initState();

    // Setup ignition controller
    _ignitionController = AnimationController(
      duration: const Duration(milliseconds: 1500), // Ignition: 1.5s
      reverseDuration: const Duration(milliseconds: 1000), // Retract: 1.0s
      vsync: this,
    );

    _bladeHeightAnimation = Tween<double>(
      begin: 0.0, // Lama nascosta
      end: 1.0, // Lama completa
    ).animate(CurvedAnimation(
      parent: _ignitionController,
      curve: Curves.easeOutCubic, // Ignition smooth
      reverseCurve: Curves.easeInCubic, // Retract più rapido
    ));

    // Listener per aggiornare _lastProcessedState quando l'animazione finisce
    _ignitionController.addStatusListener((status) {
      if (status == AnimationStatus.completed) {
        _lastProcessedState = 'on';
      } else if (status == AnimationStatus.dismissed) {
        _lastProcessedState = 'off';
      }
    });

    // Setup pulse controller (loop infinito)
    _pulseController = AnimationController(
      duration: const Duration(milliseconds: 2000), // Pulse cycle: 2s
      vsync: this,
    )..repeat(reverse: true);

    _pulseAnimation = Tween<double>(
      begin: 0.8, // Glow minimo
      end: 1.0, // Glow massimo
    ).animate(CurvedAnimation(
      parent: _pulseController,
      curve: Curves.easeInOut,
    ));

    // Inizializza stato in base a bladeState corrente
    _updateAnimationState(widget.bladeState);
  }

  @override
  void didUpdateWidget(LightsaberWidget oldWidget) {
    super.didUpdateWidget(oldWidget);

    // Reagisci a cambi di bladeState solo se effettivamente diverso
    // e non stiamo già animando verso quello stato
    if (oldWidget.bladeState != widget.bladeState &&
        widget.bladeState != _lastProcessedState) {
      _updateAnimationState(widget.bladeState);
    }
  }

  /// Aggiorna stato animazione in base a bladeState
  void _updateAnimationState(String state) {
    switch (state) {
      case 'igniting':
        // Avvia animazione ignition solo se non già in corso o completata
        if (!_ignitionController.isAnimating && !_ignitionController.isCompleted) {
          _lastProcessedState = state;
          _ignitionController.forward();
        }
        break;
      case 'retracting':
        // Avvia animazione retraction solo se non già in corso o a zero
        if (!_ignitionController.isAnimating && !_ignitionController.isDismissed) {
          _lastProcessedState = state;
          _ignitionController.reverse();
        }
        break;
      case 'on':
        // Se non è già ON, completa l'animazione
        if (_lastProcessedState != 'on') {
          _lastProcessedState = state;
          if (!_ignitionController.isCompleted) {
            _ignitionController.forward();
          }
        }
        break;
      case 'off':
        // Se non è già OFF, completa l'animazione
        if (_lastProcessedState != 'off') {
          _lastProcessedState = state;
          if (!_ignitionController.isDismissed) {
            _ignitionController.reverse();
          }
        }
        break;
    }
  }

  @override
  void dispose() {
    _ignitionController.dispose();
    _pulseController.dispose();
    super.dispose();
  }

  /// Verifica se il tap è sulla lama
  bool _isTapOnBlade(Offset localPosition, Size widgetSize) {
    final centerX = widgetSize.width / 2;
    final hiltHeight = 150.0;
    final hiltBottom = widgetSize.height;
    final hiltTop = hiltBottom - hiltHeight;
    final bladeMaxHeight = widgetSize.height * 0.7;
    final bladeCurrentHeight = bladeMaxHeight * _bladeHeightAnimation.value;
    final bladeBottom = hiltTop;
    final bladeTop = bladeBottom - bladeCurrentHeight;
    final bladeWidth = 80.0; // Include glow esterno

    // Controlla se il punto è nella regione della lama
    final isInBladeX = (localPosition.dx - centerX).abs() <= bladeWidth / 2;
    final isInBladeY = localPosition.dy >= bladeTop && localPosition.dy <= bladeBottom;

    return isInBladeX && isInBladeY && _bladeHeightAnimation.value > 0.01;
  }

  @override
  Widget build(BuildContext context) {
    return RepaintBoundary(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.end,
        children: [
          // Spada (lama + elsa + power button)
          Expanded(
            child: GestureDetector(
              onTapUp: (details) {
                final RenderBox box = context.findRenderObject() as RenderBox;
                final localPosition = box.globalToLocal(details.globalPosition);
                final isBlade = _isTapOnBlade(localPosition, box.size);

                if (isBlade) {
                  if (widget.isPreviewMode) {
                    // Tap sulla lama con picker attivo: seleziona colore in quel punto
                    if (widget.onBladePan != null) {
                      final relativeY = (localPosition.dy / box.size.height).clamp(0.0, 1.0);
                      widget.onBladePan!(relativeY);
                    }
                  } else {
                    // Tap sulla lama con picker non attivo: attiva il picker
                    if (widget.onBladeTap != null) {
                      widget.onBladeTap!();
                    }
                  }
                } else {
                  // Tap fuori dalla lama: disattiva color picker se attivo
                  if (widget.isPreviewMode && widget.onBladeTap != null) {
                    widget.onBladeTap!();
                  }
                }
              },
              onDoubleTapDown: (details) {
                final RenderBox box = context.findRenderObject() as RenderBox;
                final localPosition = box.globalToLocal(details.globalPosition);
                final isBlade = _isTapOnBlade(localPosition, box.size);

                // Double tap solo sulla lama per cambiare modalità
                if (isBlade && widget.onBladeDoubleTap != null) {
                  widget.onBladeDoubleTap!();
                }
              },
              onPanUpdate: (details) {
                if (widget.onBladePan != null && widget.isPreviewMode) {
                  // Calcola posizione verticale relativa (0.0 in alto, 1.0 in basso)
                  final RenderBox box = context.findRenderObject() as RenderBox;
                  final localPosition = box.globalToLocal(details.globalPosition);
                  final relativeY = (localPosition.dy / box.size.height).clamp(0.0, 1.0);
                  widget.onBladePan!(relativeY);
                }
              },
              child: AnimatedBuilder(
                animation: Listenable.merge([
                  _bladeHeightAnimation,
                  _pulseAnimation,
                ]),
                builder: (context, child) {
                  return CustomPaint(
                    painter: LightsaberPainter(
                      bladeColor: widget.bladeColor,
                      brightness: widget.brightness,
                      bladeHeightFactor: _bladeHeightAnimation.value,
                      pulseOpacity: _pulseAnimation.value,
                      bladeState: widget.bladeState,
                      isConnected: widget.isConnected,
                      isPreviewMode: widget.isPreviewMode,
                      pickerMode: widget.pickerMode,
                      baseColorForGradient: widget.isPreviewMode ? widget.bladeColor : null,
                    ),
                    size: Size(
                      100, // Larghezza fissa
                      widget.maxHeight,
                    ),
                  );
                },
              ),
            ),
          ),

          // Power Button (sotto la spada)
          const SizedBox(height: 8),
          _buildPowerButton(),
          const SizedBox(height: 16),
        ],
      ),
    );
  }

  /// Costruisce il power button
  Widget _buildPowerButton() {
    final bool isAnimating =
        widget.bladeState == 'igniting' || widget.bladeState == 'retracting';

    // Determina colore bordo in base a stato
    Color borderColor;
    Color iconColor;

    switch (widget.bladeState) {
      case 'on':
        borderColor = const Color(0xFF00FF88); // Verde neon
        iconColor = Colors.white;
        break;
      case 'igniting':
      case 'retracting':
        borderColor = const Color(0xFFFFCC00); // Arancione
        iconColor = Colors.white;
        break;
      case 'off':
      default:
        borderColor = const Color(0xFFFF3B30); // Rosso
        iconColor = Colors.grey;
        break;
    }

    return GestureDetector(
      onTap: isAnimating ? null : widget.onPowerTap,
      child: Container(
        width: 70,
        height: 70,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: const RadialGradient(
            colors: [
              Color(0xFF2A2A2A),
              Color(0xFF0D0D0D),
            ],
          ),
          border: Border.all(
            color: borderColor,
            width: 3,
          ),
          boxShadow: [
            BoxShadow(
              color: borderColor.withValues(alpha: 0.5),
              blurRadius: 10,
              spreadRadius: 2,
            ),
          ],
        ),
        child: Center(
          child: Icon(
            Icons.bolt,
            size: 32,
            color: iconColor,
          ),
        ),
      ),
    );
  }
}
