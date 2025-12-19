# 🎨 Chrono Themes - Sistema Modulare

Sistema ibrido di temi per l'effetto `chrono_hybrid` che permette di combinare diversi stili visivi per marker delle ore e cursori secondi/minuti.

## 🧩 Architettura

Il rendering è diviso in **2 layer indipendenti**:

1. **Hour Markers** (`chronoHourTheme`) - Sfondo che rappresenta le 12 ore
2. **Second/Minute Cursors** (`chronoSecondTheme`) - Cursori che mostrano il tempo corrente

Ogni layer può usare un tema diverso, per un totale di **4 × 6 = 24 combinazioni** possibili!

---

## 📊 Temi Hour Markers

### 0. Classic
Marker statici uniformi al 15% brightness, posizionati ogni ora (12 punti fissi).

**Caratteristiche:**
- Minimalista
- Basso consumo energetico
- Ottimo per vedere i cursori in primo piano

---

### 1. Neon Cyberpunk
L'ora corrente brilla intensamente con effetto glow, le altre ore sono dim.

**Caratteristiche:**
- Ora corrente: 78% brightness + tint cyan
- Altre ore: 23% brightness
- Effetto "spotlight" sull'ora attiva
- Glow esteso su 3 LED per marker

**Ideale per:** Estetica cyberpunk, facile lettura dell'ora

---

### 2. Plasma
Blend arcobaleno fluido che ruota lentamente, intensificato sui marker delle ore.

**Caratteristiche:**
- Onda sinusoidale di colore HSV
- Hue base ruota con ore + minuti
- Marker evidenziati con boost brightness
- Effetto ipnotico e psichedelico

**Ideale per:** Visual spettacolari, party mode

---

### 3. Digital Glitch
Scan lines RGB digitali che pulsano sincronizzate con i secondi.

**Caratteristiche:**
- Marker alternati R-G-B (primari)
- Pulse sinusoidale (15-78% brightness)
- Effetto "matrix" scanline
- Sincronizzazione con secondi

**Ideale per:** Estetica retro/8-bit, sci-fi

---

## ⏱️ Temi Second/Minute Cursors

### 0. Classic
Cursori lineari solidi: minuti (5 LED) + secondi (3 LED).

**Caratteristiche:**
- Cursore minuti: 70% brightness, 5 LED
- Cursore secondi: bianco brillante, 3 LED
- Glitch cyan quando motion attivo (offset > 1s)
- Battito globale ogni secondo (fade-in/out)

**Ideale per:** Precisione, leggibilità massima

---

### 1. Time Spiral
Spirale colorata rotante a 3 bracci con scia (trail).

**Caratteristiche:**
- 3 bracci offset 120° (hue spacing)
- Trail di 8 LED per braccio (fade progressivo)
- Hue ruota con i secondi (4°/sec)
- Cursore minuti: punto brillante HSV

**Ideale per:** Visual dinamici, effetto "vortex"

---

### 2. Fire Clock 🔥
Fuoco che cresce dalla base: altezza = minuti, intensità = secondi.

**Caratteristiche:**
- Palette: rosso → arancione → giallo → bianco
- Fiamme tremolanti (random flicker)
- Pulse sincronizzato con secondi
- Motion: fuoco diventa blu ("cold fire glitch")

**Ideale per:** Drammatico, energia crescente

---

### 3. Lightning ⚡
Fulmini pulsanti con ramificazioni casuali.

**Caratteristiche:**
- Posizione strike basata sui minuti
- Frequenza 1-3 strike/sec (aumenta con secondi)
- Flash 80ms + afterglow residuo
- Ramificazioni casuali (30% probabilità)
- Tint blu elettrico

**Ideale per:** Effetto potenza, high-energy

---

### 4. Particle Flow
Particelle colorate che fluiscono lungo la lama.

**Caratteristiche:**
- Numero particelle: 3-9 (aumenta con secondi)
- Velocità: 1-7 unità/frame (aumenta con minuti)
- Trail di 4 LED per particella
- Hue spacing uniforme tra particelle

**Ideale per:** Fluido, organico, "data stream"

---

### 5. Quantum Wave
Onda sinusoidale con pattern di interferenza.

**Caratteristiche:**
- Frequenza: 1-6 Hz (aumenta con secondi)
- Ampiezza brightness: 20-200 (minuti)
- Doppia onda (interferenza quantistica)
- Hue shift basato su posizione + tempo
- Motion offset: tint blu

**Ideale per:** Scientifico, mesmerizing, "energy field"

---

## 🎮 Comandi BLE

### Cambiare Temi

Invia JSON al characteristic `CHAR_LED_EFFECT_UUID`:

```json
{
  "mode": "chrono_hybrid",
  "chronoHourTheme": 2,
  "chronoSecondTheme": 3
}
```

**Parametri:**
- `chronoHourTheme`: 0-3 (Classic, Neon, Plasma, Digital)
- `chronoSecondTheme`: 0-5 (Classic, Spiral, Fire, Lightning, Particle, Quantum)

### Esempi Python (Dashboard)

```python
# Plasma hours + Fire clock seconds
await send_effect_command({
    "mode": "chrono_hybrid",
    "chronoHourTheme": 2,
    "chronoSecondTheme": 2
})

# Neon hours + Lightning seconds
await send_effect_command({
    "mode": "chrono_hybrid",
    "chronoHourTheme": 1,
    "chronoSecondTheme": 3
})

# Digital hours + Quantum wave seconds
await send_effect_command({
    "mode": "chrono_hybrid",
    "chronoHourTheme": 3,
    "chronoSecondTheme": 5
})
```

---

## 💡 Combinazioni Consigliate

### 🎯 Leggibilità Massima
```
chronoHourTheme: 0 (Classic)
chronoSecondTheme: 0 (Classic)
```
Ottimo per: Lettura precisa del tempo, uso quotidiano

---

### 🌆 Cyberpunk Night
```
chronoHourTheme: 1 (Neon)
chronoSecondTheme: 3 (Lightning)
```
Ottimo per: Combattimento, energia alta

---

### 🔥 Elemental Fire
```
chronoHourTheme: 2 (Plasma)
chronoSecondTheme: 2 (Fire Clock)
```
Ottimo per: Visual spettacolare, mood intenso

---

### 🌀 Vortex Time
```
chronoHourTheme: 2 (Plasma)
chronoSecondTheme: 1 (Time Spiral)
```
Ottimo per: Hypnotic, visual fluido

---

### ⚡ Matrix Mode
```
chronoHourTheme: 3 (Digital)
chronoSecondTheme: 4 (Particle Flow)
```
Ottimo per: Estetica hacker, data flow

---

### 🌌 Quantum Field
```
chronoHourTheme: 2 (Plasma)
chronoSecondTheme: 5 (Quantum Wave)
```
Ottimo per: Sci-fi, dimensione alternativa

---

## 🎛️ Parametri Personalizzabili

Ogni tema rispetta:
- **Colore base** (`state.r`, `state.g`, `state.b`)
- **Motion offset** (`_visualOffset`) - distorce il tempo quando la spada si muove
- **FoldPoint** (`state.foldPoint`) - adatta rendering alla lunghezza LED

I temi automaticamente:
- ✅ Ritornano all'ora esatta dopo movimento
- ✅ Scalano con qualsiasi lunghezza LED
- ✅ Rispettano il colore primario impostato
- ✅ Applicano effetti glitch/shift quando motion attivo

---

## 🧠 Note Tecniche

### Performance
- Tutti i temi ottimizzati per 60 FPS
- Memory footprint minimo (condividono calcoli temporali)
- Zero allocazioni dinamiche nel rendering

### Compatibilità Motion
- Motion offset applicato solo ai cursori secondi (layer 2)
- Hour markers rimangono stabili per riferimento
- Effetti speciali attivati quando `abs(_visualOffset) > 1.0f`

### Estensibilità
Per aggiungere nuovi temi:
1. Aggiungere enum in `LedEffectEngine.h` (es. `NEBULA = 6`)
2. Implementare `renderChronoSeconds_Nebula()` in `LedEffectEngine.cpp`
3. Aggiungere `case 6` nello switch di `renderChronoHybrid()`

---

## 📝 Changelog

**v1.0** (2025-12-19)
- ✨ Implementazione sistema modulare ibrido
- ✨ 4 temi hour markers + 6 temi second cursors
- ✨ 24 combinazioni totali
- ✨ Supporto BLE per cambio tema runtime
- ✨ Zero breaking changes (default = Classic/Classic)
