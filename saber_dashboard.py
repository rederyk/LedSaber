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

from textual import events
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
    """Header responsive con titolo e status BLE - senza ASCII art fisso"""

    connected: reactive[bool] = reactive(False)
    device_name: reactive[str] = reactive("NO DEVICE")
    device_address: reactive[str] = reactive("")

    def render(self):
        # Status BLE
        if self.connected:
            ble_status = f"[green]◉[/] {self.device_name}"
            ble_detail = f"[dim]{self.device_address}[/]"
        else:
            ble_status = f"[red]●[/] DISCONNECTED"
            ble_detail = "[dim]No device[/]"

        # Crea tabella a 2 colonne
        table = Table.grid(padding=(0, 2), expand=True)
        table.add_column(justify="left", ratio=1)
        table.add_column(justify="right", ratio=1)

        # Row 1: Titolo + Modalità
        title_text = Text()
        title_text.append("⚔️  ", style="yellow bold")
        title_text.append("LEDSABER DASHBOARD", style="cyan bold")

        mode_text = Text("Resistance Edition", style="magenta")

        table.add_row(title_text, mode_text)

        # Row 2: BLE status
        ble_text = Text()
        ble_text.append("BLE: ", style="cyan")
        ble_text.append(ble_status)

        table.add_row(ble_text, ble_detail)

        width = self.size.width or 100
        horizontal_padding = 1 if width < 100 else 2

        return Panel(
            table,
            border_style="cyan bold",
            box=box.DOUBLE,
            padding=(0, horizontal_padding)
        )


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


class MotionStatusCard(Static):
    """Card compatta per status motion + shake"""

    motion_state: reactive[Dict] = reactive({})

    def render(self):
        state = self.motion_state or {}
        enabled = state.get('enabled', False)
        motion_detected = state.get('motionDetected', False)
        shake_detected = state.get('shakeDetected', False)

        status_icon = "[green]◉ ENABLED[/]" if enabled else "[red]● DISABLED[/]"
        motion_icon = "[yellow]⚡[/]" if motion_detected else "[dim]○[/]"
        shake_icon = "[red blink]⚠[/]" if shake_detected else "[dim]·[/]"

        table = Table.grid(padding=(0, 1))
        table.add_column(justify="left")
        table.add_row(f"Status: {status_icon}")
        table.add_row(f"Motion: {motion_icon}")
        table.add_row(f"Shake: {shake_icon}")

        return Panel(
            table,
            title="[bold magenta]⚡ STATUS[/]",
            border_style="magenta",
            box=box.ROUNDED,
            height=7
        )


class MotionIntensityCard(Static):
    """Card per intensity bar + sparkline + pixel count"""

    motion_state: reactive[Dict] = reactive({})
    intensity_history: deque = deque(maxlen=40)

    def watch_motion_state(self, new_state: Dict):
        if new_state:
            intensity = new_state.get('intensity', 0)
            self.intensity_history.append(intensity)

    def _sparkline(self, data: List[int], max_val: int = 255, width: int = 40) -> str:
        if not data:
            return "░" * width

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
        intensity = state.get('intensity', 0)
        changed_pixels = state.get('changedPixels', 0)

        # Intensity bar
        intensity_pct = int((intensity / 255) * 20)
        intensity_bar = f"[yellow]{'█' * intensity_pct}{'░' * (20 - intensity_pct)}[/]"

        # Sparkline
        sparkline = self._sparkline(list(self.intensity_history))

        table = Table.grid(padding=(0, 1))
        table.add_column(justify="left")
        table.add_row(f"[cyan]Int: {intensity:<3}[/] {intensity_bar}")
        table.add_row(f"[dim]Hist:[/]{sparkline}")
        table.add_row(f"[dim]Px: {changed_pixels:,}[/]")

        return Panel(
            table,
            title="[bold yellow]📊 INTENSITY[/]",
            border_style="yellow",
            box=box.ROUNDED,
            height=7
        )


class MotionDirectionCard(Static):
    """Card per direzione + gesture"""

    motion_state: reactive[Dict] = reactive({})

    def render(self):
        state = self.motion_state or {}
        direction = state.get('direction', 'none')
        gesture = state.get('gesture', 'none')
        gesture_confidence = state.get('gestureConfidence', 0)

        # Direction arrows
        direction_arrows = {
            'up': '↑', 'down': '↓', 'left': '←', 'right': '→',
            'up-right': '↗', 'up-left': '↖', 'down-right': '↘', 'down-left': '↙',
            'none': '·'
        }
        dir_arrow = direction_arrows.get(direction, '·')

        # Gesture display
        if gesture != 'none' and gesture_confidence > 50:
            gesture_display = f"[bold yellow]⚔ {gesture.upper()}[/]\n[dim]{gesture_confidence}%[/]"
        else:
            gesture_display = "[dim]no gesture[/]"

        table = Table.grid(padding=(0, 1), expand=True)
        table.add_column(justify="center")
        table.add_row(f"[cyan bold]{dir_arrow}[/] [cyan]{direction.upper()}[/]")
        table.add_row("─" * 15)
        table.add_row(gesture_display)

        return Panel(
            table,
            title="[bold cyan]🧭 DIRECTION[/]",
            border_style="cyan",
            box=box.ROUNDED,
            height=7
        )


class OpticalFlowGridWidget(Static):
    """Widget griglia optical flow - full width"""

    motion_state: reactive[Dict] = reactive({})

    def render(self):
        state = self.motion_state or {}

        raw_rows = state.get('grid', [])
        grid_cols = state.get('gridCols') or (len(raw_rows[0]) if raw_rows else 8)
        grid_rows_count = state.get('gridRows') or (len(raw_rows) if raw_rows else 8)
        grid_cols = max(1, grid_cols)
        grid_rows_count = max(1, grid_rows_count)
        block_size = state.get('blockSize', 40)
        has_live_data = bool(raw_rows)

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

        # Normalizza righe (garantisce sempre una matrice completa)
        normalized_rows: List[str] = []
        if not raw_rows:
            normalized_rows = ["." * grid_cols for _ in range(grid_rows_count)]
        else:
            for row_str in raw_rows[:grid_rows_count]:
                padded = row_str[:grid_cols].ljust(grid_cols, ".")
                normalized_rows.append(padded)
            # Se dati ricevuti ma meno righe del previsto, completa con placeholder
            if len(normalized_rows) < grid_rows_count:
                missing = grid_rows_count - len(normalized_rows)
                normalized_rows.extend(["." * grid_cols for _ in range(missing)])

        # Calculate available space
        width = self.size.width or 120
        height = self.size.height or 40

        # Reserve space for borders/padding (riduciamo per massimizzare lo spazio)
        # Horizontal: ~6 chars (border + padding)
        usable_width = max(20, width - 6)
        # Vertical: ~4 lines (border minimo)
        usable_height = max(5, height - 4)

        # I caratteri terminale sono più alti che larghi
        # Per proporzioni visive corrette: h_gap deve essere maggiore di v_gap
        # Tipicamente i caratteri hanno aspect ratio height/width ~ 2.0-2.5
        # Ma per compensare la spaziatura visiva usiamo un valore più alto
        CHAR_ASPECT_RATIO = 3.5

        # Calcola il gap massimo possibile per ogni dimensione
        if grid_cols > 1:
            max_h_gap = (usable_width - grid_cols) // (grid_cols - 1)
        else:
            max_h_gap = 1

        if grid_rows_count > 1:
            max_v_gap = (usable_height - grid_rows_count) // (grid_rows_count - 1)
        else:
            max_v_gap = 1

        # Calcola quale dimensione è più limitante considerando l'aspect ratio
        # Se usiamo tutto lo spazio verticale, quanto h_gap avremmo bisogno?
        needed_h_gap_for_max_v = int(max_v_gap * CHAR_ASPECT_RATIO)

        # Se usiamo tutto lo spazio orizzontale, quanto v_gap avremmo bisogno?
        needed_v_gap_for_max_h = int(max_h_gap / CHAR_ASPECT_RATIO)

        # Scegli l'opzione che massimizza l'uso dello spazio mantenendo le proporzioni
        if needed_h_gap_for_max_v <= max_h_gap:
            # Possiamo usare tutto lo spazio verticale
            v_gap = max_v_gap
            h_gap = needed_h_gap_for_max_v
        else:
            # Limitati dallo spazio orizzontale
            h_gap = max_h_gap
            v_gap = needed_v_gap_for_max_h

        # Assicurati che non siano zero
        v_gap = max(1, v_gap)
        h_gap = max(1, h_gap)

        cell_gap = " " * h_gap
        row_gap = "\n" * v_gap if v_gap > 0 else ""

        grid_lines: List[str] = []
        for i, row_str in enumerate(normalized_rows):
            colored_cells = []
            for char in row_str:
                color = symbol_colors.get(char, 'white')
                if not has_live_data and char == '.':
                    color = "dim"
                colored_cells.append(f"[{color}]{char}[/]")
            row_line = cell_gap.join(colored_cells)
            grid_lines.append(row_line)

            # Aggiungi gap verticale tra le righe (ma non dopo l'ultima)
            if i < len(normalized_rows) - 1 and v_gap > 0:
                for _ in range(v_gap):
                    grid_lines.append("")

        grid_markup = "\n".join(grid_lines)
        grid_text = Text.from_markup(grid_markup)

        legend = Table.grid(expand=True, padding=0)
        legend.add_column(justify="left")
        legend_text = Text()
        legend_text.append("Legend: ", style="dim")
        legend_text.append(". idle  ", style="dim white")
        legend_text.append("^ v up/down  ", style="cyan")
        legend_text.append("< > left/right  ", style="yellow")
        legend_text.append("A B C D diagonals", style="green")
        legend.add_row(legend_text)

        pad_x = 1
        grid_label = "Live optical flow" if has_live_data else "Waiting for motion data"
        info_line = Text(f"Grid: {grid_cols}x{grid_rows_count} @ {block_size}px  •  {grid_label}", style="dim cyan")

        grid_panel = Panel(
            Align.center(grid_text, vertical="middle"),
            border_style="cyan",
            box=box.SQUARE,
            padding=(0, 2),  # Ridotto padding verticale e orizzontale
            # Remove fixed dimensions to allow expansion
        )

        content = Group(
            Align.center(info_line),
            Align.center(grid_panel),
            legend
        )

        return Panel(
            content,
            title="[bold magenta]🔍 OPTICAL FLOW GRID[/]",
            border_style="magenta",
            box=box.ROUNDED,  # Più accattivante
            padding=(0, pad_x),  # Ridotto padding esterno
            expand=True  # Allow panel to expand with terminal
        )


class BLERSSICard(Static):
    """Mini card RSSI BLE"""

    rssi: reactive[int] = reactive(-99)
    connected: reactive[bool] = reactive(False)

    def render(self):
        if not self.connected:
            display = "[dim]N/A[/]"
            color = "dim"
        else:
            display = f"{self.rssi} dBm"
            if self.rssi > -60:
                color = "green"
            elif self.rssi > -75:
                color = "yellow"
            else:
                color = "red"

        content = Text(display, style=color, justify="center")

        return Panel(
            content,
            title="[cyan]📡 RSSI[/]",
            border_style="cyan",
            box=box.ROUNDED,
            height=4
        )


class ActiveFXCard(Static):
    """Mini card effetto LED attivo"""

    effect: reactive[str] = reactive("none")
    speed: reactive[int] = reactive(0)

    def render(self):
        if self.effect == "none" or self.effect == "unknown":
            display = "[dim]No FX[/]"
        else:
            display = f"[magenta bold]{self.effect.upper()}[/]\n[dim]{self.speed}ms[/]"

        return Panel(
            Align.center(display, vertical="middle"),
            title="[magenta]✨ FX[/]",
            border_style="magenta",
            box=box.ROUNDED,
            height=4
        )


class CameraFramesCard(Static):
    """Mini card frame count camera"""

    total_frames: reactive[int] = reactive(0)
    active: reactive[bool] = reactive(False)

    def render(self):
        if not self.active:
            display = "[dim]Idle[/]"
        else:
            display = f"[green bold]{self.total_frames:,}[/]\n[dim]frames[/]"

        return Panel(
            Align.center(display, vertical="middle"),
            title="[green]🎬 Frames[/]",
            border_style="green",
            box=box.ROUNDED,
            height=4
        )


class MotionSection(Container):
    """Container responsive per widget Motion summary"""

    motion_state: reactive[Dict] = reactive({})

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.status_card = MotionStatusCard(id="motion_status_card")
        self.intensity_card = MotionIntensityCard(id="motion_intensity_card")
        self.direction_card = MotionDirectionCard(id="motion_direction_card")

    def compose(self) -> ComposeResult:
        yield self.status_card
        yield self.intensity_card
        yield self.direction_card

    def watch_motion_state(self, new_state: Dict):
        self.status_card.motion_state = new_state or {}
        self.intensity_card.motion_state = new_state or {}
        self.direction_card.motion_state = new_state or {}


class ConsoleWidget(Static):
    """Console per log + input comandi (minimale in basso)"""

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.log_lines: deque = deque(maxlen=400)  # Buffer più ampio

    def add_log(self, message: str, style: str = "white"):
        """Aggiungi messaggio al log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        line = f"[dim]{timestamp}[/] [{style}]{message}[/]"
        self.log_lines.append(line)
        self.refresh()

    def render(self):
        lines = list(self.log_lines)

        if not lines:
            log_content = "[dim]No logs yet. Type 'help' for commands.[/]"
        else:
            log_content = "\n".join(lines)

        width = max(40, (self.size.width or 80) - 4)
        separator = "─" * width

        content = Text()
        content.append("CONSOLE LOG (scroll with mouse/keys)\n", style="cyan bold")
        content.append(separator + "\n", style="dim")
        content.append(log_content + "\n")
        content.append(separator + "\n", style="dim")
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
        height: 5;
        margin: 0 1;
        padding: 0;
    }

    #main_body {
        height: 1fr;
        padding: 0 1;
    }

    #stats_grid {
        layout: grid;
        grid-size: 6;
        grid-columns: 1fr 1fr 1fr 1fr 1fr 1fr;
        padding: 0;
        margin: 0;
        height: auto;
    }

    #stats_grid.cols-3 {
        grid-size: 6;
        grid-columns: 1fr 1fr 1fr 1fr 1fr 1fr;
        height: auto;
    }

    #stats_grid.cols-2 {
        grid-size: 2;
        grid-columns: 1fr 1fr;
        height: auto;
    }

    #stats_grid.cols-1 {
        grid-size: 1;
        grid-columns: 1fr;
        height: auto;
    }

    #kpi_row, #camera_frames_card {
        margin: 0;
    }

    /* Prima riga 3x (1/3), seconda riga 2x (1/2) su desktop */
    #led_column,
    #camera_column,
    #motion_summary {
        column-span: 2;
    }

    #optical_flow,
    #console_column {
        column-span: 3;
    }

    /* Su layout 2-colonne riportiamo a span=1 */
    #stats_grid.cols-2 > * {
        column-span: 1;
    }

    /* Su layout 1-colonna forziamo span singolo */
    #stats_grid.cols-1 > * {
        column-span: 1;
    }

    #motion_summary {
        layout: grid;
        grid-size: 3;
        grid-columns: 1fr 1fr 1fr;
        padding-left: 1;
        height: auto;
        margin: 0;
    }

    #motion_summary.cols-1 {
        grid-size: 1;
        grid-columns: 1fr;
    }

    #kpi_row {
        layout: grid;
        grid-size: 2;
        grid-columns: 1fr 1fr;
        margin: 0;
        height: auto;
    }

    #kpi_row.cols-1 {
        grid-size: 1;
        grid-columns: 1fr;
    }

    #optical_flow {
        width: 100%;
        height: 1fr;
        min-height: 15;
    }

    #console_column {
        width: 100%;
        min-width: 40;
        layout: vertical;
        height: auto;
    }

    #console_scroll {
        border: round cyan;
        background: $panel;
        padding: 1;
        height: 1fr;
        min-height: 15;
    }

    #console_log {
        background: $surface;
        padding: 0 0 1 0;
    }

    #cmd_input {
        border: solid cyan;
        background: $surface;
        margin: 1 0 0 0;
    }

    #led_column {
        padding: 0 1 0 0;
    }

    #camera_column {
        padding: 0 1;
    }

    #led_panel,
    #camera_panel {
        margin: 0;
    }

    #ble_rssi_card {
        margin-right: 1;
        margin-bottom: 0;
    }

    #active_fx_card,
    #camera_frames_card {
        margin: 0;
    }

    #motion_status_card,
    #motion_intensity_card,
    #motion_direction_card {
        margin: 0;
    }

    #motion_status_card,
    #motion_intensity_card {
        margin-right: 1;
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
        self.motion_section: Optional[MotionSection] = None
        self.optical_widget: Optional[OpticalFlowGridWidget] = None
        self.console_widget: Optional[ConsoleWidget] = None
        self.console_scroll: Optional[VerticalScroll] = None
        self.rssi_card: Optional[BLERSSICard] = None
        self.fx_card: Optional[ActiveFXCard] = None
        self.camera_frames_card: Optional[CameraFramesCard] = None
        self.device_rssi_map: Dict[str, Optional[int]] = {}
        self.stats_grid: Optional[Container] = None
        self.kpi_row: Optional[Container] = None
        self.main_scroll: Optional[VerticalScroll] = None

    def compose(self) -> ComposeResult:
        """Componi layout"""
        yield HeaderWidget(id="header")

        with VerticalScroll(id="main_body"):
            with Container(id="stats_grid"):
                with Vertical(id="led_column"):
                    yield LEDPanelWidget(id="led_panel")
                    with Container(id="kpi_row"):
                        yield BLERSSICard(id="ble_rssi_card")
                        yield ActiveFXCard(id="active_fx_card")
                with Vertical(id="camera_column"):
                    yield CameraPanelWidget(id="camera_panel")
                    yield CameraFramesCard(id="camera_frames_card")
                yield MotionSection(id="motion_summary")
                
                # Optical Flow e Console ora fanno parte della griglia principale
                yield OpticalFlowGridWidget(id="optical_flow")
                with Vertical(id="console_column"):
                    with VerticalScroll(id="console_scroll"):
                        yield ConsoleWidget(id="console_log")
                    yield CommandInputWidget()

        yield Footer()

    def on_mount(self) -> None:
        """Inizializza dopo mount"""
        self.header_widget = self.query_one("#header", HeaderWidget)
        self.led_widget = self.query_one("#led_panel", LEDPanelWidget)
        self.camera_widget = self.query_one("#camera_panel", CameraPanelWidget)
        self.stats_grid = self.query_one("#stats_grid", Container)
        self.motion_section = self.query_one("#motion_summary", MotionSection)
        self.optical_widget = self.query_one("#optical_flow", OpticalFlowGridWidget)
        self.console_widget = self.query_one("#console_log", ConsoleWidget)
        self.console_scroll = self.query_one("#console_scroll", VerticalScroll)
        self.rssi_card = self.query_one("#ble_rssi_card", BLERSSICard)
        self.fx_card = self.query_one("#active_fx_card", ActiveFXCard)
        self.camera_frames_card = self.query_one("#camera_frames_card", CameraFramesCard)
        self.kpi_row = self.query_one("#kpi_row", Container)
        self.main_scroll = self.query_one("#main_body", VerticalScroll)

        # Setup callbacks
        self.client.state_callback = self._on_led_update
        self.client.camera_callback = self._on_camera_update
        self.client.motion_callback = self._on_motion_update
        self.client.motion_event_callback = self._on_motion_event

        self._log("Dashboard initialized. Press F1 for help.", "green")
        self._log("Quick start: Ctrl+S to scan and connect", "cyan")
        self._update_responsive_layout(self.size.width)

    def on_resize(self, event: events.Resize) -> None:
        """Aggiorna classi responsive al resize"""
        self._update_responsive_layout(event.size.width)

    def _update_responsive_layout(self, width: int) -> None:
        """Applica classi CSS in base alla larghezza corrente - i widget mantengono altezza fissa e si impilano"""

        # Stats grid: cambia solo il numero di colonne, non l'altezza dei widget
        if self.stats_grid:
            self.stats_grid.remove_class("cols-2", "cols-1", "cols-3")
            if width < 120:
                # Terminale stretto: 1 colonna (widget impilati verticalmente)
                self.stats_grid.add_class("cols-1")
            elif width < 170:
                # Terminale medio: 2 colonne
                self.stats_grid.add_class("cols-2")
            else:
                # Terminale largo: 3 colonne (default)
                self.stats_grid.add_class("cols-3")

        # Motion summary: adatta il numero di colonne
        if self.motion_section:
            self.motion_section.remove_class("cols-1")
            if width < 140:
                # Terminale stretto: 1 colonna (motion cards impilate)
                self.motion_section.add_class("cols-1")
            # else: 3 colonne (default dal CSS)

        # KPI row (RSSI + FX cards): adatta il numero di colonne
        if self.kpi_row:
            self.kpi_row.remove_class("cols-1")
            if width < 100:
                # Terminale molto stretto: 1 colonna
                self.kpi_row.add_class("cols-1")
            # else: 2 colonne (default dal CSS)

    def _log(self, message: str, style: str = "white") -> None:
        """Scrive sul log in modo thread-safe"""
        if not self.console_widget:
            return

        def _write():
            self.console_widget.add_log(message, style)
            if self.console_scroll:
                self.console_scroll.scroll_end(animate=False)

        try:
            self.call_from_thread(_write)
        except Exception:
            _write()

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
                self._log("Shutting down...", "yellow")
                await self.client.disconnect()
                self.exit()

            elif cmd == "scan":
                auto = bool(args and args[0].lower() == "auto")
                self._log("Scanning for devices...", "cyan")
                devices = await self.client.scan()
                self.device_rssi_map = {dev.address: getattr(dev, "rssi", None) for dev in devices if getattr(dev, "address", None)}

                if not devices:
                    self._log("No LedSaber devices found", "yellow")
                    return

                if auto and len(devices) == 1:
                    device = devices[0]
                    await self._connect_device(device.address, device.name, getattr(device, "rssi", None))
                else:
                    for dev in devices:
                        self._log(f"Found: {dev.name} ({dev.address})", "green")

            elif cmd == "connect":
                if not args:
                    self._log("Usage: connect <address>", "red")
                    return
                cached_rssi = self.device_rssi_map.get(args[0])
                await self._connect_device(args[0], "LedSaber", cached_rssi)

            elif cmd == "disconnect":
                await self.client.disconnect()
                self.header_widget.connected = False
                self.header_widget.device_name = "NO DEVICE"
                if self.rssi_card:
                    self.rssi_card.connected = False
                self._log("Disconnected", "yellow")

            # === LED COMMANDS ===
            elif cmd == "color":
                if len(args) < 3:
                    self._log("Usage: color <r> <g> <b>", "red")
                    return
                r, g, b = int(args[0]), int(args[1]), int(args[2])
                await self.client.set_color(r, g, b)
                self._log(f"Color set: RGB({r},{g},{b})", "magenta")

            elif cmd == "effect":
                if not args:
                    self._log("Usage: effect <mode> [speed]", "red")
                    return
                mode = args[0]
                speed = int(args[1]) if len(args) > 1 else 150
                await self.client.set_effect(mode, speed)
                self._log(f"Effect: {mode} @ {speed}ms", "magenta")

            elif cmd == "brightness":
                if not args:
                    self._log("Usage: brightness <0-255>", "red")
                    return
                brightness = int(args[0])
                await self.client.set_brightness(brightness, True)
                self._log(f"Brightness: {brightness}", "magenta")

            elif cmd == "on":
                await self.client.set_brightness(255, True)
                self._log("LED ON", "green")

            elif cmd == "off":
                await self.client.set_brightness(0, False)
                self._log("LED OFF", "red")

            # === CAMERA COMMANDS ===
            elif cmd == "cam":
                if not args:
                    self._log("Usage: cam <init|start|stop|status>", "red")
                    return

                subcmd = args[0].lower()

                if subcmd == "init":
                    await self.client.camera_send_command("init")
                    self._log("Camera init sent", "green")
                    await asyncio.sleep(1)
                    status = await self.client.get_camera_status()
                    if status:
                        self._log(f"Camera initialized: {status.get('initialized', False)}", "cyan")

                elif subcmd == "start":
                    await self.client.camera_send_command("start")
                    self._log("Camera start sent", "green")

                elif subcmd == "stop":
                    await self.client.camera_send_command("stop")
                    self._log("Camera stop sent", "yellow")

                elif subcmd == "status":
                    status = await self.client.get_camera_status()
                    if status:
                        self._log(f"Cam: init={status.get('initialized')} active={status.get('active')} fps={status.get('fps', 0):.1f}", "cyan")
                    else:
                        self._log("No camera status available", "yellow")

                else:
                    self._log(f"Unknown cam command: {subcmd}", "red")

            # === MOTION COMMANDS ===
            elif cmd == "motion":
                if not args:
                    self._log("Usage: motion <enable|disable|status|sensitivity N>", "red")
                    return

                subcmd = args[0].lower()

                if subcmd == "enable":
                    await self.client.motion_send_command("enable")
                    self._log("Motion detection enabled", "green")

                elif subcmd == "disable":
                    await self.client.motion_send_command("disable")
                    self._log("Motion detection disabled", "yellow")

                elif subcmd == "status":
                    status = await self.client.get_motion_status()
                    if status:
                        self._log(f"Motion: enabled={status.get('enabled')} detected={status.get('motionDetected')} intensity={status.get('intensity')}", "cyan")
                    else:
                        self._log("No motion status available", "yellow")

                elif subcmd == "sensitivity":
                    if len(args) < 2:
                        self._log("Usage: motion sensitivity <0-255>", "red")
                        return
                    sensitivity = int(args[1])
                    await self.client.motion_send_command(f"sensitivity {sensitivity}")
                    self._log(f"Sensitivity set: {sensitivity}", "green")

                else:
                    self._log(f"Unknown motion command: {subcmd}", "red")

            # === HELP ===
            elif cmd == "help":
                self._log("Commands: scan, connect, disconnect, color, effect, brightness, on, off", "cyan")
                self._log("          cam <init|start|stop|status>, motion <enable|disable|status|sensitivity N>", "cyan")
                self._log("Shortcuts: Ctrl+S=scan, Ctrl+D=disconnect, F2=cam init, F3=start, F4=stop, F5=motion toggle", "cyan")

            else:
                self._log(f"Unknown command: {cmd}. Type 'help' for list.", "red")

        except Exception as e:
            self._log(f"Error: {e}", "red")

    async def _connect_device(self, address: str, name: str, rssi: Optional[int] = None):
        """Connette a dispositivo"""
        self._log(f"Connecting to {address}...", "cyan")
        success = await self.client.connect(address)

        if success:
            self.header_widget.connected = True
            self.header_widget.device_address = address
            self.header_widget.device_name = name
            self._log(f"Connected to {name}", "green")
            if self.rssi_card:
                self.rssi_card.connected = True
                if rssi is None:
                    rssi = self.device_rssi_map.get(address)
                if rssi is not None:
                    self.rssi_card.rssi = rssi

            # Leggi stato iniziale
            state = await self.client.get_state()
            if state:
                self.led_widget.led_state = state
        else:
            self._log("Connection failed", "red")

    # ═══════════════════════════════════════════════════════════════════════
    # CALLBACKS
    # ═══════════════════════════════════════════════════════════════════════

    def _on_led_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback LED state"""
        self.led_widget.led_state = state
        if self.fx_card:
            self.fx_card.effect = state.get('effect', 'none')
            self.fx_card.speed = state.get('speed', 0)

        if not is_first and changes:
            for key in changes:
                if key in ['r', 'g', 'b']:
                    r, g, b = state.get('r', 0), state.get('g', 0), state.get('b', 0)
                    self._log(f"LED → RGB({r},{g},{b})", "magenta")
                    break
                elif key == 'effect':
                    self._log(f"LED → Effect: {changes[key]['new']}", "magenta")
                elif key == 'brightness':
                    self._log(f"LED → Brightness: {changes[key]['new']}", "magenta")

    def _on_camera_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback camera state"""
        self.camera_widget.camera_state = state
        if self.camera_frames_card:
            self.camera_frames_card.total_frames = state.get('totalFrames', 0)
            self.camera_frames_card.active = state.get('active', False)

        if not is_first and changes:
            for key, change in changes.items():
                if key == 'fps':
                    self._log(f"Camera → FPS: {change['new']:.1f}", "green")
                elif key == 'active':
                    status = "ACTIVE" if change['new'] else "STOPPED"
                    self._log(f"Camera → {status}", "green")

    def _on_motion_update(self, state: Dict, is_first: bool = False, changes: Dict = None):
        """Callback motion state"""
        if self.motion_section:
            self.motion_section.motion_state = state
        if self.optical_widget:
            self.optical_widget.motion_state = state

        if not is_first and changes:
            for key, change in changes.items():
                if key == 'motionDetected':
                    status = "DETECTED" if change['new'] else "ENDED"
                    self._log(f"Motion {status}", "yellow")
                elif key == 'gesture':
                    gesture = change['new']
                    confidence = state.get('gestureConfidence', 0)
                    if gesture != 'none' and confidence > 50:
                        self._log(f"⚔ GESTURE: {gesture.upper()} ({confidence}%)", "magenta")
                elif key == 'shakeDetected' and change['new']:
                    self._log("⚠ SHAKE DETECTED!", "red")

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
        self._log(f"{emoji} {event_type.upper()} (I:{intensity})", "yellow")

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
        enabled = False
        if self.motion_section:
            enabled = self.motion_section.motion_state.get('enabled', False)
        if enabled:
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
