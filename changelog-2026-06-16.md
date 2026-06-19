# Changelog - 2026-06-16

## Implemented
- **Step Mode**: Added a new "Step mode" option in the Curve Editor.
  - When activated, interpolation between points is drawn as steps (horizontal hold followed by a vertical jump) instead of a direct diagonal line.
  - This perfectly mimics hard-tuning/robotic steps directly via the graphical interface.
  - State is saved and restored automatically within the XML state structure.
- **Icon Toolbar Refactor**: Replaced the text buttons for Curve Editor tools with modern vector icon buttons.
  - Implemented custom SVG/Path drawing for `Snap to scale`, `Snap to grid`, `Step mode`, `Clear curve`, and `Reset playhead`.
  - Configured Toggle states to highlight active icons in blue.
  - Added a themed tooltip system for the icon toolbar (custom LookAndFeel + smart positioning) with clear, ergonomic descriptions on hover.
- **Time Grid Snapping**: Added a "Snap to grid" toggle button in the Curve Editor.
  - When enabled, points snap to the nearest 8th note grid line (0.5s resolution based on the 120 BPM default).
  - When disabled, points retain a slight magnetic snap (±0.05s) when dragged very close to grid lines, allowing free placement otherwise.
- **Scroll & Zoom Refinement**: Fixed an issue where the scroll direction was inverted in the Pitch Curve Editor and Pitch Visualizer.
  - Scrolling up now correctly shifts the view up the pitch axis (higher notes).
  - Pan speed is now proportional to the current zoom level, preventing erratic jumps when zoomed in tightly.
- **Tooltip Formatting**: Updated the hover/drag tooltip in the Pitch Curve Editor to display time in `Measure.Beat[.Decimal]` format (e.g., `1.4` instead of `3.00 b`), matching the time ruler above the editor for better UX.
- **UI Instructions**: Added explicit "Scroll: Pan | Ctrl+Scroll: Zoom" instructions to the bottom right of both the Pitch Curve Editor and the Pitch Visualizer tabs.
- **Editor Piano Highlighting**: Applied the same sung-note / corrected-note key highlighting used in the Live visualizer to the Curve Editor piano keyboard.
- **Presets Menu (Toolbar)**: Integrated the Curve Editor preset list into the icon toolbar as a hierarchical, scrollable dropdown.
  - Added two categories: `Factory` (built-in presets) and `Custom` (user presets).
  - Implemented `Save current as...` for creating Custom presets and `Delete...` for removing them (Factory presets are protected).

## Fixed
- Fixed zoom and pan (scrolling) logic where scrolling after zooming caused the view to jump out of bounds or move in the wrong direction.
