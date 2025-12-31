# 🐛 WELLNESS MODE - ERRORI E INCONGRUENZE DA CORREGGERE

**Data**: 2025-12-31
**Autore**: Claude Sonnet 4.5
**Status**: ⚠️ SISTEMA INCONSISTENTE - RICHIEDE REFACTORING

---

## ❌ PROBLEMA PRINCIPALE: CONFUSIONE TRA TEMI E MODALITÀ

### Errore Concettuale

Il sistema attuale ha **DUE controlli separati ma sovrapposti**:

1. **Hour Theme selector** (0-11): Include sia temi classici (0-5) CHE wellness (6-11)
2. **Wellness Mode toggle**: Switch separato che dovrebbe attivare "wellness behavior"

**CONSEGUENZA**: L'utente può:
- Selezionare tema "Circadian" (6) ma avere wellness mode OFF → comportamento inconsistente
- Avere wellness mode ON ma tema "Classic" (0) → non ha senso
- Second themes (0-5) sono ancora visibili anche con wellness mode ON → confusione

---

## 🔧 SOLUZIONI PROPOSTE

### **OPZIONE A: Temi Wellness Sono Modalità Separate** ⭐ CONSIGLIATO

I temi 6-11 dovrebbero **SEMPRE** essere in wellness mode, senza switch separato.

**Modifiche richieste**:

1. **Rimuovere lo switch "Enable Wellness Mode"** da UI
2. **Auto-attivare wellness mode** quando `chronoHourTheme >= 6`
3. **Nascondere second themes** quando tema >= 6 (hanno effetti integrati)
4. **Mostrare solo breathing rate slider** quando tema >= 6

**Logica firmware** (LedEffectEngine.cpp):
```cpp
// Determina automaticamente wellness mode
bool isWellnessTheme = (state.chronoHourTheme >= 6);

if (isWellnessTheme) {
    // Forza wellness mode
    // Ignora chronoSecondTheme
    // Usa effetti integrati (breeze, fireflies, etc.)
} else {
    // Temi classici 0-5
    // Usa chronoSecondTheme normalmente
}
```

**UI Changes**:
- Dashboard Python: Nascondi second theme selector quando hour >= 6
- Flutter App: Same logic
- Mostra breathing slider SOLO per temi 6-11

---

### **OPZIONE B: Wellness Mode È Toggle Globale**

Wellness mode modifica il COMPORTAMENTO di TUTTI i temi.

**Modifiche richieste**:

1. **Tenere lo switch wellness**
2. **Rimuovere temi 6-11 separati** dalla lista hour themes
3. **Creare varianti wellness** di TUTTI i temi 0-5
4. **Second themes** diventano "wellness effects" quando toggle ON

**Logica**:
```cpp
if (state.chronoWellnessMode) {
    // TUTTI i temi (0-5) diventano slow/breathing
    // Second themes trasformati in particle effects
    renderChronoHours_ClassicWellness(...);
    renderChronoMinutes_Breeze(...);
} else {
    // Comportamento normale
    renderChronoHours_Classic(...);
    renderChronoSeconds_Classic(...);
}
```

**Problema**: Molto più lavoro (6 temi × 2 varianti = 12 implementazioni)

---

### **OPZIONE C: Sistema Ibrido Migliorato**

Tenere entrambi ma con regole chiare.

**Regole**:
1. **Temi 0-5**: Possono avere wellness ON/OFF
   - OFF: Comportamento attuale (marker + second themes)
   - ON: Versione slow + breathing (no second themes)

2. **Temi 6-11**: SEMPRE wellness mode (switch disabilitato/nascosto)
   - Effetti integrati fissi
   - No second themes selector

**UI Logic**:
```python
# Dashboard
if hour_theme >= 6:
    # Nascondi wellness toggle (sempre ON)
    # Nascondi second theme selector
    # Mostra solo breathing slider
else:
    # Mostra wellness toggle
    if wellness_mode:
        # Nascondi second theme selector
        # Mostra breathing slider
    else:
        # Mostra second theme selector
```

---

## 📋 CHECKLIST REFACTORING (OPZIONE A - CONSIGLIATO)

### Firmware (LedEffectEngine.cpp)

- [ ] Rimuovere dipendenza da `state.chronoWellnessMode` per temi 6-11
- [ ] Auto-determinare wellness: `bool isWellness = (hourTheme >= 6)`
- [ ] Modificare logica rendering (linea 3050-3112):
  ```cpp
  bool isWellnessTheme = (state.chronoHourTheme >= 6);

  if (!isWellnessTheme) {
      // Render second themes normalmente (0-5)
  } else {
      // Render wellness effects integrati
      // (codice esistente 3082-3111)
  }
  ```

### Dashboard Python (saber_dashboard.py)

- [ ] Rimuovere variabili `wellness_mode`, `breathing_rate` da ChronoThemesCard
- [ ] Rimuovere comando `wellness`
- [ ] Rimuovere funzione `_send_wellness_config()`
- [ ] Rimuovere action `action_wellness_toggle()`
- [ ] Modificare `render()` ChronoThemesCard:
  ```python
  # Mostra second theme SOLO se hour < 6
  if self.chrono_hour_theme < 6:
      # Render second theme selector
  else:
      # Render breathing rate slider (sempre attivo per 6-11)
  ```
- [ ] Aggiornare `_send_chrono_themes()`:
  ```python
  command = {
      "mode": "chrono_hybrid",
      "chronoHourTheme": hour_theme,
      "chronoSecondTheme": second_theme if hour_theme < 6 else 0,
      "breathingRate": breathing_rate if hour_theme >= 6 else 5
  }
  ```

### Flutter App

#### lib/models/led_state.dart
- [ ] **OPZIONALE**: Rimuovere `chronoWellnessMode` (o mantenerlo per compatibilità backward)
- [ ] Tenere `breathingRate` (usato da temi 6-11)

#### lib/screens/tabs/clock_tab.dart
- [ ] Rimuovere `_wellnessMode` (determina automaticamente da `_hourTheme >= 6`)
- [ ] Nascondere SwitchListTile "Enable Wellness Mode"
- [ ] Mostrare breathing slider SOLO quando `_hourTheme >= 6`:
  ```dart
  // Nascondere second theme
  if (_hourTheme < 6) ...[
    // Second theme dropdown
  ],

  // Mostrare breathing SOLO per wellness themes
  if (_hourTheme >= 6) ...[
    // Breathing rate slider
  ],
  ```
- [ ] Aggiornare `_applyChronoThemes()`:
  ```dart
  ledProvider.setEffect(
    'chrono_hybrid',
    chronoHourTheme: _hourTheme,
    chronoSecondTheme: _hourTheme < 6 ? _secondTheme : 0,
    breathingRate: _hourTheme >= 6 ? _breathingRate.toInt() : 5,
  );
  ```

#### lib/providers/led_provider.dart
- [ ] Rimuovere parametro `chronoWellnessMode` da `setEffect()` (opzionale, può restare per compatibilità)
- [ ] Tenere `breathingRate`

#### lib/services/led_service.dart
- [ ] Rimuovere `chronoWellnessMode` dal payload (opzionale)
- [ ] Tenere `breathingRate`

---

## 🎯 COMPORTAMENTO ATTESO FINALE

### Temi Classici (0-5)
```
Hour Theme: [Classic ▼]
Second Theme: [Classic ▼]  ← VISIBILE

[Apply Clock]
```

### Temi Wellness (6-11)
```
Hour Theme: [Circadian ▼]
(Second theme nascosto)

Breathing Rate: 5 BPM  ← VISIBILE
[━━━━●━━] (slider)
Relaxation

[Apply Clock]
```

---

## 🚨 BACKWARD COMPATIBILITY

**Attenzione**: Se rimuovi `chronoWellnessMode` dal protocollo BLE:
- Dashboard vecchie potrebbero inviare il parametro → ignorato (OK)
- Firmware vecchio riceve nuovi comandi senza parametro → usa default (OK)

**Soluzione sicura**: TENERE `chronoWellnessMode` nel protocollo ma **auto-settarlo nel firmware**:
```cpp
// In BLE callback handler
bool receivedWellnessMode = json.get("chronoWellnessMode", false);
bool computedWellnessMode = (chronoHourTheme >= 6);

// Usa computedWellnessMode, ignora receivedWellnessMode
state.chronoWellnessMode = computedWellnessMode;
```

---

## 📝 NOTE IMPLEMENTAZIONE

### Perché Opzione A è migliore:

1. **UX più semplice**: Un solo controllo (hour theme) invece di due
2. **Meno confusione**: Tema = comportamento, nessuna ambiguità
3. **Meno codice**: Non servono varianti wellness di temi classici
4. **Coerente**: Temi wellness SONO sempre wellness, no switch inutile

### Svantaggi:

- Gli utenti non possono "provare" wellness mode su temi classici
  - **Soluzione**: Non è necessario, i temi 6-11 sono specificamente progettati per wellness

---

## ⚙️ TESTING POST-REFACTORING

- [ ] Selezionare tema Classic (0) → second theme visibile, breathing nascosto
- [ ] Selezionare tema Circadian (6) → second theme nascosto, breathing visibile
- [ ] Dashboard: F6 cicla temi, UI si adatta automaticamente
- [ ] Flutter: Dropdown hour theme aggiorna UI correttamente
- [ ] Firmware: Temi 6-11 attivano sempre effetti wellness integrati
- [ ] Firmware: Temi 0-5 usano second themes normalmente

---

## 🔗 FILE DA MODIFICARE

### Priorità ALTA (blockers)
1. `src/LedEffectEngine.cpp` (linea 3050-3112) - Logica rendering
2. `saber_dashboard.py` (linea 694-750, 1334-1344, 1937-1947) - UI + comandi
3. `lib/screens/tabs/clock_tab.dart` (linea 91-100, 191-245) - UI Flutter

### Priorità MEDIA (cleanup)
4. `lib/providers/led_provider.dart` - Rimuovi parametro unused
5. `lib/services/led_service.dart` - Rimuovi parametro unused

### Priorità BASSA (opzionale)
6. `lib/models/led_state.dart` - Considera rimuovere chronoWellnessMode
7. `include/BLELedController.h` - Aggiungi commento deprecation

---

**PROSSIMO AGENTE**: Implementa OPZIONE A seguendo questa checklist.

**Stima tempo**: 1-2 ore
**Difficoltà**: Media (principalmente UI logic, nessun algoritmo complesso)
