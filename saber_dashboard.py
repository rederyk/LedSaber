#!/usr/bin/env python3
"""
LedSaber Dashboard - BTop-style Live Monitor
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Dashboard interattiva live per LedSaber BLE
Stile: Resistenza Cyberpunk / BTTop
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
"""

import asyncio
import json
from datetime import datetime
from typing import Optional, Dict, List
from collections import deque

from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, Vertical, VerticalScroll
from textual.widgets import Static, Input, Footer
from textual.reactive import reactive
from textual.binding import Binding
from rich.panel import Panel
from rich.table import Table
from rich.text import Text
from rich.align import Align
from rich.console import Group
from rich import box

# Import del client BLE esistente
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).parent))
from ledsaber_control import LedSaberClient


# ═══════════════════════════════════════════════════════════════════════════
# CUSTOM WIDGETS - DASHBOARD STYLE
# ═══════════════════════════════════════════════════════════════════════════

class HeaderWidget(Static):
    """Header con titolo e status BLE"""

    connected: reactive[bool] = reactive(False)
    device_name: reactive[str] = reactive("NO DEVICE")
    device_address: reactive[str] = reactive("")

    def render(self):
        if self.connected:
            device_info = f"[green]◉[/] {self.device_name} [dim]({self.device_address})[/]"
        else:
            device_info = f"[red]●[/] DISCONNECTED"

        title = Text()
        title.append("╔═══════════════════════════════════════════════════════════════════════╗\n", style="cyan bold")
        title.append("║   ", style="cyan bold")
        title.append("⚔️  LEDSABER LIVE DASHBOARD", style="yellow bold")
        title.append("  ─  ", style="cyan bold")
        title.append(f"Resistance Edition", style="magenta")
        title.append("   ║\n", style="cyan bold")
        title.append("╠═══════════════════════════════════════════════════════════════════════╣\n", style="cyan bold")
        title.append("║  BLE: ", style="cyan bold")

        # Calcola spazi per allineare a destra
        device_str = device_info
        # Rimuovi markup per calcolare lunghezza effettiva
        clean_device = self.device_name if self.connected else "DISCONNECTED"
        padding = 59 - len(clean_device)
        title.append(device_info)
        title.append(" " * max(0, padding))
        title.append("║\n", style="cyan bold")
        title.append("╚═══════════════════════════════════════════════════════════════════════╝", style="cyan bold")

        return Align.center(title)


class LEDPanelWidget(Static):
    """Panel LED compatto - stile gauge"""

    led_state: reactive[Dict] = reactive({})

    def render(self):
        state = self.led_state or {}

        enabled = state.get('enabled', False)
        r = state.get('r', 0)
        g = state.get('g', 0)
        b = state.get('b', 0)
        brightness = state.get('brightness', 0)
        effect = state.get('effect', 'unknown')
        speed = state.get('speed', 0)

        # Status icon
        status_icon = "[green]◉ ON[/]" if enabled else "[red]● OFF[/]"

        # RGB bar
        rgb_bar = f"[red]R:{r:3d}[/] [green]G:{g:3d}[/] [blue]B:{b:3d}[/]"

        # Brightness bar (compatta)
        bright_pct = int((brightness / 255) * 20)
        bright_bar = f"[yellow]{'█' * bright_pct}{'░' * (20 - bright_pct)}[/]"

        content = Text()
        content.append(f"{status_icon}  │  {rgb_bar}  │  ", style="")
        content.append(f"Bright:{brightness:3d} ", style="cyan")
        content.append(f"{bright_bar}", style="")
        content.append(f"  │  FX:[magenta]{effect}[/]", style="")

        return Panel(
            Align.center(content),
            title="[bold cyan]⚡ LED STATUS[/]",
            border_style="cyan",
            box=box.ROUNDED,
            height=3
        )


class CameraPanelWidget(Static):
    """Panel Camera compatto con FPS sparkline"""

    camera_state: reactive[Dict] = reactive({})
    fps_history: deque = deque(maxlen=40)

    def watch_camera_state(self, new_state: Dict):
        if new_state:
            fps = new_state.get('fps', 0)
            self.fps_history.append(fps)

    def _sparkline(self, data: List[float], max_val: float = 30.0) -> str:
        if not data:
            return "░" * 40

        blocks = "▁▂▃▄▅▆▇█"
        result = []
        for val in data:
            idx = int((val / max_val) * (len(blocks) - 1))
            idx = max(0, min(len(blocks) - 1, idx))
            result.append(blocks[idx])

        # Colora in base al valore
        colored = "".join([
            f"[green]{c}[/]" if data[i] > 20 else
            f"[yellow]{c}[/]" if data[i] > 10 else
            f"[red]{c}[/]"
            for i, c in enumerate(result)
        ])

        return colored

    def render(self):
        state = self.camera_state or {}

        initialized = state.get('initialized', False)
        active = state.get('active', False)
        fps = state.get('fps', 0.0)
        total_frames = state.get('totalFrames', 0)

        status_icon = "[green]◉ ACTIVE[/]" if active else "[yellow]◎ IDLE[/]"
        init_icon = "[green]✓[/]" if initialized else "[red]✗[/]"

        sparkline = self._sparkline(list(self.fps_history))

        content = Text()
        content.append(f"{status_icon}  │  Init:{init_icon}  │  ", style="")
        content.append(f"FPS:[bold yellow]{fps:5.1f}[/]  ", style="")
        content.append(f"│  {sparkline}  ", style="")
        content.append(f"│  Frames:[dim]{total_frames}[/]", style="")

        return Panel(
            Align.center(content),
            title="[bold green]📸 CAMERA[/]",
            border_style="green",
            box=box.ROUNDED,
            height=3
        )


class MotionLiveWidget(Static):
    """Widget MASSIVO per motion detection live - optical flow grid centrale"""

    motion_state: reactive[Dict] = reactive({})
    intensity_history: deque = deque(maxlen=60)

    def watch_motion_state(self, new_state: Dict):
        if new_state:
            intensity = new_state.get('intensity', 0)
            self.intensity_history.append(intensity)

    def _sparkline(self, data: List[int], max_val: int = 255) -> str:
        if not data:
            return "░" * 60

        blocks = "▁▂▃▄▅▆▇█"
        result = []
        for val in data:
            idx = int((val / max_val) * (len(blocks) - 1))
            idx = max(0, min(len(blocks) - 1, idx))
            result.append(blocks[idx])

        colored = "".join([
            f"[red]{c}[/]" if data[i] > 200 else
            f"[yellow]{c}[/]" if data[i] > 100 else
            f"[green]{c}[/]"
            for i, c in enumerate(result)
        ])

        return colored

    def render(self):
        state = self.motion_state or {}

        enabled = state.get('enabled', False)
        motion_detected = state.get('motionDetected', False)
        intensity = state.get('intensity', 0)
        direction = state.get('direction', 'none')
        changed_pixels = state.get('changedPixels', 0)
        shake_detected = state.get('shakeDetected', False)
        gesture = state.get('gesture', 'none')
        gesture_confidence = state.get('gestureConfidence', 0)

        # Grid optical flow
        grid_rows = state.get('grid', [])
        grid_cols = state.get('gridCols', 8)
        block_size = state.get('blockSize', 40)

        # Status icons
        status_icon = "[green]◉ ENABLED[/]" if enabled else "[red]● DISABLED[/]"
        motion_icon = "[yellow]⚡ MOTION[/]" if motion_detected else "[dim]○ STILL[/]"
        shake_icon = "[red blink]⚠ SHAKE![/]" if shake_detected else ""

        # Direction arrows
        direction_arrows = {
            'up': '↑', 'down': '↓', 'left': '←', 'right': '→',
            'up-right': '↗', 'up-left': '↖', 'down-right': '↘', 'down-left': '↙',
            'none': '·'
        }
        dir_arrow = direction_arrows.get(direction, '·')

        # Intensity bar
        intensity_pct = int((intensity / 255) * 30)
        intensity_bar = f"[yellow]{'█' * intensity_pct}{'░' * (30 - intensity_pct)}[/]"

        # Intensity sparkline
        intensity_graph = self._sparkline(list(self.intensity_history))

        # === BUILD CONTENT ===
        content = Text()

        # Row 1: Status + Motion + Shake
        content.append("┌─ STATUS ──────────────────────────────────────────────────────────┐\n", style="magenta bold")
        content.append(f"│ {status_icon}  │  {motion_icon}", style="")
        if shake_icon:
            content.append(f"  │  {shake_icon}", style="")
        content.append(" " * (67 - len(f"  │  {motion_icon}") - (len("  │  ⚠ SHAKE!") if shake_icon else 0)))
        content.append(" │\n", style="magenta bold")

        # Row 2: Intensity bar
        content.append("├─ INTENSITY ───────────────────────────────────────────────────────┤\n", style="magenta bold")
        content.append(f"│ ", style="magenta bold")
        content.append(f"[cyan]I:{intensity:3d}[/]  {intensity_bar}  ", style="")
        content.append(f"[dim]Pixels:{changed_pixels:,}[/]", style="")
        # Padding
        pixel_str = f"Pixels:{changed_pixels:,}"
        padding = 61 - len(pixel_str) - 4 - 30 - 4
        content.append(" " * max(0, padding))
        content.append("│\n", style="magenta bold")

        # Row 3: Intensity history sparkline
        content.append("├─ HISTORY ─────────────────────────────────────────────────────────┤\n", style="magenta bold")
        content.append(f"│ {intensity_graph}", style="magenta bold")
        content.append(" │\n", style="magenta bold")

        # Row 4: Direction + Gesture
        content.append("├─ DIRECTION & GESTURE ─────────────────────────────────────────────┤\n", style="magenta bold")
        dir_display = f"[cyan]{dir_arrow} {direction.upper()}[/]"

        gesture_display = ""
        if gesture != 'none' and gesture_confidence > 50:
            gesture_display = f"[bold yellow]⚔ {gesture.upper()}[/] [dim]({gesture_confidence}%)[/]"
        else:
            gesture_display = "[dim]no gesture[/]"

        content.append(f"│ Dir: {dir_display}", style="magenta bold")
        content.append("  │  ", style="")
        content.append(f"Gesture: {gesture_display}", style="")
        # Padding dinamico
        clean_dir = direction.upper()
        clean_gest = gesture.upper() if gesture != 'none' and gesture_confidence > 50 else "no gesture"
        used_len = len(f"Dir:  {clean_dir}  │  Gesture: {clean_gest}")
        if gesture != 'none' and gesture_confidence > 50:
            used_len += len(f"({gesture_confidence}%)")
        padding = 67 - used_len
        content.append(" " * max(0, padding))
        content.append(" │\n", style="magenta bold")

        # === OPTICAL FLOW GRID (MASSIVE) ===
        content.append("╞═══════════════════════════════════════════════════════════════════╡\n", style="magenta bold")
        content.append(f"│         [bold cyan]OPTICAL FLOW GRID ({grid_cols}x{len(grid_rows)} @ {block_size}px)[/]", style="magenta bold")
        grid_title = f"OPTICAL FLOW GRID ({grid_cols}x{len(grid_rows)} @ {block_size}px)"
        padding = 67 - len(grid_title) - 9
        content.append(" " * max(0, padding))
        content.append(" │\n", style="magenta bold")
        content.append("├───────────────────────────────────────────────────────────────────┤\n", style="magenta bold")

        # Mappa simboli -> colori
        symbol_colors = {
            '.': 'dim white',
            '^': 'cyan',
            'v': 'cyan',
            '<': 'yellow',
            '>': 'yellow',
            'A': 'green',
            'B': 'green',
            'C': 'magenta',
            'D': 'magenta',
        }

        if not grid_rows:
            content.append("│                       [dim]No motion data yet[/]", style="magenta bold")
            content.append(" " * 22)
            content.append("│\n", style="magenta bold")
        else:
            # Calcola padding per centrare la griglia
            grid_width = grid_cols * 2 + 1  # Ogni char + spazio
            grid_padding_left = (67 - grid_width) // 2

            for row_str in grid_rows:
                content.append("│", style="magenta bold")
                content.append(" " * grid_padding_left, style="")

                for char in row_str:
                    color = symbol_colors.get(char, 'white')
                    content.append(f"[{color}]{char}[/] ", style="")

                # Padding a destra
                grid_padding_right = 67 - grid_padding_left - grid_width
                content.append(" " * grid_padding_right, style="")
                content.append(" │\n", style="magenta bold")

        # Legend
        content.append("├───────────────────────────────────────────────────────────────────┤\n", style="magenta bold")
        content.append("│ [dim]Legend: ", style="magenta bold")
        content.append(". idle  ", style="dim white")
        content.append("^ v", style="cyan")
        content.append(" up/down  ", style="dim white")
        content.append("< >", style="yellow")
        content.append(" left/right  ", style="dim white")
        content.append("A B C D", style="green")
        content.append(" diagonals[/]", style="dim white")
        # Padding finale
        legend_len = len("Legend: . idle  ^ v up/down  < > left/right  A B C D diagonals")
        padding = 67 - legend_len - 2
        content.append(" " * max(0, padding))
        content.append(" │\n", style="magenta bold")
        content.append("└───────────────────────────────────────────────────────────────────┘", style="magenta bold")

        return Panel(
            content,
            title="[bold magenta]🔍 MOTION DETECTION LIVE[/]",
            border_style="magenta",
            box=box.DOUBLE_EDGE,
            padding=(0, 1)
        )


class ConsoleWidget(Static):
    """Console per log + input comandi (minimale in basso)"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.log_lines: deque = deque(maxlen=100)  # Salva fino a 100 righe
        self.visible_lines = 5  # Mostra solo ultime 5

    def add_log(self, message: str, style: str = "white"):
        """Aggiungi messaggio al log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        line = f"[dim]{timestamp}[/] [{style}]{message}[/]"
        self.log_lines.append(line)
        self.refresh()

    def render(self):
        # Prendi ultime N righe
        recent_lines = list(self.log_lines)[-self.visible_lines:]

        if not recent_lines:
            log_content = "[dim]No logs yet. Type 'help' for commands.[/]"
        else:
            log_content = "\n".join(recent_lines)

        content = Text()
        content.append("CONSOLE LOG (last 5 lines)\n", style="cyan bold")
        content.append("─" * 71 + "\n", style="dim")
        content.append(log_content + "\n")
        content.append("─" * 71 + "\n", style="dim")
        content.append("[cyan]>[/] ", style="bold")

        return content


class CommandInputWidget(Static):
    """Input minimale per comandi"""

    def compose(self) -> ComposeResult:
        yield Input(placeholder="Type command (help for list, Ctrl+S=scan, Ctrl+D=disconnect)...", id="cmd_input")


# ═══════════════════════════════════════════════════════════════════════════
# MAIN APP
# ═══════════════════════════════════════════════════════════════════════════

class SaberDashboard(App):
    """Dashboard principale btop-style"""

    CSS = """
    Screen {
        background: $surface;
        layout: vertical;
    }

    #header {
        height: auto;
        background: $panel;
    }

    #main_panels {
        height: auto;
    }

    #led_panel {
        height: 3;
    }

    #camera_panel {
        height: 3;
    }

    #motion_panel {
        height: auto;
        min-height: 20;
    }

    #console_container {
        height: auto;
        dock: bottom;
        background: $panel;
    }

    #console_log {
        height: auto;
        padding: 1;
        background: $surface;
    }

    #cmd_input {
        border: solid cyan;
        background: $panel;
        margin: 0 1;
    }
    """

    BINDINGS = [
        Binding("ctrl+c", "quit", "Quit", priority=True),
        Binding("ctrl+s", "scan_auto", "Scan & Connect"),
        Binding("ctrl+d", "disconnect", "Disconnect"),
        Binding("f1", "toggle_help", "Help"),
        Binding("f2", "camera_init", "Cam Init"),
        Binding("f3", "camera_start", "Cam Start"),
        Binding("f4", "camera_stop", "Cam Stop"),
        Binding("f5", "motion_toggle", "Motion Toggle"),
    ]

    TITLE = "LedSaber Live Dashboard"

    def __init__(self):
        super().__init__()
        self.client = LedSaberClient()
        self.header_widget: Optional[HeaderWidget] = None
        self.led_widget: Optional[LEDPanelWidget] = None
        self.camera_widget: Optional[CameraPanelWidget] = None
        self.motion_widget: Optional[MotionLiveWidget] = None
        self.console_widget: Optional[ConsoleWidget] = None

    def compose(self) -> ComposeResult:
        """Componi layout"""
        # Header
        yield HeaderWidget(id="header")

        # Main panels container
        with Container(id="main_panels"):
            yield LEDPanelWidget(id="led_panel")
            yield CameraPanelWidget(id="camera_panel")
            yield MotionLiveWidget(id="motion_panel")

        # Console container (bottom)
        with Container(id="console_container"):
            yield ConsoleWidget(id="console_log")
            yield CommandInputWidget()

        # Footer con shortcuts
        yield Footer()

    def on_mount(self) -> None:
        """Inizializza dopo mount"""
        self.header_widget = self.query_one("#header", HeaderWidget)
        self.led_widget = self.query_one("#led_panel", LEDPanelWidget)
        self.camera_widget = self.query_one("#camera_panel", CameraPanelWidget)
        self.motion_widget = self.query_one("#motion_panel", MotionLiveWidget)
        self.console_widget = self.query_one("#console_log", ConsoleWidget)

        # Setup callbacks
        self.client.state_callback = self._on_led_update
        self.client.camera_callback = self._on_camera_update
        self.client.motion_callback = self._on_motion_update
        self.client.motion_event_callback = self._on_motion_event

        self.console_widget.add_log("Dashboard initialized. Press F1 for help.", "green")
        self.console_widget.add_log("Quick start: Ctrl+S to scan and connect", "cyan")

    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Gestisce comando"""
        command = event.value.strip()
        if command:
            self.run_worker(self._execute_command(command))
            event.input.value = ""

    async def _execute_command(self, command: str):
        """Esegue comando async"""
        parts = command.split()
        if not parts:
            return

        cmd = parts[0].lower()
        args = parts[1:]

        try:
            # === BLE COMMANDS ===
            if cmd in ["quit", "exit", "q"]:
                self.console_widget.add_log("Shutting down...", "yellow")
                await self.client.disconnect()
                self.exit()

            elif cmd == "scan":
                auto = bool(args and args[0].lower() == "auto")
                self.console_widget.add_log("Scanning for devices...", "cyan")
                devices = await self.client.scan()

                if not devices:
                    self.console_widget.add_log("No LedSaber devices found", "yellow")
                    return

                if auto and len(devices) == 1:
                    device = devices[0]
                    await self._connect_device(device.address, device.name)
                else:
                    for dev in devices:
                        self.console_widget.add_log(f"Found: {dev.name} ({dev.address})", "green")

            elif cmd == "connect":
                if not args:
                    self.console_widget.add_log("Usage: connect <address>", "red")
                    return
                await self._connect_device(args[0], "LedSaber")

            elif cmd == "disconnect":
                await self.client.disconnect()
                self.header_widget.connected = False
                self.header_widget.device_name = "NO DEVICE"
                self.console_widget.add_log("Disconnected", "yellow")

            # === LED COMMANDS ===
            elif cmd == "color":
                if len(args) < 3:
                    self.console_widget.add_log("Usage: color <r> <g> <b>", "red")
                    return
                r, g, b = int(args[0]), int(args[1]), int(args[2])
                await self.client.set_color(r, g, b)
                self.console_widget.add_log(f"Color set: RGB({r},{g},{b})", "magenta")

            elif cmd == "effect":
                if not args:
                    self.console_widget.add_log("Usage: effect <mode> [speed]", "red")
                    return
                mode = args[0]
                speed = int(args[1]) if len(args) > 1 else 150
                await self.client.set_effect(mode, speed)
                self.console_widget.add_log(f"Effect: {mode} @ {speed}ms", "magenta")

            elif cmd == "brightness":
                if not args:
                    self.console_widget.add_log("Usage: brightness <0-255>", "red")
                    return
                brightness = int(args[0])
                await self.client.set_brightness(brightness, True)
                self.console_widget.add_log(f"Brightness: {brightness}", "magenta")

            elif cmd == "on":
                await self.client.set_brightness(255, True)
                self.console_widget.add_log("LED ON", "green")

            elif cmd == "off":
                await self.client.set_brightness(0, False)
                self.console_widget.add_log("LED OFF", "red")

            # === CAMERA COMMANDS ===
            elif cmd == "cam":
                if not args:
                    self.console_widget.add_log("Usage: cam <init|start|stop|status>", "red")
                    return

                subcmd = args[0].lower()

                if subcmd == "init":
                    await self.client.camera_send_command("init")
                    self.console_widget.add_log("Camera init sent", "green")
                    await asyncio.sleep(1)
                    status = await self.client.get_camera_status()
                    if status:
                        self.console_widget.add_log(f"Camera initialized: {status.get('initialized', False)}", "cyan")

                elif subcmd == "start":
                    await self.client.camera_send_command("start")
                    self.console_widget.add_log("Camera start sent", "green")

                elif subcmd == "stop":
                    await self.client.camera_send_command("stop")
                    self.console_widget.add_log("Camera stop sent", "yellow")

                elif subcmd == "status":
                    status = await self.client.get_camera_status()
                    if status:
                        self.console_widget.add_log(f"Cam: init={status.get('initialized')} active={status.get('active')} fps={status.get('fps', 0):.1f}", "cyan")
                    else:
                        self.console_widget.add_log("No camera status available", "yellow")

                else:
                    self.console_widget.add_log(f"Unknown cam command: {subcmd}", "red")

            # === MOTION COMMANDS ===
            elif cmd == "motion":
                if not args:
                    self.console_widget.add_log("Usage: motion <enable|disable|status|sensitivity N>", "red")
                    return

                subcmd = args[0].lower()

                if subcmd == "enable":
                    await self.client.motion_send_command("enable")
                    self.console_widget.add_log("Motion detection enabled", "green")

                elif subcmd == "disable":
                    await self.client.motion_send_command("disable")
                    self.console_widget.add_log("Motion detection disabled", "yellow")

                elif subcmd == "status":
                    status = await self.client.get_motion_status()
                    if status:
                        self.console_widget.add_log(f"Motion: enabled={status.get('enabled')} detected={status.get('motionDetected')} intensity={status.get('intensity')}", "cyan")
                    else:
                        self.console_widget.add_log("No motion status available", "yellow")

                elif subcmd == "sensitivity":
                    if len(args) < 2:
                        self.console_widget.add_log("Usage: motion sensitivity <0-255>", "red")
                        return
                    sensitivity = int(args[1])
                    await self.client.motion_send_command(f"sensitivity {sensitivity}")
                    self.console_widget.add_log(f"Sensitivity set: {sensitivity}", "green")

                else:
                    self.console_widget.add_log(f"Unknown motion command: {subcmd}", "red")

            # === HELP ===
            elif cmd == "help":
                self.console_widget.add_log("Commands: scan, connect, disconnect, color, effect, brightness, on, off", "cyan")
                self.console_widget.add_log("          cam <init|start|stop|status>, motion <enable|disable|status|sensitivity N>", "cyan")
                self.console_widget.add_log("Shortcuts: Ctrl+S=scan, Ctrl+D=disconnect, F2=cam init, F3=start, F4=stop, F5=motion toggle", "cyan")

            else:
                self.console_widget.add_log(f"Unknown command: {cmd}. Type 'help' for list.", "red")

        except Exception as e:
            self.console_widget.add_log(f"Error: {e}", "red")

    async def _connect_device(self, address: str, name: str):
        """Connette a dispositivo"""
        self.console_widget.add_log(f"Connecting to {address}...", "cyan")
        success = await self.client.connect(address)

        if success:
            self.header_widget.connected = True
            self.header_widget.device_address = address
            self.header_widget.device_name = name
            self.console_widget.add_log(f"Connected to {name}", "green")

            # Leggi stato iniziale
            state = await self.client.get_state()
            if state:
                self.led_widget.led_state = state
        else:
            self.console_widget.add_log("Connection failed", "red")

    # ═══════════════════════════════════════════════════════════════════════
    # CALLBACKS
    # ═══════════════════════════════════════════════════════════════════════

    def _on_led_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback LED state"""
        self.led_widget.led_state = state

        if not is_first and changes:
            for key in changes:
                if key in ['r', 'g', 'b']:
                    r, g, b = state.get('r', 0), state.get('g', 0), state.get('b', 0)
                    self.console_widget.add_log(f"LED → RGB({r},{g},{b})", "magenta")
                    break
                elif key == 'effect':
                    self.console_widget.add_log(f"LED → Effect: {changes[key]['new']}", "magenta")
                elif key == 'brightness':
                    self.console_widget.add_log(f"LED → Brightness: {changes[key]['new']}", "magenta")

    def _on_camera_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback camera state"""
        self.camera_widget.camera_state = state

        if not is_first and changes:
            for key, change in changes.items():
                if key == 'fps':
                    self.console_widget.add_log(f"Camera → FPS: {change['new']:.1f}", "green")
                elif key == 'active':
                    status = "ACTIVE" if change['new'] else "STOPPED"
                    self.console_widget.add_log(f"Camera → {status}", "green")

    def _on_motion_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback motion state"""
        self.motion_widget.motion_state = state

        if not is_first and changes:
            for key, change in changes.items():
                if key == 'motionDetected':
                    status = "DETECTED" if change['new'] else "ENDED"
                    self.console_widget.add_log(f"Motion {status}", "yellow")
                elif key == 'gesture':
                    gesture = change['new']
                    confidence = state.get('gestureConfidence', 0)
                    if gesture != 'none' and confidence > 50:
                        self.console_widget.add_log(f"⚔ GESTURE: {gesture.upper()} ({confidence}%)", "magenta")
                elif key == 'shakeDetected' and change['new']:
                    self.console_widget.add_log("⚠ SHAKE DETECTED!", "red")

    def _on_motion_event(self, event: Dict):
        """Callback motion events"""
        event_type = event.get('event', 'unknown')
        intensity = event.get('intensity', 0)

        emoji_map = {
            "shake_detected": "⚡",
            "motion_started": "▶",
            "motion_ended": "⏹",
            "still_detected": "😴"
        }

        emoji = emoji_map.get(event_type, "🔔")
        self.console_widget.add_log(f"{emoji} {event_type.upper()} (I:{intensity})", "yellow")

    # ═══════════════════════════════════════════════════════════════════════
    # ACTIONS (KEYBOARD SHORTCUTS)
    # ═══════════════════════════════════════════════════════════════════════

    def action_scan_auto(self) -> None:
        """Ctrl+S: Scan automatico"""
        self.run_worker(self._execute_command("scan auto"))

    def action_disconnect(self) -> None:
        """Ctrl+D: Disconnect"""
        self.run_worker(self._execute_command("disconnect"))

    def action_toggle_help(self) -> None:
        """F1: Toggle help"""
        self.run_worker(self._execute_command("help"))

    def action_camera_init(self) -> None:
        """F2: Camera init"""
        self.run_worker(self._execute_command("cam init"))

    def action_camera_start(self) -> None:
        """F3: Camera start"""
        self.run_worker(self._execute_command("cam start"))

    def action_camera_stop(self) -> None:
        """F4: Camera stop"""
        self.run_worker(self._execute_command("cam stop"))

    def action_motion_toggle(self) -> None:
        """F5: Toggle motion detection"""
        # Check current state and toggle
        if self.motion_widget.motion_state.get('enabled', False):
            self.run_worker(self._execute_command("motion disable"))
        else:
            self.run_worker(self._execute_command("motion enable"))


# ═══════════════════════════════════════════════════════════════════════════
# ENTRY POINT
# ═══════════════════════════════════════════════════════════════════════════

def main():
    """Entry point"""
    app = SaberDashboard()
    app.run()


if __name__ == "__main__":
    main()
