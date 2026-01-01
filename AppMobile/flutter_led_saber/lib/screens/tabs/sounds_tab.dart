import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../providers/audio_provider.dart';
import '../../providers/led_provider.dart';

/// Tab per Audio Settings
class SoundsTab extends StatelessWidget {
  const SoundsTab({super.key});

  @override
  Widget build(BuildContext context) {
    final audioProvider = Provider.of<AudioProvider>(context);
    final ledProvider = Provider.of<LedProvider>(context);

    return SingleChildScrollView(
      padding: const EdgeInsets.all(6),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _buildAudioSettings(context, audioProvider, ledProvider),
        ],
      ),
    );
  }

  /// Audio Settings Section
  Widget _buildAudioSettings(BuildContext context, AudioProvider audioProvider, LedProvider ledProvider) {
    return Card(
      margin: EdgeInsets.zero,
      child: Padding(
        padding: const EdgeInsets.all(8),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Header
            Row(
              children: [
                const Icon(Icons.volume_up, size: 12, color: Colors.grey),
                const SizedBox(width: 4),
                Text(
                  'Audio Settings',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.bold,
                    fontSize: 10,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),

            // Enable Sounds Toggle
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  'Enable Sounds',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(fontSize: 10),
                ),
                Transform.scale(
                  scale: 0.7,
                  child: Switch(
                    value: audioProvider.soundsEnabled,
                    onChanged: (value) => audioProvider.setSoundsEnabled(value),
                  ),
                ),
              ],
            ),

            const SizedBox(height: 8),

            // Master Volume Slider
            Text(
              'Master Volume: ${(audioProvider.masterVolume * 100).toInt()}%',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(fontSize: 10),
            ),
            Slider(
              value: audioProvider.masterVolume,
              min: 0.0,
              max: 1.0,
              divisions: 20,
              onChanged: audioProvider.soundsEnabled
                  ? (value) => audioProvider.setMasterVolume(value)
                  : null,
            ),

            const SizedBox(height: 8),

            // Sound Pack Selection
            Text(
              'Sound Pack',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                fontWeight: FontWeight.bold,
                fontSize: 10,
              ),
            ),
            const SizedBox(height: 4),
            Wrap(
              spacing: 6,
              runSpacing: 4,
              children: audioProvider.availableSoundPacks.map((pack) {
                final isSelected = pack.id == audioProvider.currentSoundPackId;
                return ChoiceChip(
                  label: Text(
                    pack.name,
                    style: const TextStyle(fontSize: 9),
                  ),
                  selected: isSelected,
                  onSelected: audioProvider.soundsEnabled
                      ? (selected) {
                          if (selected) {
                            audioProvider.setSoundPack(pack.id);
                          }
                        }
                      : null,
                  visualDensity: VisualDensity.compact,
                  padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                );
              }).toList(),
            ),

            const SizedBox(height: 6),

            // Current Pack Description
            if (audioProvider.currentSoundPack != null)
              Text(
                audioProvider.currentSoundPack!.description,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  fontSize: 9,
                  color: Colors.grey,
                  fontStyle: FontStyle.italic,
                ),
              ),

            const SizedBox(height: 8),

            // Blade Status Indicator (usa bladeState dal LedProvider)
            Row(
              children: [
                Icon(
                  ledProvider.isBladeOn ? Icons.lightbulb : Icons.lightbulb_outline,
                  size: 12,
                  color: ledProvider.isBladeOn ? Colors.green : Colors.grey,
                ),
                const SizedBox(width: 4),
                Text(
                  'Blade: ${ledProvider.isBladeOn ? "ON (Hum active)" : "OFF"}',
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    fontSize: 9,
                    color: ledProvider.isBladeOn ? Colors.green : Colors.grey,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
