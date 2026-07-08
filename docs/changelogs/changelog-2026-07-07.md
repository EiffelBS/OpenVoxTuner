# Changelog - 2026-07-07

## Tab Rendering & Theme Background Fixes (01:15 CEST)

### Fixed

- **Tab text invisible (empty tabs)**: The custom `drawTabButton` override in `LookAndFeel.cpp` replaced JUCE's default tab drawing but never called `drawTabButtonText`, so tab text was never rendered. Added a `drawTabButtonText()` call at the end of `drawTabButton` to delegate text rendering to the existing text-drawing method.

- **vizBg() semi-transparent causing light bleed-through**: The `vizBg()` colour used alpha `0x40` (25% opacity), allowing the tab background to show through in light mode. Changed to fully opaque `0xff` so the visualizer and curve editor areas are always solid dark regardless of theme.

- **Tab content areas using theme-dependent background**: `addTab()` calls used `ovt::bgPanel()` which returns a light colour in light mode, making the tab content areas bright. Replaced with a hardcoded dark colour (`0xff14151c`) so tab content is always dark.

- **Inconsistent dark grey shades across UI layers (second pass)**: `bgDark()`, `bgPanel()`, and `headerBg()` were previously unified to `#FF14151C`, but this value didn't match the tab content colour. Updated all three to use `#FF16171E` in dark mode for a single consistent shade across the entire plugin.

### Changed

- **LookAndFeel.cpp**: Added `drawTabButtonText(button, g, isMouseOver, isMouseDown)` call at the end of `drawTabButton`.
- **OVTTheme.h**: Changed `vizBg()` alpha from `0x40` to `0xff`. Changed `bgDark()`, `bgPanel()`, and `headerBg()` dark mode colours from `#FF14151C` to `#FF16171E`.
- **PluginEditor.cpp**: Replaced `ovt::bgPanel()` in `addTab()` calls with a hardcoded `juce::Colour(0xff14151c)` constant for always-dark tab content.

---

## Harmony Combo Box Fix & Grey Shade Unification (22:45 CEST)

### Fixed

- **Harmony combo box only showing 3 options instead of 20**: The `refreshLabels()` method in `PluginEditor.cpp` only added 3 harmony items to the combo box (3rd Below+Above, 5th Below+Above, Octave) because translation keys only covered those 3. Added all 20 harmony translation keys (`kHarmonyNone`, `kHarmony3rdBelow`, `kHarmony3rdAbove`, `kHarmony3rdBelowAbove`, `kHarmony4thBelow`, `kHarmony4thAbove`, `kHarmony4thBelowAbove`, `kHarmony5thBelow`, `kHarmony5thAbove`, `kHarmony5thBelowAbove`, `kHarmony3rdBelow5thAbove`, `kHarmony5thBelow3rdAbove`, `kHarmonyOctaveBelow`, `kHarmonyOctaveAbove`, `kHarmonyOctaveBelowAbove`, `kHarmonyVocalStack3`, `kHarmonyVocalStack4`, `kHarmonyPowerChord`, `kHarmonyParallel3rd`, `kHarmonyDrone`) with translations in all 5 languages (English, French, German, Spanish, Japanese). Updated `refreshLabels()` to populate all 20 items and added bounds-safe selection restoration.

- **Inconsistent dark grey shades across UI layers**: `bgDark()` (#121318), `bgPanel()` (#14151C), and `headerBg()` (#15151e) were using slightly different dark grey values, creating visible contrast seams between background layers. Unified `bgDark()` and `headerBg()` to use the same #14151C shade as `bgPanel()` for a seamless dark mode appearance.

### Changed

- **OVTLanguages.h**: Replaced 3 harmony translation keys with 20 complete keys. Added full translation sets for all 20 harmony options in English, French, German, Spanish, and Japanese.
- **PluginEditor.cpp**: Updated `refreshLabels()` harmony section to reference all 20 translation keys and populate the combo box with all 20 items. Added `juce::jmin()` bounds check for selection restoration.
- **OVTTheme.h**: Changed `bgDark()` dark mode colour from #121318 to #14151C. Changed `headerBg()` dark mode colour from 0xff15151e to #FF14151C (same as `bgPanel()`).

---

## Modern Tab Appearance (22:37 CEST)

### Added

- **Modern tab drawing overrides in LookAndFeel**: Replaced the default Windows 95-style tabbed tab bar rendering with a custom modern appearance. The tab bar background uses the dark theme color (`ovt::bgDark()`) with a subtle accent bottom line. Active tabs are filled with the accent color and display white text with rounded corners (4px radius). Inactive tabs are transparent with a subtle hover effect using a semi-transparent accent color. Tab text uses the centralized combo box font for consistency.

### Changed

- **LookAndFeel.h**: Added `drawTabbedTabBarBackground()` and `drawTab()` virtual method overrides to the `AutotuneLookAndFeel` class declaration.
- **LookAndFeel.cpp**: Implemented both tab drawing methods with modern rounded-rectangle styling, accent-colored active tab, dim-colored inactive tab text, and hover highlighting.

---

## Bug Fixes: Light Theme, Export Image, Help Overlay Z-Order (10:30 CEST)

### Fixed

- **Light theme: text still invisible after theme switch**: When `setColour()` is called on a component, it overrides the LookAndFeel default. So when the theme was changed and `refreshThemeColours()` was called, only the LookAndFeel defaults were updated, but components still used their cached per-component colours from the dark theme. Created `applyThemeToAllComponents()` helper method that re-applies colours to ALL components (sliders, labels, combo boxes, toggle buttons, other buttons, tabbed component) after a theme change. Both theme toggle lambdas and the constructor now call this method instead of just refreshing the LookAndFeel.
- **Export as Image: nothing happens on Windows**: The `juce::FileChooser` was stored as a `std::unique_ptr` member that could be destroyed before the async callback fires. Replaced with a `std::shared_ptr` captured by the callback lambda to extend its lifetime. Also added an alert message when the visualizer component cannot be found, and success/failure feedback after export.
- **Keyboard shortcuts overlay hidden behind child components**: The overlay was painted in the editor's `paint()` method, but child components (visualizer, controls, buttons) are rendered ON TOP of the paint layer in JUCE's z-order. Extracted the overlay into a separate `HelpOverlayComponent` inner class that is added LAST via `addAndMakeVisible()`, ensuring it renders on top of all other components. The overlay now handles its own mouse events (click to dismiss), eliminating the old `suppressNextMouseDown` workaround.

### Changed

- **PluginEditor.h**: Added `applyThemeToAllComponents()` public method declaration. Added `HelpOverlayComponent` inner class with `paint()` and `mouseDown()` overrides. Replaced old `suppressNextMouseDown` flag with `helpOverlay` member.
- **PluginEditor.cpp**: Added `HelpOverlayComponent` implementation (constructor, mouseDown, paint). Added `applyThemeToAllComponents()` implementation with comprehensive colour re-application for all component types. Removed old overlay painting code from `paint()` method. Simplified `mouseDown()` to empty body (overlay handles its own events). Updated `toggleHelpOverlay()` to show/hide the overlay component. Replaced both theme toggle lambdas to use `applyThemeToAllComponents()`. Replaced constructor's label colour restoration with `applyThemeToAllComponents()` call. Added `addAndMakeVisible(helpOverlay)` as the last child component.

---

## Bug Fixes: i18n Labels, Export Image, Help Overlay (01:30 CEST)

### Fixed

- **i18n: Labels not translated when language changes**: Knob and combo labels ("Speed (ms)", "Amount", "Blend", "Volume", "Flex", "Humanize", "Root", "Scale") were hardcoded English strings that never updated when the language was switched. Added a `TranslatableLabel` registration system in `PluginEditor.h` that stores label pointers paired with their i18n keys. A new `refreshLabels()` method iterates all registered labels and updates their text via `ovt::tr()`. Labels are refreshed on language change and at startup after restoring persisted language preference.
- **Export as Image: FileChooser destroyed before callback fires**: The `juce::FileChooser` was created as a local variable inside the menu item lambda and got destroyed when the lambda exited, before the async dialog callback could fire. Moved the FileChooser to a `std::unique_ptr<juce::FileChooser> exportFileChooser` member variable so it stays alive for the duration of the async operation, and reset it in the callback.
- **Keyboard Shortcuts overlay dismissed immediately by mouseDown**: When the user clicks the "Keyboard Shortcuts (?)" menu item, the menu closes and generates a mouseDown event on the editor, which the `mouseDown` handler saw as a dismiss click. Added a `suppressNextMouseDown` flag that is set when the overlay is opened, causing the first mouseDown after opening to be ignored.

### Changed

- **PluginEditor.h**: Added `TranslatableLabel` struct, `translatableLabels` vector, `refreshLabels()` declaration, `suppressNextMouseDown` flag, and `exportFileChooser` member.
- **PluginEditor.cpp**: Registered 8 translatable labels after setupKnob/setupCombo calls. Added `refreshLabels()` implementation. Replaced Export as Image lambda with FileChooser-as-member pattern. Updated `toggleHelpOverlay()` to set suppress flag. Updated `mouseDown()` to check suppress flag.

---

## Light Theme Color Fix & Keyboard Shortcut Overlay Stabilization (01:15 CEST)

### Fixed

- **Light theme: invisible text in UI**: Replaced all 51 static color constant references (`kBgPanel`, `kAccent`, `kText`, `kAccentSoft`) in `PluginEditor.cpp` constructor and `timerCallback()` with dynamic theme-aware functions (`ovt::bgPanel()`, `ovt::accent()`, `ovt::text()`, `ovt::accentSoft()`). Component colors now correctly reflect the active theme at initialization and when the theme is toggled. Affected components: updateButton, all sliders/knobs, all ComboBoxes (harmony type, shifted voices, harmony tone, key, scale, latency mode), all toggle buttons (harmony enable, formant, reverb, bypass, MIDI out, use voice), correction mode button, tabbed component background, and drawable button on-state colors.
- **Keyboard shortcuts overlay causing accidental theme change**: Removed the `?` key handler from `keyPressed()` that was toggling the help overlay on key press. The overlay is now exclusively toggled via the "Keyboard Shortcuts (?)" menu item in the hamburger menu, preventing accidental theme changes caused by `?` keypresses on international keyboards (e.g., French AZERTY where `?` requires Shift+).

### Changed

- **PluginEditor.cpp paint() method**: Already uses `ovt::` theme functions -- no changes needed in paint().
- **PluginEditor.cpp constructor**: All `setColour()` calls now use `ovt::` functions instead of static `kColors`, ensuring correct colors for both dark and light themes.

---

## CPU Meter Position Fix & i18n Label Keys (01:00 CEST)

### Fixed

- **CPU meter position**: The CPU usage meter in the header strip was overlapping with the A/B comparison button. It is now correctly positioned to the left of the A/B button with proper spacing, based on the actual layout coordinates from the `resized()` method.

### Added

- **i18n translation keys for labels and tooltips**: Added 13 new translation keys (`kLabelCorrectionMode`, `kLabelModern`, `kLabelTransparent`, `kLabelBypass`, `kLabelMidiOut`, `kLabelUseVoice`, `kLabelSnap`, `kLabelSnapGrid`, `kLabelStepMode`, `kLabelClear`, `kLabelPresets`, `kLabelHelp`, `kLabelUpdates`) to the `OVTLanguages.h` i18n system. Each key has translations in all 5 supported languages (English, French, German, Spanish, Japanese).

---

## MIDI Learn for Sliders

### Added

- **MIDI Learn submenu in hamburger menu**: A new "MIDI Learn" submenu lists all learnable parameters (Speed, Amount, Formant, Reverb Mix, FlexTune, Humanize, Harmony Gain, Harmony Blend, Harmony Tone). Selecting a parameter enters learn mode and shows a dialog prompting the user to move a MIDI controller. The first CC message received is bound to the parameter. The `MidiLearnState` struct tracks learn mode status, target parameter ID, and assigned CC number.

### Implementation

- `MidiLearnState` struct added to `PluginEditor.h` with `isLearning`, `parameterId`, and `assignedCc` fields.
- `startMidiLearn()` method enters learn mode and shows an info dialog.
- `handleMidiMessage()` method processes incoming MIDI CC messages and assigns the CC to the pending parameter when in learn mode.

---

## ARA2 Waveform Overlay Placeholder

### Added

- **ARA2 waveform overlay in PitchVisualizer**: A `setWaveformOverlay()` public method accepts audio sample data, sample count, and sample rate. When waveform data is provided, a semi-transparent waveform visualization is drawn behind the pitch curves in the live view. The `paintWaveformOverlay()` method renders min/max envelopes per pixel column using rounded rectangles. This serves as a placeholder for future ARA2 integration where DAW waveform data will be streamed to the visualizer.

### Implementation

- `setWaveformOverlay()` stores audio data in a `juce::AudioBuffer<float>` and sets the `hasWaveform` flag.
- `paintWaveformOverlay()` renders the waveform as a series of rounded rectangles with `0x18ffffff` color (very transparent white).
- The waveform is drawn before pitch curves in the `paint()` method so it appears as a background layer.

## CPU Usage Meter & A/B Comparison

### Added

- **CPU Usage Meter**: A small real-time CPU usage display in the plugin header strip (top-right, next to the hamburger menu). Shows a color-coded percentage label and a thin progress bar. Color scheme: green (< 30%), yellow (< 60%), orange (< 85%), red (>= 85%). CPU usage is calculated as the ratio of `processBlock` execution time to available block time, smoothed with an exponential moving average for stable readings.
- **A/B Comparison**: A toggle button ("A" / "B") in the header strip allows comparing two plugin states. Left-click toggles between slot A and slot B. Right-click opens a popup menu to save the current plugin state to either slot. State is serialized as base64-encoded binary data in an `XmlElement` for efficient in-memory storage. On toggle, the current state is saved to the outgoing slot and the incoming slot's state is loaded.

---

## Pitch Visualizer - Tuning Statistics Dashboard & Curve Editor Undo/Redo Buttons

### Added

- **Tuning Statistics Dashboard**: A compact statistics panel in the PitchVisualizer (bottom-right of plot area) displaying real-time tuning accuracy metrics. Tracks a rolling window of 300 samples (~10 seconds at 30fps) and shows: (1) In-tune percentage (samples within +/- 15 cents, color-coded: green >= 80%, yellow >= 50%, orange < 50%), (2) Average cents offset. The panel replaces the previous 2-row legend with an expanded 3-row layout that includes curve legend, shortcut hints, and tuning statistics.
- **Curve Editor Undo/Redo buttons**: Visual TextButton controls for Undo and Redo in the PitchCurveEditor, positioned below the piano keyboard at the bottom-left. Buttons use arrow symbols and tooltips indicating keyboard shortcuts (Ctrl+Z / Ctrl+Y). These complement the existing keyboard shortcuts for users who prefer clicking.

---

## Pitch Visualizer - Font Fix, Theme, i18n, Export, and Piano Labels

### Fixed

- **Font bold/non-bold inconsistency resolved**: The plugin's font was randomly switching between bold and non-bold styles during use. This was caused by inconsistent `juce::Font` constructor patterns across the codebase (some using named typefaces, others using default typeface resolution). All font creation is now centralized through `OVTFonts.h` using the `ovt::createFont()` helper, which guarantees the same "Segoe UI" typeface for both bold and regular variants. Every component now uses `ovt::font*()` constants instead of ad-hoc font constructors.
- **Legend truncation resolved**: The legend block is now a compact 130x26px panel with abbreviated labels.
- **Octave reference lines generalized to all octaves**: Dynamic computation from visible frequency range.
- **Scale note lines generalized to all visible octaves**: Full range coverage instead of octaves 2-5 only.

### Added

- **Keyboard shortcuts help overlay**: A new help overlay displays all available keyboard shortcuts and mouse interactions in a centered panel. The overlay is triggered by pressing the "?" key or via the "Keyboard Shortcuts (?)" item in the hamburger menu. The panel shows 12 shortcuts organized in two columns covering mouse wheel, keyboard shortcuts (Ctrl+C/V/Z/Y, Delete), and mouse interactions (click, double-click, right-click). Clicking anywhere or pressing "?" again dismisses the overlay. The overlay uses a semi-transparent dark backdrop with a styled panel matching the plugin's dark theme.
- **Dark/Light theme toggle**: A "Theme" submenu in the hamburger menu allows switching between Dark and Light themes. The preference is persisted via the `ui_theme` parameter in the plugin state and restored on reload. All UI elements (background, panels, text, accent colors) adapt to the selected theme.
- **Internationalization (i18n) system**: A modular multi-language system (`OVTLanguages.h`) supports English, French, German, Spanish, and Japanese. A "Language" submenu in the hamburger menu allows switching languages. The preference is persisted via the `ui_language` parameter. New languages can be added by extending the `getTranslations()` function with a new map.
- **Visualizer image export**: An "Export as Image..." menu item in the hamburger menu opens a save dialog to export the visualizer as a high-quality PNG or JPEG image at 2x resolution. The `exportAsImage()` method renders the entire component to a `juce::Image` and writes it using JUCE's image format classes.
- **Piano key note labels (D, E, F, G, A, B)**: The vertical piano keyboard now displays note names on all white keys (not just C) when the key height is >= 20px. C notes additionally show the octave number (e.g. "C 4"). Labels are hidden when keys are too small to prevent visual clutter.
- **Centralized font system (`OVTFonts.h`)**: A new header provides 20 named font functions (`ovt::fontTitle()`, `ovt::fontLabel()`, etc.) that all use the same "Segoe UI" typeface family. This eliminates all ad-hoc font creation across the plugin.
- **Centralized theme system (`OVTTheme.h`)**: A new header provides theme-aware colour accessors (`ovt::bgDark()`, `ovt::accent()`, `ovt::text()`, etc.) with dark and light variants. Components use these instead of hardcoded colour constants.
- **Plugin parameters for UI state**: Added `ui_theme` (int, 0=Dark, 1=Light) and `ui_language` (int, 0=English..4=Japanese) parameters to the processor for persisting UI preferences across sessions.
- **Feature proposals document**: Comprehensive analysis of 10 additional feature proposals with complexity and UX impact assessments.

### Changed

- **All font creation centralized**: Replaced ~25 ad-hoc `juce::Font(size)` and `juce::Font(size, style)` calls across PitchVisualizer, PianoKeyboard, PluginEditor, LookAndFeel, and PitchCurveEditor with centralized `ovt::font*()` calls.
- **SVG icon buttons**: Scroll/zoom/reset buttons use DrawableButton with SVG icons (magnifying glass, chevrons, cross).
- **Smooth animated transitions**: Zoom and scroll operations use lerp interpolation.

---

## Earlier Changes (same day)

### Fixed

- **Keyboard shortcut text truncation resolved**: Split into two non-truncated lines with smaller font.
