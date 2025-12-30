import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/ble_provider.dart';
import 'providers/led_provider.dart';
import 'providers/motion_provider.dart';
import 'providers/audio_provider.dart';
import 'screens/home_screen.dart';
import 'theme/app_theme.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => BleProvider()),
        ChangeNotifierProvider(create: (_) => LedProvider()),
        ChangeNotifierProvider(create: (_) => AudioProvider()),
        // AudioProvider ascolta LedProvider per sincronizzare l'audio con bladeState
        ChangeNotifierProxyProvider<LedProvider, AudioProvider>(
          create: (_) => AudioProvider(),
          update: (_, ledProvider, audioProvider) {
            // Sincronizza l'audio ogni volta che bladeState cambia
            audioProvider!.syncWithBladeState(ledProvider.currentState?.bladeState);
            return audioProvider;
          },
        ),
        ChangeNotifierProxyProvider<AudioProvider, MotionProvider>(
          create: (_) => MotionProvider(),
          update: (_, audioProvider, motionProvider) {
            motionProvider!.setAudioProvider(audioProvider);
            return motionProvider;
          },
        ),
      ],
      child: MaterialApp(
        title: 'LED Saber Controller',
        theme: AppTheme.lightTheme,
        darkTheme: AppTheme.darkTheme,
        themeMode: ThemeMode.system,
        home: const HomeScreen(),
      ),
    );
  }
}
