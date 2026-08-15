# Changelog - 2026-08-15

## macOS CI fix — ovtchord Clang compile error (2026-08-15)
- **Problem**: the macOS Release build (`build-ci-mac`) failed in `libs/ovtchord` with AppleClang: "default member initializer for 'X' needed within definition of enclosing class 'Y' outside of member functions". The nested `struct Config` types carry *default member initializers* (`int minNotes = 3;`), and using `Config()` as an **in-class default argument** (`explicit Y (const Config& cfg = Config());`) is ill-formed per the C++ standard. MSVC accepts it as an extension, which is why the Windows build/tests passed.
- **Fix**: split each affected constructor into a no-arg constructor plus a `Config`-taking constructor, both declared in the header and defined **out-of-line** in the `.cpp`. The no-arg constructor delegates (`Y() : Y (Config()) {}`), so all existing call sites are unchanged:
  - `ChordEngine e;` (tests, benchmark, C API)
  - `AudioProcessor ap (cfg);` and `AudioProcessor audio;` (C API)
  - member default construction of `AudioPreprocessor` / `ChromaExtractor`
- **Files changed**: `libs/ovtchord/include/ovtchord/chord_engine.h`, `libs/ovtchord/src/core/chord_engine.cpp`, `libs/ovtchord/src/audio/audio_processor.h/.cpp`, `libs/ovtchord/src/audio/chroma.h/.cpp`, `libs/ovtchord/src/audio/preprocess.h/.cpp`.
- **Verification (Windows / MSVC)**:
  - `ovtchord` builds clean; unit tests: **65 checks / 0 failures**.
  - Full plugin unit-test suite: **102 OK / 0 KO**.
  - `OpenVoxTuner_VST3` (Release) builds clean.
- The real validation is the macOS CI run itself (AppleClang) once this fix is pushed.

## Tests
- N/A beyond the re-verification above (no behavioral change; constructor API preserved).
