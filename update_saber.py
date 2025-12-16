#!/usr/bin/env python3
"""
LedSaber OTA Updater
Script per aggiornare il firmware via Bluetooth LE usando GATT OTA service
"""

import asyncio
import sys
import os
import struct
import time
from pathlib import Path
from typing import Optional
from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic

# UUIDs del servizio OTA
OTA_SERVICE_UUID = "4fafc202-1fb5-459e-8fcc-c5c9c331914b"
CHAR_OTA_DATA_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a8"       # WRITE
CHAR_OTA_STATUS_UUID = "d1e5a4c4-eb10-4a3e-8a4c-1234567890ab"     # READ + NOTIFY
CHAR_OTA_CONTROL_UUID = "e2f6b5d5-fc21-5b4f-9b5d-2345678901bc"    # WRITE
CHAR_OTA_PROGRESS_UUID = "f3e7c6e6-0d32-4c5a-ac6e-3456789012cd"   # READ + NOTIFY
CHAR_FW_VERSION_UUID = "a4b8d7fa-1e43-6c7d-ad8f-456789abcdef"     # READ

# Comandi OTA
OTA_CMD_START = 0x01
OTA_CMD_ABORT = 0x02
OTA_CMD_VERIFY = 0x03
OTA_CMD_REBOOT = 0x04

# Stati OTA (corrispondono all'enum C++)
OTA_STATE_IDLE = 0
OTA_STATE_WAITING = 1
OTA_STATE_RECEIVING = 2
OTA_STATE_VERIFYING = 3
OTA_STATE_READY = 4
OTA_STATE_ERROR = 5
OTA_STATE_RECOVERY = 6

# Chunk size (512 bytes come da firmware)
CHUNK_SIZE = 512  # max (il chunk effettivo verrà adattato a MTU-3)

# Colori ANSI
class Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    MAGENTA = "\033[95m"
    CYAN = "\033[96m"


class OTAUpdater:
    """Client OTA per LedSaber"""

    def __init__(self):
        self.client: Optional[BleakClient] = None
        self.ota_state = OTA_STATE_IDLE
        self.ota_error = ""
        self.progress_percent = 0
        self.received_bytes = 0
        self.total_bytes = 0
        self.start_time = 0
        self.negotiated_mtu: int = 0
        self.mtu_acquired: bool = False

    async def _try_acquire_mtu(self) -> None:
        """Best-effort MTU acquisition for BlueZ (Bleak 2.x may report default 23 unless acquired)."""
        if not self.client or not self.client.is_connected:
            return

        backend = getattr(self.client, "_backend", None)

        # Ensure services are discovered (API differs across Bleak versions)
        try:
            if hasattr(self.client, "get_services"):
                await self.client.get_services()
            else:
                get_services = getattr(backend, "get_services", None)
                if get_services is not None:
                    await get_services()
        except Exception as e:
            print(f"{Colors.YELLOW}⚠ Services discovery fallita (per MTU): {e}{Colors.RESET}")

        # Acquire MTU if backend supports it (BlueZ)
        try:
            acquire_mtu = getattr(backend, "_acquire_mtu", None)
            if acquire_mtu is not None:
                await acquire_mtu()
                self.mtu_acquired = True
        except Exception as e:
            self.mtu_acquired = False
            print(f"{Colors.YELLOW}⚠ MTU acquire fallito: {e}{Colors.RESET}")

    async def scan(self, timeout: float = 5.0) -> list:
        """Cerca dispositivi LedSaber"""
        print(f"{Colors.CYAN}🔍 Scansione dispositivi LedSaber...{Colors.RESET}")
        devices = await BleakScanner.discover(timeout=timeout)

        ledsaber_devices = []
        for device in devices:
            if device.name and "LedSaber" in device.name:
                ledsaber_devices.append(device)
                print(f"{Colors.GREEN}✓ Trovato: {device.name} ({device.address}){Colors.RESET}")

        return ledsaber_devices

    async def connect(self, address: str) -> bool:
        """Connette al dispositivo"""
        try:
            print(f"{Colors.YELLOW}📡 Connessione a {address}...{Colors.RESET}")
            self.client = BleakClient(address, timeout=20.0)
            await self.client.connect()

            if self.client.is_connected:
                print(f"{Colors.GREEN}✓ Connesso!{Colors.RESET}")

                # Attendi stabilizzazione connessione
                await asyncio.sleep(1.0)

                # Acquisisci MTU reale su BlueZ (altrimenti Bleak può restare a 23 "di default")
                await self._try_acquire_mtu()
                try:
                    self.negotiated_mtu = int(self.client.mtu_size or 0)
                    print(f"{Colors.CYAN}📊 MTU riportato da Bleak: {self.negotiated_mtu} bytes{Colors.RESET}")
                    if self.negotiated_mtu <= 23 and not self.mtu_acquired:
                        print(f"{Colors.YELLOW}⚠ MTU sembra default (23): Bleak/BlueZ potrebbe non aver fatto MTU exchange{Colors.RESET}")
                except Exception as e:
                    self.negotiated_mtu = 0
                    print(f"{Colors.YELLOW}⚠ Impossibile leggere MTU: {e}{Colors.RESET}")

                await asyncio.sleep(0.5)

                # Leggi versione firmware corrente
                try:
                    fw_version_data = await self.client.read_gatt_char(CHAR_FW_VERSION_UUID)
                    fw_version = fw_version_data.decode('utf-8')
                    print(f"{Colors.CYAN}📦 Versione firmware corrente: {fw_version}{Colors.RESET}")
                except Exception as e:
                    print(f"{Colors.YELLOW}⚠ Impossibile leggere versione firmware: {e}{Colors.RESET}")

                # Abilita notifiche per stato e progresso
                await self.client.start_notify(CHAR_OTA_STATUS_UUID, self._status_handler)
                await self.client.start_notify(CHAR_OTA_PROGRESS_UUID, self._progress_handler)
                await self.refresh_status()
                print(f"{Colors.GREEN}✓ Notifiche OTA abilitate{Colors.RESET}")

                return True

            return False

        except Exception as e:
            print(f"{Colors.RED}✗ Errore connessione: {e}{Colors.RESET}")
            return False

    async def disconnect(self):
        """Disconnette dal dispositivo"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print(f"{Colors.YELLOW}📡 Disconnesso{Colors.RESET}")

    async def refresh_status(self):
        """Legge manualmente lo stato OTA (fallback se mancano notifiche)"""
        if not self.client or not self.client.is_connected:
            return

        try:
            data = await self.client.read_gatt_char(CHAR_OTA_STATUS_UUID)
            self._status_handler(None, bytearray(data))
        except Exception as e:
            print(f"{Colors.YELLOW}⚠ Impossibile leggere stato OTA: {e}{Colors.RESET}")

    def _status_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Handler per notifiche di stato OTA"""
        status_str = data.decode('utf-8')
        parts = status_str.split(':', 1)

        self.ota_state = int(parts[0])
        self.ota_error = parts[1] if len(parts) > 1 else ""

        state_names = ["IDLE", "WAITING", "RECEIVING", "VERIFYING", "READY", "ERROR", "RECOVERY"]
        state_name = state_names[self.ota_state] if self.ota_state < len(state_names) else "UNKNOWN"

        if self.ota_state == OTA_STATE_ERROR:
            print(f"\n{Colors.RED}✗ Errore OTA: {self.ota_error}{Colors.RESET}")
        elif self.ota_state == OTA_STATE_READY:
            elapsed = time.time() - self.start_time
            print(f"\n{Colors.GREEN}✓ Firmware caricato con successo in {elapsed:.1f}s!{Colors.RESET}")
        elif self.ota_state == OTA_STATE_VERIFYING:
            print(f"\n{Colors.CYAN}🔍 Verifica firmware...{Colors.RESET}")

    def _progress_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Handler per notifiche di progresso"""
        progress_str = data.decode('utf-8')
        parts = progress_str.split(':')

        if len(parts) >= 3:
            self.progress_percent = int(parts[0])
            self.received_bytes = int(parts[1])
            self.total_bytes = int(parts[2])

            # Progress bar
            bar_length = 40
            filled = int(bar_length * self.progress_percent / 100)
            bar = '█' * filled + '░' * (bar_length - filled)

            elapsed = time.time() - self.start_time
            speed = (self.received_bytes / 1024) / elapsed if elapsed > 0 else 0

            print(f"\r{Colors.CYAN}[{bar}] {self.progress_percent}% | "
                  f"{self.received_bytes}/{self.total_bytes} bytes | "
                  f"{speed:.2f} KB/s{Colors.RESET}", end='', flush=True)

    async def send_command(self, command: int, data: bytes = b'', wait_response: bool = False):
        """Invia comando OTA"""
        cmd_data = bytes([command]) + data
        await self.client.write_gatt_char(CHAR_OTA_CONTROL_UUID, cmd_data, response=wait_response)

    async def upload_firmware(self, firmware_path: Path) -> bool:
        """Carica firmware via OTA"""
        if not firmware_path.exists():
            print(f"{Colors.RED}✗ File firmware non trovato: {firmware_path}{Colors.RESET}")
            return False

        firmware_size = firmware_path.stat().st_size
        print(f"\n{Colors.BOLD}📤 Upload firmware:{Colors.RESET}")
        print(f"  File: {firmware_path.name}")
        print(f"  Dimensione: {firmware_size} bytes ({firmware_size / 1024:.2f} KB)")

        # Verifica dimensione (max ~1.9MB per partizione app0/app1)
        max_size = 1966080  # 0x1E0000 in decimale
        if firmware_size > max_size:
            print(f"{Colors.RED}✗ Firmware troppo grande! Max: {max_size} bytes{Colors.RESET}")
            return False

        # Leggi firmware
        with open(firmware_path, 'rb') as f:
            firmware_data = f.read()

        # Invia comando START con dimensione firmware
        print(f"{Colors.YELLOW}📡 Invio comando START...{Colors.RESET}")
        size_bytes = struct.pack('<I', firmware_size)  # Little-endian uint32
        await self.send_command(OTA_CMD_START, size_bytes)
        await self.refresh_status()

        # Con OTA_SIZE_UNKNOWN, esp_ota_begin() non causa più disconnessione BLE
        # Attendiamo solo che il dispositivo elabori il comando START
        print(f"{Colors.CYAN}⏳ Attendo che il dispositivo elabori il comando START...{Colors.RESET}")

        # Attendi che il dispositivo entri nello stato WAITING
        max_wait = 10  # secondi
        for i in range(max_wait * 2):  # Check ogni 0.5s
            await asyncio.sleep(0.5)

            # Controlla se ancora connesso
            if not self.client.is_connected:
                print(f"{Colors.YELLOW}⚠ Disconnessione inaspettata durante START. Tento riconnessione...{Colors.RESET}")
                await asyncio.sleep(2.0)

                # Riconnessione
                address = self.client.address
                if not await self.connect(address):
                    print(f"{Colors.RED}✗ Riconnessione fallita. Impossibile continuare.{Colors.RESET}")
                    return False
                break

            # Verifica stato OTA
            await self.refresh_status()
            if self.ota_state == OTA_STATE_WAITING:
                print(f"{Colors.GREEN}✓ Dispositivo pronto a ricevere il firmware (stato: WAITING).{Colors.RESET}")
                break
            elif self.ota_state == OTA_STATE_ERROR:
                print(f"{Colors.RED}✗ Errore durante START: {self.ota_error}{Colors.RESET}")
                return False
        else:
            # Timeout - dispositivo non ha risposto
            print(f"{Colors.RED}✗ Timeout: dispositivo non è entrato nello stato WAITING dopo {max_wait}s{Colors.RESET}")
            return False

        # Invio chunk
        print(f"{Colors.CYAN}📤 Invio firmware...{Colors.RESET}")
        self.start_time = time.time()

        # Usa MTU-3 solo se abbiamo un MTU affidabile; se Bleak riporta 23 "default", NON ridurre a 20 bytes.
        chunk_size_max = CHUNK_SIZE
        if self.negotiated_mtu >= 64:
            chunk_size_max = min(CHUNK_SIZE, self.negotiated_mtu - 3)
        elif self.negotiated_mtu > 0 and self.negotiated_mtu != 23:
            chunk_size_max = min(CHUNK_SIZE, max(20, self.negotiated_mtu - 3))

        offset = 0
        chunk_count = 0
        CHUNKS_PER_BATCH = 100  # Invia 100 chunk (50KB) prima di una pausa minima

        while offset < firmware_size:
            chunk_size = min(chunk_size_max, firmware_size - offset)
            chunk = firmware_data[offset:offset + chunk_size]

            # Invia chunk
            # Usa response=False per "Write Without Response", molto più veloce
            await self.client.write_gatt_char(CHAR_OTA_DATA_UUID, chunk, response=False)

            offset += chunk_size
            chunk_count += 1

            # Controllo di flusso minimo: solo per evitare saturazione completa del buffer
            if chunk_count % CHUNKS_PER_BATCH == 0:
                await asyncio.sleep(0.001)  # 1ms di pausa ogni 100 chunk (50KB)

            # Verifica errori
            if self.ota_state == OTA_STATE_ERROR:
                print(f"\n{Colors.RED}✗ Errore durante upload: {self.ota_error}{Colors.RESET}")
                await self.send_command(OTA_CMD_ABORT)
                return False

        print()  # Newline dopo progress bar

        # Attendi verifica (automatica nel firmware)
        print(f"{Colors.YELLOW}⏳ Attendo verifica firmware...{Colors.RESET}")

        # Attendi fino a 30 secondi per la verifica
        for _ in range(30):
            await asyncio.sleep(1.0)
            if self.ota_state not in (OTA_STATE_READY, OTA_STATE_ERROR):
                await self.refresh_status()
            if self.ota_state == OTA_STATE_READY:
                return True
            elif self.ota_state == OTA_STATE_ERROR:
                print(f"{Colors.RED}✗ Verifica fallita: {self.ota_error}{Colors.RESET}")
                return False

        print(f"{Colors.RED}✗ Timeout verifica firmware{Colors.RESET}")
        return False

    async def reboot_device(self):
        """Riavvia il dispositivo per applicare il nuovo firmware"""
        print(f"{Colors.YELLOW}🔄 Invio comando REBOOT...{Colors.RESET}")
        await self.send_command(OTA_CMD_REBOOT)
        print(f"{Colors.GREEN}✓ Dispositivo in fase di riavvio{Colors.RESET}")
        await asyncio.sleep(2.0)


async def main():
    """Entry point"""
    print(f"""
{Colors.CYAN}{Colors.BOLD}
╔═══════════════════════════════════════╗
║     🔧 LEDSABER OTA UPDATER 🔧        ║
╚═══════════════════════════════════════╝
{Colors.RESET}
    """)

    if len(sys.argv) < 2:
        print(f"{Colors.YELLOW}Uso: {sys.argv[0]} <firmware.bin> [indirizzo_ble]{Colors.RESET}")
        print(f"\nEsempio:")
        print(f"  {sys.argv[0]} .pio/build/esp32cam/firmware.bin")
        print(f"  {sys.argv[0]} firmware.bin AA:BB:CC:DD:EE:FF")
        sys.exit(1)

    firmware_path = Path(sys.argv[1])
    target_address = sys.argv[2] if len(sys.argv) > 2 else None

    updater = OTAUpdater()

    # Scansione o connessione diretta
    if target_address:
        print(f"{Colors.CYAN}📡 Connessione diretta a {target_address}...{Colors.RESET}")
        if not await updater.connect(target_address):
            sys.exit(1)
    else:
        devices = await updater.scan()
        if not devices:
            print(f"{Colors.RED}✗ Nessun dispositivo LedSaber trovato{Colors.RESET}")
            sys.exit(1)

        if len(devices) == 1:
            target = devices[0]
        else:
            print(f"\n{Colors.BOLD}Dispositivi trovati:{Colors.RESET}")
            for idx, dev in enumerate(devices, 1):
                print(f"  {idx}. {dev.name} ({dev.address})")

            selection = input(f"\n{Colors.BOLD}Seleziona dispositivo (1-{len(devices)}): {Colors.RESET}")
            try:
                idx = int(selection) - 1
                target = devices[idx]
            except (ValueError, IndexError):
                print(f"{Colors.RED}✗ Selezione non valida{Colors.RESET}")
                sys.exit(1)

        if not await updater.connect(target.address):
            sys.exit(1)

    # Conferma upload
    print(f"\n{Colors.YELLOW}⚠ ATTENZIONE: Durante l'aggiornamento NON spegnere il dispositivo!{Colors.RESET}")
    confirm = input(f"{Colors.BOLD}Procedere con l'aggiornamento? (s/N): {Colors.RESET}")

    if confirm.lower() != 's':
        print(f"{Colors.YELLOW}↩ Aggiornamento annullato{Colors.RESET}")
        await updater.disconnect()
        sys.exit(0)

    # Upload firmware
    success = await updater.upload_firmware(firmware_path)

    if success:
        # Chiedi se riavviare
        reboot = input(f"\n{Colors.BOLD}Riavviare il dispositivo ora? (S/n): {Colors.RESET}")
        if reboot.lower() != 'n':
            await updater.reboot_device()
        else:
            print(f"{Colors.CYAN}💡 Ricorda di riavviare il dispositivo manualmente per applicare l'aggiornamento{Colors.RESET}")
    else:
        print(f"\n{Colors.RED}✗ Aggiornamento fallito{Colors.RESET}")

    await updater.disconnect()
    print(f"\n{Colors.CYAN}👋 Completato!{Colors.RESET}\n")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}Interruzione da tastiera...{Colors.RESET}")
        sys.exit(1)
