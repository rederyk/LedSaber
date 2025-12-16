#!/bin/bash
# Script di test BLE per LedSaber tramite bluetoothctl
# Richiede BlueZ installato (bluetoothctl)

set -e

# Colori per output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# UUID delle caratteristiche BLE
CHAR_COLOR_UUID="d1e5a4c3-eb10-4a3e-8a4c-1234567890ab"
CHAR_BRIGHTNESS_UUID="f3e7c6e5-0d32-4c5a-ac6e-3456789012cd"

# MAC address del dispositivo ESP32 (da modificare)
ESP32_MAC="${ESP32_MAC:-}"

echo -e "${GREEN}=== Test BLE LedSaber ===${NC}"

# Verifica se è stato fornito il MAC address
if [ -z "$ESP32_MAC" ]; then
    echo -e "${YELLOW}MAC address non specificato. Uso: ESP32_MAC=XX:XX:XX:XX:XX:XX $0${NC}"
    echo -e "${YELLOW}Scansione dispositivi BLE disponibili...${NC}"
    echo ""

    # Avvia scansione BLE
    bluetoothctl <<EOF
power on
scan on
EOF

    echo -e "${YELLOW}Premi Ctrl+C per interrompere la scansione${NC}"
    echo -e "${YELLOW}Poi rilancia lo script con: ESP32_MAC=XX:XX:XX:XX:XX:XX $0${NC}"
    exit 0
fi

echo -e "${GREEN}Connessione a dispositivo: $ESP32_MAC${NC}"

# Test 1: Accendi LED rosso
echo -e "\n${GREEN}Test 1: Accendi LED rosso${NC}"
bluetoothctl <<EOF
power on
connect $ESP32_MAC
select-attribute $CHAR_COLOR_UUID
write '{"r":255,"g":0,"b":0}'
disconnect
EOF

sleep 2

# Test 2: Accendi LED verde
echo -e "\n${GREEN}Test 2: Accendi LED verde${NC}"
bluetoothctl <<EOF
power on
connect $ESP32_MAC
select-attribute $CHAR_COLOR_UUID
write '{"r":0,"g":255,"b":0}'
disconnect
EOF

sleep 2

# Test 3: Accendi LED blu
echo -e "\n${GREEN}Test 3: Accendi LED blu${NC}"
bluetoothctl <<EOF
power on
connect $ESP32_MAC
select-attribute $CHAR_COLOR_UUID
write '{"r":0,"g":0,"b":255}'
disconnect
EOF

sleep 2

# Test 4: Spegni LED
echo -e "\n${GREEN}Test 4: Spegni LED${NC}"
bluetoothctl <<EOF
power on
connect $ESP32_MAC
select-attribute $CHAR_BRIGHTNESS_UUID
write '{"brightness":0,"enabled":false}'
disconnect
EOF

sleep 2

# Test 5: Riaccendi LED
echo -e "\n${GREEN}Test 5: Riaccendi LED${NC}"
bluetoothctl <<EOF
power on
connect $ESP32_MAC
select-attribute $CHAR_BRIGHTNESS_UUID
write '{"brightness":255,"enabled":true}'
disconnect
EOF

echo -e "\n${GREEN}=== Test completati ===${NC}"
echo -e "${YELLOW}Note:${NC}"
echo -e "  - Verifica che i LED cambino colore durante i test"
echo -e "  - Se i test falliscono, controlla che il dispositivo sia acceso e in range"
echo -e "  - I nomi leggibili (LED Color, LED Brightness, etc.) dovrebbero essere visibili in bluetoothctl"
