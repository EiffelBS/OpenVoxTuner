<p align="center">
  <a href="https://opensource.org/license/agpl-v3"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg?color=3F51B5&style=for-the-badge&label=License&logoColor=000000&labelColor=ececec" alt="Lizenz: AGPLv3"></a>
  <img src="https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=cplusplus&logoColor=000000&labelColor=ececec" alt="C++17">
  <img src="https://img.shields.io/badge/JUCE-8-orange.svg?style=for-the-badge&labelColor=ececec" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Platform-Win%20%7C%20Mac-lightgrey.svg?style=for-the-badge&labelColor=ececec" alt="Plattformen">
  <img src="https://img.shields.io/badge/Format-VST3%20%7C%20AU%20%7C%20Standalone-green.svg?style=for-the-badge&labelColor=ececec" alt="Formate">
</p>

<p align="center">
  <img src="../assets/icon.png" width="120" alt="OpenVoxTuner-Symbol">
</p>

<h1 align="center">OpenVoxTuner</h1>

<h3 align="center">Echtzeit-Pitchkorrektur &amp; Harmonie-Generierung für Gesang</h3>

<p align="center">
  VST3 / AU / Standalone &mdash; gebaut mit JUCE 8 (C++17)
</p>

<p align="center">
  <a href="#funktionen">Funktionen</a> &bull;
  <a href="#screenshots">Screenshots</a> &bull;
  <a href="#lizenz">Lizenz</a> &bull;
  <a href="#build">Build</a>
</p>

<p align="center">
  <a href="../README.md">English</a> &mdash;
  <a href="README_fr_FR.md">Fran&ccedil;ais</a> &mdash;
  Deutsch &mdash;
  <a href="README_es_ES.md">Espa&ntilde;ol</a> &mdash;
  <a href="README_ja_JP.md">&#26085;&#26412;&#35486;</a> &mdash;
  <a href="README_zh_CN.md">&#20013;&#25991;</a>
</p>

## Inhaltsverzeichnis

- [Screenshots](#screenshots)
- [Funktionen](#funktionen)
- [Warum OpenVoxTuner?](#warum-openvoxtuner)
- [Repository-Struktur](#repository-struktur)
- [Lizenz](#lizenz)
- [Projekt unterstützen](#projekt-unterstützen)
- [Entwickler-Lizenz](#entwickler-lizenz)
- [Mitwirken](#mitwirken)
- [Build](#build)
- [Dokumentation](#dokumentation)

---

## Screenshots

<!-- Screenshots hier einfügen:
<p align="center">
  <img src="../docs/screenshots/live-visualizer.png" width="45%" alt="Live-Visualizer">
  <img src="../docs/screenshots/curve-editor.png" width="45%" alt="Curve-Editor">
</p>
-->

## Funktionen

### Pitchkorrektur

- **Auto-Modus** — Tonhöhen-Quantisierung mit Tonart- und Skalenauswahl (Dur, Moll, Pentatonisch, Chromatisch, Benutzerdefiniert)
- **Grafischer Modus** — zeichnen Sie Ihre eigene Pitchkurve (Melodyne-ähnlicher Editor mit Snap, Raster, Kopieren/Einfügen, Rückgängig/Wiederholen)
- **Geschwindigkeitsregler** — Rückführungs-Hüllkurve für natürliches oder robotisches Korrekturverhalten
- **Amount-Regler** — Mischung zwischen korrigiertem und trockenem Signal

### Effekte

- **Formant-Shift** — unabhängige Formanterhaltung/-transposition (-12 bis +12 Halbtonschritte)
- **Reverb** — integrierter Reverb-Effekt mit einstellbarem Mix-Pegel
- **Noise Gate** — Eingangs-Noise-Gate mit Schwellenwert (-80 bis 0 dB), vor der Pitch-Erkennung angewendet für sauberere Ergebnisse

### A/B-Vergleich & Morphing

- **A/B-Slots** — speichern und abrufen von zwei vollständigen Plugin-Zuständen (A und B)
- **Morph-Regler** — kontinuierliche Interpolation zwischen A- und B-Zustand
- **Auto-Save** — aktueller Slot wird automatisch beim Wechsel gespeichert
- Alle Parameter morphen weich (kontinuierliches Lerp für Regler, Umschaltung bei 50 % für Toggles)
- A/B-Zustand bleibt über Sessions erhalten

### Harmonie-Engine

- **Use-Voice-Modus** — verschiebt Ihren Live-Gesang in Harmonienoten (1–4 verschobene Stimmen mit Stereo-Panning)
- **Synth-Modus** — synthetisierte Harmonietöne (Choir, Bright, Synth Lead, Strings, Guitar, vocoderartig) mit einstellbarer Klangfarbe
- **Harmonie-Typ-Presets** — Terz/Quinte unter/über, Vocal Stack, Power Chord, Parallel 3rd, Drone
- **Volume- & Mix-Regler** für unabhängiges Harmonie-Level und Wet/Dry-Mix
- Harmonie-Spurüberlagerung im Curve-Editor

### Pitch-Erkennung

- **YIN** — Autokorrelation im Zeitbereich (schnell, geringe CPU, einzig verwendeter Detektor)
- SWIPE' und PYIN wurden evaluiert und entfernt (nur YIN aus Geschwindigkeits- und Stabilitätsgründen beibehalten)

### ARA2-Integration

- Voller ARA2-Support (Audio Random Access) — nahtlose DAW-Timeline-Integration
- Automatische Tonart-/Skalenextraktion aus dem ARA-Musikkontext
- Takt-Raster mit Taktart-Erkennung (3/4, 4/4, 6/8, 12/8)
- Auto-Scroll folgt dem DAW-Playhead während der Wiedergabe
- Multi-Signatur-Support (Taktartwechsel mitten im Projekt)

### Curve-Editor

- Grafischer Pitchkurven-Editor mit Punkt ziehen, hinzufügen, löschen
- Snap to Scale, Snap to Grid
- Taktauswahl (1, 2, 4, 8, 16, 32)
- Kopieren/Einfügen und Rückgängig/Wiederholen
- Harmonie-Spurüberlagerung
- Horizontale Cursor-Linie mit Notenname und Hz-Anzeige
- Skalen-Referenzlinien (wie im Live-Visualizer)
- Waveform-Overlay (Line oder Mirror, synchron mit Live-Visualizer)
- Auto-Scroll-Umschalter (funktioniert in allen Modi)

### Live-Visualizer

- Echtzeit-Pitch-Visualisierung mit Eingabe-/Ausgabe-/Harmonie-Spuren
- Klaviertastatur mit automatischer Notenhervorhebung
- Horizontale Cursor-Linie mit Notenname und Hz-Anzeige
- Waveform-Overlay (Line oder Mirror)
- Legendenblock mit Statistiken (in-tune %, durchschnittliche Cent)
- Export als PNG-Bild (2x-Auflösung)

### Waveform-Overlay

- Aus der Eingabe in allen Modi erfasste Audio-Waveform (Plugin, Standalone, ARA)
- Zwei im Menü wählbare Anzeigetypen:
  - **Line** — einfache Waveform-Kontur (40 % Deckkraft)
  - **Mirror** — gespiegelte Balken symmetrisch um die Mitte (Standard)
- Anzeigetyp gilt einheitlich für Live-Visualizer und Curve-Editor
- Präferenz bleibt über Sessions erhalten

### Theme-System

- Dunkle und helle Themen mit einheitlicher Farbpalette
- Automatische Theme-Umschaltung mit vollständigem UI-Refresh
- Konsistente Popup-Menüfarben (Hamburger-Menü, Presets, Combos)
- Korrigierte Tooltips mit sauberem rechteckigem Rendering

### Sonstiges

- MIDI-Notenausgabe (aus erkannter Pitch generiert)
- Stereo-Verarbeitung
- PSOLA-Pitch-Shifting mit geringer Latenz
- Bypass-Umschalter (Standalone-Modus)
- Standalone-Modus mit internem 120-BPM-Transport

## Warum OpenVoxTuner?

- **Open Source by Design** — jede Zeile DSP, UI und Preset-Logik ist öffentlich. Keine Black Box, keine Telemetrie, keine Funktions-Paywalls. Sie können genau nachlesen, wie Ihre Stimme verarbeitet wird.
- **AGPLv3 für Freiheit** — die Lizenz garantiert, dass das Projekt frei und offen bleibt. Jeder darf es nutzen (sogar kommerziell), und Verbesserungen müssen mit der Community geteilt werden.
- **ARA2 nativ** — tiefe DAW-Integration bedeutet, dass Tonart, Skala und Tempo direkt aus Ihrem Projekt gelesen werden. Keine manuelle Einrichtung, kein Raten — OpenVoxTuner folgt Ihrer Arrangement automatisch.
- **Für echten Gesang gebaut** — YIN-Pitch-Erkennung, formanterhaltendes PSOLA und ein Melodyne-artiger Curve-Editor sind auf die Nuancen gesungener Performances abgestimmt, nicht nur auf Proof-of-Concept-Demos.

### KI-unterstützte Entwicklung

OpenVoxTuner nutzt KI-Coding-Assistenten, um die Entwicklung zu beschleunigen, stets unter strenger menschlicher Aufsicht. Jede Codezeile wird überprüft, getestet und vollständig für Community-Audits offengelegt. Es wird kein ungeprüfter KI-generierter Code gemergt.

## Repository-Struktur

```text
.
├─ Source/
│  ├─ dsp/                        # DSP-Module
│  │  ├─ IPitchDetector.h         # Abstrakte Pitch-Detektor-Schnittstelle
│  │  ├─ YinPitchDetector.*       # YIN-Algorithmus (aktiv)
│  │  ├─ ScaleQuantizer.*         # Skalen-Quantisierungs-Engine
│  │  ├─ PitchShifter.*           # PSOLA-Pitch-Shifter
│  │  ├─ RetargetEnvelope.*       # Speed-Hüllkurven-Glätter
│  │  ├─ FormantPreserver.*       # Formant-Kompensationsfilter
│  │  ├─ NoiseGate.h              # Eingangs-Noise-Gate (RMS-basiert, Hysterese)
│  │  ├─ PitchCurve.*             # Curve-Datenmodell
│  │  ├─ PresetMorpher.h          # A/B-Morphing-Engine
│  │  ├─ HarmonyEngine.*          # Harmonie-Synthese-Engine
│  │  ├─ PitchDetector.*          # Originale YIN-Referenz (nicht kompiliert)
│  │  └─ NoteUtils.h / IPitchShifter.h
│  ├─ ui/                         # UI-Komponenten
│  │  ├─ PitchCurveEditor.*       # Curve-Editor-Komponente
│  │  ├─ PitchVisualizer.*        # Pitch-Visualisierung
│  │  ├─ PianoKeyboard.*          # Klavier-Widget
│  │  ├─ ScaleKeyboardComponent.* # Skalen-Tastatur-Anzeige
│  │  ├─ LookAndFeel.*            # Eigener Look and Feel
│  │  ├─ OVTTheme.h               # Theme-Farben und gemeinsamer Waveform-Renderer
│  │  ├─ OVTFonts.h               # Font-Helfer
│  │  └─ OVTLanguages.h           # Mehrsprachige Übersetzungen
│  ├─ external/presonus/          # PreSonus-Plugin-Erweiterungen (Studio One)
│  ├─ resources/                  # Binäre Ressourcen (BuildInfo.h.in)
│  ├─ PluginProcessor.*           # Haupt-Audio-Prozessor
│  └─ PluginEditor.*              # Haupt-Editor-UI
├─ scripts/                       # Build- und Entwicklungsskripte
│  ├─ build.ps1                   # Windows-Build
│  ├─ build_installer.ps1         # Windows-Installer (Inno Setup)
│  ├─ build_macos_vst3.sh         # macOS VST3-Build
│  ├─ build_macos_au.sh           # macOS AU-Build
│  ├─ build_macos_pkg.sh          # macOS .pkg-Installer
│  ├─ build_macos.sh              # macOS Universal-Build
│  └─ ... (Installations-, Symlink-, Release-Helfer)
├─ test/                          # Unit-Tests (Catch2)
│  ├─ Main.cpp
│  └─ dsp/                        # Test-Suites pro Modul
├─ docs/                        # Dokumentation
│  ├─ releases/                   # Release-Notes (latest.json, v0.1.1.md)
│  ├─ architecture.md
│  ├─ default-parameters.md
│  └─ ...
├─ installer/                     # Windows-Installer-Ressourcen
│  └─ OpenVoxTuner.iss            # Inno-Setup-Skript
├─ .github/                       # CI/CD und Issue-Vorlagen
│  ├─ workflows/                  # GitHub Actions (CI, Release)
│  └─ ISSUE_TEMPLATE/             # Bug report / Feature request
├─ assets/                        # Binäre Ressourcen
│  └─ icon.png
├─ external/ARA_SDK/              # Celemony ARA SDK (v2.2, Submodule)
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ roadmap.md
├─ .gitignore
├─ .gitattributes
└─ .gitmodules
```

## Lizenz

OpenVoxTuner ist **für alle kostenlos** unter der [AGPLv3](../LICENSE)-Lizenz — Musiker, Produzenten, Studios, Pädagogen. Keine Einschränkungen bei kommerzieller Nutzung.

### Drittanbieter-Lizenzen

| Bibliothek | Lizenz | Kompatibilität |
|---------|---------|--------------|
| JUCE 8 | AGPLv3 | Gleiche Lizenz |
| ARA SDK | Apache 2.0 | Voll kompatibel |
| PreSonus Extensions | Public Domain | Voll kompatibel |
| Catch2 (Tests) | Boost (BSL-1.0) | Voll kompatibel |

Alle Drittanbieter-Lizenzen sind mit AGPLv3 kompatibel.

### Was AGPLv3 für Sie bedeutet

| Sie sind... | Kostenlos? | Verpflichtung? |
|---|---|---|
| Musiker / Produzent | Ja | Keine — machen Sie einfach Musik |
| Studio (Mix, Master, Produktion) | Ja | Keine — Sie nutzen das Plugin als Werkzeug |
| Pädagoge / Student | Ja | Keine |
| Entwickler (modifiziert & redistribuiert) | Ja | Sie müssen Ihren modifizierten Quellcode unter AGPLv3 teilen |
| Firma (Fork in ein Closed-Source-Produkt) | Nein | Sie benötigen eine kommerzielle Lizenz |

> In der Praxis ist die AGPLv3-Lizenz, wenn Sie OpenVoxTuner zur Musikproduktion nutzen — selbst professionell — völlig kostenlos ohne Verpflichtungen.

### Projekt unterstützen

OpenVoxTuner ist für alle kostenlos. Wenn OpenVoxTuner Ihnen Zeit spart oder Ihre Musik unterstützt, erwägen Sie, das Projekt zu unterstützen — selbst eine kleine Spende macht einen riesigen Unterschied.

| Stufe | Plattform | Preis | Vorteile |
|------|----------|-------|----------|
| **Kostenlos** | — | 0 € | Volles Plugin, alle Funktionen |
| **Buy me a coffee** | [Ko-fi](https://ko-fi.com/) | Einmalig | Ein schnelles Dankeschön ❤️ |
| **Sponsor** | [GitHub Sponsors](https://github.com/sponsors/) | Monatlich | Unterstützung der laufenden Entwicklung |
| **Supporter** | [Patreon](https://patreon.com/) | 5 €/Monat | Privater Discord + Abstimmung über kommende Features |
| **Gold** | [Patreon](https://patreon.com/) | 20 €/Monat | Alle Supporter-Vorteile + Early Access / Beta-Builds + Name in Credits |

Jeder Beitrag hilft, das Projekt am Leben und frei für alle zu erhalten.

### Entwickler-Lizenz

Eine kommerzielle Lizenz ist für Entwickler oder Firmen verfügbar, die OpenVoxTuner in ein **Closed-Source-Produkt** integrieren möchten, ohne die AGPLv3-Copyleft-Verpflichtung.

**Was sie gewährt:**
- Erlaubnis, OpenVoxTuners DSP, UI-Komponenten und Algorithmen in proprietärer Software zu nutzen
- Keine Copyleft-Verpflichtung — Sie sind **nicht** verpflichtet, Ihren Quellcode zu veröffentlichen
- Keine Anforderung, Abwandlungen unter AGPLv3 zu veröffentlichen

**Was sie enthält:**
- Prioritärer E-Mail-Support
- Optionale Custom-Features und DSP-Beratung
- Perpetuelle Lizenz für die gekaufte Version (Updates je nach Stufe)

**Kontakt:** Issue auf GitHub öffnen oder an [license@openvoxtuner.com](mailto:license@openvoxtuner.com) schreiben.

## Mitwirken

Beiträge sind willkommen! Sie können helfen durch:

- Beheben von Bugs
- Verbesserung von DSP-Algorithmen
- Hinzufügen von Übersetzungen
- Verbesserung der UI
- Schreiben von Dokumentation
- Testen der DAW-Kompatibilität

Details siehe [CONTRIBUTING.md](../CONTRIBUTING.md).

### Review KI-generierten Codes

Einige Teile von OpenVoxTuner werden mit Hilfe von KI-Coding-Agenten geschrieben, stets unter menschlicher Aufsicht. Alle Codes werden vor dem Merge überprüft, und Community-Pull-Requests sind ausdrücklich erwünscht, um KI-assistierte Abschnitte zu auditieren, zu verbessern oder zu ersetzen.

## Build

### Windows (Visual Studio)

Voraussetzungen:
- Visual Studio 2022
- CMake >= 3.22
- JUCE 8 (Standardpfad in CMake: `C:/JUCE`)
- Git LFS (für binäre Ressourcen)

```powershell
# Debug-Build
.\scripts\build.ps1 -Configuration Debug

# Release-Build
.\scripts\build.ps1 -Configuration Release

# Windows-Installer bauen (benötigt Inno Setup)
.\scripts\build_installer.ps1
```

### macOS (VST3 / AU / pkg)

Voraussetzungen:

```bash
xcode-select --install
brew install cmake ninja
```

VST3 bauen:

```bash
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

AU bauen:

```bash
./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install
```

macOS `.pkg`-Installer bauen. Die offiziellen Releases liefern **VST3 + Standalone** (das AU wird weggelassen, da ein unsigniertes AU nicht von einem DAW geladen werden kann):

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

Um die AU-Komponente auch in einem lokalen Build einzuschließen, fügen Sie sie zu `--formats` hinzu (z. B. `VST3,AU,STANDALONE`).

Detaillierte Build-Guides:
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## Formate

| Format   | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅*   |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

> \* Die AU-Komponente ist aus den Quellen baubar, wird aber in den unsignierten Releases **nicht ausgeliefert** — ein unsigniertes AU kann von einem DAW auf macOS nicht geladen werden. Die verteilten Releases enthalten **VST3 + Standalone** auf beiden Plattformen.

## Dokumentation

- [docs/architecture.md](../docs/architecture.md) — Überblick über die Software-Architektur
- [docs/default-parameters.md](../docs/default-parameters.md) — Referenz aller Plugin-Parameter
- [docs/implementation-roadmap.md](../docs/implementation-roadmap.md) — Feature-Roadmap
- [docs/ARA_Specifications.md](../docs/ARA_Specifications.md) — Technische Spezifikationen des ARA2-Supports
- [docs/deployment-and-packaging-guide.md](../docs/deployment-and-packaging-guide.md) — Release-Workflow
- [docs/GITHUB_SETUP_AND_RELEASE.md](../docs/GITHUB_SETUP_AND_RELEASE.md) — GitHub-Repository-Einrichtung
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md) — macOS VST3-Build-Guide
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md) — macOS AU + Installer-Guide
- [docs/preset-morphing-technical-strategy.md](../docs/preset-morphing-technical-strategy.md) — A/B-Preset-Morphing-Strategie
- [docs/releases/v0.1.1.md](../docs/releases/v0.1.1.md) — Release-Notes

## Lizenz

Siehe [LICENSE](../LICENSE).

## Issue-Meldung

Verwenden Sie die GitHub-Issue-Vorlagen:
- [Bug report](../.github/ISSUE_TEMPLATE/bug_report.md)
- [Feature request](../.github/ISSUE_TEMPLATE/feature_request.md)

## Star-Historie

<a href="https://star-history.com/#EiffelBS/OpenVoxTuner&type=date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" width="100%" />
  </picture>
</a>
