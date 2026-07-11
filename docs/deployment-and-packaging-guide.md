# Deployment and Packaging Guide

This document outlines the requirements and optimal strategies for compiling, packaging, and deploying the OpenVoxTuner audio plugin (VST3, AU, AAX) across multiple platforms (Windows, macOS, Linux).

## 1. VST3 Cross-Platform Compatibility

**Can a Windows `.vst3` file be used on macOS?**
**No.** A `.vst3` file is essentially a bundle (a directory structure) containing platform-specific compiled binaries. 
- On Windows, the VST3 bundle contains a `Contents/x86_64-win/` folder with a PE32+ `.vst3` (which is actually a `.dll`).
- On macOS, the VST3 bundle requires a `Contents/MacOS/` folder containing a Mach-O binary (typically a Universal Binary supporting both Intel `x86_64` and Apple Silicon `arm64`).

**Requirement:** You must natively compile the C++ source code on each target operating system. A Windows-compiled plugin will not be recognized by a macOS DAW.

---

## 2. Compilation Guidelines by Format

### Audio Units (AU)
- **Target:** macOS only.
- **Prerequisites:** macOS environment, Xcode, Command Line Tools.
- **JUCE Configuration:** AU is optional and enabled via the `OVT_ENABLE_AU` CMake option (default `ON` on macOS; VST3 and Standalone are always built). The plugin uses manufacturer code `Eiff` and plugin code `OvtP`. No `AU_MAIN_TYPE` is set, so JUCE defaults to `aufx`.
- **Validation:** Use Apple's `auval` tool via terminal: `auval -v aufx OvtP Eiff`.

### AAX (Pro Tools)
- **Target:** Windows, macOS.
- **Prerequisites:** 
  - An active Avid Developer account.
  - PACE Anti-Piracy SDK and iLok tools installed on the build machines.
- **JUCE Configuration:** Enable `AAX` in the CMake `FORMATS` list. Point the build system to the PACE SDK directory.
- **Validation:** Requires signing the binary with PACE's `wraptool` using an active iLok USB key or cloud session. Test using Pro Tools Developer version.

### Linux Builds (VST3)
- **Prerequisites:** GCC or Clang, Make/Ninja, and Linux development headers (`libasound2-dev`, `libfreetype6-dev`, `libx11-dev`, `libxext-dev`).
- **Validation:** Test with `pluginval` or host in Carla / Bitwig for Linux.

---

## 3. Optimal Deployment Strategy

Distributing raw `.vst3` or `.component` folders is poor practice. The optimal solution involves creating dedicated installers, implementing code signing, and setting up an automated CI/CD pipeline.

### A. Installers
1. **Windows (Inno Setup):**
   - Create an executable installer (`.exe`).
   - Copies `.vst3` to `C:\Program Files\Common Files\VST3\`.
   - Copies the Standalone application to `C:\Program Files\OpenVoxTuner\`.
   - Handles uninstallation cleanly.
2. **macOS (pkgbuild / productbuild):**
   - Create a `.pkg` installer.
   - Copies `.vst3` to `/Library/Audio/Plug-Ins/VST3/`.
   - Copies `.component` (AU) to `/Library/Audio/Plug-Ins/Components/`.
   - Copies the Standalone application (`.app`) to `/Applications/`.

### B. Code Signing and Notarization (Standard vs Unsigned)

**Standard Professional Workflow:**
Modern operating systems actively block unsigned software.
- **macOS:** Requires signing binaries (`codesign`), building a `.pkg`, signing the `.pkg`, and notarizing it via Apple's servers (`xcrun notarytool`).
- **Windows:** Requires an EV Code Signing Certificate and signing both the binaries and the installer using `signtool.exe`.

**Deploying Unsigned Plugins (Cost-Saving Workaround):**
If you choose not to invest in code signing certificates, users *can* still install and use the plugins, but they will face significant security friction:

- **On Windows (VST3):** 
  - **Installer:** Windows SmartScreen will block the `.exe` installer with a blue warning ("Windows protected your PC"). Users must click "More info" -> "Run anyway" to proceed.
  - **DAW Loading:** Once installed, Windows DAWs (FL Studio, Ableton, Studio One) generally do **not** block unsigned `.vst3` DLLs. The plugin will load normally.

- **On macOS (VST3, AU):**
  - **Installer:** Gatekeeper will block the `.pkg` ("Cannot be opened because the developer cannot be verified"). Users must right-click (Control-click) the `.pkg` and select "Open" to bypass this.
  - **DAW Loading (The Quarantine Issue):** When a user downloads a file from the internet, macOS attaches a `com.apple.quarantine` attribute to it. If a DAW tries to load an unsigned, quarantined plugin, it will either crash or silently fail validation (especially true for Logic Pro and AU formats). 
  - **Mandatory User Action:** To make unsigned plugins work on macOS, the user **must** open the Terminal and run a specific command to strip the quarantine attribute from the installed plugins:
    ```bash
    sudo xattr -rd com.apple.quarantine /Library/Audio/Plug-Ins/VST3/YourPlugin.vst3
    sudo xattr -rd com.apple.quarantine /Library/Audio/Plug-Ins/Components/YourPlugin.component
    ```
    Without this terminal command, the plugins are effectively unusable on modern macOS.

### C. Update Mechanism
- **Version Check:** Implement a lightweight HTTP GET request on plugin UI load (e.g., querying a JSON file on your server).
- **Notification:** If the current version is lower than the server version, display an "Update Available" badge in the plugin UI.
- **Execution:** Clicking the badge should open the user's web browser to download the latest installer. **Do not attempt to hot-swap the plugin binary while the DAW is running**, as DAWs heavily lock audio plugin files.

### D. Automation (CI/CD) and GitHub Actions Quotas
Use GitHub Actions to automate the entire process:
- **macOS Runner:** Compiles VST3, AU, and Standalone, builds the PKG, and attaches it to a GitHub Release.
- **Windows Runner:** Compiles VST3 and Standalone, builds the Inno Setup EXE, and attaches it to the release.

**GitHub Actions Pricing & Limits (e.g., for `github.com/EiffelBS`):**
GitHub Actions offers a generous free tier, but the cost depends entirely on whether your repository is **Public** or **Private**:
1. **If the repository is Public:** GitHub Actions is **100% free and unlimited** for Windows, macOS, and Linux runners. You can compile your plugins as often as you want without any restrictions.
2. **If the repository is Private:** You are granted **2,000 free minutes per month**. 
   - *The Multiplier Catch:* GitHub charges different rates depending on the OS. Windows runners consume minutes at a **2x** multiplier, and macOS runners consume minutes at a **10x** multiplier. 
   - *Example:* If compiling your VST3/AU on macOS takes 15 minutes, it will consume 150 minutes of your monthly quota. Compiling on Windows for 10 minutes consumes 20 minutes. Therefore, on a private repository, you will hit the free limit very quickly (after roughly 10-15 full macOS builds per month). If you exceed this, builds will fail unless you set up billing.
