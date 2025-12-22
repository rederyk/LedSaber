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
CHAR_STATUS_LED_UUID = "a4b8d7f9-1e43-6c7d-ad8f-456789abcdef"
CHAR_FOLD_POINT_UUID = "a5b0f9a7-3c65-8e9f-cf0c-6789abcdef01"
CHAR_TIME_SYNC_UUID = "d6e1a0b8-4a76-9f0c-dc1a-789abcdef012"
CHAR_FW_VERSION_UUID = "a4b8d7fa-1e43-6c7d-ad8f-456789abcdef"
CHAR_DEVICE_CONTROL_UUID = "c7f8e0d9-5b87-1a2b-be9d-7890abcdef23"
CHAR_EFFECTS_LIST_UUID = "d8f9e1ea-6c98-2b3c-cf0e-890abcdef234"

# Camera Service UUIDs
CAMERA_SERVICE_UUID = "5fafc301-1fb5-459e-8fcc-c5c9c331914b"
CHAR_CAMERA_STATUS_UUID = "6eb5483e-36e1-4688-b7f5-ea07361b26a8"
CHAR_CAMERA_CONTROL_UUID = "7dc5a4c3-eb10-4a3e-8a4c-1234567890ab"
CHAR_CAMERA_METRICS_UUID = "8ef6b5d4-fc21-5b4f-9b5d-2345678901bc"
CHAR_CAMERA_FLASH_UUID = "9fe7c6e5-0d32-4c5a-ac6e-3456789012cd"

# Motion Service UUIDs
MOTION_SERVICE_UUID = "6fafc401-1fb5-459e-8fcc-c5c9c331914b"
CHAR_MOTION_STATUS_UUID = "7eb5583e-36e1-4688-b7f5-ea07361b26a9"
CHAR_MOTION_CONTROL_UUID = "8dc5b4c3-eb10-4a3e-8a4c-1234567890ac"
CHAR_MOTION_EVENTS_UUID = "9ef6c5d4-fc21-5b4f-9b5d-2345678901bd"
CHAR_MOTION_CONFIG_UUID = "aff7d6e5-0d32-4c5a-ac6e-3456789012ce"

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
        self.camera_callback: Optional[Callable] = None
        self.first_state_received = False  # Flag per primo stato

        # Camera state tracking
        self.camera_state = {}
        self.previous_camera_state = {}
        self.first_camera_state_received = False

        # Motion state tracking
        self.motion_state = {}
        self.previous_motion_state = {}
        self.first_motion_state_received = False
        self.motion_callback: Optional[Callable] = None
        self.motion_event_callback: Optional[Callable] = None

    async def _ensure_services(self) -> bool:
        """Forza la discovery dei servizi GATT (alcune versioni di Bleak non la fanno subito)."""
        if not self.client or not self.client.is_connected:
            return False
        try:
            if hasattr(self.client, "get_services"):
                getter = getattr(self.client, "get_services")
                if asyncio.iscoroutinefunction(getter):
                    await getter()
                else:
                    getter()
            return True
        except Exception as e:
            print(f"{Colors.YELLOW}⚠ Impossibile caricare servizi GATT: {e}{Colors.RESET}")
            return False

    def _has_characteristic(self, char_uuid: str) -> bool:
        """True se la characteristic UUID è presente nella cache servizi di Bleak."""
        if not self.client:
            return False
        services = getattr(self.client, "services", None)
        if services is None:
            return False
        getter = getattr(services, "get_characteristic", None)
        if callable(getter):
            return getter(char_uuid) is not None
        try:
            for service in services:
                for char in service.characteristics:
                    if char.uuid.lower() == char_uuid.lower():
                        return True
        except Exception:
            return False
        return False

    async def _write(self, char_uuid: str, payload: bytes, *, label: str = ""):
        """
        Write robusto.

        Nota: su Bleak/BlueZ, se usi `response=False` e la characteristic espone solo `write` (con response),
        può comparire l'errore fuorviante "Characteristic ... was not found!".
        """
        if not self.client or not self.client.is_connected:
            raise RuntimeError("Not connected")

        await self._ensure_services()

        if not self._has_characteristic(char_uuid):
            extra = f" ({label})" if label else ""
            addr = self.device_address or "?"
            print(f"{Colors.RED}✗ Characteristic non presente nella discovery GATT{extra}: {char_uuid}{Colors.RESET}")
            print(f"{Colors.YELLOW}💡 Se hai appena aggiornato il firmware, BlueZ può usare una cache GATT vecchia.{Colors.RESET}")
            print(f"{Colors.YELLOW}   Prova:{Colors.RESET}")
            print(f"   - `bluetoothctl` → `remove {addr}` → riconnetti")
            print(f"   - oppure `./fix_bluetooth.sh` (richiede sudo)")
            print(f"   - poi `debug services` per verificare che compaia la UUID")
            raise RuntimeError(f"GATT characteristic not discovered: {char_uuid}")

        try:
            await self.client.write_gatt_char(char_uuid, payload, response=True)
        except Exception as e:
            extra = f" ({label})" if label else ""
            print(f"{Colors.YELLOW}⚠ Write fallita{extra} su {char_uuid}: {e}{Colors.RESET}")
            if "was not found" in str(e).lower():
                await self.debug_services()
            raise


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

                # Forza discovery servizi/characteristics (utile per write_gatt_char su alcune stack)
                await self._ensure_services()

                # Abilita notifiche per stato LED
                try:
                    await self.client.start_notify(
                        CHAR_LED_STATE_UUID,
                        self._notification_handler
                    )
                    print(f"{Colors.GREEN}✓ Notifiche LED abilitate{Colors.RESET}")
                except Exception as notify_error:
                    print(f"{Colors.YELLOW}⚠ Notifiche LED non disponibili: {notify_error}{Colors.RESET}")

                # Abilita notifiche per stato Camera
                try:
                    await self.client.start_notify(
                        CHAR_CAMERA_STATUS_UUID,
                        self._camera_notification_handler
                    )
                    print(f"{Colors.GREEN}✓ Notifiche Camera abilitate{Colors.RESET}")
                except Exception as notify_error:
                    print(f"{Colors.YELLOW}⚠ Notifiche Camera non disponibili: {notify_error}{Colors.RESET}")

                # Abilita notifiche per stato Motion
                try:
                    await self.client.start_notify(
                        CHAR_MOTION_STATUS_UUID,
                        self._motion_notification_handler
                    )
                    print(f"{Colors.GREEN}✓ Notifiche Motion abilitate{Colors.RESET}")
                except Exception as notify_error:
                    print(f"{Colors.YELLOW}⚠ Notifiche Motion non disponibili: {notify_error}{Colors.RESET}")

                # Abilita notifiche per eventi Motion
                try:
                    await self.client.start_notify(
                        CHAR_MOTION_EVENTS_UUID,
                        self._motion_events_handler
                    )
                    print(f"{Colors.GREEN}✓ Eventi Motion abilitati{Colors.RESET}")
                except Exception as notify_error:
                    print(f"{Colors.YELLOW}⚠ Eventi Motion non disponibili: {notify_error}{Colors.RESET}")

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
        """Gestisce notifiche di stato LED dal dispositivo"""
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
            print(f"{Colors.RED}✗ Errore parsing notifica LED: {e}{Colors.RESET}")

    def _camera_notification_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Gestisce notifiche di stato Camera dal dispositivo"""
        try:
            status_json = data.decode('utf-8')
            new_camera_state = json.loads(status_json)

            # Primo stato camera ricevuto - mostra sempre
            if not self.first_camera_state_received:
                self.first_camera_state_received = True
                self.camera_state = new_camera_state
                self.previous_camera_state = new_camera_state.copy()
                if self.camera_callback:
                    self.camera_callback(self.camera_state, is_first=True)
                else:
                    print(f"\n{Colors.GREEN}✓ Camera stato iniziale ricevuto{Colors.RESET}")
                return

            # Calcola cambiamenti SIGNIFICATIVI
            significant_changes = {}
            for key in new_camera_state:
                old_value = self.previous_camera_state.get(key)
                new_value = new_camera_state[key]

                # Ignora cambiamenti di FPS minori di 2.0 fps (riduce spam)
                if key == 'fps':
                    if old_value is None or abs(new_value - old_value) >= 2.0:
                        significant_changes[key] = {'old': old_value, 'new': new_value}
                # Ignora cambiamento totalFrames (troppo frequente, non importante)
                elif key == 'totalFrames':
                    pass  # Non notificare mai
                # Altri cambiamenti: notifica sempre
                elif old_value != new_value:
                    significant_changes[key] = {'old': old_value, 'new': new_value}

            # Mostra SOLO se ci sono cambiamenti significativi
            if significant_changes:
                self.camera_state = new_camera_state
                self.previous_camera_state = new_camera_state.copy()

                if self.camera_callback:
                    self.camera_callback(self.camera_state, is_first=False, changes=significant_changes)
                else:
                    print(f"\n{Colors.CYAN}📸 Camera Update:{Colors.RESET}")
                    for key, change in significant_changes.items():
                        if key == 'initialized':
                            status = "✓ Initialized" if change['new'] else "✗ Not initialized"
                            print(f"  {status}")
                        elif key == 'active':
                            status = "ACTIVE" if change['new'] else "STOPPED"
                            print(f"  Status → {status}")
                        elif key == 'fps':
                            print(f"  FPS → {change['new']:.2f}")
                        elif key == 'failedCaptures':
                            print(f"  Failed Captures → {change['new']}")

                    print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)
            else:
                # Aggiorna stato silenziosamente (per totalFrames)
                self.camera_state = new_camera_state
                self.previous_camera_state = new_camera_state.copy()

        except Exception as e:
            print(f"{Colors.RED}✗ Errore parsing notifica camera: {e}{Colors.RESET}")

    def _motion_notification_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Gestisce notifiche di stato Motion dal dispositivo"""
        try:
            status_json = data.decode('utf-8')
            new_motion_state = json.loads(status_json)

            # Primo stato motion ricevuto - mostra sempre
            if not self.first_motion_state_received:
                self.first_motion_state_received = True
                self.motion_state = new_motion_state
                self.previous_motion_state = new_motion_state.copy()
                if self.motion_callback:
                    self.motion_callback(self.motion_state, is_first=True)
                else:
                    print(f"\n{Colors.GREEN}✓ Motion stato iniziale ricevuto{Colors.RESET}")
                return

            # Calcola cambiamenti SIGNIFICATIVI
            significant_changes = {}
            for key in new_motion_state:
                old_value = self.previous_motion_state.get(key)
                new_value = new_motion_state[key]

                # Ignora cambiamenti di intensity minori di 10 (riduce spam)
                if key == 'intensity':
                    if old_value is None or abs(new_value - old_value) >= 10:
                        significant_changes[key] = {'old': old_value, 'new': new_value}
                # Ignora totalFrames/motionFrames (troppo frequente)
                elif key in ['totalFrames', 'motionFrames']:
                    pass
                elif key in ['grid', 'gridRows', 'gridCols', 'blockSize']:
                    pass
                # Altri cambiamenti: notifica sempre
                elif old_value != new_value:
                    significant_changes[key] = {'old': old_value, 'new': new_value}

            # Mostra SOLO se ci sono cambiamenti significativi
            if significant_changes:
                self.motion_state = new_motion_state
                self.previous_motion_state = new_motion_state.copy()

                if self.motion_callback:
                    self.motion_callback(self.motion_state, is_first=False, changes=significant_changes)
                else:
                    print(f"\n{Colors.MAGENTA}🔍 Motion Update:{Colors.RESET}")
                    for key, change in significant_changes.items():
                        if key == 'enabled':
                            status = "ENABLED" if change['new'] else "DISABLED"
                            print(f"  Motion Detection → {status}")
                        elif key == 'motionDetected':
                            status = "DETECTED" if change['new'] else "ENDED"
                            print(f"  Motion → {status}")
                        elif key == 'intensity':
                            print(f"  Intensity → {change['new']}")
                        elif key == 'direction':
                            print(f"  Direction → {Colors.CYAN}{change['new']}{Colors.RESET}")
                        elif key == 'gesture':
                            gesture = change['new']
                            confidence = new_motion_state.get('gestureConfidence', 0)
                            if gesture != 'none' and confidence > 50:
                                print(f"  {Colors.BOLD}{Colors.GREEN}⚔️  GESTURE: {gesture.upper()} ({confidence}%){Colors.RESET}")
                        elif key == 'shakeDetected':
                            if change['new']:
                                print(f"  {Colors.YELLOW}🔔 SHAKE DETECTED!{Colors.RESET}")

                    print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)
            else:
                # Aggiorna stato silenziosamente
                self.motion_state = new_motion_state
                self.previous_motion_state = new_motion_state.copy()

        except Exception as e:
            print(f"{Colors.RED}✗ Errore parsing notifica motion: {e}{Colors.RESET}")

    def _motion_events_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Gestisce eventi motion dal dispositivo"""
        try:
            event_json = data.decode('utf-8')
            event = json.loads(event_json)

            event_type = event.get('event', 'unknown')
            intensity = event.get('intensity', 0)
            pixels = event.get('changedPixels', 0)
            direction = event.get('direction', 'none')
            speed = event.get('speed', 0)
            confidence = event.get('confidence', 0)
            active_blocks = event.get('activeBlocks', 0)
            frame_diff = event.get('frameDiff', 0)
            gesture = event.get('gesture', 'none')
            gesture_confidence = event.get('gestureConfidence', 0)

            if self.motion_event_callback:
                self.motion_event_callback(event)
            else:
                # Visualizza evento con emoji appropriate
                emoji = "🔔"
                if event_type == "shake_detected":
                    emoji = "⚡"
                elif event_type == "motion_started":
                    emoji = "▶️ "
                elif event_type == "motion_ended":
                    emoji = "⏹️ "
                elif event_type == "still_detected":
                    emoji = "😴"

                print(f"\n{Colors.YELLOW}{emoji} Motion Event: {event_type.upper()}{Colors.RESET}")
                print(f"  Intensity: {intensity} | Dir: {direction} | Spd: {speed} | Conf: {confidence}%")
                print(f"  Blocks: {active_blocks} | FrameDiff: {frame_diff} | Changed Pixels: {pixels}")

                # Visualizza gesture se rilevata
                if gesture != 'none' and gesture_confidence > 50:
                    print(f"  {Colors.BOLD}{Colors.GREEN}⚔️  GESTURE: {gesture.upper()} ({gesture_confidence}%){Colors.RESET}")

                print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)

        except Exception as e:
            print(f"{Colors.RED}✗ Errore parsing evento motion: {e}{Colors.RESET}")

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
        print(f"{Colors.CYAN}[DEBUG] Writing to {CHAR_LED_COLOR_UUID}: {color_data}{Colors.RESET}")
        await self._write(CHAR_LED_COLOR_UUID, color_data.encode('utf-8'), label="set_color")
        print(f"{Colors.GREEN}✓ Colore impostato: RGB({r},{g},{b}){Colors.RESET}")

    async def set_effect(self, mode: str, speed: int = 150):
        """Imposta effetto LED (solid, rainbow, breathe, ignition, retraction, flicker, unstable, pulse, dual_pulse, dual_pulse_simple, clash, rainbow_blade)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        effect_data = json.dumps({"mode": mode, "speed": speed})
        await self._write(CHAR_LED_EFFECT_UUID, effect_data.encode('utf-8'), label="set_effect")
        print(f"{Colors.GREEN}✓ Effetto impostato: {mode} (speed: {speed}){Colors.RESET}")

    async def set_effect_raw(self, effect_dict: dict):
        """Imposta effetto LED con parametri custom (per chrono themes, etc.)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        effect_data = json.dumps(effect_dict)
        await self._write(CHAR_LED_EFFECT_UUID, effect_data.encode('utf-8'), label="set_effect_raw")
        mode = effect_dict.get('mode', 'unknown')
        print(f"{Colors.GREEN}✓ Effetto custom impostato: {mode}{Colors.RESET}")

    async def set_brightness(self, brightness: int, enabled: bool = True):
        """Imposta luminosità (0-255) e stato on/off"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        brightness_data = json.dumps({"brightness": brightness, "enabled": enabled})
        await self._write(CHAR_LED_BRIGHTNESS_UUID, brightness_data.encode('utf-8'), label="set_brightness")
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

    async def get_firmware_version(self) -> Optional[str]:
        """Legge versione firmware esposta dal servizio OTA"""
        if not self.client or not self.client.is_connected:
            return None

        try:
            version_data = await self.client.read_gatt_char(CHAR_FW_VERSION_UUID)
            return version_data.decode('utf-8').strip()
        except Exception as e:
            print(f"{Colors.YELLOW}⚠ Impossibile leggere versione firmware: {e}{Colors.RESET}")
            return None

    async def read_firmware_version(self) -> Optional[str]:
        """Alias per get_firmware_version (compatibilità dashboard)"""
        return await self.get_firmware_version()

    async def set_status_led(self, enabled: Optional[bool] = None, brightness: Optional[int] = None):
        """Imposta stato/brightness del LED integrato (pin 4)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        payload = {}
        if enabled is not None:
            payload["enabled"] = enabled
        if brightness is not None:
            brightness = max(0, min(255, int(brightness)))
            payload["brightness"] = brightness

        if not payload:
            print(f"{Colors.YELLOW}⚠ Nessun parametro per statusled{Colors.RESET}")
            return

        status_data = json.dumps(payload)
        print(f"{Colors.CYAN}[DEBUG] Writing to {CHAR_STATUS_LED_UUID}: {status_data}{Colors.RESET}")
        await self._write(CHAR_STATUS_LED_UUID, status_data.encode('utf-8'), label="set_status_led")

        parts = []
        if enabled is not None:
            parts.append("ON" if enabled else "OFF")
        if brightness is not None:
            parts.append(f"lum={brightness}")
        summary = " ".join(parts)
        print(f"{Colors.GREEN}✓ LED di stato (pin 4): {summary}{Colors.RESET}")

    async def get_status_led(self) -> dict:
        """Legge stato LED integrato (pin 4)"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            status_data = await self.client.read_gatt_char(CHAR_STATUS_LED_UUID)
            status_json = status_data.decode('utf-8')
            return json.loads(status_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura LED di stato: {e}{Colors.RESET}")
            return {}

    async def set_fold_point(self, fold_point: int):
        """Imposta punto di piegatura striscia LED (1-143)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        # Validazione
        fold_point = max(1, min(143, int(fold_point)))

        fold_data = json.dumps({"foldPoint": fold_point})
        print(f"{Colors.CYAN}[DEBUG] Writing to {CHAR_FOLD_POINT_UUID}: {fold_data}{Colors.RESET}")
        await self._write(CHAR_FOLD_POINT_UUID, fold_data.encode('utf-8'), label="set_fold_point")
        print(f"{Colors.GREEN}✓ Fold point impostato: {fold_point}{Colors.RESET}")

    async def get_fold_point(self) -> dict:
        """Legge punto di piegatura striscia LED"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            fold_data = await self.client.read_gatt_char(CHAR_FOLD_POINT_UUID)
            fold_json = fold_data.decode('utf-8')
            return json.loads(fold_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura fold point: {e}{Colors.RESET}")
            return {}

    async def sync_time(self):
        """Sincronizza l'orologio ESP32 con il timestamp del PC"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        import time
        from datetime import datetime

        epoch = int(time.time())
        time_data = json.dumps({"epoch": epoch})

        print(f"{Colors.CYAN}[DEBUG] Sending time sync: epoch={epoch}, json={time_data}{Colors.RESET}")

        try:
            await self._write(CHAR_TIME_SYNC_UUID, time_data.encode('utf-8'), label="sync_time")
            dt = datetime.fromtimestamp(epoch)
            print(f"{Colors.GREEN}✓ Tempo sincronizzato: {dt.strftime('%Y-%m-%d %H:%M:%S')}{Colors.RESET}")
        except Exception as e:
            print(f"{Colors.RED}✗ Errore sincronizzazione tempo: {e}{Colors.RESET}")

    async def camera_send_command(self, command: str):
        """Invia comando alla camera (init, capture, start, stop, reset_metrics)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        await self._write(CHAR_CAMERA_CONTROL_UUID, command.encode('utf-8'), label="camera_send_command")
        print(f"{Colors.GREEN}✓ Comando camera inviato: {command}{Colors.RESET}")

    async def get_camera_status(self) -> dict:
        """Legge stato camera"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            status_data = await self.client.read_gatt_char(CHAR_CAMERA_STATUS_UUID)
            status_json = status_data.decode('utf-8')
            return json.loads(status_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura camera status: {e}{Colors.RESET}")
            return {}

    async def get_camera_metrics(self) -> dict:
        """Legge metriche camera"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            metrics_data = await self.client.read_gatt_char(CHAR_CAMERA_METRICS_UUID)
            metrics_json = metrics_data.decode('utf-8')
            return json.loads(metrics_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura camera metrics: {e}{Colors.RESET}")
            return {}

    async def set_camera_flash(self, enabled: bool, brightness: int = 255):
        """Imposta flash camera"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        flash_data = json.dumps({"enabled": enabled, "brightness": brightness})
        await self._write(CHAR_CAMERA_FLASH_UUID, flash_data.encode('utf-8'), label="set_camera_flash")
        status = "ON" if enabled else "OFF"
        print(f"{Colors.GREEN}✓ Flash camera: {status} (brightness: {brightness}){Colors.RESET}")

    # ========================================================================
    # MOTION DETECTION METHODS
    # ========================================================================

    async def motion_send_command(self, command: str):
        """Invia comando al motion detector (enable, disable, reset, quality <val>)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        await self._write(CHAR_MOTION_CONTROL_UUID, command.encode('utf-8'), label="motion_send_command")
        print(f"{Colors.GREEN}✓ Comando motion inviato: {command}{Colors.RESET}")

    async def get_motion_status(self) -> dict:
        """Legge stato motion detector"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            status_data = await self.client.read_gatt_char(CHAR_MOTION_STATUS_UUID)
            status_json = status_data.decode('utf-8')
            return json.loads(status_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura motion status: {e}{Colors.RESET}")
            return {}

    async def get_motion_config(self) -> dict:
        """Legge configurazione motion detector"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            config_data = await self.client.read_gatt_char(CHAR_MOTION_CONFIG_UUID)
            config_json = config_data.decode('utf-8')
            return json.loads(config_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura motion config: {e}{Colors.RESET}")
            return {}

    async def set_motion_config(self, enabled: bool = None, quality: int = None,
                                motion_intensity_min: int = None, motion_speed_min: float = None,
                                gesture_ignition_intensity: int = None,
                                gesture_retract_intensity: int = None,
                                gesture_clash_intensity: int = None):
        """Imposta configurazione motion detector"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        config = {}
        if enabled is not None:
            config["enabled"] = enabled
        if quality is not None:
            config["quality"] = max(0, min(255, int(quality)))
        if motion_intensity_min is not None:
            config["motionIntensityMin"] = max(0, min(255, int(motion_intensity_min)))
        if motion_speed_min is not None:
            config["motionSpeedMin"] = max(0.0, float(motion_speed_min))
        if gesture_ignition_intensity is not None:
            config["gestureIgnitionIntensity"] = max(0, min(255, int(gesture_ignition_intensity)))
        if gesture_retract_intensity is not None:
            config["gestureRetractIntensity"] = max(0, min(255, int(gesture_retract_intensity)))
        if gesture_clash_intensity is not None:
            config["gestureClashIntensity"] = max(0, min(255, int(gesture_clash_intensity)))

        if not config:
            print(f"{Colors.YELLOW}⚠ Nessun parametro per motion config{Colors.RESET}")
            return

        config_data = json.dumps(config)
        await self._write(CHAR_MOTION_CONFIG_UUID, config_data.encode('utf-8'), label="set_motion_config")
        print(f"{Colors.GREEN}✓ Motion config aggiornata: {config}{Colors.RESET}")

    # ========================================================================
    # DEVICE CONTROL METHODS (NEW)
    # ========================================================================

    async def device_ignition(self):
        """Trigger ignition animation (blade power on)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        command_data = json.dumps({"command": "ignition"})
        await self._write(CHAR_DEVICE_CONTROL_UUID, command_data.encode('utf-8'), label="device_ignition")
        print(f"{Colors.GREEN}✓ Ignition triggered{Colors.RESET}")

    async def device_retract(self):
        """Trigger retraction animation (blade power off)"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        command_data = json.dumps({"command": "retract"})
        await self._write(CHAR_DEVICE_CONTROL_UUID, command_data.encode('utf-8'), label="device_retract")
        print(f"{Colors.GREEN}✓ Retraction triggered{Colors.RESET}")

    async def device_reboot(self):
        """Reboot ESP32 device"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        command_data = json.dumps({"command": "reboot"})
        await self._write(CHAR_DEVICE_CONTROL_UUID, command_data.encode('utf-8'), label="device_reboot")
        print(f"{Colors.GREEN}✓ Device reboot commanded{Colors.RESET}")

    async def device_sleep(self):
        """Enter deep sleep mode"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        command_data = json.dumps({"command": "sleep"})
        await self._write(CHAR_DEVICE_CONTROL_UUID, command_data.encode('utf-8'), label="device_sleep")
        print(f"{Colors.GREEN}✓ Device entering deep sleep{Colors.RESET}")

    async def get_effects_list(self) -> dict:
        """Get available effects list from device"""
        if not self.client or not self.client.is_connected:
            return {}

        try:
            effects_data = await self.client.read_gatt_char(CHAR_EFFECTS_LIST_UUID)
            effects_json = effects_data.decode('utf-8')
            return json.loads(effects_json)
        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura effects list: {e}{Colors.RESET}")
            return {}

    # ========================================================================
    # DEBUG & UTILITIES
    # ========================================================================

    async def debug_services(self):
        """Mostra servizi/characteristics noti per il dispositivo connesso"""
        if not self.client or not self.client.is_connected:
            print(f"{Colors.RED}✗ Non connesso{Colors.RESET}")
            return

        try:
            services = None

            if hasattr(self.client, "get_services"):
                getter = getattr(self.client, "get_services")
                if asyncio.iscoroutinefunction(getter):
                    services = await getter()
                else:
                    services = getter()
            else:
                services = self.client.services

            if services is None:
                print(f"{Colors.YELLOW}⚠ Nessun servizio disponibile (Bleak non li ha ancora caricati){Colors.RESET}")
                return

        except Exception as e:
            print(f"{Colors.RED}✗ Errore lettura servizi GATT: {e}{Colors.RESET}")
            return

        print(f"\n{Colors.BOLD}🔎 Servizi GATT rilevati:{Colors.RESET}")
        for service in services:
            print(f"  Service {service.uuid}")
            for char in service.characteristics:
                props = ','.join(sorted(char.properties))
                print(f"    └─ Char {char.uuid} [{props}]")
                for desc in char.descriptors:
                    print(f"       └─ Desc {desc.uuid}")
        print()


class InteractiveCLI:
    """Interfaccia CLI interattiva"""

    def __init__(self):
        self.client = LedSaberClient()
        self.running = False
        self.notifications_enabled = False  # Flag per abilitare/disabilitare notifiche

    async def prompt_user(self, prompt: str) -> str:
        """Chiede input all'utente senza bloccare l'event loop"""
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, lambda: input(prompt))

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
  {Colors.YELLOW}scan auto{Colors.RESET}        - Scansiona e collega automaticamente
  {Colors.YELLOW}connect <addr>{Colors.RESET}    - Connetti a dispositivo
  {Colors.YELLOW}disconnect{Colors.RESET}        - Disconnetti
  {Colors.YELLOW}status{Colors.RESET}            - Mostra stato corrente

  {Colors.GREEN}color <r> <g> <b>{Colors.RESET} - Imposta colore RGB (0-255)
  {Colors.GREEN}effect <mode> [speed]{Colors.RESET} - Imposta effetto LED
  {Colors.GREEN}effects{Colors.RESET}            - Mostra lista effetti disponibili
  {Colors.GREEN}brightness <val>{Colors.RESET}  - Imposta luminosità (0-255)
  {Colors.GREEN}on{Colors.RESET}                - Accendi LED
  {Colors.GREEN}off{Colors.RESET}               - Spegni LED

  {Colors.CYAN}statusled on/off{Colors.RESET}  - Accendi o spegni LED integrato (pin 4)
  {Colors.CYAN}statusled <0-255>{Colors.RESET} - Imposta luminosità LED integrato
  {Colors.CYAN}foldpoint <1-143>{Colors.RESET} - Imposta punto di piegatura LED
  {Colors.CYAN}sync{Colors.RESET}              - Sincronizza orologio ESP32 con PC

  {Colors.MAGENTA}presets{Colors.RESET}          - Mostra preset colori
  {Colors.MAGENTA}preset <name>{Colors.RESET}    - Applica preset

  {Colors.YELLOW}notify{Colors.RESET}            - Toggle notifiche automatiche LED+Camera+Motion

  {Colors.BOLD}🎥 CAMERA COMMANDS:{Colors.RESET}
  {Colors.CYAN}cam init{Colors.RESET}          - Inizializza camera
  {Colors.CYAN}cam capture{Colors.RESET}       - Cattura frame singolo (test)
  {Colors.CYAN}cam start{Colors.RESET}         - Avvia cattura continua
  {Colors.CYAN}cam stop{Colors.RESET}          - Ferma cattura continua
  {Colors.CYAN}cam status{Colors.RESET}        - Mostra stato camera
  {Colors.CYAN}cam metrics{Colors.RESET}       - Mostra metriche dettagliate
  {Colors.CYAN}cam reset{Colors.RESET}         - Reset metriche
  {Colors.CYAN}cam flash on/off{Colors.RESET}  - Accendi/spegni flash
  {Colors.CYAN}cam flash <0-255>{Colors.RESET} - Imposta intensità flash

  {Colors.BOLD}🔍 MOTION DETECTION COMMANDS:{Colors.RESET}
  {Colors.MAGENTA}motion enable{Colors.RESET}        - Abilita motion detection
  {Colors.MAGENTA}motion disable{Colors.RESET}       - Disabilita motion detection
  {Colors.MAGENTA}motion status{Colors.RESET}        - Mostra stato motion detector
  {Colors.MAGENTA}motion config{Colors.RESET}        - Mostra configurazione
  {Colors.MAGENTA}motion quality <0-255>{Colors.RESET} - Imposta qualità
  {Colors.MAGENTA}motion motionmin <0-255>{Colors.RESET}  - Soglia intensità motion attivo
  {Colors.MAGENTA}motion speedmin <0-20>{Colors.RESET}    - Soglia velocità (px/frame)
  {Colors.MAGENTA}motion ignitionmin <0-255>{Colors.RESET} - Soglia intensità ignition gesture
  {Colors.MAGENTA}motion retractmin <0-255>{Colors.RESET}  - Soglia intensità retract gesture
  {Colors.MAGENTA}motion clashmin <0-255>{Colors.RESET}    - Soglia intensità clash gesture
  {Colors.MAGENTA}motion reset{Colors.RESET}         - Reset statistiche motion

  {Colors.BOLD}⚡ DEVICE CONTROL COMMANDS:{Colors.RESET}
  {Colors.YELLOW}ignition{Colors.RESET}         - Trigger ignition animation (power on blade)
  {Colors.YELLOW}retract{Colors.RESET}          - Trigger retraction animation (power off blade)
  {Colors.YELLOW}reboot{Colors.RESET}           - Reboot ESP32 device
  {Colors.YELLOW}sleep{Colors.RESET}            - Enter deep sleep mode
  {Colors.YELLOW}effects{Colors.RESET}          - Show available effects from device

  {Colors.RED}quit{Colors.RESET}              - Esci
  {Colors.BLUE}help{Colors.RESET}              - Mostra questo menu
        """)

    def print_state(self, state: dict, firmware_version: Optional[str] = None):
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
        status_led_enabled = state.get('statusLedEnabled', True)
        status_led_brightness = state.get('statusLedBrightness', 0)
        fold_point = state.get('foldPoint', 72)
        fw_line = (
            f"{Colors.CYAN}{firmware_version}{Colors.RESET}"
            if firmware_version
            else f"{Colors.YELLOW}non disponibile{Colors.RESET}"
        )

        status = f"{Colors.GREEN}ON{Colors.RESET}" if enabled else f"{Colors.RED}OFF{Colors.RESET}"
        status_led_status = f"{Colors.GREEN}ENABLED{Colors.RESET}" if status_led_enabled else f"{Colors.RED}DISABLED{Colors.RESET}"

        print(f"""
{Colors.BOLD}📊 Stato LED:{Colors.RESET}
  Firmware: {fw_line}
  Stato: {status}
  Colore: RGB({r}, {g}, {b})
  Effetto: {effect} (speed: {speed})
  Luminosità: {brightness}/255
  Fold Point: {fold_point}
  LED di stato (pin 4): {status_led_status} (lum: {status_led_brightness}/255)
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

    def print_effects(self):
        """Mostra effetti LED disponibili"""
        effects = {
            "solid": "Colore solido statico",
            "rainbow": "Arcobaleno animato",
            "breathe": "Respirazione (fade in/out)",
            "ignition": "Accensione progressiva (base → punta)",
            "retraction": "Spegnimento progressivo (punta → base)",
            "flicker": "Instabilità plasma (tipo Kylo Ren)",
            "unstable": "Kylo Ren avanzato (flicker + sparks)",
            "pulse": "Onda di energia che percorre la lama",
            "dual_pulse": "Due onde in direzioni opposte",
            "dual_pulse_simple": "Dual Pulse senza collasso (colore cambia mentre tieni la sfera)",
            "clash": "Flash bianco su impatto (auto ogni 3s)",
            "rainbow_blade": "Arcobaleno lineare sulla lama",
        }

        print(f"\n{Colors.BOLD}✨ Effetti disponibili:{Colors.RESET}")
        for name, description in effects.items():
            print(f"  {name:15} - {description}")
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
                auto_connect = bool(args and args[0].lower() == "auto")
                devices = await self.client.scan()
                if not devices:
                    print(f"{Colors.YELLOW}⚠ Nessun dispositivo LedSaber trovato{Colors.RESET}")
                    return

                if auto_connect:
                    target_device = None
                    if len(devices) == 1:
                        target_device = devices[0]
                        print(f"{Colors.CYAN}🔁 Connessione automatica a {target_device.name} ({target_device.address}){Colors.RESET}")
                    else:
                        print(f"\n{Colors.BOLD}Seleziona dispositivo da connettere:{Colors.RESET}")
                        for idx, device in enumerate(devices, start=1):
                            print(f"  {idx}. {device.name} ({device.address})")
                        while True:
                            selection = (await self.prompt_user(f"{Colors.BOLD}Numero dispositivo (Enter per annullare):{Colors.RESET} ")).strip()
                            if not selection:
                                print(f"{Colors.YELLOW}↩ Connessione automatica annullata{Colors.RESET}")
                                break
                            if selection.isdigit():
                                index = int(selection)
                                if 1 <= index <= len(devices):
                                    target_device = devices[index - 1]
                                    break
                            print(f"{Colors.RED}✗ Selezione non valida. Inserisci un numero tra 1 e {len(devices)}{Colors.RESET}")

                    if target_device:
                        await self.client.connect(target_device.address)

            elif cmd == "connect":
                if not args:
                    print(f"{Colors.RED}✗ Uso: connect <indirizzo>{Colors.RESET}")
                    return
                await self.client.connect(args[0])

            elif cmd == "disconnect":
                await self.client.disconnect()

            elif cmd == "status":
                state = await self.client.get_state()
                firmware_version = await self.client.get_firmware_version()
                self.print_state(state, firmware_version)

            elif cmd == "color":
                if len(args) < 3:
                    print(f"{Colors.RED}✗ Uso: color <r> <g> <b>{Colors.RESET}")
                    return
                r, g, b = int(args[0]), int(args[1]), int(args[2])
                await self.client.set_color(r, g, b)

            elif cmd == "effect":
                if not args:
                    print(f"{Colors.RED}✗ Uso: effect <mode> [speed]{Colors.RESET}")
                    print(f"{Colors.YELLOW}💡 Usa 'effects' per vedere tutti gli effetti disponibili{Colors.RESET}")
                    return
                mode = args[0]
                speed = int(args[1]) if len(args) > 1 else 150
                await self.client.set_effect(mode, speed)

            elif cmd == "effects":
                self.print_effects()

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

            elif cmd == "statusled":
                if not args:
                    print(f"{Colors.RED}✗ Uso: statusled <on|off|0-255|brightness <val>>{Colors.RESET}")
                    return

                sub = args[0].lower()

                def parse_brightness(value: str) -> Optional[int]:
                    try:
                        val = int(value)
                    except ValueError:
                        return None
                    return max(0, min(255, val))

                if sub in ['on', 'off']:
                    enabled = sub == 'on'
                    await self.client.set_status_led(enabled=enabled)
                elif sub == "brightness":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: statusled brightness <0-255>{Colors.RESET}")
                        return
                    brightness = parse_brightness(args[1])
                    if brightness is None:
                        print(f"{Colors.RED}✗ Luminosità non valida{Colors.RESET}")
                        return
                    await self.client.set_status_led(brightness=brightness)
                else:
                    brightness = parse_brightness(sub)
                    if brightness is None:
                        print(f"{Colors.RED}✗ Uso: statusled <on|off|0-255|brightness <val>>{Colors.RESET}")
                        return
                    await self.client.set_status_led(brightness=brightness)

            elif cmd == "foldpoint":
                if not args:
                    print(f"{Colors.RED}✗ Uso: foldpoint <1-143>{Colors.RESET}")
                    return
                try:
                    fold_point = int(args[0])
                    await self.client.set_fold_point(fold_point)
                except ValueError:
                    print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

            elif cmd == "debug":
                if args and args[0] == "services":
                    await self.client.debug_services()
                else:
                    print(f"{Colors.YELLOW}Uso: debug services{Colors.RESET}")

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
                print(f"📢 Notifiche automatiche LED/Camera/Motion: {status}")

            elif cmd == "cam":
                # Gestione comandi camera
                if not args:
                    print(f"{Colors.RED}✗ Uso: cam <comando>{Colors.RESET}")
                    print(f"{Colors.YELLOW}💡 Comandi: init, capture, start, stop, status, metrics, reset, flash{Colors.RESET}")
                    return

                subcmd = args[0].lower()

                if subcmd == "init":
                    await self.client.camera_send_command("init")
                    await asyncio.sleep(1)
                    status = await self.client.get_camera_status()
                    if status:
                        print(f"\n{Colors.BOLD}📸 Camera Status:{Colors.RESET}")
                        print(f"  Initialized: {status.get('initialized', False)}")
                        print(f"  Active: {status.get('active', False)}")

                elif subcmd == "capture":
                    await self.client.camera_send_command("capture")

                elif subcmd == "start":
                    await self.client.camera_send_command("start")

                elif subcmd == "stop":
                    await self.client.camera_send_command("stop")

                elif subcmd == "status":
                    status = await self.client.get_camera_status()
                    if status:
                        print(f"\n{Colors.BOLD}📸 Camera Status:{Colors.RESET}")
                        print(f"  Initialized: {status.get('initialized', False)}")
                        print(f"  Active: {status.get('active', False)}")
                        print(f"  FPS: {status.get('fps', 0):.2f}")
                        print(f"  Total Frames: {status.get('totalFrames', 0)}")
                        print(f"  Failed Captures: {status.get('failedCaptures', 0)}")
                    else:
                        print(f"{Colors.YELLOW}⚠ Nessun dato disponibile{Colors.RESET}")

                elif subcmd == "metrics":
                    metrics = await self.client.get_camera_metrics()
                    if metrics:
                        print(f"\n{Colors.BOLD}📊 Camera Metrics:{Colors.RESET}")
                        print(f"  Total Frames: {metrics.get('totalFramesCaptured', 0)}")
                        print(f"  Failed Captures: {metrics.get('failedCaptures', 0)}")
                        print(f"  Last Frame Size: {metrics.get('lastFrameSize', 0)} bytes")
                        print(f"  Last Capture Time: {metrics.get('lastCaptureTime', 0)} ms")
                        print(f"  Current FPS: {metrics.get('currentFps', 0):.2f}")
                        print(f"\n{Colors.BOLD}💾 Memory:{Colors.RESET}")
                        print(f"  Heap Free: {metrics.get('heapFree', 0)} bytes")
                        print(f"  PSRAM Total: {metrics.get('psramTotal', 0)} bytes")
                        print(f"  PSRAM Free: {metrics.get('psramFree', 0)} bytes")
                    else:
                        print(f"{Colors.YELLOW}⚠ Nessun dato disponibile{Colors.RESET}")

                elif subcmd == "reset":
                    await self.client.camera_send_command("reset_metrics")

                elif subcmd == "flash":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: cam flash <on|off|0-255>{Colors.RESET}")
                        return

                    flash_arg = args[1].lower()
                    if flash_arg == "on":
                        await self.client.set_camera_flash(True, 255)
                    elif flash_arg == "off":
                        await self.client.set_camera_flash(False, 0)
                    else:
                        try:
                            brightness = int(flash_arg)
                            brightness = max(0, min(255, brightness))
                            await self.client.set_camera_flash(brightness > 0, brightness)
                        except ValueError:
                            print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

                else:
                    print(f"{Colors.RED}✗ Comando camera sconosciuto: {subcmd}{Colors.RESET}")
                    print(f"{Colors.YELLOW}💡 Comandi: init, capture, start, stop, status, metrics, reset, flash{Colors.RESET}")

            elif cmd in ("sync", "time", "timesync"):
                # Sincronizza orologio ESP32 con PC
                await self.client.sync_time()

            elif cmd == "motion":
                # Gestione comandi motion detection
                if not args:
                    print(f"{Colors.RED}✗ Uso: motion <comando>{Colors.RESET}")
                    print(f"{Colors.YELLOW}💡 Comandi: enable, disable, status, config, quality, motionmin, speedmin, ignitionmin, retractmin, clashmin, reset{Colors.RESET}")
                    return

                subcmd = args[0].lower()

                if subcmd == "enable":
                    await self.client.motion_send_command("enable")
                    await asyncio.sleep(0.5)
                    status = await self.client.get_motion_status()
                    if status:
                        print(f"\n{Colors.BOLD}🔍 Motion Status:{Colors.RESET}")
                        print(f"  Enabled: {status.get('enabled', False)}")

                elif subcmd == "disable":
                    await self.client.motion_send_command("disable")
                    await asyncio.sleep(0.5)
                    status = await self.client.get_motion_status()
                    if status:
                        print(f"\n{Colors.BOLD}🔍 Motion Status:{Colors.RESET}")
                        print(f"  Enabled: {status.get('enabled', False)}")

                elif subcmd == "status":
                    status = await self.client.get_motion_status()
                    if status:
                        print(f"\n{Colors.BOLD}🔍 Motion Status:{Colors.RESET}")
                        print(f"  Enabled: {status.get('enabled', False)}")
                        print(f"  Motion Detected: {status.get('motionDetected', False)}")
                        print(f"  Intensity: {status.get('intensity', 0)}")
                        print(f"  Direction: {status.get('direction', 'none')}")
                        print(f"  Changed Pixels: {status.get('changedPixels', 0)}")
                        print(f"  Shake Detected: {status.get('shakeDetected', False)}")
                        print(f"  Total Frames: {status.get('totalFrames', 0)}")
                        print(f"  Motion Frames: {status.get('motionFrames', 0)}")
                        print(f"  Shake Count: {status.get('shakeCount', 0)}")

                        # Mostra intensità zone (3x3 grid)
                        zones = status.get('zones', [])
                        if zones and len(zones) == 9:
                            print(f"\n  {Colors.CYAN}Zone Intensities (3x3 grid):{Colors.RESET}")
                            print(f"    {zones[0]:3d} {zones[1]:3d} {zones[2]:3d}")
                            print(f"    {zones[3]:3d} {zones[4]:3d} {zones[5]:3d}")
                            print(f"    {zones[6]:3d} {zones[7]:3d} {zones[8]:3d}")

                        # Mostra griglia optical flow 40px (6x6)
                        grid_rows = status.get('grid', [])
                        if grid_rows:
                            grid_cols = status.get('gridCols', len(grid_rows[0]) if grid_rows else 0)
                            block_size = status.get('blockSize', 40)
                            print(f"\n  {Colors.CYAN}Optical Flow Grid ({grid_cols}x{len(grid_rows)} blocchi @ {block_size}px):{Colors.RESET}")
                            for row_str in grid_rows:
                                printable = " ".join(list(row_str))
                                print(f"    {printable}")
                            print(f"    Legend: . idle, ^/v su/giù, </> sx/dx, A up-right, B up-left, C down-right, D down-left")
                    else:
                        print(f"{Colors.YELLOW}⚠ Nessun dato disponibile{Colors.RESET}")

                elif subcmd == "config":
                    config = await self.client.get_motion_config()
                    if config:
                        print(f"\n{Colors.BOLD}⚙️  Motion Config:{Colors.RESET}")
                        print(f"  Enabled: {config.get('enabled', False)}")
                        print(f"  Quality: {config.get('quality', 128)}")
                        print(f"  MotionIntensityMin: {config.get('motionIntensityMin', 15)}")
                        print(f"  MotionSpeedMin: {config.get('motionSpeedMin', 1.2)}")
                        if "gestureIgnitionIntensity" in config:
                            print(f"  GestureIgnitionIntensity: {config.get('gestureIgnitionIntensity')}")
                        if "gestureRetractIntensity" in config:
                            print(f"  GestureRetractIntensity: {config.get('gestureRetractIntensity')}")
                        if "gestureClashIntensity" in config:
                            print(f"  GestureClashIntensity: {config.get('gestureClashIntensity')}")
                    else:
                        print(f"{Colors.YELLOW}⚠ Nessun dato disponibile{Colors.RESET}")

                elif subcmd == "quality":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: motion quality <0-255>{Colors.RESET}")
                        return

                    try:
                        quality = int(args[1])
                        if 0 <= quality <= 255:
                            await self.client.motion_send_command(f"quality {quality}")
                            await asyncio.sleep(0.5)
                            print(f"{Colors.GREEN}✓ Quality impostata: {quality}{Colors.RESET}")
                        else:
                            print(f"{Colors.RED}✗ Valore deve essere tra 0 e 255{Colors.RESET}")
                    except ValueError:
                        print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

                elif subcmd == "motionmin":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: motion motionmin <0-255>{Colors.RESET}")
                        return

                    try:
                        min_intensity = int(args[1])
                        if 0 <= min_intensity <= 255:
                            await self.client.motion_send_command(f"motionmin {min_intensity}")
                            await asyncio.sleep(0.5)
                            print(f"{Colors.GREEN}✓ Motion intensity min impostata: {min_intensity}{Colors.RESET}")
                        else:
                            print(f"{Colors.RED}✗ Valore deve essere tra 0 e 255{Colors.RESET}")
                    except ValueError:
                        print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

                elif subcmd == "speedmin":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: motion speedmin <0-20>{Colors.RESET}")
                        return

                    try:
                        min_speed = float(args[1])
                        if 0.0 <= min_speed <= 20.0:
                            await self.client.motion_send_command(f"speedmin {min_speed}")
                            await asyncio.sleep(0.5)
                            print(f"{Colors.GREEN}✓ Motion speed min impostata: {min_speed:.2f}{Colors.RESET}")
                        else:
                            print(f"{Colors.RED}✗ Valore deve essere tra 0 e 20{Colors.RESET}")
                    except ValueError:
                        print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

                elif subcmd == "ignitionmin":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: motion ignitionmin <0-255>{Colors.RESET}")
                        return
                    try:
                        value = int(args[1])
                        if 0 <= value <= 255:
                            await self.client.set_motion_config(gesture_ignition_intensity=value)
                            await asyncio.sleep(0.5)
                            print(f"{Colors.GREEN}✓ Ignition intensity min impostata: {value}{Colors.RESET}")
                        else:
                            print(f"{Colors.RED}✗ Valore deve essere tra 0 e 255{Colors.RESET}")
                    except ValueError:
                        print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

                elif subcmd == "retractmin":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: motion retractmin <0-255>{Colors.RESET}")
                        return
                    try:
                        value = int(args[1])
                        if 0 <= value <= 255:
                            await self.client.set_motion_config(gesture_retract_intensity=value)
                            await asyncio.sleep(0.5)
                            print(f"{Colors.GREEN}✓ Retract intensity min impostata: {value}{Colors.RESET}")
                        else:
                            print(f"{Colors.RED}✗ Valore deve essere tra 0 e 255{Colors.RESET}")
                    except ValueError:
                        print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

                elif subcmd == "clashmin":
                    if len(args) < 2:
                        print(f"{Colors.RED}✗ Uso: motion clashmin <0-255>{Colors.RESET}")
                        return
                    try:
                        value = int(args[1])
                        if 0 <= value <= 255:
                            await self.client.set_motion_config(gesture_clash_intensity=value)
                            await asyncio.sleep(0.5)
                            print(f"{Colors.GREEN}✓ Clash intensity min impostata: {value}{Colors.RESET}")
                        else:
                            print(f"{Colors.RED}✗ Valore deve essere tra 0 e 255{Colors.RESET}")
                    except ValueError:
                        print(f"{Colors.RED}✗ Valore non valido{Colors.RESET}")

                elif subcmd == "reset":
                    await self.client.motion_send_command("reset")
                    await asyncio.sleep(0.5)
                    print(f"{Colors.GREEN}✓ Motion statistiche resettate{Colors.RESET}")

                else:
                    print(f"{Colors.RED}✗ Comando motion sconosciuto: {subcmd}{Colors.RESET}")
                    print(f"{Colors.YELLOW}💡 Comandi: enable, disable, status, config, quality, motionmin, speedmin, ignitionmin, retractmin, clashmin, reset{Colors.RESET}")

            # ================================================================
            # DEVICE CONTROL COMMANDS
            # ================================================================
            elif cmd == "ignition":
                await self.client.device_ignition()

            elif cmd == "retract":
                await self.client.device_retract()

            elif cmd == "reboot":
                confirm = input(f"{Colors.YELLOW}⚠ Sei sicuro di voler riavviare il dispositivo? (y/n): {Colors.RESET}")
                if confirm.lower() == 'y':
                    await self.client.device_reboot()
                    print(f"{Colors.YELLOW}Disconnessione in corso...{Colors.RESET}")
                    await asyncio.sleep(1)
                    self.running = False
                else:
                    print(f"{Colors.CYAN}Operazione annullata{Colors.RESET}")

            elif cmd == "sleep":
                confirm = input(f"{Colors.YELLOW}⚠ Sei sicuro di voler mettere il dispositivo in deep sleep? (y/n): {Colors.RESET}")
                if confirm.lower() == 'y':
                    await self.client.device_sleep()
                    print(f"{Colors.YELLOW}Disconnessione in corso...{Colors.RESET}")
                    await asyncio.sleep(1)
                    self.running = False
                else:
                    print(f"{Colors.CYAN}Operazione annullata{Colors.RESET}")

            elif cmd == "effects":
                effects_list = await self.client.get_effects_list()
                if effects_list:
                    print(f"\n{Colors.BOLD}✨ Available Effects (version {effects_list.get('version', 'unknown')}):{Colors.RESET}")
                    effects = effects_list.get('effects', [])
                    for effect in effects:
                        icon = effect.get('icon', '🎨')
                        name = effect.get('name', 'Unknown')
                        effect_id = effect.get('id', 'unknown')
                        params = ', '.join(effect.get('params', []))

                        print(f"  {icon} {Colors.CYAN}{effect_id}{Colors.RESET} - {name}")
                        if params:
                            print(f"      Params: {Colors.DIM}{params}{Colors.RESET}")

                        # Se ha themes, mostrale
                        if 'themes' in effect:
                            themes = effect['themes']
                            if 'hour' in themes:
                                hour_themes = ', '.join(themes['hour'])
                                print(f"      Hour themes: {Colors.YELLOW}{hour_themes}{Colors.RESET}")
                            if 'second' in themes:
                                second_themes = ', '.join(themes['second'])
                                print(f"      Second themes: {Colors.MAGENTA}{second_themes}{Colors.RESET}")
                else:
                    print(f"{Colors.RED}✗ Impossibile recuperare la lista effetti{Colors.RESET}")

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
                        elif key == 'statusLedEnabled':
                            status = "ENABLED" if change['new'] else "DISABLED"
                            print(f"  LED di stato (pin 4) → {status}")
                        elif key == 'foldPoint':
                            print(f"  Fold Point → {change['new']}")
                print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)

        self.client.state_callback = on_state_update

        def on_camera_update(state, is_first=False, changes=None):
            if not self.notifications_enabled:
                return

            state = state or {}
            if is_first:
                print(f"\n{Colors.GREEN}✓ Camera stato iniziale ricevuto{Colors.RESET}")
                print(f"  Initialized: {state.get('initialized', False)}")
                print(f"  Active: {state.get('active', False)}")
                print(f"  FPS: {state.get('fps', 0):.2f}")
                print(f"  Failed Captures: {state.get('failedCaptures', 0)}")
            else:
                print(f"\n{Colors.CYAN}📸 Camera Update:{Colors.RESET}")
                if changes:
                    for key, change in changes.items():
                        if key == 'initialized':
                            status = "✓ Initialized" if change['new'] else "✗ Not initialized"
                            print(f"  {status}")
                        elif key == 'active':
                            status = "ACTIVE" if change['new'] else "STOPPED"
                            print(f"  Status → {status}")
                        elif key == 'fps':
                            print(f"  FPS → {change['new']:.2f}")
                        elif key == 'failedCaptures':
                            print(f"  Failed Captures → {change['new']}")
                else:
                    print("  (Nessun cambiamento significativo)")
            print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)

        self.client.camera_callback = on_camera_update

        def on_motion_update(state, is_first=False, changes=None):
            if not self.notifications_enabled:
                return

            state = state or {}
            if is_first:
                # Stato iniziale: mostra tutto compatto
                print(f"\n{Colors.GREEN}✓ Motion stato iniziale ricevuto{Colors.RESET}")
                print(f"  Enabled: {state.get('enabled', False)} | Motion: {state.get('motionDetected', False)} | Shake: {state.get('shakeDetected', False)}")
                print(f"  Intensity: {state.get('intensity', 0)} | Direction: {state.get('direction', 'none')} | Speed: {state.get('speed', 0)}")
                print(f"  Confidence: {state.get('confidence', 0)} | ActiveBlocks: {state.get('activeBlocks', 0)} | FrameDiff: {state.get('frameDiff', 0)}")
                print(f"  ComputeMs: {state.get('computeTimeMs', 0)} | Frames: {state.get('totalFrames', 0)}/{state.get('motionFrames', 0)}")
                gesture = state.get('gesture', 'none')
                if gesture != 'none':
                    print(f"  Gesture: {gesture} | GConf: {state.get('gestureConfidence', 0)} | GTs: {state.get('gestureTimestamp', 0)}")

                # Mostra griglia optical flow se presente
                grid_rows = state.get('grid', [])
                if grid_rows:
                    grid_cols = state.get('gridCols', len(grid_rows[0]) if grid_rows else 0)
                    block_size = state.get('blockSize', 40)
                    print(f"  {Colors.CYAN}Optical Flow Grid ({grid_cols}x{len(grid_rows)} @ {block_size}px):{Colors.RESET}")
                    for row_str in grid_rows:
                        print(f"    {' '.join(list(row_str))}")
                    print(f"    . idle | ^v su/giù | <> sx/dx | A↗ B↖ C↘ D↙")
            else:
                # Cambiamenti: notifica solo se ci sono differenze
                if not changes:
                    return  # Nessun cambiamento, non notificare

                print(f"\n{Colors.MAGENTA}🔍 Motion Update:{Colors.RESET}")

                # Riga compatta con info chiave
                status_parts = []
                status = "ENABLED" if state.get('enabled', False) else "DISABLED"
                status_parts.append(f"Detection={status}")
                status_parts.append(f"Motion={'YES' if state.get('motionDetected', False) else 'NO'}")
                status_parts.append(f"Int={state.get('intensity', 0)}")
                status_parts.append(f"Dir={state.get('direction', 'none')}")
                status_parts.append(f"Speed={state.get('speed', 0)}")
                status_parts.append(f"Conf={state.get('confidence', 0)}")
                status_parts.append(f"Blocks={state.get('activeBlocks', 0)}")
                status_parts.append(f"Diff={state.get('frameDiff', 0)}")
                if state.get('shakeDetected', False):
                    status_parts.append(f"{Colors.YELLOW}SHAKE!{Colors.RESET}")

                if status_parts:
                    print(f"  {' | '.join(status_parts)}")

                # Gesture detection (solo se rilevata con confidence alta)
                gesture = state.get('gesture', 'none')
                confidence = state.get('gestureConfidence', 0)
                if gesture != 'none':
                    print(f"  {Colors.BOLD}{Colors.GREEN}⚔️  GESTURE: {gesture.upper()} ({confidence}%){Colors.RESET}")
                    print(f"  GestureTs: {state.get('gestureTimestamp', 0)}")

                # Mostra griglia optical flow compatta
                grid_rows = state.get('grid', [])
                if grid_rows:
                    grid_cols = state.get('gridCols', len(grid_rows[0]) if grid_rows else 0)
                    block_size = state.get('blockSize', 40)
                    print(f"  {Colors.CYAN}Flow ({grid_cols}x{len(grid_rows)} @ {block_size}px):{Colors.RESET} {' | '.join(grid_rows)}")

                # Statistiche compatte (solo se cambiate)
                stats_parts = []
                if state.get('totalFrames', 0) > 0:
                    stats_parts.append(f"Frames: {state.get('totalFrames', 0)}")
                if state.get('motionFrames', 0) > 0:
                    stats_parts.append(f"Motion: {state.get('motionFrames', 0)}")
                if state.get('shakeCount', 0) > 0:
                    stats_parts.append(f"Shakes: {state.get('shakeCount', 0)}")
                if state.get('changedPixels', 0) > 0:
                    stats_parts.append(f"Pixels: {state.get('changedPixels', 0)}")

                if stats_parts:
                    print(f"  {' | '.join(stats_parts)}")

            print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)

        self.client.motion_callback = on_motion_update

        def on_motion_event(event):
            if not self.notifications_enabled:
                return

            event = event or {}
            event_type = event.get('event', 'unknown')
            intensity = event.get('intensity', 0)
            pixels = event.get('changedPixels', 0)
            direction = event.get('direction', 'none')
            gesture = event.get('gesture', 'none')
            gesture_confidence = event.get('gestureConfidence', 0)
            speed = event.get('speed', 0)
            confidence = event.get('confidence', 0)
            active_blocks = event.get('activeBlocks', 0)
            frame_diff = event.get('frameDiff', 0)

            emoji = "🔔"
            if event_type == "shake_detected":
                emoji = "⚡"
            elif event_type == "motion_started":
                emoji = "▶️ "
            elif event_type == "motion_ended":
                emoji = "⏹️ "
            elif event_type == "still_detected":
                emoji = "😴"

            print(f"\n{Colors.YELLOW}{emoji} Motion Event: {event_type.upper()}{Colors.RESET}")
            print(f"  Intensity: {intensity}")
            print(f"  Changed Pixels: {pixels}")
            if direction != 'none':
                print(f"  Direction: {Colors.CYAN}{direction}{Colors.RESET}")
            print(f"  Speed: {speed} | Conf: {confidence} | Blocks: {active_blocks} | Diff: {frame_diff}")

            if gesture != 'none' and gesture_confidence > 50:
                print(f"  {Colors.BOLD}{Colors.GREEN}⚔️  GESTURE: {gesture.upper()} ({gesture_confidence}%){Colors.RESET}")
            print(f"{Colors.BOLD}>{Colors.RESET} ", end="", flush=True)

        self.client.motion_event_callback = on_motion_event

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
