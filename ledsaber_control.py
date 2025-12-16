#!/usr/bin/env python3
"""
LedSaber BLE Controller
Script interattivo per controllare la spada laser LED via Bluetooth LE
"""

import asyncio
import json
import sys
from typing import Optional, Callable
from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic

# UUIDs dal firmware ESP32
LED_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_LED_STATE_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHAR_LED_COLOR_UUID = "d1e5a4c3-eb10-4a3e-8a4c-1234567890ab"
CHAR_LED_EFFECT_UUID = "e2f6b5d4-fc21-5b4f-9b5d-2345678901bc"
CHAR_LED_BRIGHTNESS_UUID = "f3e7c6e5-0d32-4c5a-ac6e-3456789012cd"

# Colori ANSI per output colorato
class Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    MAGENTA = "\033[95m"
    CYAN = "\033[96m"


class LedSaberClient:
    """Client BLE per LedSaber"""

    def __init__(self):
        self.client: Optional[BleakClient] = None
        self.device_address: Optional[str] = None
        self.current_state = {}
        self.previous_state = {}  # Stato precedente per rilevare cambiamenti
        self.state_callback: Optional[Callable] = None
        self.first_state_received = False  # Flag per primo stato

    async def scan(self, timeout: float = 5.0) -> list:
        """Cerca dispositivi BLE nelle vicinanze"""
        print(f"{Colors.CYAN}🔍 Scansione dispositivi BLE...{Colors.RESET}")
        devices = await BleakScanner.discover(timeout=timeout)

        ledsaber_devices = []
        for device in devices:
            if device.name and "LedSaber" in device.name:
                ledsaber_devices.append(device)
                print(f"{Colors.GREEN}✓ Trovato: {device.name} ({device.address}){Colors.RESET}")

        return ledsaber_devices

    async def connect(self, address: str) -> bool:
        """Connette al dispositivo BLE"""
        try:
            print(f"{Colors.YELLOW}📡 Connessione a {address}...{Colors.RESET}")

            # Crea client con timeout più lungo per connessioni lente
            self.client = BleakClient(
                address,
                timeout=20.0  # Timeout 20 secondi
            )

            await self.client.connect()

            if self.client.is_connected:
                self.device_address = address
                print(f"{Colors.GREEN}✓ Connesso!{Colors.RESET}")

                # Attendi un momento per stabilizzare la connessione
                await asyncio.sleep(0.5)

                # Abilita notifiche per stato LED
                try:
                    await self.client.start_notify(
                        CHAR_LED_STATE_UUID,
                        self._notification_handler
                    )
                    print(f"{Colors.GREEN}✓ Notifiche abilitate{Colors.RESET}")
                except Exception as notify_error:
                    print(f"{Colors.YELLOW}⚠ Notifiche non disponibili: {notify_error}{Colors.RESET}")

                return True

            return False

        except Exception as e:
            print(f"{Colors.RED}✗ Errore connessione: {e}{Colors.RESET}")
            if "BREDR" in str(e) or "ProfileUnavailable" in str(e):
                print(f"{Colors.YELLOW}💡 Suggerimento: questo errore può essere causato da:{Colors.RESET}")
                print(f"   1. Dispositivo già accoppiato come Bluetooth Classic")
                print(f"   2. Cache Bluetooth corrotta")
                print(f"   3. Permessi insufficienti")
                print(f"\n{Colors.CYAN}Prova questi comandi:{Colors.RESET}")
                print(f"   sudo systemctl restart bluetooth")
                print(f"   bluetoothctl")
                print(f"   > remove {address}")
                print(f"   > exit")
            return False

    async def disconnect(self):
        """Disconnette dal dispositivo"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print(f"{Colors.YELLOW}📡 Disconnesso{Colors.RESET}")

    def _notification_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Gestisce notifiche di stato dal dispositivo"""
        try:
            state_json = data.decode('utf-8')
            new_state = json.loads(state_json)

            # Primo stato ricevuto - sempre notifica
            if not self.first_state_received:
                self.first_state_received = True
                self.current_state = new_state
                self.previous_state = new_state.copy()
                if self.state_callback:
                    self.state_callback(self.current_state, is_first=True)
                return

            # Confronta con stato precedente - notifica solo se c'è un cambiamento
            if new_state != self.previous_state:
                # Calcola cosa è cambiato
                changes = self._calculate_changes(self.previous_state, new_state)
                self.current_state = new_state
                self.previous_state = new_state.copy()

                if self.state_callback:
                    self.state_callback(self.current_state, is_first=False, changes=changes)

        except Exception as e:
            print(f"{Colors.RED}✗ Errore parsing notifica: {e}{Colors.RESET}")

    def _calculate_changes(self, old_state: dict, new_state: dict) -> dict:
        """Calcola le differenze tra due stati"""
        changes = {}
        for key in new_state:
            if key not in old_state or old_state[key] != new_state[key]:
                changes[key] = {
                    'old': old_state.get(key),
                    'new': new_state[key]
                }
        return changes

    async def set_color(self, r: int, g: int, b: int):
        """Imposta colore RGB (0-255)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        color_data = json.dumps({"r": r, "g": g, "b": b})
        await self.client.write_gatt_char(
            CHAR_LED_COLOR_UUID,
            color_data.encode('utf-8')
        )
        print(f"{Colors.GREEN}✓ Colore impostato: RGB({r},{g},{b}){Colors.RESET}")

    async def set_effect(self, mode: str, speed: int = 50):
        """Imposta effetto LED (solid, rainbow, breathe)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        effect_data = json.dumps({"mode": mode, "speed": speed})
        await self.client.write_gatt_char(
            CHAR_LED_EFFECT_UUID,
            effect_data.encode('utf-8')
        )
        print(f"{Colors.GREEN}✓ Effetto impostato: {mode} (speed: {speed}){Colors.RESET}")

    async def set_brightness(self, brightness: int, enabled: bool = True):
        """Imposta luminosità (0-255) e stato on/off"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        brightness_data = json.dumps({"brightness": brightness, "enabled": enabled})
        await self.client.write_gatt_char(
            CHAR_LED_BRIGHTNESS_UUID,
            brightness_data.encode('utf-8')
        )
        status = "ON" if enabled else "OFF"
        print(f"{Colors.GREEN}✓ Luminosità: {brightness} ({status}){Colors.RESET}")

    async def get_state(self) -> dict:
        """Legge stato corrente LED"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            state_data = await self.client.read_gatt_char(CHAR_LED_STATE_UUID)
            state_json = state_data.decode('utf-8')
            return json.loads(state_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura stato: {e}{Colors.RESET}")
            return {}


class InteractiveCLI:
    """Interfaccia CLI interattiva"""

    def __init__(self):
        self.client = LedSaberClient()
        self.running = False
        self.notifications_enabled = True  # Flag per abilitare/disabilitare notifiche

    def print_banner(self):
        """Stampa banner iniziale"""
        print(f"""
{Colors.CYAN}{Colors.BOLD}
╔═══════════════════════════════════════╗
║     🗡️  LEDSABER BLE CONTROLLER 🗡️     ║
╚═══════════════════════════════════════╝
{Colors.RESET}
        """)

    def print_menu(self):
        """Stampa menu comandi"""
        print(f"""
{Colors.BOLD}Comandi disponibili:{Colors.RESET}
  {Colors.YELLOW}scan{Colors.RESET}              - Cerca dispositivi LedSaber
  {Colors.YELLOW}connect <addr>{Colors.RESET}    - Connetti a dispositivo
  {Colors.YELLOW}disconnect{Colors.RESET}        - Disconnetti
  {Colors.YELLOW}status{Colors.RESET}            - Mostra stato corrente

  {Colors.GREEN}color <r> <g> <b>{Colors.RESET} - Imposta colore RGB (0-255)
  {Colors.GREEN}effect <mode> [speed]{Colors.RESET} - Imposta effetto (solid/rainbow/breathe)
  {Colors.GREEN}brightness <val>{Colors.RESET}  - Imposta luminosità (0-255)
  {Colors.GREEN}on{Colors.RESET}                - Accendi LED
  {Colors.GREEN}off{Colors.RESET}               - Spegni LED

  {Colors.MAGENTA}presets{Colors.RESET}          - Mostra preset colori
  {Colors.MAGENTA}preset <name>{Colors.RESET}    - Applica preset

  {Colors.YELLOW}notify{Colors.RESET}            - Toggle notifiche automatiche on/off

  {Colors.RED}quit{Colors.RESET}              - Esci
  {Colors.BLUE}help{Colors.RESET}              - Mostra questo menu
        """)

    def print_state(self, state: dict):
        """Stampa stato LED"""
        if not state:
            return

        r = state.get('r', 0)
        g = state.get('g', 0)
        b = state.get('b', 0)
        brightness = state.get('brightness', 0)
        effect = state.get('effect', 'unknown')
        speed = state.get('speed', 0)
        enabled = state.get('enabled', False)

        status = f"{Colors.GREEN}ON{Colors.RESET}" if enabled else f"{Colors.RED}OFF{Colors.RESET}"

        print(f"""
{Colors.BOLD}📊 Stato LED:{Colors.RESET}
  Stato: {status}
  Colore: RGB({r}, {g}, {b})
  Effetto: {effect} (speed: {speed})
  Luminosità: {brightness}/255
        """)

    def print_presets(self):
        """Mostra preset colori disponibili"""
        presets = {
            "red": (255, 0, 0),
            "green": (0, 255, 0),
            "blue": (0, 0, 255),
            "white": (255, 255, 255),
            "yellow": (255, 255, 0),
            "cyan": (0, 255, 255),
            "magenta": (255, 0, 255),
            "orange": (255, 165, 0),
            "purple": (128, 0, 128),
            "pink": (255, 192, 203),
        }

        print(f"\n{Colors.BOLD}🎨 Preset disponibili:{Colors.RESET}")
        for name, (r, g, b) in presets.items():
            print(f"  {name:12} - RGB({r:3}, {g:3}, {b:3})")
        print()

    async def handle_command(self, command: str):
        """Gestisce comandi utente"""
        parts = command.strip().split()
        if not parts:
            return

        cmd = parts[0].lower()
        args = parts[1:]

        try:
            if cmd == "scan":
                devices = await self.client.scan()
                if not devices:
                    print(f"{Colors.YELLOW}⚠ Nessun dispositivo LedSaber trovato{Colors.RESET}")

            elif cmd == "connect":
                if not args:
                    print(f"{Colors.RED}✗ Uso: connect <indirizzo>{Colors.RESET}")
                    return
                await self.client.connect(args[0])

            elif cmd == "disconnect":
                await self.client.disconnect()

            elif cmd == "status":
                state = await self.client.get_state()
                self.print_state(state)

            elif cmd == "color":
                if len(args) < 3:
                    print(f"{Colors.RED}✗ Uso: color <r> <g> <b>{Colors.RESET}")
                    return
                r, g, b = int(args[0]), int(args[1]), int(args[2])
                await self.client.set_color(r, g, b)

            elif cmd == "effect":
                if not args:
                    print(f"{Colors.RED}✗ Uso: effect <solid|rainbow|breathe> [speed]{Colors.RESET}")
                    return
                mode = args[0]
                speed = int(args[1]) if len(args) > 1 else 50
                await self.client.set_effect(mode, speed)

            elif cmd == "brightness":
                if not args:
                    print(f"{Colors.RED}✗ Uso: brightness <0-255>{Colors.RESET}")
                    return
                brightness = int(args[0])
                await self.client.set_brightness(brightness, True)

            elif cmd == "on":
                await self.client.set_brightness(255, True)

            elif cmd == "off":
                await self.client.set_brightness(0, False)

            elif cmd == "presets":
                self.print_presets()

            elif cmd == "preset":
                if not args:
                    print(f"{Colors.RED}✗ Uso: preset <nome>{Colors.RESET}")
                    return
                presets = {
                    "red": (255, 0, 0),
                    "green": (0, 255, 0),
                    "blue": (0, 0, 255),
                    "white": (255, 255, 255),
                    "yellow": (255, 255, 0),
                    "cyan": (0, 255, 255),
                    "magenta": (255, 0, 255),
                    "orange": (255, 165, 0),
                    "purple": (128, 0, 128),
                    "pink": (255, 192, 203),
                }
                preset_name = args[0].lower()
                if preset_name in presets:
                    r, g, b = presets[preset_name]
                    await self.client.set_color(r, g, b)
                else:
                    print(f"{Colors.RED}✗ Preset '{preset_name}' non trovato{Colors.RESET}")

            elif cmd == "notify":
                self.notifications_enabled = not self.notifications_enabled
                status = f"{Colors.GREEN}ABILITATE{Colors.RESET}" if self.notifications_enabled else f"{Colors.RED}DISABILITATE{Colors.RESET}"
                print(f"📢 Notifiche automatiche: {status}")

            elif cmd == "help":
                self.print_menu()

            elif cmd in ("quit", "exit", "q"):
                self.running = False
                await self.client.disconnect()

            else:
                print(f"{Colors.RED}✗ Comando sconosciuto: {cmd}{Colors.RESET}")
                print(f"  Usa '{Colors.BLUE}help{Colors.RESET}' per lista comandi")

        except ValueError as e:
            print(f"{Colors.RED}✗ Errore parametri: {e}{Colors.RESET}")
        except Exception as e:
            print(f"{Colors.RED}✗ Errore: {e}{Colors.RESET}")

    async def run(self):
        """Loop principale interattivo"""
        self.running = True
        self.print_banner()
        self.print_menu()

        # Callback per notifiche stato (solo cambiamenti effettivi)
        def on_state_update(state, is_first=False, changes=None):
            if not self.notifications_enabled:
                return

            if is_first:
                # Primo stato: mostra tutto
                print(f"\n{Colors.GREEN}✓ Stato iniziale ricevuto:{Colors.RESET}")
                self.print_state(state)
                print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)
            else:
                # Cambiamento: mostra solo cosa è cambiato
                print(f"\n{Colors.CYAN}📝 Stato aggiornato:{Colors.RESET}")
                if changes:
                    for key, change in changes.items():
                        if key in ['r', 'g', 'b']:
                            # Raggruppa RGB
                            if 'r' in changes or 'g' in changes or 'b' in changes:
                                if key == 'r':  # Mostra solo una volta
                                    r = state.get('r', 0)
                                    g = state.get('g', 0)
                                    b = state.get('b', 0)
                                    print(f"  Colore → RGB({r}, {g}, {b})")
                        elif key == 'brightness':
                            print(f"  Luminosità → {change['new']}/255")
                        elif key == 'effect':
                            print(f"  Effetto → {change['new']}")
                        elif key == 'speed':
                            print(f"  Velocità → {change['new']}")
                        elif key == 'enabled':
                            status = "ON" if change['new'] else "OFF"
                            print(f"  Stato → {status}")
                print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)

        self.client.state_callback = on_state_update

        print(f"{Colors.YELLOW}💡 Suggerimento: inizia con 'scan' per cercare dispositivi{Colors.RESET}\n")

        while self.running:
            try:
                command = await asyncio.get_event_loop().run_in_executor(
                    None,
                    lambda: input(f"{Colors.BOLD}>{Colors.RESET} ")
                )
                await self.handle_command(command)

            except KeyboardInterrupt:
                print(f"\n{Colors.YELLOW}Interruzione da tastiera...{Colors.RESET}")
                self.running = False
            except EOFError:
                self.running = False

        await self.client.disconnect()
        print(f"\n{Colors.CYAN}👋 Arrivederci!{Colors.RESET}\n")


async def main():
    """Entry point"""
    cli = InteractiveCLI()
    await cli.run()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nUscita...")
        sys.exit(0)
