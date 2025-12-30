#!/usr/bin/env python3
"""
Test Seamless Loop Quality
Verifica che i file hum_base.wav abbiano loop perfettamente seamless
"""

import numpy as np
from scipy.io import wavfile
from pathlib import Path

def analyze_loop_quality(filepath):
    """
    Analizza la qualità del loop di un file audio

    Controlla:
    1. DC offset (causa click)
    2. Discontinuità alla fine/inizio
    3. Differenza RMS tra inizio e fine
    """
    print(f"\n{'='*60}")
    print(f"Analizzando: {filepath.name}")
    print(f"{'='*60}")

    # Carica audio
    sample_rate, data = wavfile.read(filepath)

    # Converti in float
    if data.dtype == np.int16:
        data = data.astype(np.float32) / 32768.0

    # Test 1: DC Offset
    dc_offset = np.mean(data)
    print(f"  DC Offset: {dc_offset:.6f} {'✓ OK' if abs(dc_offset) < 0.001 else '⚠️  WARNING'}")

    # Test 2: Discontinuità fine → inizio
    loop_samples = 1000  # ~23ms @ 44100Hz
    end_segment = data[-loop_samples:]
    start_segment = data[:loop_samples]

    # Calcola differenza
    discontinuity = np.abs(end_segment[-1] - start_segment[0])
    print(f"  Loop discontinuity: {discontinuity:.6f} {'✓ OK' if discontinuity < 0.01 else '⚠️  WARNING'}")

    # Test 3: RMS differenza tra inizio e fine
    rms_start = np.sqrt(np.mean(start_segment**2))
    rms_end = np.sqrt(np.mean(end_segment**2))
    rms_diff = abs(rms_start - rms_end)

    print(f"  RMS Start: {rms_start:.4f}")
    print(f"  RMS End:   {rms_end:.4f}")
    print(f"  RMS Diff:  {rms_diff:.4f} {'✓ OK' if rms_diff < 0.05 else '⚠️  WARNING'}")

    # Test 4: Zero crossing check
    end_value = data[-1]
    start_value = data[0]

    print(f"  End value:   {end_value:.6f}")
    print(f"  Start value: {start_value:.6f}")

    # Test 5: Spectral similarity
    from scipy import signal

    # FFT dei primi e ultimi 2048 samples
    fft_start = np.abs(np.fft.rfft(data[:2048]))
    fft_end = np.abs(np.fft.rfft(data[-2048:]))

    # Correlazione spettrale
    spectral_corr = np.corrcoef(fft_start, fft_end)[0, 1]
    print(f"  Spectral correlation: {spectral_corr:.4f} {'✓ OK' if spectral_corr > 0.85 else '⚠️  WARNING'}")

    # Score finale
    score = 0
    if abs(dc_offset) < 0.001: score += 1
    if discontinuity < 0.01: score += 1
    if rms_diff < 0.05: score += 1
    if spectral_corr > 0.85: score += 1

    print(f"\n  🎯 Loop Quality Score: {score}/4")

    if score == 4:
        print("  ✅ PERFETTO - Loop seamless di alta qualità!")
    elif score >= 3:
        print("  ✓ BUONO - Loop seamless accettabile")
    else:
        print("  ⚠️  PROBLEMATICO - Loop potrebbe avere click udibili")

    return score


def main():
    # Trova tutti i file hum_base.wav
    sounds_dir = Path("AppMobile/flutter_led_saber/assets/sounds")

    if not sounds_dir.exists():
        print(f"❌ Directory non trovata: {sounds_dir}")
        return

    hum_files = list(sounds_dir.glob("*/hum_base.wav"))

    if not hum_files:
        print(f"❌ Nessun file hum_base.wav trovato in {sounds_dir}")
        return

    print("\n" + "="*60)
    print("TEST SEAMLESS LOOP QUALITY")
    print("="*60)

    scores = []
    for hum_file in sorted(hum_files):
        score = analyze_loop_quality(hum_file)
        scores.append((hum_file.parent.name, score))

    # Riepilogo
    print("\n" + "="*60)
    print("RIEPILOGO")
    print("="*60)

    for pack_name, score in scores:
        status = "✅" if score == 4 else "✓" if score >= 3 else "⚠️"
        print(f"  {status} {pack_name:15s} - Score: {score}/4")

    avg_score = sum(s for _, s in scores) / len(scores)
    print(f"\n  📊 Average Score: {avg_score:.1f}/4")

    if avg_score >= 3.5:
        print("\n  🎉 Tutti i loop sono di alta qualità!")
    elif avg_score >= 2.5:
        print("\n  👍 La maggior parte dei loop è accettabile")
    else:
        print("\n  ⚠️  Alcuni loop potrebbero avere problemi")


if __name__ == '__main__':
    main()
