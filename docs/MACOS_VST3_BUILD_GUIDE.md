# Build VST3 macOS (OpenVoxTuner)

Ce guide correspond au projet actuel (`OpenVoxTuner`) et à son `CMakeLists.txt`.

## Prérequis macOS

Installer sur Mac :

```bash
xcode-select --install
brew install cmake ninja
```

Cloner JUCE localement (exemple) :

```bash
git clone https://github.com/juce-framework/JUCE.git ~/dev/JUCE
```

## Build rapide avec le script

Depuis la racine du projet :

```bash
chmod +x ./build_macos_vst3.sh
./build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

Ce script :
1. Configure CMake en `Release`
2. Build la target `OpenVoxTuner_VST3`
3. Copie le bundle dans `~/Library/Audio/Plug-Ins/VST3` si `--install` est fourni

## Options utiles du script

```bash
./build_macos_vst3.sh --help
```

Exemples :

```bash
# Build universal (Apple Silicon + Intel), sans installation
./build_macos_vst3.sh --juce-path ~/dev/JUCE --arch "arm64;x86_64"

# Build arm64 uniquement
./build_macos_vst3.sh --juce-path ~/dev/JUCE --arch arm64

# Build dans un dossier custom
./build_macos_vst3.sh --juce-path ~/dev/JUCE --build-dir build-mac-release
```

## Build manuel (sans script)

```bash
cmake -S . -B build-mac -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DJUCE_PATH=~/dev/JUCE \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cmake --build build-mac --config Release --target OpenVoxTuner_VST3
```

Bundle généré (chemin attendu) :

```text
build-mac/OpenVoxTuner_artefacts/Release/VST3/OpenVoxTuner.vst3
```

Copie manuelle vers le dossier utilisateur VST3 :

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
rsync -a --delete "build-mac/OpenVoxTuner_artefacts/Release/VST3/OpenVoxTuner.vst3" \
  ~/Library/Audio/Plug-Ins/VST3/
```

## Scripts macOS disponibles

- `build_macos_vst3.sh` : build VST3 (et installation locale optionnelle)
- `build_macos_au.sh` : build AU (et installation locale optionnelle)
- `build_macos_pkg.sh` : génération d'un installateur `.pkg` (par défaut : `VST3 + AU + STANDALONE`)

Voir aussi : `docs/MACOS_AU_AND_INSTALLER_GUIDE.md`.

## Distribution (optionnel)

Pour distribuer publiquement sur macOS, prévoir :
- `codesign`
- notarization Apple (`notarytool`)
- `stapler`

Le script ci-dessus couvre uniquement le build + installation locale développeur.
