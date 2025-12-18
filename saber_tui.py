#!/usr/bin/env python3
"""
LedSaber TUI - Terminal User Interface Cyberpunk Edition
Interfaccia grafica terminale per il controllo LedSaber BLE
Stile: Resistenza Cyberpunk / Matrix
"""

import asyncio
import json
from datetime import datetime
from typing import Optional, Dict, List
from collections import deque

from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, Vertical, ScrollableContainer
from textual.widgets import Header, Footer, Static, Button, Input, DataTable, Label, ProgressBar
from textual.reactive import reactive
from textual.binding import Binding
from textual import events
from rich.table import Table
from rich.panel import Panel
from rich.text import Text
from rich.console import RenderableType, Group
from rich import box

# Import del client BLE esistente
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).parent))
from ledsaber_control import (
    LedSaberClient,
    LED_SERVICE_UUID, CAMERA_SERVICE_UUID, MOTION_SERVICE_UUID
)


# ============================================================================
# WIDGETS CUSTOM
# ============================================================================

class LEDStatusWidget(Static):
    """Widget per visualizzare stato LED in stile cyberpunk"""

    led_state: reactive[Dict] = reactive({})

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.border_title = "⚡ LED CONTROL SYSTEM"

    def render(self) -> RenderableType:
        """Renderizza stato LED"""
        state = self.led_state or {}

        # Estrai valori con fallback
        enabled = state.get('enabled', False)
        r = state.get('r', 0)
        g = state.get('g', 0)
        b = state.get('b', 0)
        brightness = state.get('brightness', 0)
        effect = state.get('effect', 'unknown')
        speed = state.get('speed', 0)
        fold_point = state.get('foldPoint', 72)

        # Status indicator
        status_icon = "[bold green]● ONLINE[/]" if enabled else "[bold red]● OFFLINE[/]"

        # Crea tabella Rich
        table = Table(show_header=False, box=box.SIMPLE, padding=(0, 1))
        table.add_column("Key", style="cyan bold")
        table.add_column("Value", style="magenta")

        table.add_row("STATUS", status_icon)
        table.add_row("RGB", f"[red]{r:3d}[/] [green]{g:3d}[/] [blue]{b:3d}[/]")
        table.add_row("BRIGHTNESS", f"{brightness:3d}/255 [{int(brightness/255*10)*'█'}{(10-int(brightness/255*10))*'░'}]")
        table.add_row("EFFECT", f"[yellow]{effect.upper()}[/]")
        table.add_row("SPEED", f"{speed} ms")
        table.add_row("FOLD POINT", f"{fold_point}/143")

        return Panel(
            table,
            title=f"[bold cyan]⚡ LED CONTROL[/]",
            border_style="cyan",
            box=box.DOUBLE_EDGE
        )


class CameraMetricsWidget(Static):
    """Widget per visualizzare metriche camera"""

    camera_state: reactive[Dict] = reactive({})
    fps_history: deque = deque(maxlen=20)  # Storia FPS per sparkline

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def watch_camera_state(self, new_state: Dict):
        """Aggiorna storia FPS quando cambia lo stato"""
        if new_state:
            fps = new_state.get('fps', 0)
            self.fps_history.append(fps)

    def render(self) -> RenderableType:
        """Renderizza metriche camera"""
        state = self.camera_state or {}

        initialized = state.get('initialized', False)
        active = state.get('active', False)
        fps = state.get('fps', 0.0)
        total_frames = state.get('totalFrames', 0)
        failed = state.get('failedCaptures', 0)

        # Status indicator
        status_icon = "[bold green]◉ ACTIVE[/]" if active else "[bold yellow]◎ IDLE[/]"
        init_icon = "✓" if initialized else "✗"

        # FPS sparkline
        sparkline = self._render_sparkline(list(self.fps_history), max_val=30.0)

        table = Table(show_header=False, box=box.SIMPLE, padding=(0, 1))
        table.add_column("Key", style="cyan bold")
        table.add_column("Value", style="green")

        table.add_row("STATUS", status_icon)
        table.add_row("INITIALIZED", f"[{'green' if initialized else 'red'}]{init_icon}[/]")
        table.add_row("FPS", f"[bold yellow]{fps:.2f}[/] fps")
        table.add_row("FPS GRAPH", sparkline)
        table.add_row("FRAMES", f"{total_frames:,}")
        table.add_row("FAILED", f"[{'red' if failed > 0 else 'green'}]{failed}[/]")

        return Panel(
            table,
            title="[bold green]📸 CAMERA METRICS[/]",
            border_style="green",
            box=box.DOUBLE_EDGE
        )

    def _render_sparkline(self, data: List[float], max_val: float = 30.0) -> str:
        """Genera sparkline ASCII per FPS"""
        if not data:
            return "░" * 20

        # Normalizza a 0-8 (caratteri block)
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


class MotionDetectionWidget(Static):
    """Widget per motion detection con optical flow grid"""

    motion_state: reactive[Dict] = reactive({})
    intensity_history: deque = deque(maxlen=30)

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    def watch_motion_state(self, new_state: Dict):
        """Aggiorna storia intensity"""
        if new_state:
            intensity = new_state.get('intensity', 0)
            self.intensity_history.append(intensity)

    def render(self) -> RenderableType:
        """Renderizza motion detection"""
        state = self.motion_state or {}

        enabled = state.get('enabled', False)
        motion_detected = state.get('motionDetected', False)
        intensity = state.get('intensity', 0)
        direction = state.get('direction', 'none')
        changed_pixels = state.get('changedPixels', 0)
        shake_detected = state.get('shakeDetected', False)
        total_frames = state.get('totalFrames', 0)
        motion_frames = state.get('motionFrames', 0)
        gesture = state.get('gesture', 'none')
        gesture_confidence = state.get('gestureConfidence', 0)

        # Grid optical flow
        grid_rows = state.get('grid', [])
        grid_cols = state.get('gridCols', 8)
        block_size = state.get('blockSize', 40)

        # Status
        status_icon = "[bold green]◉ ENABLED[/]" if enabled else "[bold red]● DISABLED[/]"
        motion_icon = "[bold yellow]⚡ MOTION[/]" if motion_detected else "[dim]○ STILL[/]"
        shake_icon = "[bold red blink]⚠ SHAKE![/]" if shake_detected else ""

        # Intensity sparkline
        intensity_spark = self._render_intensity_bar(intensity)
        intensity_graph = self._render_sparkline(list(self.intensity_history), max_val=255)

        # Direzione con freccia
        direction_arrows = {
            'up': '↑', 'down': '↓', 'left': '←', 'right': '→',
            'up-right': '↗', 'up-left': '↖', 'down-right': '↘', 'down-left': '↙',
            'none': '·'
        }
        dir_arrow = direction_arrows.get(direction, '·')
        dir_display = f"[cyan]{dir_arrow} {direction.upper()}[/]"

        # Gesture display
        gesture_display = ""
        if gesture != 'none' and gesture_confidence > 50:
            gesture_display = f"\n[bold magenta]⚔ GESTURE: {gesture.upper()} ({gesture_confidence}%)[/]"

        # Tabella info
        table = Table(show_header=False, box=box.SIMPLE, padding=(0, 1))
        table.add_column("Key", style="cyan bold")
        table.add_column("Value", style="magenta")

        table.add_row("STATUS", status_icon)
        table.add_row("MOTION", motion_icon + (" " + shake_icon if shake_icon else ""))
        table.add_row("INTENSITY", intensity_spark)
        table.add_row("HISTORY", intensity_graph)
        table.add_row("DIRECTION", dir_display)
        table.add_row("PIXELS", f"{changed_pixels:,}")
        table.add_row("FRAMES", f"{motion_frames}/{total_frames}")

        # Optical flow grid
        grid_display = self._render_optical_flow_grid(grid_rows, grid_cols, block_size)

        # Componi renderables usando Group
        renderables = [table]
        if gesture_display:
            renderables.append(Text(gesture_display))
        renderables.append(Text("\n"))
        renderables.append(grid_display)

        content = Group(*renderables)

        return Panel(
            content,
            title=f"[bold magenta]🔍 MOTION DETECTOR[/]",
            border_style="magenta",
            box=box.DOUBLE_EDGE
        )

    def _render_intensity_bar(self, intensity: int) -> str:
        """Barra intensity 0-255"""
        normalized = int((intensity / 255) * 20)
        bar = "█" * normalized + "░" * (20 - normalized)

        # Colora in base all'intensità
        if intensity > 200:
            color = "red"
        elif intensity > 100:
            color = "yellow"
        else:
            color = "green"

        return f"[{color}]{intensity:3d}[/] [{color}]{bar}[/]"

    def _render_sparkline(self, data: List[int], max_val: int = 255) -> str:
        """Sparkline per history"""
        if not data:
            return "░" * 30

        blocks = "▁▂▃▄▅▆▇█"
        result = []
        for val in data:
            idx = int((val / max_val) * (len(blocks) - 1))
            idx = max(0, min(len(blocks) - 1, idx))
            result.append(blocks[idx])

        # Colora
        colored = "".join([
            f"[red]{c}[/]" if data[i] > 200 else
            f"[yellow]{c}[/]" if data[i] > 100 else
            f"[green]{c}[/]"
            for i, c in enumerate(result)
        ])

        return colored

    def _render_optical_flow_grid(self, grid_rows: List[str], cols: int, block_size: int) -> Text:
        """Renderizza optical flow grid con colori"""
        text = Text()
        text.append(f"[bold cyan]Optical Flow Grid ({cols}x{len(grid_rows)} @ {block_size}px):[/]\n")

        if not grid_rows:
            text.append("[dim]No data[/]\n")
            return text

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

        for row_str in grid_rows:
            text.append("  ")
            for char in row_str:
                color = symbol_colors.get(char, 'white')
                text.append(f"[{color}]{char}[/] ")
            text.append("\n")

        text.append("[dim]Legend: . idle, ^/v up/down, </> left/right, A↗ B↖ C↘ D↙[/]\n")

        return text


class ConnectionStatusWidget(Static):
    """Widget per stato connessione BLE"""

    connected: reactive[bool] = reactive(False)
    device_address: reactive[str] = reactive("")
    device_name: reactive[str] = reactive("Not Connected")

    def render(self) -> RenderableType:
        """Renderizza stato connessione"""
        if self.connected:
            status = f"[bold green]◉ CONNECTED[/]"
            device_info = f"[cyan]{self.device_name}[/]\n[dim]{self.device_address}[/]"
        else:
            status = f"[bold red]● DISCONNECTED[/]"
            device_info = "[dim]No device connected[/]"

        table = Table(show_header=False, box=None, padding=(0, 1))
        table.add_column("Status", style="bold")
        table.add_column("Device")
        table.add_row(status, device_info)

        return Panel(
            table,
            title="[bold yellow]📡 BLE CONNECTION[/]",
            border_style="yellow",
            box=box.ROUNDED
        )


class EventLogWidget(Static):
    """Widget per log eventi in tempo reale"""

    def __init__(self, max_lines: int = 10, **kwargs):
        super().__init__(**kwargs)
        self.max_lines = max_lines
        self.log_lines: deque = deque(maxlen=max_lines)

    def add_event(self, event_type: str, message: str, style: str = "white"):
        """Aggiungi evento al log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        line = f"[dim]{timestamp}[/] [{style}]{event_type}[/] {message}"
        self.log_lines.append(line)
        self.refresh()

    def render(self) -> RenderableType:
        """Renderizza log eventi"""
        if not self.log_lines:
            content = "[dim]No events yet...[/]"
        else:
            content = "\n".join(self.log_lines)

        return Panel(
            content,
            title="[bold white]📜 EVENT LOG[/]",
            border_style="white",
            box=box.ROUNDED
        )


class CommandInputWidget(Static):
    """Widget per input comandi"""

    def compose(self) -> ComposeResult:
        yield Label("[bold cyan]>[/] Command:", id="cmd_label")
        yield Input(placeholder="Type command... (help for list)", id="cmd_input")


# ============================================================================
# APP PRINCIPALE
# ============================================================================

class SaberTUI(App):
    """Applicazione TUI principale"""

    CSS = """
    Screen {
        background: $surface;
    }

    Header {
        background: $primary;
        color: $text;
        text-style: bold;
    }

    Footer {
        background: $primary-darken-2;
    }

    #main_container {
        height: 100%;
        background: $surface;
    }

    #top_row {
        height: auto;
        margin: 1;
    }

    #stats_row {
        height: auto;
        margin: 1;
    }

    #connection_status {
        width: 1fr;
        height: auto;
        margin: 0 1;
    }

    #led_status {
        width: 1fr;
        height: auto;
        margin: 0 1;
    }

    #camera_metrics {
        width: 1fr;
        height: auto;
        margin: 0 1;
    }

    #motion_detection {
        width: 100%;
        height: auto;
        margin: 1;
    }

    #event_log {
        width: 100%;
        height: auto;
        margin: 1;
    }

    #cmd_input {
        border: solid cyan;
        width: 100%;
    }

    #cmd_label {
        color: cyan;
        text-style: bold;
        padding: 0 1;
    }

    .command_area {
        height: auto;
        margin: 1;
        background: $panel;
    }
    """

    BINDINGS = [
        Binding("ctrl+c", "quit", "Quit", priority=True),
        Binding("ctrl+s", "scan_auto", "Scan & Connect"),
        Binding("ctrl+d", "disconnect", "Disconnect"),
        Binding("f1", "show_help", "Help"),
    ]

    TITLE = "🗡️  LEDSABER CONTROL SYSTEM - CYBERPUNK EDITION"
    SUB_TITLE = "Resistance is Futile"

    def __init__(self):
        super().__init__()
        self.client = LedSaberClient()
        self.connection_widget: Optional[ConnectionStatusWidget] = None
        self.led_widget: Optional[LEDStatusWidget] = None
        self.camera_widget: Optional[CameraMetricsWidget] = None
        self.motion_widget: Optional[MotionDetectionWidget] = None
        self.event_log: Optional[EventLogWidget] = None

    def compose(self) -> ComposeResult:
        """Compone layout TUI"""
        yield Header()

        with Container(id="main_container"):
            # Top row: Connection + Stats
            with Horizontal(id="top_row"):
                yield ConnectionStatusWidget(id="connection_status")

            # Second row: LED + Camera
            with Horizontal(id="stats_row"):
                yield LEDStatusWidget(id="led_status")
                yield CameraMetricsWidget(id="camera_metrics")

            # Motion detection
            yield MotionDetectionWidget(id="motion_detection")

            # Event log
            yield EventLogWidget(max_lines=8, id="event_log")

            # Command input
            with Container(classes="command_area"):
                yield Label("[bold cyan]>[/] Command:", id="cmd_label")
                yield Input(placeholder="Type command... (help for list)", id="cmd_input")

        yield Footer()

    def on_mount(self) -> None:
        """Inizializza widget dopo mount"""
        self.connection_widget = self.query_one("#connection_status", ConnectionStatusWidget)
        self.led_widget = self.query_one("#led_status", LEDStatusWidget)
        self.camera_widget = self.query_one("#camera_metrics", CameraMetricsWidget)
        self.motion_widget = self.query_one("#motion_detection", MotionDetectionWidget)
        self.event_log = self.query_one("#event_log", EventLogWidget)

        # Setup callbacks
        self.client.state_callback = self._on_led_state_update
        self.client.camera_callback = self._on_camera_update
        self.client.motion_callback = self._on_motion_update
        self.client.motion_event_callback = self._on_motion_event

        self.event_log.add_event("SYSTEM", "TUI initialized", "green")
        self.event_log.add_event("INFO", "Press F1 for help, Ctrl+S to scan", "cyan")

    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Gestisce comando inviato"""
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
            if cmd in ["quit", "exit", "q"]:
                self.event_log.add_event("SYSTEM", "Shutting down...", "yellow")
                await self.client.disconnect()
                self.exit()

            elif cmd == "scan":
                auto = bool(args and args[0].lower() == "auto")
                self.event_log.add_event("BLE", "Scanning for devices...", "cyan")
                devices = await self.client.scan()

                if not devices:
                    self.event_log.add_event("BLE", "No devices found", "yellow")
                    return

                if auto and len(devices) == 1:
                    device = devices[0]
                    await self._connect_device(device.address, device.name)
                else:
                    for dev in devices:
                        self.event_log.add_event("BLE", f"Found: {dev.name} ({dev.address})", "green")

            elif cmd == "connect":
                if not args:
                    self.event_log.add_event("ERROR", "Usage: connect <address>", "red")
                    return
                await self._connect_device(args[0], "LedSaber")

            elif cmd == "disconnect":
                await self.client.disconnect()
                self.connection_widget.connected = False
                self.connection_widget.device_name = "Not Connected"
                self.event_log.add_event("BLE", "Disconnected", "yellow")

            elif cmd == "color":
                if len(args) < 3:
                    self.event_log.add_event("ERROR", "Usage: color <r> <g> <b>", "red")
                    return
                r, g, b = int(args[0]), int(args[1]), int(args[2])
                await self.client.set_color(r, g, b)
                self.event_log.add_event("LED", f"Color set: RGB({r},{g},{b})", "magenta")

            elif cmd == "effect":
                if not args:
                    self.event_log.add_event("ERROR", "Usage: effect <mode> [speed]", "red")
                    return
                mode = args[0]
                speed = int(args[1]) if len(args) > 1 else 150
                await self.client.set_effect(mode, speed)
                self.event_log.add_event("LED", f"Effect: {mode} @ {speed}ms", "magenta")

            elif cmd == "brightness":
                if not args:
                    self.event_log.add_event("ERROR", "Usage: brightness <0-255>", "red")
                    return
                brightness = int(args[0])
                await self.client.set_brightness(brightness, True)
                self.event_log.add_event("LED", f"Brightness: {brightness}", "magenta")

            elif cmd == "on":
                await self.client.set_brightness(255, True)
                self.event_log.add_event("LED", "LED ON", "green")

            elif cmd == "off":
                await self.client.set_brightness(0, False)
                self.event_log.add_event("LED", "LED OFF", "red")

            elif cmd == "help":
                self.event_log.add_event("HELP", "Commands: scan, connect, color, effect, brightness, on, off", "cyan")
                self.event_log.add_event("HELP", "Shortcuts: Ctrl+S=scan, Ctrl+D=disconnect, Ctrl+C=quit", "cyan")

            else:
                self.event_log.add_event("ERROR", f"Unknown command: {cmd}", "red")

        except Exception as e:
            self.event_log.add_event("ERROR", str(e), "red")

    async def _connect_device(self, address: str, name: str):
        """Connette a dispositivo"""
        self.event_log.add_event("BLE", f"Connecting to {address}...", "cyan")
        success = await self.client.connect(address)

        if success:
            self.connection_widget.connected = True
            self.connection_widget.device_address = address
            self.connection_widget.device_name = name
            self.event_log.add_event("BLE", f"Connected to {name}", "green")

            # Leggi stato iniziale
            state = await self.client.get_state()
            if state:
                self.led_widget.led_state = state
        else:
            self.event_log.add_event("BLE", "Connection failed", "red")

    def _on_led_state_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback aggiornamento LED"""
        self.led_widget.led_state = state

        if not is_first and changes:
            for key in changes:
                if key in ['r', 'g', 'b']:
                    r, g, b = state.get('r', 0), state.get('g', 0), state.get('b', 0)
                    self.event_log.add_event("LED", f"Color → RGB({r},{g},{b})", "magenta")
                    break
                elif key == 'effect':
                    self.event_log.add_event("LED", f"Effect → {changes[key]['new']}", "magenta")
                elif key == 'brightness':
                    self.event_log.add_event("LED", f"Brightness → {changes[key]['new']}", "magenta")

    def _on_camera_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback aggiornamento Camera"""
        self.camera_widget.camera_state = state

        if not is_first and changes:
            for key, change in changes.items():
                if key == 'fps':
                    self.event_log.add_event("CAMERA", f"FPS → {change['new']:.2f}", "green")
                elif key == 'active':
                    status = "ACTIVE" if change['new'] else "STOPPED"
                    self.event_log.add_event("CAMERA", f"Status → {status}", "green")

    def _on_motion_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback aggiornamento Motion"""
        self.motion_widget.motion_state = state

        if not is_first and changes:
            for key, change in changes.items():
                if key == 'motionDetected':
                    status = "DETECTED" if change['new'] else "ENDED"
                    self.event_log.add_event("MOTION", f"Motion {status}", "yellow")
                elif key == 'gesture':
                    gesture = change['new']
                    confidence = state.get('gestureConfidence', 0)
                    if gesture != 'none' and confidence > 50:
                        self.event_log.add_event("GESTURE", f"⚔ {gesture.upper()} ({confidence}%)", "magenta")
                elif key == 'shakeDetected' and change['new']:
                    self.event_log.add_event("MOTION", "⚠ SHAKE DETECTED!", "red")

    def _on_motion_event(self, event: Dict):
        """Callback eventi motion"""
        event_type = event.get('event', 'unknown')
        intensity = event.get('intensity', 0)

        emoji_map = {
            "shake_detected": "⚡",
            "motion_started": "▶",
            "motion_ended": "⏹",
            "still_detected": "😴"
        }

        emoji = emoji_map.get(event_type, "🔔")
        self.event_log.add_event("EVENT", f"{emoji} {event_type.upper()} (I:{intensity})", "yellow")

    def action_scan_auto(self) -> None:
        """Azione: scan automatico"""
        self.run_worker(self._execute_command("scan auto"))

    def action_disconnect(self) -> None:
        """Azione: disconnect"""
        self.run_worker(self._execute_command("disconnect"))

    def action_show_help(self) -> None:
        """Azione: mostra help"""
        self.run_worker(self._execute_command("help"))


# ============================================================================
# ENTRY POINT
# ============================================================================

def main():
    """Entry point"""
    app = SaberTUI()
    app.run()


if __name__ == "__main__":
    main()
