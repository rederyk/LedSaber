#!/bin/bash
# Script di avvio per LedSaber TUI
# Stile Cyberpunk Edition

set -e

VENV_DIR="./venv"
SCRIPT="saber_tui.py"

# Colori ANSI
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
MAGENTA='\033[0;35m'
RESET='\033[0m'
BOLD='\033[1m'

echo -e "${CYAN}${BOLD}"
cat << "EOF"
╔═══════════════════════════════════════════════╗
║                                               ║
║    🗡️  L E D S A B E R   T U I   🗡️           ║
║                                               ║
║         C Y B E R P U N K   E D I T I O N    ║
║                                               ║
╚═══════════════════════════════════════════════╝
EOF
echo -e "${RESET}"

# Controlla Python
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}✗ Python3 non trovato${RESET}"
    exit 1
fi

echo -e "${GREEN}✓ Python3 trovato: $(python3 --version)${RESET}"

# Controlla/crea virtual environment
if [ ! -d "$VENV_DIR" ]; then
    echo -e "${CYAN}📦 Creazione virtual environment...${RESET}"
    python3 -m venv "$VENV_DIR"
    echo -e "${GREEN}✓ Virtual environment creato${RESET}"
else
    echo -e "${GREEN}✓ Virtual environment già esistente${RESET}"
fi

# Attiva venv
echo -e "${CYAN}🔧 Attivazione virtual environment...${RESET}"
source "$VENV_DIR/bin/activate"

# Aggiorna pip
echo -e "${CYAN}📥 Aggiornamento pip...${RESET}"
pip install --upgrade pip -q

# Installa dipendenze
echo -e "${CYAN}📥 Installazione dipendenze TUI...${RESET}"
pip install -r requirements.txt -q

echo -e "${GREEN}✓ Dipendenze installate${RESET}"

# Verifica Textual
if python3 -c "import textual" 2>/dev/null; then
    echo -e "${GREEN}✓ Textual installato correttamente${RESET}"
else
    echo -e "${RED}✗ Errore installazione Textual${RESET}"
    exit 1
fi

# Controlla permessi Bluetooth
if ! groups | grep -q bluetooth; then
    echo -e ""
    echo -e "${YELLOW}⚠ NOTA: Il tuo utente non è nel gruppo 'bluetooth'${RESET}"
    echo -e "  Per evitare problemi di permessi, esegui:"
    echo -e "  ${BOLD}sudo usermod -a -G bluetooth \$USER${RESET}"
    echo -e "  (poi riavvia la sessione)"
    echo -e ""
fi

echo -e "${CYAN}${BOLD}"
echo -e "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "✓ Setup completato!"
echo -e "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${RESET}"

echo -e "${MAGENTA}🚀 Avvio LedSaber TUI...${RESET}"
echo -e ""

# Avvia TUI
python3 "$SCRIPT"
