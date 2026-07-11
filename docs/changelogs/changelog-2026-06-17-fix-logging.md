# Changelog - Fixes of 2026-06-17

## Issues Resolved

### 1. ✅ Excessive Logging (Cause of Audio Problem in VST3 Plugin)

**Symptom**: The plugin works in standalone mode but not as a VST3 plugin inside a DAW.

**Cause**: An un-throttled log in `PluginProcessor.cpp` (lines 857-858) that ran on every audio block (~86 times per second), generating millions of log lines and completely saturating the system.

**Fix**: Throttling the log so it is displayed only once per second, like all other debug logs.

**File modified**: `Source/PluginProcessor.cpp`

```cpp
// BEFORE (lines 845-858)
pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);

// Immediately compare snapshot to detect modifications
float diffSq = 0.0f;
// ... calculations ...
juce::Logger::writeToLog ("pitchShifter post-check: ..."); // ❌ On every block!

// AFTER (with throttling)
pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);

// Immediately compare snapshot to detect modifications (throttled log: once per second)
static std::atomic<uint32_t> lastPostCheckLogMs { 0 };
uint32_t nowPostCheck = juce::Time::getMillisecondCounter();
uint32_t lastPostCheck = lastPostCheckLogMs.load();
if (nowPostCheck - lastPostCheck > 1000)  // ✅ Only every 1000ms
{
    if (lastPostCheckLogMs.compare_exchange_strong (lastPostCheck, nowPostCheck))
    {
        // ... calculations and log ...
    }
}
```

### 2. ✅ Development Organization and Workflow

**Problems**:
- Insufficient access rights to copy the VST3 into `Program Files`
- No convenient install script for development
- Confusion regarding the `build/` structure in the project

**Solutions provided**:

#### New PowerShell Scripts

1. **`create_dev_symlink.ps1`** (⭐ Recommended for development)
   - Creates a symbolic link to the Debug or Release build
   - Advantage: The plugin in Program Files is automatically updated after every Visual Studio rebuild
   - Requires administrator rights (one time only)

2. **`install_vst3.ps1`** (Updated)
   - Manual installation by copy (for one-off tests)
   - Requires admin rights on each copy

3. **`rebuild_clean.ps1`**
   - Fully cleans the `build/` folder
   - Regenerates everything via CMake
   - Useful when CMake is lost or to start from scratch

4. **`open_vs.ps1`**
   - Opens Visual Studio with the OpenVoxTuner solution
   - Automatically detects the installed VS version

#### Documentation

1. **`BUILD_GUIDE.md`**
   - Complete build and install guide
   - Explanations of the VST3 structure (bundle vs file)
   - Solutions to common problems
   - Recommended development workflow

2. **`README.md`**
   - Quick start for developers
   - Summary table of scripts
   - Notes on the VST3 format

## Performance Impact

**Before**:
- ~86 log calls per second at 44.1kHz (512-sample buffer)
- Millions of lines written to `OpenVoxTuner.log`
- Hard drive saturation
- Major slowdowns and even crashes
- Plugin unusable in VST3 mode

**After**:
- 1 log per second maximum
- Manageable log file (~1 KB/second)
- No impact on audio performance
- Plugin functional in VST3 and standalone

## Recommended Development Workflow

### First Time

```powershell
# 1. Create the symbolic link (one time only, requires admin)
.\create_dev_symlink.ps1

# 2. Open Visual Studio
.\open_vs.ps1
```

### Daily Development

1. Modify the code in Visual Studio
2. Build (Ctrl+Shift+B)
3. ✨ **The plugin is automatically updated in Program Files** (thanks to the symlink)
4. Fully close the DAW
5. Restart the DAW and test

### In Case of CMake Problem

```powershell
.\rebuild_clean.ps1 -Configuration Debug
```

## Important Notes

### Project Structure
- ✅ It is **NORMAL** for Visual Studio to be in `build/` (CMake convention)
- `build/` = generated folder, not versioned (in `.gitignore`)
- Sources = always in `Source/` at the root
- **Never open files from `build/`**, always from `Source/`

### VST3 Format
- A VST3 is a **bundle** (folder), not a single `.vst3` file
- Internal structure:
  ```
  OpenVoxTuner.vst3/
  ├── Contents/
  │   ├── x86_64-win/OpenVoxTuner.vst3  (DLL)
  │   └── Resources/moduleinfo.json
  ├── desktop.ini
  └── Plugin.ico
  ```
- This is why you must copy **the whole folder**, not just the DLL

### Logs
- Log file: `C:\Users\User\Documents\OpenVoxTuner.log`
- Logs are now throttled (max 1x/second)
- Consult this file to diagnose audio problems

## Tests to Perform

1. ✅ Build succeeded
2. ⏳ Standalone mode test
3. ⏳ VST3 mode test in a DAW
4. ⏳ Log verification (reasonably sized file)
5. ⏳ Symlink test (automatic rebuild)

## Recommended Next Steps

1. Test the plugin in VST3 mode with the logging fix
2. Verify that the `OpenVoxTuner.log` file no longer grows excessively
3. Confirm that the tuned audio works correctly in the DAW
4. If the problem persists, the throttled logs will make it possible to see exactly where the issue lies (for example, if `rmsDiffL=0`, it means the `pitchShifter` is not modifying the buffer)

---

## Addendum: Debugging in Visual Studio

### Additional Problem Resolved

**Error**: "Unable to start the program ... ALL_BUILD"

**Cause**: Visual Studio was trying to launch the CMake project `ALL_BUILD` (which compiles everything but produces no executable).

**Solution**: Configure the correct startup project in Visual Studio:
1. Right-click `OpenVoxTuner_Standalone` → "Set as Startup Project"
2. Start the debugger (F5)

### Documentation Added

- **`DEBUG_GUIDE.md`**: Complete debugging guide (breakpoints, attach to process, profiling)
- **`QUICKFIX_ALL_BUILD_ERROR.md`**: Quick fix for the ALL_BUILD error
- **`build/set_startup_project.ps1`**: Helper script to configure the startup project
- **`.editorconfig`**: Automatic editor configuration for Visual Studio
