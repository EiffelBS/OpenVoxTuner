# Changelog - 2026-06-15

## Implemented
- **Rebranding**: Successfully renamed the project from "Autotune Clone" to **OpenVoxTuner** to avoid trademark infringement and present a professional, open-source identity.
  - Updated CMake project name and JUCE target names.
  - Changed VST3 Plugin Code to `OvtP` to generate a new, distinct VST3 UID.
  - Refactored all C++ classes (`AutotuneCloneAudioProcessor` -> `OpenVoxTunerAudioProcessor`, etc.).
  - Updated Inno Setup scripts to handle the new name, application paths, and explicitly delete the old "Autotune Clone" VST3 binaries to prevent conflicts.
  - Replaced the hardcoded text in the GUI with a brand new vector logo and modern font rendering for "OpenVoxTuner".
- **PreSonus/Fender Studio Pro Micro View**: Integrated `Presonus::IEditControllerExtra` to customize the parameters displayed in the host's Micro View (Mixer and Channel Overview).
  - Mapped JUCE parameters (`Speed`, `Amount`, `Formant`) to the VST3 extension using `kParamFlagMicroEdit`.
  - Added the official `ipsleditcontroller.h` to the project's external dependencies.
  - Implemented `VST3ClientExtensions::queryIEditController` to expose the interface to the host dynamically.
- **Documentation**: Created `docs/deployment-and-packaging-guide.md` to outline cross-platform compilation, packaging, code signing, and update strategies for VST3, AU, AAX, and CLAP formats.
- **Local Installers**: Created a local pipeline to generate Windows installers (`.exe`) using Inno Setup without relying on GitHub Actions.
  - Added `installer/AutotuneClone.iss` to handle packaging the `.vst3` bundle into `C:\Program Files\Common Files\VST3` and the Standalone app into `C:\Program Files`.
  - Created `build_installer.ps1` script to automate building the project in Release mode and compiling the Inno Setup script.
  - Integrated automatic installation of Inno Setup via `winget` if it's missing on the developer machine.

### Changed
- Rebranded the project from "Autotune Clone" to "OpenVoxTuner".
- Updated CMake configuration, package identifiers, and installation paths.
- Redesigned the main logo and application icon.
- Adjusted typography and spacing for the plugin title in the UI.
- Renamed main tabs to "Live" and "Curve Editor" for clarity.
- Moved help text in Curve Editor to bottom-right to avoid overlapping with the time ruler.

### Fixed
- Fixed an issue where the standalone app's playhead wouldn't move by forcing an internal fallback timer when no host is detected.
- Fixed the Inno Setup installer failing when replacing the old single-file `.vst3` with the new folder bundle format.

### Added
- Added `PianoKeyboard` component to the Live tab for real-time visualization.
- Added real-time key highlighting on the piano keyboard (red for input, green for output, gradient for match).
- Implemented vertical zoom and scroll via mouse wheel (`Ctrl`+`Scroll` to zoom, `Scroll` to pan) in both Pitch Visualizer and Pitch Curve Editor.
