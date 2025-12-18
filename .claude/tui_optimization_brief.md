# TUI Dashboard Optimization Brief
## Textual Framework - Responsive Design Challenge

### Context
Dashboard TUI style btop per monitoraggio live BLE LedSaber, costruita con Textual (Python).
File: `saber_dashboard.py`

---

## Current State & Discoveries

### Layout Structure
```
Screen (vertical)
├─ HeaderWidget (#header) - height: 5
├─ Container (#main_body) - height: 1fr
│  ├─ Container (#stats_grid) - grid 3 columns, height: 12 (FIXED)
│  │  ├─ Vertical (#led_column)
│  │  │  ├─ LEDPanelWidget - height: 3
│  │  │  └─ Container (#kpi_row) - grid 2 cols
│  │  │     ├─ BLERSSICard - height: 4
│  │  │     └─ ActiveFXCard - height: 4
│  │  ├─ Vertical (#camera_column)
│  │  │  ├─ CameraPanelWidget - height: 3
│  │  │  └─ CameraFramesCard - height: 4
│  │  └─ MotionSection (#motion_summary) - grid 1-3 cols responsive
│  │     ├─ MotionStatusCard - height: 6
│  │     ├─ MotionIntensityCard - height: 9
│  │     └─ MotionDirectionCard - height: 9
│  └─ Horizontal (#grid_console_row) - height: 20 (FIXED)
│     ├─ OpticalFlowGridWidget (#optical_flow) - width: 2fr
│     └─ Vertical (#console_column) - width: 1fr
│        ├─ VerticalScroll (#console_scroll) - height: 14
│        │  └─ ConsoleWidget
│        └─ CommandInputWidget - Input field
└─ Footer - height: 1
```

---

## Problems Discovered

### 1. **Height Management Issues**
- ❌ `height: auto` on `#stats_grid` causes expansion beyond screen bounds
- ❌ Widget heights don't respect parent container constraints properly
- ❌ Vertical spacing accumulates, pushing content off-screen
- ✅ **Solution Found**: Fixed heights with `max-height` constraints work best

### 2. **Grid Layout Overflow**
- `#stats_grid` with `height: auto` tries to accommodate full widget heights
- Motion cards (9 lines each) + LED/Camera cards (3 lines) + KPI cards (4 lines) = ~25 lines
- But `#stats_grid` set to `height: 12` causes clipping/overlap
- **Current Issue**: Grid covers tabs above when widgets overflow

### 3. **Responsive Behavior**
- CSS classes for responsive layout work well:
  - `#stats_grid.cols-1` / `.cols-2` / default 3-column
  - `#motion_summary.cols-3` for horizontal layout on wide screens
  - `#kpi_row.single` for narrow screens
- Breakpoints applied in Python via `_update_responsive_layout(width)`
- **Issue**: Height doesn't adapt responsively, only width

### 4. **Console/Grid Row**
- Fixed `height: 20` works well for preventing bottom expansion
- `#optical_flow` and `#console_column` fill parent with `height: 100%`
- `#console_scroll` at `height: 14` leaves room for input field
- ✅ This section is stable and doesn't cause issues

---

## Key Discoveries & Constraints

### What Works ✅
1. **Fixed heights with max-height** prevent overflow:
   ```css
   height: 12;
   max-height: 12;
   ```

2. **Minimal margins** (all set to `margin: 0`) reduce wasted space

3. **Compact widget heights**:
   - LED/Camera panels: 3 lines minimum
   - KPI cards: 4 lines
   - Motion cards: 6-9 lines depending on content

4. **Percentage-based widths** in horizontal layouts (2fr/1fr)

5. **Responsive grid-size switching** based on terminal width

### What Doesn't Work ❌
1. **`height: auto`** on grid containers - causes unconstrained expansion
2. **Viewport units (`vh`)** - not well supported in Textual
3. **`overflow-y: auto`** - doesn't scroll properly in nested containers
4. **Large spacing/padding** - accumulates quickly

### Textual-Specific Quirks 🐛
- Grid layouts require explicit `height`, can't rely on content sizing
- `1fr` in parent requires children to have explicit or `auto` heights
- Nested grids (grid inside grid) compound height issues
- `max-height` works but widget content may clip without proper internal padding

---

## Design Requirements

### Functional Requirements
1. **Full visibility**: All widgets visible without scrolling on 80x50 terminal (minimum)
2. **Responsive breakpoints**:
   - < 110 cols: 1-column layout
   - 110-159 cols: 2-column layout
   - ≥ 160 cols: 3-column layout
3. **Real-time updates**: Widgets update reactively without layout shifts
4. **Compact but readable**: Maximum information density without cramping

### Visual Requirements
1. **Consistent spacing**: Uniform gaps between related widgets
2. **Visual hierarchy**: Clear separation between sections (stats vs grid/console)
3. **Color coding**:
   - Cyan: LED/BLE
   - Green: Camera
   - Magenta: Motion
   - Yellow: Intensity
4. **Box styles**: Mix of `ROUNDED`, `SQUARE`, `MINIMAL_DOUBLE_HEAD` for visual interest

### Performance Requirements
- Widget updates at 10-30 FPS (camera stream rate)
- No layout thrashing on resize
- Smooth reactive updates without flicker

---

## Optimization Goals

### Primary Goal
**Create a responsive height system** that prevents widgets from covering each other while maintaining visibility of all content.

### Specific Issues to Solve
1. **Stats grid height calculation**:
   - Currently fixed at 12 lines but widgets need ~20+ lines
   - Need dynamic height based on responsive layout (1/2/3 cols)
   - Must prevent covering header tabs

2. **Motion cards in grid**:
   - 3 cards @ 9 lines each = 27 lines total if stacked
   - 3 cards @ 9 lines side-by-side = 9 lines (wide screens)
   - Need height to adapt based on grid-size

3. **Vertical space distribution**:
   - Header: 5 lines (fixed)
   - Stats: 12-30 lines (variable, needs fix)
   - Grid+Console: 20 lines (fixed)
   - Footer: 1 line (fixed)
   - **Target total**: ≤ 48 lines for 50-line terminal

---

## Technical Constraints

### Textual CSS Limitations
- No `calc()` function
- No CSS variables for dynamic values
- Limited viewport units support
- No `min-content` / `max-content` sizing
- Grid rows must be explicit or `auto` (no fractional grid-rows)

### Python-Side Options
- Can calculate heights dynamically in `_update_responsive_layout()`
- Can modify CSS classes at runtime
- Can adjust widget heights via reactive properties
- Access to `self.size.width` and `self.size.height`

---

## Proposed Solution Approach

### Option A: Dynamic Height Classes
Create CSS classes for different height configurations:
```css
#stats_grid.layout-3cols { height: 12; }
#stats_grid.layout-2cols { height: 18; }
#stats_grid.layout-1col { height: 30; }
```
Apply via Python based on responsive breakpoint.

### Option B: Fractional Distribution
Replace fixed heights with fractional:
```css
#stats_grid { height: 60vh; }  /* if vh works */
#grid_console_row { height: 30vh; }
```
Or use `1fr` / `2fr` ratio distribution.

### Option C: Scrollable Stats Grid
Keep fixed heights but make `#stats_grid` scrollable:
```css
#stats_grid {
    height: 25;
    overflow-y: auto;
}
```

### Option D: Collapsible Sections
Add toggle to collapse/expand Motion cards:
- Default: Show compact 6-line summaries
- Expanded: Show full 9-line detail views
- User can toggle with keyboard shortcut

---

## Success Criteria

1. ✅ No widgets overlap or cover each other
2. ✅ Header always visible at top
3. ✅ Footer always visible at bottom
4. ✅ All content visible on 80x50 terminal (may require scrolling within sections)
5. ✅ Smooth transitions when resizing terminal
6. ✅ Consistent spacing maintained at all breakpoints
7. ✅ No layout thrashing during real-time updates

---

## Testing Scenarios

1. **Terminal sizes**:
   - 80x24 (minimal)
   - 80x50 (standard)
   - 120x40 (wide-short)
   - 200x60 (large)

2. **Content states**:
   - All widgets with active data
   - Disconnected (no BLE data)
   - Camera stopped (no FPS updates)
   - Motion disabled

3. **Responsive transitions**:
   - Drag resize from 80 → 200 cols
   - Rapid resize cycles
   - Aspect ratio changes

---

## Additional Context

### Current Responsive Logic (Python)
```python
def _update_responsive_layout(self, width: int) -> None:
    if self.stats_grid:
        self.stats_grid.remove_class("cols-2", "cols-1")
        if width < 110:
            self.stats_grid.add_class("cols-1")
        elif width < 160:
            self.stats_grid.add_class("cols-2")

    if self.motion_section:
        self.motion_section.remove_class("cols-3")
        if width >= 140:
            self.motion_section.add_class("cols-3")

    if self.kpi_row:
        self.kpi_row.remove_class("single")
        if width < 100:
            self.kpi_row.add_class("single")
```

### Widget Height Requirements
```python
# Minimum readable heights per widget
LEDPanelWidget: 3 (1 line content + borders)
CameraPanelWidget: 3 (sparkline needs space)
BLERSSICard: 4 (value + label)
ActiveFXCard: 4 (2-line display)
CameraFramesCard: 4 (2-line display)
MotionStatusCard: 6 (3 status lines)
MotionIntensityCard: 9 (bar + sparkline + label)
MotionDirectionCard: 9 (arrow + gesture display)
OpticalFlowGridWidget: ~15 (8x8 grid + legend)
ConsoleWidget: 14 (scrollable log)
```

---

## Request for TUI Specialist

**Please design a comprehensive responsive height system** that:

1. Solves the widget overlap/covering issue
2. Maintains all content visibility
3. Works within Textual CSS constraints
4. Adapts to terminal size changes
5. Preserves visual hierarchy and spacing
6. Minimal code changes (prefer CSS over Python refactor)

**Deliverables**:
- Updated CSS with height management strategy
- Python helper code if needed for dynamic calculations
- Documentation of responsive behavior
- Edge case handling recommendations

**Bonus**: If you can make the layout scale gracefully from 80x24 to 200x60+ terminals while keeping everything readable, that would be ideal.

---

## Files Reference
- Main file: `/home/reder/Documenti/PlatformIO/Projects/ledSaber/saber_dashboard.py`
- Current CSS: Lines 564-699
- Responsive logic: Lines 787-804
- Widget definitions: Lines 88-490

Good luck! 🚀
