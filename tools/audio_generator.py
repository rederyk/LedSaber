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
from scipy import signal


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

    def apply_lowpass(self, sig, cutoff_freq=5000, order=4):
        """Filtro passa-basso Butterworth (più realistico)"""
        nyquist = self.sample_rate / 2
        normal_cutoff = cutoff_freq / nyquist
        # Clamp per evitare errori se cutoff >= nyquist
        normal_cutoff = min(normal_cutoff, 0.99)
        sos = signal.butter(order, normal_cutoff, btype='low', output='sos')
        return signal.sosfilt(sos, sig)

    def apply_highpass(self, sig, cutoff_freq=50, order=2):
        """Filtro passa-alto per rimuovere rumble"""
        nyquist = self.sample_rate / 2
        normal_cutoff = cutoff_freq / nyquist
        sos = signal.butter(order, normal_cutoff, btype='high', output='sos')
        return signal.sosfilt(sos, sig)

    def apply_reverb(self, sig, delay_ms=150, decay=0.4, num_echoes=3):
        """Semplice riverbero tramite comb filter"""
        output = np.copy(sig)
        delay_samples = int(self.sample_rate * delay_ms / 1000)

        for i in range(1, num_echoes + 1):
            current_delay = delay_samples * i
            current_decay = decay ** i

            # Aggiungi echo ritardato
            padded = np.zeros(len(sig) + current_delay)
            padded[current_delay:] = sig * current_decay

            # Somma all'output (tronca se necessario)
            output[:len(padded)] += padded[:len(output)]

        return output

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
            # Suono pulito, LAYERING con detuning per spessore
            fund1 = self.generate_sine(base_freq, duration, amplitude=0.4)
            fund2 = self.generate_sine(base_freq * 1.003, duration, amplitude=0.4)  # +0.3% detune
            fund3 = self.generate_sine(base_freq * 0.997, duration, amplitude=0.4)  # -0.3% detune
            fundamental = (fund1 + fund2 + fund3) / 3

            # Armoniche ricche
            harmonic2 = self.generate_sine(base_freq * 2, duration, amplitude=0.25)
            harmonic3 = self.generate_sine(base_freq * 3, duration, amplitude=0.12)
            harmonic4 = self.generate_sine(base_freq * 4, duration, amplitude=0.06)

            hum = fundamental + harmonic2 + harmonic3 + harmonic4

            # Rumore filtrato (più naturale)
            noise = np.random.normal(0, 0.03, int(self.sample_rate * duration))
            noise_filtered = self.apply_lowpass(noise, cutoff_freq=2000)
            hum = hum + noise_filtered

            hum = self.apply_vibrato(hum, vibrato_freq=4.5, vibrato_depth=0.012)
            hum = self.apply_lowpass(hum, cutoff_freq=6000)
            hum = self.apply_reverb(hum, delay_ms=80, decay=0.25, num_echoes=2)

        elif style == 'sith':
            # Suono più grave, denso e minaccioso
            fund1 = self.generate_sine(base_freq, duration, amplitude=0.35)
            fund2 = self.generate_sine(base_freq * 1.005, duration, amplitude=0.35)
            fundamental = (fund1 + fund2) / 2

            # Armoniche distorte (uso sawtooth)
            harmonic2 = self.generate_sawtooth(base_freq * 1.5, duration, amplitude=0.25)
            harmonic3 = self.generate_square(base_freq * 2, duration, amplitude=0.18)
            harmonic4 = self.generate_sine(base_freq * 2.5, duration, amplitude=0.1)

            hum = fundamental + harmonic2 + harmonic3 + harmonic4

            # Rumore più presente
            noise = np.random.normal(0, 0.05, int(self.sample_rate * duration))
            noise_filtered = self.apply_lowpass(noise, cutoff_freq=1500)
            hum = hum + noise_filtered

            hum = self.apply_vibrato(hum, vibrato_freq=3.2, vibrato_depth=0.022)
            hum = self.apply_lowpass(hum, cutoff_freq=4500)
            hum = self.apply_reverb(hum, delay_ms=120, decay=0.35, num_echoes=3)

        elif style == 'unstable':
            # Stile Kylo Ren - crackling instabile con FM synthesis
            t = np.linspace(0, duration, int(self.sample_rate * duration))

            # FM Modulation per suono organico
            modulator = np.sin(2 * np.pi * 6.5 * t)  # LFO
            carrier_freq = base_freq * (1 + 0.15 * modulator)
            phase = np.cumsum(2 * np.pi * carrier_freq / self.sample_rate)
            fundamental = 0.4 * np.sin(phase)

            # Crackle layer
            crackle1 = self.generate_square(base_freq * 1.3, duration, amplitude=0.2)
            crackle2 = self.generate_sawtooth(base_freq * 1.7, duration, amplitude=0.15)

            # Rumore impulsivo (bursts casuali)
            noise = np.random.normal(0, 0.1, int(self.sample_rate * duration))
            impulse_mask = np.random.rand(len(noise)) > 0.98  # Burst casuali
            noise = noise * impulse_mask

            # Modulazione casuale per instabilità
            random_mod = 1 + 0.12 * np.sin(2 * np.pi * np.random.rand(5).sum() * t)

            hum = (fundamental + crackle1 + crackle2 + noise) * random_mod
            hum = self.apply_vibrato(hum, vibrato_freq=6.5, vibrato_depth=0.035)
            hum = self.apply_lowpass(hum, cutoff_freq=5500)
            hum = self.apply_reverb(hum, delay_ms=100, decay=0.3, num_echoes=2)

        elif style == 'crystal':
            # Suono cristallino, eterico e brillante
            fund1 = self.generate_sine(base_freq * 1.5, duration, amplitude=0.35)
            fund2 = self.generate_sine(base_freq * 1.502, duration, amplitude=0.35)
            fundamental = (fund1 + fund2) / 2

            # Armoniche alte (shimmer)
            harmonic2 = self.generate_sine(base_freq * 3, duration, amplitude=0.22)
            harmonic3 = self.generate_sine(base_freq * 5, duration, amplitude=0.15)
            harmonic4 = self.generate_sine(base_freq * 7, duration, amplitude=0.1)
            shimmer = self.generate_sine(base_freq * 9, duration, amplitude=0.05)

            hum = fundamental + harmonic2 + harmonic3 + harmonic4 + shimmer

            # Rumore minimo, molto filtrato
            noise = np.random.normal(0, 0.015, int(self.sample_rate * duration))
            noise_filtered = self.apply_lowpass(noise, cutoff_freq=8000)
            hum = hum + noise_filtered

            hum = self.apply_vibrato(hum, vibrato_freq=8, vibrato_depth=0.008)
            hum = self.apply_highpass(hum, cutoff_freq=100)  # Rimuovi rumble
            hum = self.apply_reverb(hum, delay_ms=180, decay=0.4, num_echoes=4)

        else:
            raise ValueError(f"Style non riconosciuto: {style}")

        # Normalizza e garantisci loop perfetto
        hum = self.apply_highpass(hum, cutoff_freq=30)  # Rimuovi DC offset
        hum = self.normalize(hum, target_amplitude=0.85)
        hum = self.ensure_loop_seamless(hum, crossfade_samples=int(0.05 * self.sample_rate))

        return hum

    def generate_ignition(self, duration=1.5, base_freq=80, style='jedi'):
        """
        Genera suono di accensione lama

        Simulazione: "vvvvRRRMMM" - sweep up + stabilizzazione
        """
        print(f"  Generando ignition.wav ({style}, {duration}s)...")

        t = np.linspace(0, duration, int(self.sample_rate * duration))

        # Sweep crescente con curva esponenziale (più naturale)
        freq_start = base_freq * 0.2
        freq_end = base_freq * 1.6
        freq_sweep = freq_start * (freq_end / freq_start) ** (t / duration)

        # Multi-layer sweep per spessore
        phase1 = np.cumsum(2 * np.pi * freq_sweep / self.sample_rate)
        phase2 = np.cumsum(2 * np.pi * freq_sweep * 1.01 / self.sample_rate)  # Detune
        sweep = 0.4 * np.sin(phase1) + 0.4 * np.sin(phase2)

        # Armoniche che crescono progressivamente
        harmonic2 = 0.25 * np.sin(phase1 * 2)
        harmonic3 = 0.15 * np.sin(phase1 * 3)

        # Noise burst iniziale (impatto energetico)
        noise = np.random.normal(0, 0.15, len(t))
        # Envelope esponenziale per noise
        noise_envelope = np.exp(-t * 3)  # Decade rapidamente
        noise_filtered = self.apply_lowpass(noise * noise_envelope, cutoff_freq=3000)

        # Sub-bass layer per impatto
        subbass = 0.2 * np.sin(2 * np.pi * 40 * t) * np.exp(-t * 4)

        ignition = sweep + harmonic2 + harmonic3 + noise_filtered + subbass

        # Envelope cinematico: attack rapido, crescendo, stabilizzazione
        ignition = self.apply_envelope(
            ignition,
            attack=0.08,
            decay=0.15,
            sustain=0.75,
            release=0.25
        )

        # Vibrato crescente (si stabilizza)
        vibrato_depth = 0.03 * (1 - np.exp(-t * 2))  # Cresce gradualmente
        ignition = ignition * (1 + vibrato_depth * np.sin(2 * np.pi * 5 * t))

        # Reverb per profondità
        ignition = self.apply_reverb(ignition, delay_ms=100, decay=0.3, num_echoes=2)

        # Filtri finali
        ignition = self.apply_highpass(ignition, cutoff_freq=35)
        ignition = self.apply_lowpass(ignition, cutoff_freq=7000)

        return self.normalize(ignition, target_amplitude=0.92)

    def generate_retract(self, duration=1.2, base_freq=100, style='jedi'):
        """
        Genera suono di spegnimento lama

        Simulazione: "vmmmmm" - sweep down + fade out
        """
        print(f"  Generando retract.wav ({style}, {duration}s)...")

        t = np.linspace(0, duration, int(self.sample_rate * duration))

        # Sweep decrescente con curva esponenziale inversa
        freq_start = base_freq * 1.4
        freq_end = base_freq * 0.15
        freq_sweep = freq_start * (freq_end / freq_start) ** (t / duration)

        # Multi-layer per spessore
        phase1 = np.cumsum(2 * np.pi * freq_sweep / self.sample_rate)
        phase2 = np.cumsum(2 * np.pi * freq_sweep * 0.995 / self.sample_rate)
        sweep = 0.5 * np.sin(phase1) + 0.4 * np.sin(phase2)

        # Armoniche che svaniscono progressivamente
        harmonic_envelope = np.exp(-t * 1.5)
        harmonic2 = 0.2 * np.sin(phase1 * 1.5) * harmonic_envelope
        harmonic3 = 0.1 * np.sin(phase1 * 2) * harmonic_envelope

        # Rumore decrescente filtrato
        noise = np.random.normal(0, 0.08, len(t))
        noise_envelope = np.exp(-t * 2.5)
        noise_filtered = self.apply_lowpass(noise * noise_envelope, cutoff_freq=2500)

        # Tail reverberante (coda lunga)
        tail = 0.15 * np.sin(2 * np.pi * base_freq * 0.5 * t) * np.exp(-t * 1.2)

        retract = sweep + harmonic2 + harmonic3 + noise_filtered + tail

        # Envelope: release molto lungo per fade naturale
        retract = self.apply_envelope(
            retract,
            attack=0.03,
            decay=0.08,
            sustain=0.55,
            release=0.6
        )

        # Reverb per coda atmosferica
        retract = self.apply_reverb(retract, delay_ms=140, decay=0.45, num_echoes=3)

        # Filtri
        retract = self.apply_highpass(retract, cutoff_freq=30)
        retract = self.apply_lowpass(retract, cutoff_freq=5500)

        return self.normalize(retract, target_amplitude=0.88)

    def generate_clash(self, duration=0.4, base_freq=200, style='jedi'):
        """
        Genera suono di colpo/scontro

        Simulazione: "KZZZT!" - impatto metallico cinematico
        """
        print(f"  Generando clash.wav ({style}, {duration}s)...")

        t = np.linspace(0, duration, int(self.sample_rate * duration))

        # Impatto iniziale multi-layer
        impact1 = 0.4 * self.generate_square(base_freq * 2.2, duration, amplitude=1.0)
        impact2 = 0.3 * self.generate_sawtooth(base_freq * 3.1, duration, amplitude=1.0)

        # Noise burst intenso (impatto energetico)
        noise = np.random.normal(0, 0.5, len(t))
        # Envelope esplosivo per noise
        noise_envelope = np.exp(-t * 15)  # Decade molto velocemente
        noise_filtered = self.apply_lowpass(noise * noise_envelope, cutoff_freq=9000)

        # Risonanze metalliche multiple (ring-out)
        resonance1 = 0.25 * np.sin(2 * np.pi * base_freq * 1.8 * t) * np.exp(-t * 5)
        resonance2 = 0.18 * np.sin(2 * np.pi * base_freq * 2.4 * t) * np.exp(-t * 7)
        resonance3 = 0.12 * np.sin(2 * np.pi * base_freq * 3.2 * t) * np.exp(-t * 9)

        # Sub-bass impact per punch cinematico
        subbass = 0.3 * np.sin(2 * np.pi * 60 * t) * np.exp(-t * 12)

        # Componente "spark" (scintille ad alta frequenza)
        spark_freq = np.random.uniform(3000, 6000, 10)
        spark = np.zeros_like(t)
        for freq in spark_freq:
            spark += 0.05 * np.sin(2 * np.pi * freq * t) * np.exp(-t * 20)

        clash = (impact1 + impact2 + noise_filtered + resonance1 +
                resonance2 + resonance3 + subbass + spark)

        # Envelope percussivo molto rapido (stile trailer)
        clash = self.apply_envelope(
            clash,
            attack=0.005,  # Attack istantaneo
            decay=0.03,
            sustain=0.25,
            release=0.12
        )

        # Reverb corto per spazio
        clash = self.apply_reverb(clash, delay_ms=60, decay=0.35, num_echoes=2)

        # Filtri per controllo
        clash = self.apply_highpass(clash, cutoff_freq=40)
        clash = self.apply_lowpass(clash, cutoff_freq=10000)

        return self.normalize(clash, target_amplitude=0.98)

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
