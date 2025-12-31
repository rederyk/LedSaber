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
import time
from datetime import datetime
from typing import Optional, Dict, List
from collections import deque

from textual import events, on
from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, Vertical, VerticalScroll
from textual.widgets import Static, Input, Footer, RichLog, TextArea
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
    firmware_version: reactive[str] = reactive("")

    def render(self):
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

        if self.connected:
            ble_text.append("◉ ", style="green")
            ble_text.append(self.device_name, style="white")
            if self.firmware_version:
                ble_text.append(" v", style="dim")
                ble_text.append(self.firmware_version, style="yellow")
        else:
            ble_text.append("● ", style="red")
            ble_text.append("DISCONNECTED", style="white")

        # Detail row (indirizzo MAC)
        ble_detail = Text()
        if self.connected and self.device_address:
            ble_detail.append(self.device_address, style="dim")
        else:
            ble_detail.append("No device", style="dim")

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
        status_icon = Text("◉ ON", style="bold green") if enabled else Text("● OFF", style="bold red")

        # RGB values with colors
        rgb_bar = Text()
        rgb_bar.append("R:", style="red")
        rgb_bar.append(f"{r:3d}", style="red bold")
        rgb_bar.append("  ")
        rgb_bar.append("G:", style="green")
        rgb_bar.append(f"{g:3d}", style="green bold")
        rgb_bar.append("  ")
        rgb_bar.append("B:", style="blue")
        rgb_bar.append(f"{b:3d}", style="blue bold")

        # Brightness indicator
        bright_pct = int((brightness / 255) * 20)
        bright_bar = Text()
        bright_bar.append("█" * bright_pct, style="yellow")
        bright_bar.append("░" * (20 - bright_pct), style="dim")

        content = Text()
        content.append(status_icon)
        content.append("  │  ")
        content.append(rgb_bar)
        content.append("  │  ")
        content.append("Bright:", style="cyan")
        content.append(f"{brightness:3d} ", style="cyan bold")
        content.append(bright_bar)
        content.append("  │  FX: ", style="")
        content.append(effect, style="magenta")

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

    def _sparkline(self, data: List[float], max_val: float = 30.0) -> Text:
        if not data:
            return Text("░" * 40, style="dim")

        blocks = "▁▂▃▄▅▆▇█"
        sparkline = Text()
        for val in data:
            idx = int((val / max_val) * (len(blocks) - 1))
            idx = max(0, min(len(blocks) - 1, idx))
            block = blocks[idx]
            style = "green" if val > 20 else "yellow" if val > 10 else "red"
            sparkline.append(block, style=style)

        return sparkline

    def render(self):
        state = self.camera_state or {}

        initialized = state.get('initialized', False)
        active = state.get('active', False)
        fps = state.get('fps', 0.0)
        total_frames = state.get('totalFrames', 0)

        status_icon = Text("◉ ACTIVE", style="bold green") if active else Text("◎ IDLE", style="yellow")
        init_icon = Text("✓", style="green") if initialized else Text("✗", style="red")

        sparkline = self._sparkline(list(self.fps_history))

        content = Text()
        content.append(status_icon)
        content.append("  │  ")
        content.append("Init:", style="")
        content.append(init_icon)
        content.append("  │  ")
        content.append("FPS:", style="")
        content.append(f"{fps:5.1f}  ", style="bold yellow")
        content.append("│  ")
        content.append(sparkline)
        content.append("  │  ")
        content.append("Frames:", style="dim")
        content.append(f"{total_frames}", style="dim")

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
        quality = state.get('quality', 0)
        gi = state.get('gestureIgnitionIntensity')
        gr = state.get('gestureRetractIntensity')
        gc = state.get('gestureClashIntensity')
        map_up = state.get('effectMapUp')
        map_down = state.get('effectMapDown')
        map_left = state.get('effectMapLeft')
        map_right = state.get('effectMapRight')
        clash_fx = state.get('gestureClashEffect')
        clash_ms = state.get('gestureClashDurationMs')

        status_icon = "[green]◉ ENABLED[/]" if enabled else "[red]● DISABLED[/]"
        motion_icon = "[yellow]⚡[/]" if motion_detected else "[dim]○[/]"
        shake_icon = "[red blink]⚠[/]" if shake_detected else "[dim]·[/]"

        table = Table.grid(padding=(0, 1))
        table.add_column(justify="left")
        table.add_row(f"Status: {status_icon}")
        table.add_row(f"Qual:   [cyan]{quality}[/]")
        table.add_row(f"Motion: {motion_icon}")
        table.add_row(f"Shake: {shake_icon}")
        if gi is not None or gr is not None or gc is not None:
            gi_val = "?" if gi is None else f"{gi}"
            gr_val = "?" if gr is None else f"{gr}"
            gc_val = "?" if gc is None else f"{gc}"
            table.add_row(f"G-I/R/C: [dim]{gi_val}/{gr_val}/{gc_val}[/]")
        if map_up or map_down or map_left or map_right:
            up_val = "?" if map_up is None else f"{map_up}"
            down_val = "?" if map_down is None else f"{map_down}"
            left_val = "?" if map_left is None else f"{map_left}"
            right_val = "?" if map_right is None else f"{map_right}"
            table.add_row(f"Map U/D/L/R: [dim]{up_val}/{down_val}/{left_val}/{right_val}[/]")
        if clash_fx or clash_ms:
            fx_val = "?" if clash_fx is None else f"{clash_fx}"
            ms_val = "?" if clash_ms is None else f"{clash_ms}ms"
            table.add_row(f"Clash FX: [dim]{fx_val} {ms_val}[/]")

        return Panel(
            table,
            title="[bold magenta]⚡ STATUS[/]",
            border_style="magenta",
            box=box.ROUNDED,
            height=9
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
        centroid_valid = state.get('centroidValid', False)
        centroid_x = state.get('centroidX', 0.0)
        centroid_y = state.get('centroidY', 0.0)
        centroid_norm_x = state.get('centroidNormX', 0.0)
        centroid_norm_y = state.get('centroidNormY', 0.0)
        centroid_norm_available = ("centroidNormX" in state) or ("centroidNormY" in state)
        trail = state.get('trail', []) or []

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

        if centroid_valid:
            centroid_line = f"[dim]C: {centroid_x:.1f},{centroid_y:.1f}[/]"
            if centroid_norm_available:
                centroid_norm_line = f"[dim]N: {centroid_norm_x:.2f},{centroid_norm_y:.2f}[/]"
            else:
                centroid_norm_line = "[dim]N: --[/]"
        else:
            centroid_line = "[dim]C: --[/]"
            centroid_norm_line = "[dim]N: --[/]"

        trail_points = []
        for point in trail[-3:]:
            try:
                trail_points.append(f"{point.get('x', 0):.2f},{point.get('y', 0):.2f}")
            except Exception:
                continue
        trail_line = "[dim]Trail: --[/]" if not trail_points else f"[dim]Trail: {' -> '.join(trail_points)}[/]"

        table = Table.grid(padding=(0, 1), expand=True)
        table.add_column(justify="center")
        table.add_row(f"[cyan bold]{dir_arrow}[/] [cyan]{direction.upper()}[/]")
        table.add_row("─" * 15)
        table.add_row(gesture_display)
        table.add_row(centroid_line)
        table.add_row(centroid_norm_line)
        table.add_row(trail_line)

        return Panel(
            table,
            title="[bold cyan]🧭 DIRECTION[/]",
            border_style="cyan",
            box=box.ROUNDED,
            height=10
        )


class MotionSection(Container):
    """Container responsive per widget Motion summary + Device Info + Chrono Themes"""

    motion_state: reactive[Dict] = reactive({})

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.status_card = MotionStatusCard(id="motion_status_card")
        self.intensity_card = MotionIntensityCard(id="motion_intensity_card")
        self.direction_card = MotionDirectionCard(id="motion_direction_card")
        self.chrono_themes_card = ChronoThemesCard(id="chrono_themes_card")
        self.device_info_card = DeviceInfoCard(id="device_info_card")

    def compose(self) -> ComposeResult:
        yield self.status_card
        yield self.intensity_card
        yield self.direction_card
        yield self.chrono_themes_card
        yield self.device_info_card

    def watch_motion_state(self, new_state: Dict):
        self.status_card.motion_state = new_state or {}
        self.intensity_card.motion_state = new_state or {}
        self.direction_card.motion_state = new_state or {}


class OpticalFlowCanvas(Static):
    """Widget griglia optical flow - parte grafica (ex OpticalFlowGridWidget)"""

    motion_state: reactive[Dict] = reactive({})

    def render(self):
        state = self.motion_state or {}

        raw_rows = state.get('grid', [])
        grid_cols = state.get('gridCols') or (len(raw_rows[0]) if raw_rows else 8)
        grid_rows_count = state.get('gridRows') or (len(raw_rows) if raw_rows else 8)
        grid_cols = max(1, grid_cols)
        grid_rows_count = max(1, grid_rows_count)
        block_size = state.get('blockSize', 30)  # 240/8 = 30px per blocco
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
            'X': 'red',
            '*': 'red',
            'O': 'bright_red',
            'o': 'dim red',
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

        # Apply centroid trail overlay (latest point has higher priority)
        trail = state.get('trail', []) or []
        if trail:
            grid_chars = [list(row) for row in normalized_rows]
            max_points = 5
            trail_points = trail[-max_points:]
            for idx, point in enumerate(trail_points):
                try:
                    tx = float(point.get('x', 0.0))
                    ty = float(point.get('y', 0.0))
                except Exception:
                    continue
                col = int(tx * grid_cols)
                row = int(ty * grid_rows_count)
                if col < 0:
                    col = 0
                elif col >= grid_cols:
                    col = grid_cols - 1
                if row < 0:
                    row = 0
                elif row >= grid_rows_count:
                    row = grid_rows_count - 1
                age = len(trail_points) - 1 - idx
                if age == 0:
                    marker = "*"
                elif age == 1:
                    marker = "O"
                else:
                    marker = "o"
                if grid_chars[row][col] != "X":
                    grid_chars[row][col] = marker
            normalized_rows = ["".join(row) for row in grid_chars]

        # Calculate available space
        width = self.size.width or 120
        height = self.size.height or 40

        # Reserve space for borders/padding (riduciamo per massimizzare lo spazio)
        # Horizontal: ~2 chars (border + padding)
        usable_width = max(10, width - 2)
        # Vertical: ~2 lines (border minimo)
        usable_height = max(4, height - 2)

        # I caratteri terminale sono più alti che larghi
        # Per proporzioni visive corrette: h_gap deve essere maggiore di v_gap
        # Tipicamente i caratteri hanno aspect ratio height/width ~ 2.0-2.5
        # Ma per compensare la spaziatura visiva usiamo un valore più alto
        CHAR_ASPECT_RATIO = 2.2

        # Calcola gap per riempire un'area QUADRATA basata sul lato più corto
        if grid_cols > 1:
            max_h_gap = (usable_width - grid_cols) // (grid_cols - 1)
        else:
            max_h_gap = usable_width

        if grid_rows_count > 1:
            max_v_gap = (usable_height - grid_rows_count) // (grid_rows_count - 1)
        else:
            max_v_gap = usable_height

        # Usa lo spazio disponibile su entrambi gli assi e rispetta l'aspect ratio
        h_gap_candidate = max(1, max_h_gap)
        v_gap_candidate = max(0, max_v_gap)
        v_gap_from_h = int(round((1 + h_gap_candidate) / CHAR_ASPECT_RATIO - 1))
        if v_gap_from_h <= v_gap_candidate:
            h_gap = h_gap_candidate
            v_gap = max(0, v_gap_from_h)
        else:
            v_gap = v_gap_candidate
            h_gap = int(round((1 + v_gap) * CHAR_ASPECT_RATIO - 1))
            h_gap = max(1, min(h_gap, max_h_gap))

        cell_gap = " " * h_gap
        row_gap = "\n"

        grid_lines: List[str] = []
        spacer_len = max(1, grid_cols + (grid_cols - 1) * h_gap)
        spacer_line = " " * spacer_len
        for idx, row_str in enumerate(normalized_rows):
            colored_cells = []
            for char in row_str:
                color = symbol_colors.get(char, 'white')
                if not has_live_data and char == '.':
                    color = "dim"
                colored_cells.append(f"[{color}]{char}[/]")
            row_line = cell_gap.join(colored_cells)
            grid_lines.append(row_line)
            if idx < len(normalized_rows) - 1:
                for _ in range(v_gap):
                    grid_lines.append(spacer_line)

        grid_markup = row_gap.join(grid_lines)
        grid_text = Text.from_markup(grid_markup)
        grid_text.no_wrap = True
        grid_text.overflow = "ignore"

        legend = Table.grid(expand=True, padding=0)
        legend.add_column(justify="left")
        legend_text = Text()
        legend_text.append("Legend: ", style="dim")
        legend_text.append(". idle  ", style="dim white")
        legend_text.append("^ v up/down  ", style="cyan")
        legend_text.append("< > left/right  ", style="yellow")
        legend_text.append("A B C D diagonals", style="green")
        legend_text.append("  X centroid", style="red")
        legend_text.append("  o O * trail", style="red")
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


class OpticalFlowGridWidget(Container):
    """Container principale per sezione Optical Flow (Cards + Grid)"""

    motion_state: reactive[Dict] = reactive({})

    def compose(self) -> ComposeResult:
        yield MotionSection(id="motion_summary")
        yield OpticalFlowCanvas(id="optical_canvas")

    def watch_motion_state(self, new_state: Dict):
        try:
            self.query_one("#optical_canvas", OpticalFlowCanvas).motion_state = new_state
        except Exception:
            pass


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


class ChronoThemesCard(Static):
    """Card per selezione temi Chrono - stile cyberpunk interattivo"""

    chrono_hour_theme: reactive[int] = reactive(0)
    chrono_second_theme: reactive[int] = reactive(0)
    wellness_mode: reactive[bool] = reactive(False)
    breathing_rate: reactive[int] = reactive(5)
    connected: reactive[bool] = reactive(False)

    HOUR_THEMES = ["Classic", "Neon", "Plasma", "Digital", "Inferno", "Storm", "Circadian", "Forest", "Ocean", "Ember", "Moon", "Aurora"]
    SECOND_THEMES = ["Classic", "Spiral", "Fire", "Lightning", "Particle", "Quantum"]

    def render(self):
        if not self.connected:
            content = Text("Connect to\nconfigure", style="dim", justify="center")
        else:
            # Nome temi correnti
            hour_name = self.HOUR_THEMES[self.chrono_hour_theme % len(self.HOUR_THEMES)]
            second_name = self.SECOND_THEMES[self.chrono_second_theme % len(self.SECOND_THEMES)]

            # Icone tematiche
            hour_icons = ["◎", "◉", "◈", "◆", "🔥", "⚡", "🌅", "🌲", "🌊", "🔥", "🌙", "🌌"]
            second_icons = ["○", "◐", "🔥", "⚡", "●", "◯"]

            hour_icon = hour_icons[self.chrono_hour_theme % len(hour_icons)]
            second_icon = second_icons[self.chrono_second_theme % len(second_icons)]

            table = Table.grid(padding=(0, 1))
            table.add_column(justify="left")

            # Hours row con frecce
            hours_text = Text()
            hours_text.append("⏰ ", style="yellow")
            hours_text.append("◀ ", style="dim cyan")
            hours_text.append(f"{hour_icon} {hour_name}", style="cyan bold")
            hours_text.append(" ▶", style="dim cyan")
            table.add_row(hours_text)

            # Seconds row con frecce
            seconds_text = Text()
            seconds_text.append("⏱  ", style="magenta")
            seconds_text.append("◀ ", style="dim magenta")
            seconds_text.append(f"{second_icon} {second_name}", style="magenta bold")
            seconds_text.append(" ▶", style="dim magenta")
            table.add_row(seconds_text)

            # Wellness mode indicator
            if self.wellness_mode:
                wellness_text = Text()
                wellness_text.append("🧘 ", style="green")
                wellness_text.append(f"Wellness ({self.breathing_rate} BPM)", style="green bold")
                table.add_row(wellness_text)

            # Hint
            hint = "F6/F7 cycle | F11 wellness" if self.chrono_hour_theme >= 6 else "F6/F7 to cycle"
            table.add_row(Text(hint, style="dim", justify="center"))

            content = table

        return Panel(
            Align.center(content, vertical="middle"),
            title="[bold yellow]🎨 CHRONO[/]",
            border_style="yellow",
            box=box.ROUNDED,
            height=7
        )


class DeviceInfoCard(Static):
    """Card con informazioni sul device: FW version, MAC, uptime, tempo sincronizzato, etc."""

    firmware_version: reactive[str] = reactive("")
    device_address: reactive[str] = reactive("")
    connected: reactive[bool] = reactive(False)
    mtu: reactive[int] = reactive(0)
    time_synced: reactive[bool] = reactive(False)
    sync_time_str: reactive[str] = reactive("")

    def render(self):
        if not self.connected:
            content = Text("No device\nconnected", style="dim", justify="center")
        else:
            content = Text()

            if self.firmware_version:
                content.append("FW: ", style="yellow")
                content.append(f"{self.firmware_version}", style="bold white")
                content.append("\n")

            if self.device_address:
                mac_short = self.device_address[-8:] if len(self.device_address) > 8 else self.device_address
                content.append("MAC: ", style="cyan")
                content.append(f"{mac_short}", style="dim white")
                content.append("\n")

            if self.mtu > 0:
                content.append("MTU: ", style="green")
                content.append(f"{self.mtu}", style="dim white")
                content.append("\n")

            # Time sync status
            if self.time_synced:
                content.append("Time: ", style="magenta")
                content.append("✓ ", style="green")
                if self.sync_time_str:
                    content.append(f"{self.sync_time_str}", style="dim white")
                else:
                    content.append("synced", style="dim white")
            else:
                content.append("Time: ", style="magenta")
                content.append("✗ ", style="red dim")
                content.append("not synced", style="red dim")

            if not content.plain:
                content = Text("Loading...", style="dim", justify="center")

        return Panel(
            Align.center(content, vertical="middle"),
            title="[cyan]📱 Device Info[/]",
            border_style="cyan",
            box=box.ROUNDED,
            height=7
        )


class ConsoleWidget(TextArea):
    """Console per log con TextArea - testo completamente selezionabile e copiabile

    Keybindings:
    - F8: Copia la selezione
    - F9: Copia tutto il log
    - Ctrl+A: Seleziona tutto
    """

    BINDINGS = [
        Binding("f8", "copy_selection", "Copy Selection", show=True),
        Binding("f9", "copy_all", "Copy All", show=True),
        Binding("ctrl+a", "select_all_text", "Select All", show=False),
    ]

    def __init__(self, **kwargs):
        super().__init__(
            text="",
            language="",
            theme="monokai",
            read_only=True,
            show_line_numbers=False,
            **kwargs
        )
        self.log_lines: deque = deque(maxlen=400)

    def add_log(self, message: str, style: str = "white"):
        """Aggiungi messaggio al log - senza markup Rich per garantire selezione"""
        timestamp = datetime.now().strftime("%H:%M:%S")

        # Mappa stili a simboli ASCII per mantenere colori visibili come testo
        style_prefix = {
            "green": "✓",
            "cyan": "ℹ",
            "yellow": "⚠",
            "red": "✗",
            "magenta": "★"
        }.get(style, "•")

        line = f"{timestamp} {style_prefix} {message}"
        self.log_lines.appendleft(line)

        # Aggiorna il contenuto completo
        full_text = "\n".join(self.log_lines)
        self.load_text(full_text)

        # Scroll alla fine
        self.scroll_home(animate=False)

    def action_copy_selection(self) -> None:
        """Copia il testo selezionato nella clipboard di sistema"""
        selected_text = self.selected_text
        if selected_text:
            self.app.copy_to_clipboard(selected_text)
            self.app.notify("Selection copied!", severity="information", timeout=1.5)
        else:
            self.app.notify("No text selected. Use Ctrl+A to select all, or F9 to copy all.",
                          severity="warning", timeout=2)

    def action_copy_all(self) -> None:
        """Copia tutto il log nella clipboard"""
        if self.text:
            self.app.copy_to_clipboard(self.text)
            lines_count = len(self.log_lines)
            self.app.notify(f"All log copied! ({lines_count} lines)",
                          severity="information", timeout=1.5)
        else:
            self.app.notify("Log is empty.", severity="warning", timeout=1.5)

    def action_select_all_text(self) -> None:
        """Seleziona tutto il testo"""
        self.select_all()
        self.app.notify("All text selected. Press F8 to copy selection.",
                       severity="information", timeout=1.5)


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

    /* Prima riga 2x (1/2), seconda riga 2x (1/2) su desktop */
    #led_column,
    #camera_column {
        column-span: 3;
    }

    #motion_summary {
        column-span: 3;
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
        grid-size: 5;
        grid-columns: 1fr 1fr 1fr 1fr 1fr;
        padding-left: 1;
        height: auto;
        margin: 0 0 1 0;
    }

    #motion_summary.cols-3 {
        grid-size: 5;
        grid-columns: 1fr 1fr 1fr 1fr 1fr;
    }

    #motion_summary.cols-2 {
        grid-size: 3;
        grid-columns: 1fr 1fr 1fr;
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
        height: auto;
        min-height: 22;
        margin: 0;
        padding: 1;
        border: round purple;
    }

    #optical_canvas {
        width: 100%;
        height: auto;
        min-height: 16;
        margin: 0;
        padding: 0;
    }

    #console_column {
        width: 100%;
        min-width: 20;
        layout: vertical;
        height: auto;
        max-height: 1fr;
    }

    #console_log {
        background: $surface;
        border: round cyan;
        padding: 1;
        height: auto;
        min-height: 15;
        margin: 0;
    }

    #cmd_input {
        border: solid cyan;
        background: $surface;
        margin: 0 0 0 0;
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
    #motion_direction_card,
    #chrono_themes_card,
    #device_info_card {
        margin: 0;
    }

    #motion_status_card,
    #motion_intensity_card,
    #motion_direction_card,
    #chrono_themes_card {
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
        Binding("f6", "chrono_cycle_hours", "Chrono Hours"),
        Binding("f7", "chrono_cycle_seconds", "Chrono Seconds"),
        Binding("f8", "device_ignition", "Ignition"),
        Binding("f9", "device_retract", "Retract"),
        Binding("f10", "toggle_motion_boot", "Motion Boot"),
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
        self.rssi_card: Optional[BLERSSICard] = None
        self.fx_card: Optional[ActiveFXCard] = None
        self.camera_frames_card: Optional[CameraFramesCard] = None
        self.chrono_themes_card: Optional[ChronoThemesCard] = None
        self.device_info_card: Optional[DeviceInfoCard] = None
        self.device_rssi_map: Dict[str, Optional[int]] = {}
        self.stats_grid: Optional[Container] = None
        self.kpi_row: Optional[Container] = None
        self.main_scroll: Optional[VerticalScroll] = None
        self.motion_config: Dict = {}
        self.last_motion_state: Dict = {}
        self.last_led_state: Dict = {}
        self.last_motion_status_ts: float = 0.0
        self.motion_poll_in_flight: bool = False

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
                
                # Optical Flow e Console ora fanno parte della griglia principale
                yield OpticalFlowGridWidget(id="optical_flow")
                with Vertical(id="console_column"):
                    yield CommandInputWidget()
                    yield ConsoleWidget(id="console_log")

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
        self.rssi_card = self.query_one("#ble_rssi_card", BLERSSICard)
        self.fx_card = self.query_one("#active_fx_card", ActiveFXCard)
        self.camera_frames_card = self.query_one("#camera_frames_card", CameraFramesCard)
        self.chrono_themes_card = self.query_one("#chrono_themes_card", ChronoThemesCard)
        self.device_info_card = self.query_one("#device_info_card", DeviceInfoCard)
        self.kpi_row = self.query_one("#kpi_row", Container)
        self.main_scroll = self.query_one("#main_body", VerticalScroll)

        # Setup callbacks
        self.client.state_callback = self._on_led_update
        self.client.camera_callback = self._on_camera_update
        self.client.motion_callback = self._on_motion_update
        self.client.motion_event_callback = self._on_motion_event

        self._log("Dashboard initialized. Press F1 for help.", "green")
        self._log("Quick start: Ctrl+S to scan and connect", "cyan")
        self._log("Console: Click to focus, Ctrl+A=select all, F8=copy selection, F9=copy all", "cyan")
        self._update_responsive_layout(self.size.width)
        self.set_interval(0.7, self._poll_motion_status)

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

        # Motion summary: adatta il numero di colonne (ora 5 card)
        if self.motion_section:
            self.motion_section.remove_class("cols-1", "cols-2", "cols-3")
            if width < 140:
                # Terminale stretto: 1 colonna (motion cards impilate)
                self.motion_section.add_class("cols-1")
            elif width < 200:
                # Terminale medio: 3 colonne
                self.motion_section.add_class("cols-2")
            else:
                # Terminale largo: 5 colonne (default)
                self.motion_section.add_class("cols-3")

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
            # RichLog ha auto_scroll=True, non serve scroll_end manuale

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

            elif cmd == "chrono":
                if not args:
                    self._log("Usage: chrono <hour_theme> <second_theme>", "red")
                    self._log("Hour themes: 0=Classic, 1=Neon, 2=Plasma, 3=Digital", "cyan")
                    self._log("Second themes: 0=Classic, 1=Spiral, 2=Fire, 3=Lightning, 4=Particle, 5=Quantum", "cyan")
                    return

                hour_theme = int(args[0]) if len(args) > 0 else 0
                second_theme = int(args[1]) if len(args) > 1 else 0

                # Invia comando al dispositivo
                await self._send_chrono_themes(hour_theme, second_theme)

            elif cmd == "wellness":
                # wellness <on|off> [bpm]
                if not args:
                    self._log("Usage: wellness <on|off> [bpm]", "red")
                    self._log("BPM range: 2-8 (default 5)", "cyan")
                    return

                enable = args[0].lower() == "on"
                bpm = int(args[1]) if len(args) > 1 else 5

                await self._send_wellness_config(enable, bpm)

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
                    self._log("Usage: motion <enable|disable|onboot|status|config|quality N|motionmin N|speedmin N|ignitionmin N|retractmin N|clashmin N|isup G|isdown G|isleft G|isright G|clashfx FX [ms]>", "red")
                    return

                subcmd = args[0].lower()

                if subcmd == "enable":
                    await self.client.motion_send_command("enable")
                    self._log("Motion detection enabled", "green")
                    status = await self.client.get_motion_status()
                    if status:
                        self._on_motion_update(status, is_first=False, changes=None)

                elif subcmd == "disable":
                    await self.client.motion_send_command("disable")
                    self._log("Motion detection disabled", "yellow")
                    status = await self.client.get_motion_status()
                    if status:
                        self._on_motion_update(status, is_first=False, changes=None)

                elif subcmd == "onboot":
                    if len(args) < 2:
                        self._log("Usage: motion onboot <on|off>", "red")
                        return
                    val = args[1].lower()
                    if val not in ["on", "off"]:
                        self._log("Usage: motion onboot <on|off>", "red")
                        return
                    enabled = (val == "on")
                    await self.client.set_boot_config(motion_enabled=enabled)
                    self._log(f"Motion on boot set to: {enabled}", "green")

                elif subcmd == "status":
                    status = await self.client.get_motion_status()
                    if status:
                        self._log(f"Motion: enabled={status.get('enabled')} detected={status.get('motionDetected')} intensity={status.get('intensity')}", "cyan")
                    else:
                        self._log("No motion status available", "yellow")

                elif subcmd == "config":
                    config = await self.client.get_motion_config()
                    if config:
                        self._update_motion_config(config)
                        self._log(
                            "Motion config: "
                            f"quality={config.get('quality')} "
                            f"motionMin={config.get('motionIntensityMin')} "
                            f"speedMin={config.get('motionSpeedMin')} "
                            f"ignition={config.get('gestureIgnitionIntensity')} "
                            f"retract={config.get('gestureRetractIntensity')} "
                            f"clash={config.get('gestureClashIntensity')} "
                            f"mapU={config.get('effectMapUp')} "
                            f"mapD={config.get('effectMapDown')} "
                            f"mapL={config.get('effectMapLeft')} "
                            f"mapR={config.get('effectMapRight')}",
                            "cyan"
                        )
                    else:
                        self._log("No motion config available", "yellow")

                elif subcmd == "quality":
                    if len(args) < 2:
                        self._log("Usage: motion quality <0-255>", "red")
                        return
                    quality = int(args[1])
                    await self.client.motion_send_command(f"quality {quality}")
                    self._log(f"Quality set: {quality}", "green")

                elif subcmd == "motionmin":
                    if len(args) < 2:
                        self._log("Usage: motion motionmin <0-255>", "red")
                        return
                    min_intensity = int(args[1])
                    await self.client.motion_send_command(f"motionmin {min_intensity}")
                    self._log(f"Motion intensity min set: {min_intensity}", "green")

                elif subcmd == "speedmin":
                    if len(args) < 2:
                        self._log("Usage: motion speedmin <0-20>", "red")
                        return
                    min_speed = float(args[1])
                    await self.client.motion_send_command(f"speedmin {min_speed}")
                    self._log(f"Motion speed min set: {min_speed:.2f}", "green")

                elif subcmd == "ignitionmin":
                    if len(args) < 2:
                        self._log("Usage: motion ignitionmin <0-255>", "red")
                        return
                    value = int(args[1])
                    await self.client.set_motion_config(gesture_ignition_intensity=value)
                    self._set_motion_config_field("gestureIgnitionIntensity", value)
                    self._log(f"Ignition intensity min set: {value}", "green")

                elif subcmd == "retractmin":
                    if len(args) < 2:
                        self._log("Usage: motion retractmin <0-255>", "red")
                        return
                    value = int(args[1])
                    await self.client.set_motion_config(gesture_retract_intensity=value)
                    self._set_motion_config_field("gestureRetractIntensity", value)
                    self._log(f"Retract intensity min set: {value}", "green")

                elif subcmd == "clashmin":
                    if len(args) < 2:
                        self._log("Usage: motion clashmin <0-255>", "red")
                        return
                    value = int(args[1])
                    await self.client.set_motion_config(gesture_clash_intensity=value)
                    self._set_motion_config_field("gestureClashIntensity", value)
                    self._log(f"Clash intensity min set: {value}", "green")

                elif subcmd in ("isup", "isdown", "isleft", "isright"):
                    if len(args) < 2:
                        self._log("Usage: motion is<dir> <effect_id>", "red")
                        return
                    effect_id = args[1]
                    await self.client.motion_send_command(f"{subcmd} {effect_id}")
                    config_key = {
                        "isup": "effectMapUp",
                        "isdown": "effectMapDown",
                        "isleft": "effectMapLeft",
                        "isright": "effectMapRight",
                    }.get(subcmd)
                    if config_key:
                        self._set_motion_config_field(config_key, effect_id)
                    self._log(f"Effect map {subcmd[2:]} set: {effect_id}", "green")

                elif subcmd == "clashfx":
                    if len(args) < 2:
                        self._log("Usage: motion clashfx <effect_id> [duration_ms]", "red")
                        return
                    effect_id = args[1]
                    duration_ms = None
                    if len(args) > 2:
                        duration_ms = int(args[2])
                    payload = {"gestureClashEffect": effect_id}
                    if duration_ms is not None:
                        payload["gestureClashDurationMs"] = duration_ms
                    await self.client.set_effect_raw(payload)
                    self._set_motion_config_field("gestureClashEffect", effect_id)
                    if duration_ms is not None:
                        self._set_motion_config_field("gestureClashDurationMs", duration_ms)
                    self._log(f"Clash gesture effect set: {effect_id}", "green")

                else:
                    self._log(f"Unknown motion command: {subcmd}", "red")

            # === DEVICE CONTROL ===
            elif cmd == "ignition":
                await self.client.device_ignition()

            elif cmd == "retract":
                await self.client.device_retract()

            elif cmd == "reboot":
                self._log("Rebooting device...", "yellow")
                await self.client.device_reboot()
                await asyncio.sleep(1)
                self.exit()

            elif cmd == "sleep":
                self._log("Entering deep sleep...", "yellow")
                await self.client.device_sleep()
                await asyncio.sleep(1)
                self.exit()

            elif cmd == "effects":
                effects_list = await self.client.get_effects_list()
                if effects_list:
                    self._log(f"Available effects (v{effects_list.get('version', 'unknown')}):", "cyan")
                    for effect in effects_list.get('effects', []):
                        self._log(f"  {effect.get('icon', '🎨')} {effect['id']} - {effect['name']}", "cyan")
                else:
                    self._log("Could not retrieve effects list", "red")

            # === HELP ===
            elif cmd == "help":
                self._log("Commands: scan, connect, disconnect, color, effect, brightness, on, off", "cyan")
                self._log("          chrono <hour_theme> <second_theme> - Set chrono themes (0-11 for hours, 0-5 for seconds)", "cyan")
                self._log("          wellness <on|off> [bpm] - Toggle wellness mode (BPM: 2-8)", "cyan")
                self._log("          cam <init|start|stop|status>, motion <enable|disable|status|config|quality N|motionmin N|speedmin N|ignitionmin N|retractmin N|clashmin N|isup G|isdown G|isleft G|isright G|clashfx FX [ms]>", "cyan")
                self._log("          ignition, retract, reboot, sleep, effects, motion onboot <on|off>", "cyan")
                self._log("Shortcuts: Ctrl+S=scan, Ctrl+D=disconnect, F2=cam init, F3=start, F4=stop, F5=motion toggle", "cyan")
                self._log("           F6=cycle hour theme, F7=cycle second theme, F8=ignition, F9=retract, F10=toggle motion boot, F11=wellness toggle", "cyan")

            else:
                self._log(f"Unknown command: {cmd}. Type 'help' for list.", "red")

        except Exception as e:
            self._log(f"Error: {e}", "red")

    async def _send_chrono_themes(self, hour_theme: int, second_theme: int):
        """Invia i temi chrono al dispositivo"""
        try:
            # Aggiorna widget locale
            if self.chrono_themes_card:
                self.chrono_themes_card.chrono_hour_theme = hour_theme
                self.chrono_themes_card.chrono_second_theme = second_theme

            # Prepara comando JSON
            command = {
                "mode": "chrono_hybrid",
                "chronoHourTheme": hour_theme,
                "chronoSecondTheme": second_theme
            }

            # Invia via BLE
            await self.client.set_effect_raw(command)

            hour_name = ChronoThemesCard.HOUR_THEMES[hour_theme % len(ChronoThemesCard.HOUR_THEMES)]
            second_name = ChronoThemesCard.SECOND_THEMES[second_theme % len(ChronoThemesCard.SECOND_THEMES)]

            self._log(f"Chrono themes: {hour_name} + {second_name}", "yellow")
        except Exception as e:
            self._log(f"Failed to set chrono themes: {e}", "red")

    async def _send_wellness_config(self, enable: bool, bpm: int = 5):
        """Configura wellness mode"""
        try:
            # Aggiorna widget
            if self.chrono_themes_card:
                self.chrono_themes_card.wellness_mode = enable
                self.chrono_themes_card.breathing_rate = bpm

            # Comando JSON
            command = {
                "mode": "chrono_hybrid",
                "chronoWellnessMode": enable,
                "breathingRate": bpm
            }

            # Se wellness è ON, forza un tema wellness (Circadian di default)
            if enable and self.chrono_themes_card and self.chrono_themes_card.chrono_hour_theme < 6:
                command["chronoHourTheme"] = 6  # Circadian

            await self.client.set_effect_raw(command)

            mode_str = "ON" if enable else "OFF"
            self._log(f"Wellness mode: {mode_str} (breathing {bpm} BPM)", "green")
        except Exception as e:
            self._log(f"Failed to set wellness mode: {e}", "red")

    async def _connect_device(self, address: str, name: str, rssi: Optional[int] = None):
        """Connette a dispositivo"""
        self._log(f"Connecting to {address}...", "cyan")
        success = await self.client.connect(address)

        if success:
            self.header_widget.connected = True
            self.header_widget.device_address = address
            self.header_widget.device_name = name
            self._log(f"Connected to {name}", "green")

            # Aggiorna Device Info Card
            if self.device_info_card:
                self.device_info_card.connected = True
                self.device_info_card.device_address = address

            # Aggiorna Chrono Themes Card
            if self.chrono_themes_card:
                self.chrono_themes_card.connected = True

            # Aggiorna RSSI card
            if self.rssi_card:
                self.rssi_card.connected = True
                if rssi is None:
                    rssi = self.device_rssi_map.get(address)
                if rssi is not None:
                    self.rssi_card.rssi = rssi

            # Leggi versione firmware
            try:
                fw_version = await self.client.read_firmware_version()
                if fw_version:
                    self.header_widget.firmware_version = fw_version
                    if self.device_info_card:
                        self.device_info_card.firmware_version = fw_version
                    self._log(f"Firmware version: {fw_version}", "cyan")
            except Exception as e:
                self._log(f"Could not read firmware version: {e}", "yellow")

            # Prova a leggere MTU
            try:
                if hasattr(self.client.client, 'mtu_size'):
                    mtu = self.client.client.mtu_size
                    if self.device_info_card and mtu > 0:
                        self.device_info_card.mtu = mtu
            except Exception:
                pass

            # Sincronizza tempo (ChronoSaber)
            try:
                await self.client.sync_time()
                if self.device_info_card:
                    self.device_info_card.time_synced = True
                    from datetime import datetime
                    import time
                    self.device_info_card.sync_time_str = datetime.fromtimestamp(int(time.time())).strftime('%H:%M:%S')
                self._log("Time synchronized", "green")
            except Exception as e:
                self._log(f"Time sync failed: {e}", "yellow")
                if self.device_info_card:
                    self.device_info_card.time_synced = False

            # Leggi configurazione motion (include soglie gesture)
            try:
                config = await self.client.get_motion_config()
                if config:
                    self._update_motion_config(config)
                    self._log(
                        "Motion config loaded: "
                        f"quality={config.get('quality')} "
                        f"motionMin={config.get('motionIntensityMin')} "
                        f"speedMin={config.get('motionSpeedMin')} "
                        f"ignition={config.get('gestureIgnitionIntensity')} "
                        f"retract={config.get('gestureRetractIntensity')} "
                        f"clash={config.get('gestureClashIntensity')} "
                        f"mapU={config.get('effectMapUp')} "
                        f"mapD={config.get('effectMapDown')} "
                        f"mapL={config.get('effectMapLeft')} "
                        f"mapR={config.get('effectMapRight')}",
                        "cyan"
                    )
            except Exception as e:
                self._log(f"Could not read motion config: {e}", "yellow")

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
        self.last_led_state = state or {}
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
        self.last_motion_state = state or {}
        self.last_motion_status_ts = time.monotonic()
        merged_state = self._merge_motion_state(self.last_motion_state)
        if self.motion_section:
            self.motion_section.motion_state = merged_state
        if self.optical_widget:
            self.optical_widget.motion_state = merged_state

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
        direction = event.get('direction', 'none')
        speed = event.get('speed', 0)
        confidence = event.get('confidence', 0)
        active_blocks = event.get('activeBlocks', 0)
        frame_diff = event.get('frameDiff', 0)
        gesture = event.get('gesture', 'none')
        gesture_conf = event.get('gestureConfidence', 0)

        emoji_map = {
            "shake_detected": "⚡",
            "motion_started": "▶",
            "motion_ended": "⏹",
            "still_detected": "😴"
        }

        emoji = emoji_map.get(event_type, "🔔")
        base = f"{emoji} {event_type.upper()} I:{intensity} dir:{direction} spd:{speed} conf:{confidence}% blocks:{active_blocks} diff:{frame_diff}"
        if gesture != 'none':
            base += f" gesture:{gesture}({gesture_conf}%)"
        self._log(base, "yellow")
        self._maybe_poll_motion_status()

    def _maybe_poll_motion_status(self) -> None:
        if self.motion_poll_in_flight:
            return
        now = time.monotonic()
        if now - self.last_motion_status_ts < 1.2:
            return
        self.motion_poll_in_flight = True
        self.run_worker(self._poll_motion_status_async())

    def _poll_motion_status(self) -> None:
        self._maybe_poll_motion_status()

    async def _poll_motion_status_async(self) -> None:
        try:
            if not self.client or not self.client.client or not self.client.client.is_connected:
                return
            status = await self.client.get_motion_status()
            if status:
                self._on_motion_update(status, is_first=False, changes=None)
        finally:
            self.motion_poll_in_flight = False

    def _merge_motion_state(self, state: Dict) -> Dict:
        merged = dict(state or {})
        if self.last_led_state:
            for key in ("gestureClashEffect", "gestureClashDurationMs"):
                if key in self.last_led_state:
                    merged[key] = self.last_led_state[key]
        if self.motion_config:
            for key in (
                "gestureIgnitionIntensity",
                "gestureRetractIntensity",
                "gestureClashIntensity",
                "effectMapUp",
                "effectMapDown",
                "effectMapLeft",
                "effectMapRight",
            ):
                if key in self.motion_config:
                    merged[key] = self.motion_config[key]
        return merged

    def _update_motion_config(self, config: Dict) -> None:
        self.motion_config = config or {}
        merged_state = self._merge_motion_state(self.last_motion_state)
        if self.motion_section:
            self.motion_section.motion_state = merged_state
        if self.optical_widget:
            self.optical_widget.motion_state = merged_state

    def _set_motion_config_field(self, key: str, value) -> None:
        config = dict(self.motion_config or {})
        config[key] = value
        self._update_motion_config(config)

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

    def action_chrono_cycle_hours(self) -> None:
        """F6: Cycle chrono hour themes"""
        if not self.chrono_themes_card or not self.chrono_themes_card.connected:
            self._log("Connect device first to change chrono themes", "yellow")
            return

        # Cycle to next theme
        current = self.chrono_themes_card.chrono_hour_theme
        next_theme = (current + 1) % len(ChronoThemesCard.HOUR_THEMES)
        second_theme = self.chrono_themes_card.chrono_second_theme

        self.run_worker(self._send_chrono_themes(next_theme, second_theme))

    def action_chrono_cycle_seconds(self) -> None:
        """F7: Cycle chrono second themes"""
        if not self.chrono_themes_card or not self.chrono_themes_card.connected:
            self._log("Connect device first to change chrono themes", "yellow")
            return

        # Cycle to next theme
        hour_theme = self.chrono_themes_card.chrono_hour_theme
        current = self.chrono_themes_card.chrono_second_theme
        next_theme = (current + 1) % len(ChronoThemesCard.SECOND_THEMES)

        self.run_worker(self._send_chrono_themes(hour_theme, next_theme))

    def action_wellness_toggle(self) -> None:
        """F11: Toggle wellness mode"""
        if not self.chrono_themes_card or not self.chrono_themes_card.connected:
            self._log("Connect device first", "yellow")
            return

        current = self.chrono_themes_card.wellness_mode
        new_state = not current
        bpm = self.chrono_themes_card.breathing_rate

        self.run_worker(self._send_wellness_config(new_state, bpm))

    def action_device_ignition(self) -> None:
        """F8: Trigger ignition animation"""
        self.run_worker(self._execute_command("ignition"))

    def action_device_retract(self) -> None:
        """F9: Trigger retraction animation"""
        self.run_worker(self._execute_command("retract"))

    def action_toggle_motion_boot(self) -> None:
        """F10: Toggle motion on boot configuration"""
        # Legge lo stato attuale della configurazione di boot dal widget LED
        # (Il firmware invia 'motionEnabled' nel pacchetto di stato LED che corrisponde a motionOnBoot)
        current_boot_setting = False
        if self.led_widget and self.led_widget.led_state:
            current_boot_setting = self.led_widget.led_state.get('motionEnabled', False)

        if current_boot_setting:
            self.run_worker(self._execute_command("motion onboot off"))
        else:
            self.run_worker(self._execute_command("motion onboot on"))


# ═══════════════════════════════════════════════════════════════════════════
# ENTRY POINT
# ═══════════════════════════════════════════════════════════════════════════

def main():
    """Entry point"""
    app = SaberDashboard()
    app.run()


if __name__ == "__main__":
    main()
