#!/usr/bin/env python3
"""
Lightsaber Sound Generator
Genera suoni procedurali per spada laser usando sintesi audio.

Dipendenze:
    pip install numpy scipy

Usage:
    python audio_generator.py --pack jedi --output AppMobile/flutter_led_saber/assets/sounds/jedi/
"""

import sys
import os
import subprocess
from pathlib import Path
import argparse

# ═══════════════════════════════════════════════════════════════════
# DEPENDENCY CHECK
# ═══════════════════════════════════════════════════════════════════

def check_dependencies():
    """Verifica che le dipendenze siano installate"""
    missing_deps = []

    try:
        import numpy
    except ImportError:
        missing_deps.append('numpy')

    try:
        import scipy
    except ImportError:
        missing_deps.append('scipy')

    if missing_deps:
        print("\n" + "="*70)
        print("❌ ERRORE: Dipendenze mancanti!")
        print("="*70)
        print(f"\nModuli richiesti non trovati: {', '.join(missing_deps)}\n")
        print("📦 OPZIONE 1 (Consigliata): Usa il virtual environment del progetto")
        print("-" * 70)
        print("  cd /home/reder/Documenti/PlatformIO/Projects/ledSaber")
        print("  ./start.sh")
        print("  # Poi esegui questo script:")
        print("  ./venv/bin/python tools/audio_generator.py --pack all")
        print()
        print("📦 OPZIONE 2: Installa dipendenze manualmente")
        print("-" * 70)
        print("  pip install numpy scipy")
        print("  # Oppure:")
        print("  pip install -r requirements.txt")
        print()
        print("="*70)
        print()
        sys.exit(1)

# Controlla dipendenze prima di importare
check_dependencies()

# Ora importa le librerie (sicuro che esistono)
import numpy as np
from scipy.io import wavfile


class LightsaberSoundGenerator:
    """Generatore procedurale di suoni lightsaber"""

    def __init__(self, sample_rate=44100):
        self.sample_rate = sample_rate

    # ═══════════════════════════════════════════════════════════════════
    # UTILITY FUNCTIONS
    # ═══════════════════════════════════════════════════════════════════

    def generate_sine(self, frequency, duration, amplitude=0.5):
        """Genera onda sinusoidale pura"""
        t = np.linspace(0, duration, int(self.sample_rate * duration))
        return amplitude * np.sin(2 * np.pi * frequency * t)

    def generate_square(self, frequency, duration, amplitude=0.5):
        """Genera onda quadra (più metallica)"""
        t = np.linspace(0, duration, int(self.sample_rate * duration))
        return amplitude * np.sign(np.sin(2 * np.pi * frequency * t))

    def generate_sawtooth(self, frequency, duration, amplitude=0.5):
        """Genera onda a dente di sega (più aspra)"""
        t = np.linspace(0, duration, int(self.sample_rate * duration))
        return amplitude * 2 * (t * frequency - np.floor(t * frequency + 0.5))

    def add_noise(self, signal, noise_level=0.05):
        """Aggiunge rumore bianco per texture"""
        noise = np.random.normal(0, noise_level, len(signal))
        return signal + noise

    def apply_envelope(self, signal, attack=0.05, decay=0.1, sustain=0.8, release=0.2):
        """Applica inviluppo ADSR"""
        total_samples = len(signal)
        envelope = np.ones(total_samples)

        # Attack
        attack_samples = int(attack * self.sample_rate)
        if attack_samples > 0:
            envelope[:attack_samples] = np.linspace(0, 1, attack_samples)

        # Decay
        decay_samples = int(decay * self.sample_rate)
        if decay_samples > 0:
            decay_end = attack_samples + decay_samples
            envelope[attack_samples:decay_end] = np.linspace(1, sustain, decay_samples)

        # Sustain (mantiene il livello)
        sustain_end = total_samples - int(release * self.sample_rate)
        envelope[attack_samples + decay_samples:sustain_end] = sustain

        # Release
        release_samples = int(release * self.sample_rate)
        if release_samples > 0:
            envelope[sustain_end:] = np.linspace(sustain, 0, release_samples)

        return signal * envelope

    def apply_lowpass(self, signal, cutoff_freq=5000):
        """Filtro passa-basso semplice (moving average)"""
        window_size = int(self.sample_rate / cutoff_freq)
        return np.convolve(signal, np.ones(window_size)/window_size, mode='same')

    def apply_vibrato(self, signal, vibrato_freq=5, vibrato_depth=0.02):
        """Aggiunge vibrato (modulazione frequenza)"""
        t = np.linspace(0, len(signal)/self.sample_rate, len(signal))
        modulation = 1 + vibrato_depth * np.sin(2 * np.pi * vibrato_freq * t)
        return signal * modulation

    def normalize(self, signal, target_amplitude=0.9):
        """Normalizza il segnale"""
        max_val = np.max(np.abs(signal))
        if max_val > 0:
            return signal * (target_amplitude / max_val)
        return signal

    def ensure_loop_seamless(self, signal, crossfade_samples=2205):  # 50ms @ 44100Hz
        """Garantisce loop senza click tramite crossfade"""
        fade_out = np.linspace(1, 0, crossfade_samples)
        fade_in = np.linspace(0, 1, crossfade_samples)

        # Crossfade tra fine e inizio
        signal[-crossfade_samples:] = (
            signal[-crossfade_samples:] * fade_out +
            signal[:crossfade_samples] * fade_in
        )
        return signal

    # ═══════════════════════════════════════════════════════════════════
    # SOUND GENERATORS
    # ═══════════════════════════════════════════════════════════════════

    def generate_hum_base(self, duration=3.0, base_freq=110, style='jedi'):
        """
        Genera HUM base (usato per loop continuo + swing)

        Args:
            duration: Durata in secondi (3-5s per loop)
            base_freq: Frequenza fondamentale (Hz)
                - Jedi: 110-120 Hz (più acuto)
                - Sith: 60-80 Hz (più grave)
            style: 'jedi', 'sith', 'unstable', 'crystal'
        """
        print(f"  Generando hum_base.wav ({style}, {base_freq}Hz, {duration}s)...")

        if style == 'jedi':
            # Suono pulito, armoniche semplici
            fundamental = self.generate_sine(base_freq, duration, amplitude=0.6)
            harmonic2 = self.generate_sine(base_freq * 2, duration, amplitude=0.2)
            harmonic3 = self.generate_sine(base_freq * 3, duration, amplitude=0.1)

            hum = fundamental + harmonic2 + harmonic3
            hum = self.add_noise(hum, noise_level=0.02)
            hum = self.apply_vibrato(hum, vibrato_freq=4, vibrato_depth=0.015)

        elif style == 'sith':
            # Suono più grave e distorto
            fundamental = self.generate_sine(base_freq, duration, amplitude=0.5)
            harmonic2 = self.generate_square(base_freq * 1.5, duration, amplitude=0.3)
            harmonic3 = self.generate_sine(base_freq * 2.5, duration, amplitude=0.15)

            hum = fundamental + harmonic2 + harmonic3
            hum = self.add_noise(hum, noise_level=0.04)
            hum = self.apply_vibrato(hum, vibrato_freq=3, vibrato_depth=0.025)

        elif style == 'unstable':
            # Stile Kylo Ren - crackling instabile
            fundamental = self.generate_sine(base_freq, duration, amplitude=0.4)
            crackle = self.generate_square(base_freq * 1.3, duration, amplitude=0.25)
            noise = np.random.normal(0, 0.08, int(self.sample_rate * duration))

            # Modulazione casuale per instabilità
            t = np.linspace(0, duration, int(self.sample_rate * duration))
            random_mod = 1 + 0.1 * np.sin(2 * np.pi * np.random.rand(10).sum() * t)

            hum = (fundamental + crackle + noise) * random_mod
            hum = self.apply_vibrato(hum, vibrato_freq=6, vibrato_depth=0.04)

        elif style == 'crystal':
            # Suono cristallino, armoniche alte
            fundamental = self.generate_sine(base_freq * 1.5, duration, amplitude=0.5)
            harmonic2 = self.generate_sine(base_freq * 3, duration, amplitude=0.25)
            harmonic3 = self.generate_sine(base_freq * 5, duration, amplitude=0.15)
            shimmer = self.generate_sine(base_freq * 7, duration, amplitude=0.08)

            hum = fundamental + harmonic2 + harmonic3 + shimmer
            hum = self.add_noise(hum, noise_level=0.01)
            hum = self.apply_vibrato(hum, vibrato_freq=7, vibrato_depth=0.01)

        else:
            raise ValueError(f"Style non riconosciuto: {style}")

        # Normalizza e garantisci loop perfetto
        hum = self.normalize(hum, target_amplitude=0.85)
        hum = self.ensure_loop_seamless(hum, crossfade_samples=int(0.05 * self.sample_rate))

        return hum

    def generate_ignition(self, duration=1.5, base_freq=80, style='jedi'):
        """
        Genera suono di accensione lama

        Simulazione: "vvvvRRRMMM" - sweep up + stabilizzazione
        """
        print(f"  Generando ignition.wav ({style}, {duration}s)...")

        # Sweep crescente da bassa a alta frequenza
        t = np.linspace(0, duration, int(self.sample_rate * duration))
        freq_sweep = np.linspace(base_freq * 0.3, base_freq * 1.5, len(t))

        # Sintesi con sweep
        phase = np.cumsum(2 * np.pi * freq_sweep / self.sample_rate)
        sweep = 0.6 * np.sin(phase)

        # Armoniche
        harmonic = 0.3 * np.sin(phase * 2)

        # Rumore crescente (energia)
        noise = np.random.normal(0, 0.1, len(t))
        noise_envelope = np.linspace(0.1, 0.05, len(t))  # Rumore che decresce

        ignition = sweep + harmonic + (noise * noise_envelope)

        # Envelope: attack rapido, sustain, release
        ignition = self.apply_envelope(
            ignition,
            attack=0.1,
            decay=0.2,
            sustain=0.7,
            release=0.3
        )

        # Vibrato finale (quando si stabilizza)
        ignition = self.apply_vibrato(ignition, vibrato_freq=5, vibrato_depth=0.02)

        return self.normalize(ignition, target_amplitude=0.9)

    def generate_retract(self, duration=1.2, base_freq=100, style='jedi'):
        """
        Genera suono di spegnimento lama

        Simulazione: "vmmmmm" - sweep down + fade out
        """
        print(f"  Generando retract.wav ({style}, {duration}s)...")

        # Sweep decrescente da alta a bassa frequenza
        t = np.linspace(0, duration, int(self.sample_rate * duration))
        freq_sweep = np.linspace(base_freq * 1.2, base_freq * 0.2, len(t))

        # Sintesi con sweep
        phase = np.cumsum(2 * np.pi * freq_sweep / self.sample_rate)
        sweep = 0.7 * np.sin(phase)

        # Armoniche che svaniscono
        harmonic = 0.2 * np.sin(phase * 1.5)

        # Rumore decrescente
        noise = np.random.normal(0, 0.05, len(t))
        noise_envelope = np.linspace(0.08, 0.01, len(t))

        retract = sweep + harmonic + (noise * noise_envelope)

        # Envelope: attack corto, release lungo
        retract = self.apply_envelope(
            retract,
            attack=0.05,
            decay=0.1,
            sustain=0.6,
            release=0.5
        )

        return self.normalize(retract, target_amplitude=0.85)

    def generate_clash(self, duration=0.4, base_freq=200, style='jedi'):
        """
        Genera suono di colpo/scontro

        Simulazione: "KZZZT!" - impatto metallico
        """
        print(f"  Generando clash.wav ({style}, {duration}s)...")

        t = np.linspace(0, duration, int(self.sample_rate * duration))

        # Componente metallica (frequenze alte)
        metallic = 0.5 * self.generate_square(base_freq * 2, duration, amplitude=1.0)

        # Noise burst (impatto)
        noise = np.random.normal(0, 0.4, len(t))

        # Risonanza (ring down)
        resonance = 0.3 * np.sin(2 * np.pi * base_freq * 1.5 * t)

        clash = metallic + noise + resonance

        # Envelope percussivo: attack veloce, decay rapido
        clash = self.apply_envelope(
            clash,
            attack=0.01,
            decay=0.05,
            sustain=0.3,
            release=0.15
        )

        # Filtro passa-basso per non essere troppo stridente
        clash = self.apply_lowpass(clash, cutoff_freq=8000)

        return self.normalize(clash, target_amplitude=0.95)

    # ═══════════════════════════════════════════════════════════════════
    # PACK GENERATORS
    # ═══════════════════════════════════════════════════════════════════

    def generate_pack(self, pack_name, output_dir):
        """
        Genera un pack completo di suoni

        Args:
            pack_name: 'jedi', 'sith', 'unstable', 'crystal'
            output_dir: Directory output
        """
        print(f"\n{'='*60}")
        print(f"Generando sound pack: {pack_name.upper()}")
        print(f"{'='*60}\n")

        # Crea directory se non esiste
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)

        # Parametri per pack
        pack_params = {
            'jedi': {
                'hum_freq': 110,
                'hum_duration': 3.0,
                'ignition_duration': 1.5,
                'retract_duration': 1.2,
                'clash_freq': 200,
            },
            'sith': {
                'hum_freq': 70,
                'hum_duration': 3.5,
                'ignition_duration': 1.8,
                'retract_duration': 1.4,
                'clash_freq': 150,
            },
            'unstable': {
                'hum_freq': 90,
                'hum_duration': 3.0,
                'ignition_duration': 2.0,
                'retract_duration': 1.5,
                'clash_freq': 180,
            },
            'crystal': {
                'hum_freq': 150,
                'hum_duration': 3.0,
                'ignition_duration': 1.3,
                'retract_duration': 1.1,
                'clash_freq': 250,
            },
        }

        params = pack_params.get(pack_name, pack_params['jedi'])

        # Genera tutti i suoni
        sounds = {
            'hum_base.wav': self.generate_hum_base(
                duration=params['hum_duration'],
                base_freq=params['hum_freq'],
                style=pack_name
            ),
            'ignition.wav': self.generate_ignition(
                duration=params['ignition_duration'],
                base_freq=params['hum_freq'],
                style=pack_name
            ),
            'retract.wav': self.generate_retract(
                duration=params['retract_duration'],
                base_freq=params['hum_freq'],
                style=pack_name
            ),
            'clash.wav': self.generate_clash(
                duration=0.4,
                base_freq=params['clash_freq'],
                style=pack_name
            ),
        }

        # Salva tutti i file
        print(f"\nSalvataggio file in: {output_path}")
        for filename, signal in sounds.items():
            filepath = output_path / filename

            # Converti in int16 per WAV
            signal_int16 = np.int16(signal * 32767)

            wavfile.write(str(filepath), self.sample_rate, signal_int16)

            # Info file
            duration = len(signal) / self.sample_rate
            size_kb = filepath.stat().st_size / 1024
            print(f"  ✓ {filename:20s} - {duration:.2f}s - {size_kb:.1f}KB")

        print(f"\n✓ Pack '{pack_name}' generato con successo!\n")


def show_directory_listing(directory):
    """Usa `ls` per mostrare quanto generato; fa il fallback su Python se serve."""
    print(f"\nContenuto della directory '{directory}':")
    if not os.path.isdir(directory):
        print("  ⚠️ Directory non trovata, nessun file generato.")
        return False

    try:
        subprocess.run(["ls", "-1", directory], check=True)
    except FileNotFoundError:
        print("  ⚠️ Impossibile eseguire 'ls'; elenco file con Python:")
        for entry in sorted(os.listdir(directory)):
            print(f"  {entry}")
    except subprocess.CalledProcessError:
        print("  ⚠️ 'ls' ha restituito un errore; elenco file con Python:")
        for entry in sorted(os.listdir(directory)):
            print(f"  {entry}")
    return True


# CLI INTERFACE
# ═══════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description='Lightsaber Sound Generator - Genera suoni procedurali per spada laser'
    )

    parser.add_argument(
        '--pack',
        type=str,
        choices=['jedi', 'sith', 'unstable', 'crystal', 'all'],
        default='all',
        help='Sound pack da generare (default: all)'
    )

    parser.add_argument(
        '--output',
        type=str,
        default='AppMobile/flutter_led_saber/assets/sounds',
        help='Directory base output (default: AppMobile/flutter_led_saber/assets/sounds)'
    )

    parser.add_argument(
        '--sample-rate',
        type=int,
        default=44100,
        help='Sample rate in Hz (default: 44100)'
    )

    args = parser.parse_args()

    # Crea generatore
    generator = LightsaberSoundGenerator(sample_rate=args.sample_rate)

    # Genera pack
    if args.pack == 'all':
        packs = ['jedi', 'sith', 'unstable', 'crystal']
    else:
        packs = [args.pack]

    for pack_name in packs:
        output_dir = os.path.join(args.output, pack_name)
        generator.generate_pack(pack_name, output_dir)
        show_directory_listing(output_dir)

    print("="*60)
    print("✓ GENERAZIONE COMPLETATA!")
    print("="*60)
    print(f"\nFile generati in: {os.path.abspath(args.output)}")


if __name__ == '__main__':
    main()
