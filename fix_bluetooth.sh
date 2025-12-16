#!/bin/bash
# Script per risolvere problemi di connessione BLE su Linux

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}  🔧 Fix Bluetooth BLE Issues${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo

# Controlla se bluetoothctl è disponibile
if ! command -v bluetoothctl &> /dev/null; then
    echo -e "${RED}✗ bluetoothctl non trovato!${NC}"
    echo -e "  Installa: sudo apt install bluez"
    exit 1
fi

echo -e "${YELLOW}Questo script risolverà problemi comuni di connessione BLE:${NC}"
echo -e "  - Rimuove dispositivi dalla cache Bluetooth"
echo -e "  - Riavvia il servizio Bluetooth"
echo -e "  - Pulisce accoppiamenti BR/EDR (Classic)"
echo
echo -e "${YELLOW}⚠  Richiede permessi sudo${NC}"
echo
read -p "Continua? [s/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[SsYy]$ ]]; then
    echo "Annullato."
    exit 0
fi

echo
echo -e "${CYAN}1️⃣  Scansione dispositivi accoppiati...${NC}"
DEVICES=$(bluetoothctl devices | grep -i "LedSaber")

if [ -z "$DEVICES" ]; then
    echo -e "${GREEN}✓ Nessun dispositivo LedSaber nella cache${NC}"
else
    echo -e "${YELLOW}Trovati dispositivi:${NC}"
    echo "$DEVICES"
    echo

    while IFS= read -r line; do
        ADDR=$(echo "$line" | awk '{print $2}')
        NAME=$(echo "$line" | cut -d' ' -f3-)

        echo -e "${YELLOW}Rimozione: ${NAME} (${ADDR})...${NC}"
        echo "remove $ADDR" | bluetoothctl > /dev/null 2>&1
        echo -e "${GREEN}✓ Rimosso${NC}"
    done <<< "$DEVICES"
fi

echo
echo -e "${CYAN}2️⃣  Riavvio servizio Bluetooth...${NC}"
sudo systemctl restart bluetooth

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Servizio Bluetooth riavviato${NC}"
    sleep 2
else
    echo -e "${RED}✗ Errore riavvio servizio${NC}"
    exit 1
fi

echo
echo -e "${CYAN}3️⃣  Verifica servizio...${NC}"
if systemctl is-active --quiet bluetooth; then
    echo -e "${GREEN}✓ Bluetooth attivo${NC}"
else
    echo -e "${RED}✗ Bluetooth non attivo${NC}"
    exit 1
fi

echo
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✓ Completato!${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo
echo -e "${CYAN}Ora puoi provare a riconnettere con:${NC}"
echo -e "  ${YELLOW}./start.sh${NC}"
echo
