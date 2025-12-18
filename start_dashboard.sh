#!/bin/bash
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# LedSaber Dashboard Launcher
# BTTop-style Live Monitor - Resistance Edition
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"
PYTHON_SCRIPT="$SCRIPT_DIR/saber_dashboard.py"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "⚔️  LEDSABER DASHBOARD - RESISTANCE EDITION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# ═══════════════════════════════════════════════════════════
# 1. Controlla Virtual Environment
# ═══════════════════════════════════════════════════════════

if [ ! -d "$VENV_DIR" ]; then
    echo "🔧 Virtual environment non trovato. Creazione in corso..."
    python3 -m venv "$VENV_DIR"
    echo "✓ Virtual environment creato"
fi

echo "🐍 Attivazione virtual environment..."
source "$VENV_DIR/bin/activate"

# ═══════════════════════════════════════════════════════════
# 2. Verifica dipendenze
# ═══════════════════════════════════════════════════════════

echo "📦 Verifica dipendenze..."

# Controlla se textual è installato
if ! python -c "import textual" 2>/dev/null; then
    echo "⚠️  Textual non trovato. Installazione dipendenze..."
    pip install -q textual rich bleak
    echo "✓ Dipendenze installate"
else
    echo "✓ Dipendenze OK"
fi

# ═══════════════════════════════════════════════════════════
# 3. Verifica permessi Bluetooth
# ═══════════════════════════════════════════════════════════

echo "📡 Verifica permessi Bluetooth..."

if ! groups | grep -q bluetooth; then
    echo "⚠️  ATTENZIONE: L'utente non è nel gruppo 'bluetooth'"
    echo "   Per risolvere, esegui:"
    echo "   sudo usermod -a -G bluetooth $USER"
    echo "   Poi riavvia la sessione (logout/login)"
    echo ""
    echo "   Provo comunque ad avviare..."
else
    echo "✓ Permessi Bluetooth OK"
fi

# ═══════════════════════════════════════════════════════════
# 4. Controlla dimensioni terminale
# ═══════════════════════════════════════════════════════════

TERM_COLS=$(tput cols)
TERM_LINES=$(tput lines)

echo "📐 Dimensioni terminale: ${TERM_COLS}x${TERM_LINES}"

if [ "$TERM_COLS" -lt 80 ] || [ "$TERM_LINES" -lt 24 ]; then
    echo "⚠️  ATTENZIONE: Terminale troppo piccolo!"
    echo "   Dimensioni minime raccomandate: 80x24"
    echo "   Dimensioni ottimali: 100x30 o superiori"
    echo ""
    echo "   Ridimensiona il terminale per una migliore esperienza."
    echo ""
    read -p "   Vuoi continuare comunque? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Uscita."
        exit 1
    fi
else
    echo "✓ Dimensioni terminale OK"
fi

# ═══════════════════════════════════════════════════════════
# 5. Avvio Dashboard
# ═══════════════════════════════════════════════════════════

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 Avvio Dashboard..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "💡 Quick Start:"
echo "   • Premi Ctrl+S per scansionare e connettere"
echo "   • Premi F2 per inizializzare camera"
echo "   • Premi F3 per avviare camera"
echo "   • Premi F5 per toggle motion detection"
echo "   • Digita 'help' per lista comandi completa"
echo ""
echo "   Ctrl+C per uscire"
echo ""
sleep 2

# Avvia dashboard
python "$PYTHON_SCRIPT"

# ═══════════════════════════════════════════════════════════
# 6. Cleanup
# ═══════════════════════════════════════════════════════════

echo ""
echo "👋 Dashboard chiusa."
echo ""
