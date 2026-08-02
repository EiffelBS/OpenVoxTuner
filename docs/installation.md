# Installation

OpenVoxTuner is a real-time pitch correction & harmony generation plugin for vocals,
available as **VST3**, **AU**, and a **Standalone** application, built with JUCE 8
(C++17) and licensed under **AGPLv3**.

This page explains how to install OpenVoxTuner on Windows and macOS, either with the
official installers or by installing plugin files manually.

## System requirements

| | Windows | macOS |
|---|---|---|
| OS | Windows 10 / 11 (64-bit) | macOS 11.0 (Big Sur) or later |
| Architecture | x64 | Universal (arm64 + x86_64) |
| Formats | VST3, Standalone | VST3, AU, Standalone |
| ARA2 | ✅ | ✅ |

!!! note "About the AU plugin"
    The **AU** format is buildable from source, but it is **not shipped** in the
    official releases for now — an unsigned AU cannot be loaded by a DAW on macOS.
    This is a **temporary** situation: code signing & notarization (including AU) are
    planned for a later release. Until then, if you need the AU,
    [build it from source](build-guide.md).

## Official installers

OpenVoxTuner is distributed as GitHub Releases for each version:

| Platform | Artifact | Notes |
|----------|----------|-------|
| Windows | `OpenVoxTuner_Windows_Installer.exe` | Installer (Inno Setup) |
| macOS | `OpenVoxTuner-macOS.zip` | Drag-and-drop: VST3 + Standalone (universal arm64/x86_64) |
| macOS | `OpenVoxTuner-macOS.pkg` | Installer (temporarily unsigned): VST3 → `/Library/Audio/Plug-Ins/VST3`, Standalone → `/Applications` |

!!! warning "The macOS `.pkg` is temporarily unsigned"
    The macOS `.pkg` is **unsigned** for now — code signing and notarization are
    **planned for a later release** and this limitation will be removed. To install it
    today, right-click (Control-click) the package and choose **Open**, or run:

    ```bash
    sudo installer -pkg OpenVoxTuner-macOS.pkg -target /
    ```

    The macOS `.zip` also contains **VST3 + Standalone** only — the AU is not included
    in the current releases (it will be added once signing is in place).

## Windows installation

### Option 1 — Installer (recommended)

1. Download `OpenVoxTuner_Windows_Installer.exe` from the latest [GitHub Release](https://github.com/EiffelBS/OpenVoxTuner/releases).
2. Run the installer. It copies:
   - the **VST3** plugin to `C:\Program Files\Common Files\VST3\`
   - the **Standalone** application to `C:\Program Files\OpenVoxTuner\`
3. The installer also handles uninstallation cleanly.

!!! note "SmartScreen"
    Because the installer is not code-signed, Windows SmartScreen may show a blue
    warning ("Windows protected your PC"). Click **More info → Run anyway** to proceed.

### Option 2 — Manual copy

If you built the plugin yourself or received the `.vst3` / `.clap` files, copy them
to the system-wide plugin folders:

```powershell
# VST3
Copy-Item .\OpenVoxTuner.vst3 "C:\Program Files\Common Files\VST3\" -Recurse

# CLAP (if a CLAP build is available)
Copy-Item .\OpenVoxTuner.clap "C:\Program Files\Common Files\CLAP\"
```

!!! tip "Rescan your DAW"
    After copying, restart your DAW or trigger a plugin rescan so the newly added
    plugin is discovered.

## macOS installation

### Option 1 — Drag-and-drop `.zip`

1. Download `OpenVoxTuner-macOS.zip`.
2. Unzip it and drag **OpenVoxTuner.vst3** to `~/Library/Audio/Plug-Ins/VST3/`.
3. Drag the **OpenVoxTuner.app** (Standalone) to your `Applications` folder.

```bash
# Manual equivalent
mkdir -p ~/Library/Audio/Plug-Ins/VST3
rsync -a --delete ~/Downloads/OpenVoxTuner.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

### Option 2 — `.pkg` installer

1. Download `OpenVoxTuner-macOS.pkg`.
2. Right-click (Control-click) the package and choose **Open**, or run:

   ```bash
   sudo installer -pkg OpenVoxTuner-macOS.pkg -target /
   ```

   This installs:
   - **VST3** → `/Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3`
   - **Standalone** → `/Applications/OpenVoxTuner.app`

### Building the AU (from source)

The **AU** is not shipped in the official releases (temporarily — signing & notarization,
which will include AU, are planned for a later release). But you **can** use it locally by
building it from source and ad-hoc signing it:

1. [Build the AU](build-guide.md) with `./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install`.
2. The component installs to `~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component`.
3. **Ad-hoc sign** it (satisfies Apple Silicon's code-signing requirement; the build may
   already be ad-hoc signed — run this to be sure):

   ```bash
   codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component
   ```

4. **Remove the quarantine attribute** (Gatekeeper will otherwise block it):

   ```bash
   sudo xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component
   ```

5. Restart your DAW and rescan the plugin (in Logic: **Plug-in Manager → Reset & Rescan**).

!!! warning "DAW AU validation"
    Some DAWs (Logic Pro, GarageBand) run a **strict AU validation** and may still reject
    an unsigned / non-notarized AU. This is the reason the AU is not shipped in the
    official releases yet. If a DAW blocks it, open **System Settings → Privacy &
    Security** and click **Allow Anyway** when OpenVoxTuner is reported as blocked.

## Standalone application

The **Standalone** version runs OpenVoxTuner as a desktop application without a DAW.
It includes an internal 120 BPM transport and is useful for quick previews and testing.

- **Windows:** `C:\Program Files\OpenVoxTuner\OpenVoxTuner.exe`
- **macOS:** `/Applications/OpenVoxTuner.app`

## Loading in a DAW

1. After installing, restart your DAW.
2. Add OpenVoxTuner to a vocal track (or any audio track).
3. On macOS, if the plugin was quarantined and the DAW silently fails to load it,
   clear the quarantine attribute (see above) and rescan.
4. In DAWs that support it, ARA2 integration provides seamless timeline integration;
   otherwise the plugin reads key/scale from the audio input (auto or via the
   OpenVoxKey companion plugin on a sidechain bus).

!!! tip "OpenVoxKey companion plugin"
    OpenVoxTuner ships with a companion **OpenVoxKey** plugin (VST3/AU) that you place
    on an accompaniment track. It detects the musical key/scale and publishes it to the
    main instance via shared memory IPC — set **Key Source** to **Companion** on
    OpenVoxTuner to use it.

## Uninstalling

- **Windows:** use the Inno Setup installer's uninstall entry (Add or Remove Programs).
- **macOS:**
  ```bash
  rm -rf "/Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3"
  rm -rf "~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component"
  rm -rf /Applications/OpenVoxTuner.app
  ```

## Where to go next

- [Build Guide](build-guide.md) — build OpenVoxTuner from source on Windows and macOS.
- [Testing](testing.md) — how the unit test suite works.
- [Contributing](contributing.md) — how you can help the project.
