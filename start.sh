#!/bin/bash
# Script di avvio automatico per LedSaber BLE Controller
# Crea virtual environment, installa dipendenze e avvia lo script

set -e  # Esci in caso di errore

VENV_DIR="./venv"
SCRIPT="ledsaber_control.py"

# Colori per output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  🗡️  LedSaber BLE Controller - Setup${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo

# Verifica che Python3 sia installato
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}✗ Python3 non trovato!${NC}"
    echo -e "${YELLOW}  Installa Python3:${NC}"
    echo -e "  sudo apt install python3 python3-pip python3-venv"
    exit 1
fi

PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
echo -e "${GREEN}✓ Python3 trovato: ${PYTHON_VERSION}${NC}"

# Crea virtual environment se non esiste
if [ ! -d "$VENV_DIR" ]; then
    echo -e "${YELLOW}📦 Creazione virtual environment...${NC}"
    python3 -m venv "$VENV_DIR"
    echo -e "${GREEN}✓ Virtual environment creato in ${VENV_DIR}${NC}"
else
    echo -e "${GREEN}✓ Virtual environment già esistente${NC}"
fi

# Attiva virtual environment
echo -e "${YELLOW}🔧 Attivazione virtual environment...${NC}"
source "$VENV_DIR/bin/activate"

# Aggiorna pip
echo -e "${YELLOW}📥 Aggiornamento pip...${NC}"
pip install --upgrade pip -q

# Installa dipendenze
if [ -f "requirements.txt" ]; then
    echo -e "${YELLOW}📥 Installazione dipendenze...${NC}"
    pip install -r requirements.txt -q
    echo -e "${GREEN}✓ Dipendenze installate${NC}"
else
    echo -e "${RED}✗ File requirements.txt non trovato!${NC}"
    exit 1
fi

# Verifica installazione bleak
if python3 -c "import bleak" 2>/dev/null; then
    BLEAK_VERSION=$(python3 -c "from importlib.metadata import version; print(version('bleak'))")
    echo -e "${GREEN}✓ bleak ${BLEAK_VERSION} installato correttamente${NC}"
else
    echo -e "${RED}✗ Errore installazione bleak${NC}"
    exit 1
fi

echo
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✓ Setup completato!${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo

# Verifica permessi Bluetooth
if ! groups | grep -q bluetooth; then
    echo -e "${YELLOW}⚠ NOTA: Il tuo utente non è nel gruppo 'bluetooth'${NC}"
    echo -e "${YELLOW}  Per evitare problemi di permessi, esegui:${NC}"
    echo -e "  ${CYAN}sudo usermod -a -G bluetooth \$USER${NC}"
    echo -e "  ${YELLOW}(poi riavvia la sessione)${NC}"
    echo
fi

# Avvia lo script
echo -e "${CYAN}🚀 Avvio LedSaber Controller...${NC}"
echo

if [ -f "$SCRIPT" ]; then
    python3 "$SCRIPT"
else
    echo -e "${RED}✗ Script ${SCRIPT} non trovato!${NC}"
    exit 1
fi

# Deattiva virtual environment all'uscita
deactivate
