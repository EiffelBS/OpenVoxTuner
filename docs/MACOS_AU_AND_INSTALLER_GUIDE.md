# macOS : Build AU + Installateur `.pkg`

Ce document couvre les deux scripts :
- `build_macos_au.sh`
- `build_macos_pkg.sh`

## Prérequis

```bash
xcode-select --install
brew install cmake ninja
```

JUCE doit être disponible localement (exemple `~/dev/JUCE`).

## 1) Build AU (Audio Unit)

Script :

```bash
chmod +x ./build_macos_au.sh
./build_macos_au.sh --juce-path ~/dev/JUCE --install
```

Résultat attendu :
- artefact : `build-mac-au/OpenVoxTuner_artefacts/Release/AU/OpenVoxTuner.component`
- install locale (option `--install`) : `~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component`

Options utiles :

```bash
./build_macos_au.sh --help
```

## 2) Générer un installateur macOS `.pkg`

Script :

```bash
chmod +x ./build_macos_pkg.sh
./build_macos_pkg.sh --juce-path ~/dev/JUCE
```

Par défaut, le script :
- build `VST3` + `AU` + `STANDALONE`
- génère `dist/OpenVoxTuner-macOS.pkg`

### Emplacements installés par le `.pkg`

- `/Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3`
- `/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component`
- `/Applications/OpenVoxTuner.app` (Standalone)

### Exemples

VST3 seulement :

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3
```

AU seulement :

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats AU
```

Standalone seulement :

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats STANDALONE
```

VST3 + Standalone :

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

Sans rebuild (package depuis artefacts existants) :

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --skip-build
```

Signer le package (Developer ID Installer) :

```bash
./build_macos_pkg.sh \
  --juce-path ~/dev/JUCE \
  --sign-installer "Developer ID Installer: Votre Nom (TEAMID)"
```

Toutes les options :

```bash
./build_macos_pkg.sh --help
```

## Notes importantes

- Le projet CMake a été préparé pour activer `AU` sur macOS via l'option `OVT_ENABLE_AU`.
- Le `.pkg` généré installe dans :
  - `/Library/Audio/Plug-Ins/VST3` (si VST3 inclus)
  - `/Library/Audio/Plug-Ins/Components` (si AU inclus)
- Pour distribution publique, ajouter ensuite notarization (`notarytool`) et `stapler`.
