# 🎵 Audio Resources - Lightsaber Sounds

Guida completa per ottenere suoni lightsaber: **generazione procedurale Python** + **risorse gratuite online**.

---

## 🐍 OPZIONE 1: Generazione Procedurale (Python)

### Setup

```bash
# Installa dipendenze
pip install numpy scipy

# Genera tutti i pack
cd tools/
python audio_generator.py --pack all
```

### Comandi Disponibili

```bash
# Genera solo pack Jedi
python audio_generator.py --pack jedi

# Genera solo pack Sith
python audio_generator.py --pack sith

# Genera in directory custom
python audio_generator.py --pack jedi --output /path/to/custom/dir

# Sample rate custom (default 44100Hz)
python audio_generator.py --pack all --sample-rate 48000
```

### Output

```
AppMobile/flutter_led_saber/assets/sounds/
├── jedi/
│   ├── hum_base.wav    (3.0s, ~260KB)
│   ├── ignition.wav    (1.5s, ~130KB)
│   ├── clash.wav       (0.4s, ~35KB)
│   └── retract.wav     (1.2s, ~100KB)
├── sith/
│   └── ... (stessi file)
├── unstable/
│   └── ... (stessi file)
└── crystal/
    └── ... (stessi file)
```

### Parametri Sonori per Pack

| Pack       | Freq Base | Caratteristiche                          |
|------------|-----------|------------------------------------------|
| **jedi**   | 110 Hz    | Pulito, armoniche semplici, vibrato 4Hz  |
| **sith**   | 70 Hz     | Grave, distorto, vibrato 3Hz             |
| **unstable** | 90 Hz   | Crackling, modulazione casuale           |
| **crystal** | 150 Hz   | Acuto, armoniche alte, shimmer           |

### Customizzazione

Modifica i parametri in `audio_generator.py`:

```python
# Esempio: rendere il suono Jedi più acuto
'jedi': {
    'hum_freq': 130,          # Era 110
    'hum_duration': 4.0,      # Era 3.0
    'ignition_duration': 1.2, # Era 1.5
    # ...
}
```

### Vantaggi
- ✅ **100% gratuito**
- ✅ **Customizzabile** (frequenze, durate, stile)
- ✅ **Loop perfetto** garantito
- ✅ **Nessun copyright**
- ✅ **Ripetibile** (regenera quando vuoi)

### Svantaggi
- ❌ Suono "sintetico" (meno realistico di registrazioni vere)
- ❌ Richiede Python + dipendenze

---

## 🌐 OPZIONE 2: Risorse Gratuite Online

### 🎯 **MIGLIORI SITI - SOUND EFFECTS**

#### 1. **Freesound.org** ⭐⭐⭐⭐⭐
**URL:** https://freesound.org/

**Ricerca:**
- `lightsaber hum`
- `lightsaber swing`
- `lightsaber ignition`
- `laser sword`

**License:** CC0, CC-BY (verificare per ogni file)

**Pacchetti consigliati:**
- [Lightsaber Collection by jobro](https://freesound.org/people/jobro/packs/2489/)
- [Star Wars Sounds by michorvath](https://freesound.org/people/michorvath/packs/11315/)

**Pros:**
- ✅ Qualità alta
- ✅ Molte varianti
- ✅ Community attiva

**Cons:**
- ⚠️ Richiede account (gratis)
- ⚠️ License miste (controllare sempre)

---

#### 2. **Zapsplat.com** ⭐⭐⭐⭐
**URL:** https://www.zapsplat.com/

**Ricerca:**
- `lightsaber`
- `sci-fi sword`
- `energy weapon`

**License:** Standard License (uso personale/commerciale gratuito)

**Pros:**
- ✅ Suoni professionali
- ✅ Download diretto MP3/WAV
- ✅ Categorizzazione eccellente

**Cons:**
- ⚠️ Richiede crediti (attribution richiesta)

---

#### 3. **BBC Sound Effects** ⭐⭐⭐⭐⭐
**URL:** https://sound-effects.bbcrewind.co.uk/

**Ricerca:**
- `electronic buzz`
- `humming`
- `laser`

**License:** RemArc License (uso non-commerciale educational)

**Pros:**
- ✅ Archivio BBC storico
- ✅ Qualità broadcast
- ✅ 16000+ effetti

**Cons:**
- ⚠️ Uso commerciale limitato
- ⚠️ Non specifico lightsaber (ma ottimi buzz/hum)

---

#### 4. **Pixabay Sound Effects** ⭐⭐⭐
**URL:** https://pixabay.com/sound-effects/

**Ricerca:**
- `sword whoosh`
- `electric hum`
- `sci-fi`

**License:** Pixabay License (uso libero)

**Pros:**
- ✅ No account richiesto
- ✅ License semplice
- ✅ Download diretto

**Cons:**
- ⚠️ Meno varietà per lightsaber specifici

---

### 🛠️ **STRUMENTI ONLINE - GENERATORI**

#### 1. **SFXR / JSFXR** ⭐⭐⭐⭐
**URL:** https://sfxr.me/

**Cosa fa:** Genera effetti sonori 8-bit/retro

**Come usare per lightsaber:**
1. Categoria: `Blip/Select`
2. Aumenta `Sustain` (suono continuo)
3. Aggiungi `Vibrato`
4. Export WAV

**Pros:**
- ✅ Browser-based
- ✅ Export immediato
- ✅ Randomize button

**Cons:**
- ⚠️ Suono retro (non realistico)

---

#### 2. **ChipTone** ⭐⭐⭐
**URL:** http://sfbgames.com/chiptone/

**Cosa fa:** Synth avanzato per effetti sonori

**Preset consigliati:**
- Laser
- Sci-Fi
- Hum

**Pros:**
- ✅ Controllo fine parametri
- ✅ Waveforms multiple
- ✅ Export WAV

---

### 🎹 **DAW GRATUITI - Creazione Custom**

#### 1. **Audacity** (Free, Open Source) ⭐⭐⭐⭐⭐
**URL:** https://www.audacityteam.org/

**Come creare lightsaber hum:**
```
1. Generate → Tone
   - Waveform: Square o Sine
   - Frequency: 100-150 Hz
   - Duration: 3 seconds

2. Effect → Tremolo (vibrato)
   - Depth: 10-20%
   - Frequency: 4-6 Hz

3. Effect → Repeat (loop test)

4. Export → WAV (16-bit, 44100Hz)
```

**Tutorial:**
- Generate Tone (base hum)
- Add harmonics (Generate Tone × 2 freq, lower amplitude)
- Mix tracks
- Add noise (Generate → Noise, 5% mix)
- Apply effects (Tremolo, Chorus)
- Normalize & Export

---

#### 2. **LMMS** (Free, Open Source) ⭐⭐⭐⭐
**URL:** https://lmms.io/

**Synth consigliati:**
- TripleOscillator (layering)
- ZynAddSubFX (harmonics)

**Pros:**
- ✅ Synth engine potente
- ✅ Automation (pitch bend, filter sweep)
- ✅ VST support

---

### 📦 **PACCHETTI COMPLETI GRATUITI**

#### 1. **Star Wars Lightsaber SFX Pack (Fan-made)**
**Cerca su Google:** `star wars lightsaber sfx pack free download`

**Siti comuni:**
- Reddit r/lightsabers (check wiki)
- SoundBoard apps (estrai file)
- YouTube (rip audio con youtube-dl)

**⚠️ ATTENZIONE COPYRIGHT:**
- I suoni originali Star Wars sono © Lucasfilm/Disney
- Uso commerciale NON permesso
- Uso personale/educativo OK (gray area)
- Per app pubblica, meglio usare suoni sintetici

---

#### 2. **OpenGameArt.org** ⭐⭐⭐⭐
**URL:** https://opengameart.org/

**Ricerca:**
- `laser sword`
- `energy weapon`
- `sci-fi combat`

**License:** CC0, CC-BY (verificare)

**Pros:**
- ✅ Pensato per gamedev
- ✅ License chiare
- ✅ Spesso loop-ready

---

## 🎧 GUIDA RAPIDA: WORKFLOW CONSIGLIATO

### Per Prototyping Veloce
1. Usa **Python generator** (`audio_generator.py`)
2. Genera pack base: `python audio_generator.py --pack jedi`
3. Testa nell'app
4. Itera parametri

### Per Qualità Professionale
1. Scarica samples da **Freesound.org**
2. Importa in **Audacity**
3. Taglia loop perfetto (Find Zero Crossings)
4. Applica Crossfade (50ms)
5. Normalizza a -3dB
6. Export WAV 44100Hz 16-bit

### Per App Commerciale
1. Usa **Python generator** (no copyright)
2. Oppure commissiona sound designer (Fiverr, $20-50)
3. Oppure registra + synth custom in LMMS
4. **MAI** usare audio Star Wars originale

---

## 📝 CHECKLIST VALIDAZIONE AUDIO

Dopo aver ottenuto i file, verifica:

- [ ] **hum_base.wav**
  - [ ] Loop perfetto (no click quando ripete)
  - [ ] Durata 2-5 secondi
  - [ ] Sample rate 44100Hz
  - [ ] Bit depth 16-bit
  - [ ] Format WAV (non MP3 per latency)

- [ ] **ignition.wav**
  - [ ] Crescendo naturale
  - [ ] Durata 1-2 secondi
  - [ ] Peak non clipping (-3dB max)

- [ ] **clash.wav**
  - [ ] Impatto percussivo
  - [ ] Durata 0.3-0.5 secondi
  - [ ] Frequenze alte presenti

- [ ] **retract.wav**
  - [ ] Fade out graduale
  - [ ] Durata 1-2 secondi
  - [ ] Pitch decrescente

---

## 🔧 TOOLS PER EDITING

### Verifica Loop (Zero Crossing)

**Audacity:**
1. Apri `hum_base.wav`
2. Select → All (Ctrl+A)
3. View → Show Clipping
4. Zooma su fine traccia
5. Verifica che inizio = fine (zero crossing)

**Python (verifica programmtica):**
```python
import numpy as np
from scipy.io import wavfile

rate, data = wavfile.read('hum_base.wav')

# Controlla se inizio ≈ fine
start_sample = data[0]
end_sample = data[-1]

if abs(start_sample - end_sample) > 100:
    print("⚠️ Loop NON perfetto! Differenza:", abs(start_sample - end_sample))
else:
    print("✓ Loop OK")
```

### Crossfade per Loop

**Audacity:**
1. Select ultimi 50ms
2. Effect → Fade Out
3. Select primi 50ms
4. Effect → Fade In
5. File → Export → WAV

---

## 💡 TIPS AVANZATI

### Rendere suoni più "lightsaber-like"

**Aggiungi vibrato:**
- Frequenza: 4-6 Hz
- Depth: 10-20%

**Layering:**
- Track 1: Sine wave (fondamentale)
- Track 2: Square wave (armoniche)
- Track 3: Pink noise (texture)
- Mix 60% / 30% / 10%

**Filtri:**
- High-pass @ 80Hz (rimuovi rumble)
- Low-pass @ 8kHz (rimuovi hiss)
- Notch filter per risonanze indesiderate

**Chorus/Flanger:**
- Aggiunge "spazialità" al suono
- Parametri leggeri (mix 10-15%)

---

## 📚 TUTORIAL VIDEO CONSIGLIATI

**YouTube searches:**
- "How to make lightsaber sound effect"
- "Audacity lightsaber tutorial"
- "Synthesize laser sword sound"

**Canali consigliati:**
- In The Mix (sound design basics)
- Michael Curtis (Audacity tutorials)
- Game Audio (procedural sfx)

---

## 🎯 RECAP: QUALE SOLUZIONE USARE?

| Scenario | Soluzione Consigliata |
|----------|----------------------|
| **Prototipo veloce** | Python generator (`audio_generator.py`) |
| **App personale** | Freesound.org + Audacity editing |
| **App commerciale** | Python generator OR custom synth |
| **Qualità massima** | Sound designer professionista |
| **Zero budget** | Python generator (100% gratis) |
| **Copyright-safe** | Python generator OR CC0 da Freesound |

---

## ✅ PROSSIMI STEP

1. **Scegli approccio:**
   - [ ] Python generator (veloce)
   - [ ] Freesound download (qualità)
   - [ ] DAW custom (controllo totale)

2. **Genera/Scarica file**

3. **Valida qualità:**
   - [ ] Testa loop hum_base.wav
   - [ ] Controlla durate
   - [ ] Verifica no clipping

4. **Integra in app:**
   - [ ] Copia in `assets/sounds/`
   - [ ] Aggiorna `pubspec.yaml`
   - [ ] Implementa AudioService

---

**Happy Sound Design! 🎵**
