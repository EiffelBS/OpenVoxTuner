# OpenVoxTuner Documentation

!!! info "Documentation in development"
    This documentation is a **work in progress**. Some pages may be incomplete or not yet available. If you find a broken link or missing content, please [open an issue](https://github.com/EiffelBS/OpenVoxTuner/issues).

> **Real-time pitch correction & harmony generation for vocals**  
> VST3 / AU / Standalone — Open Source AGPLv3

---

## Quick Links

| | |
|---|---|
| [:material-download: **Download Latest Release**](https://github.com/EiffelBS/OpenVoxTuner/releases) | [:material-github: **GitHub Repository**](https://github.com/EiffelBS/OpenVoxTuner) |
| [:material-book-open-variant: **User Guide**](user-guide/quickstart.md) | [:material-cog: **Parameters Reference**](default-parameters.md) |
| [:material-code-braces: **Architecture**](architecture.md) | [:material-hammer-wrench: **Build Guide**](build-guide.md) |

---

## What is OpenVoxTuner?

OpenVoxTuner is a **real-time vocal pitch correction and harmony generation plugin** built with JUCE 8 (C++17). It provides:

- **Auto Mode**: Scale-aware pitch correction with adjustable speed, amount, and character
- **Graphic Mode**: Melodyne-style curve editor for manual pitch editing
- **Harmony Engine**: 21 harmony types with formant-preserved pitch shifting (up to 4 voices)
- **ARA2 Integration**: Full DAW timeline sync with key/scale detection
- **Cross-platform**: Windows (VST3/CLAP), macOS (VST3/AU/CLAP), Linux (VST3/CLAP)

---

## Key Features

### Pitch Correction
- **YIN pitch detector** with configurable threshold
- **Scale quantization** with 14 built-in scales + custom scale editor
- **Speed control** (0-200ms) for natural to hard-tuned sound
- **Amount control** (0-100%) for blending corrected/original signal
- **Correction modes**: Transparent (gentle) vs Modern (aggressive)

### Harmony Engine
- **21 harmony types**: intervals, triads, vocal stacks, octaves
- **Use Voice mode**: pitch-shifted live vocal (formant-preserved)
- **Synth mode**: 6 tone colors (Choir, Organ, etc.) with tone color parameter
- **Voice-type aware**: Bass, Baritone, Tenor, Alto, Soprano, Universal
- **Independent formant shift** (±5 semitones) per voice

### ARA2 Integration
- Automatic key/scale detection from audio
- Time-signature aware ruler in curve editor
- Playhead follow & loop sync with DAW
- Multi-signature project support

---

## Quick Start

```bash
# 1. Download your platform
Windows:  VST3 / CLAP / Standalone (~45 MB)
macOS:    VST3 / AU / CLAP / Standalone (~52 MB)
Linux:    VST3 / CLAP / Standalone (~48 MB)

# 2. Install
Windows:  Copy .vst3 to C:\Program Files\Common Files\VST3\
          Copy .clap to C:\Program Files\Common Files\CLAP\
macOS:    Copy .vst3 to ~/Library/Audio/Plug-Ins/VST3/
          Copy .component to ~/Library/Audio/Plug-Ins/Components/
Linux:    Copy .vst3 to ~/.vst3/ or /usr/lib/vst3/

# 3. Load in your DAW
- Insert on vocal track
- Enable ARA2 if supported (Reaper, Studio One, Logic, Cubase, Bitwig)
- Select key/scale or enable auto-detection
- Adjust Speed & Amount to taste
```

---

## License

OpenVoxTuner is licensed under **AGPLv3** — free for personal and commercial use, with source code available on GitHub.

[View License →](license.md)

---

## Support the Project

- [:material-star: **Star on GitHub**](https://github.com/EiffelBS/OpenVoxTuner)
- [:material-bug: **Report Issues**](https://github.com/EiffelBS/OpenVoxTuner/issues)
- [:material-forum: **Discussions**](https://github.com/EiffelBS/OpenVoxTuner/discussions)
- [:material-coffee: **Sponsor**](https://github.com/sponsors/EiffelBS)

---

*Built with :heart: by EiffelBS using JUCE 8. Documentation generated with MkDocs Material.*