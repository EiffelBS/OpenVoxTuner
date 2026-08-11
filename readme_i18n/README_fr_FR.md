<p align="center">
  <a href="https://opensource.org/license/agpl-v3"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg?color=3F51B5&style=for-the-badge&label=License&logoColor=000000&labelColor=ececec" alt="Licence : AGPLv3"></a>
  <img src="https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=cplusplus&logoColor=000000&labelColor=ececec" alt="C++17">
  <img src="https://img.shields.io/badge/JUCE-8-orange.svg?style=for-the-badge&labelColor=ececec" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Platform-Win%20%7C%20Mac-lightgrey.svg?style=for-the-badge&labelColor=ececec" alt="Plateformes">
  <img src="https://img.shields.io/badge/Format-VST3%20%7C%20AU%20%7C%20Standalone-green.svg?style=for-the-badge&labelColor=ececec" alt="Formats">
</p>

<p align="center">
  <img src="../assets/icon.png" width="120" alt="Icône OpenVoxTuner">
</p>

<h1 align="center">OpenVoxTuner</h1>

<h3 align="center">Correction de hauteur et génération d'harmonie en temps réel pour la voix</h3>

<p align="center">
  VST3 / AU / Standalone &mdash; construit avec JUCE 8 (C++17)
</p>

<p align="center">
  <a href="#fonctionnalités">Fonctionnalités</a> &bull;
  <a href="#captures">Captures</a> &bull;
  <a href="#licence">Licence</a> &bull;
  <a href="#build">Build</a> &bull;
  <a href="https://openvoxtuner.eiffelbs.ovh" target="_blank">Site web</a> &bull;
  <a href="https://ovtdocs.eiffelbs.ovh" target="_blank">Docs</a>
</p>

<p align="center">
  <a href="../README.md">English</a> &mdash;
  Fran&ccedil;ais &mdash;
  <a href="README_de_DE.md">Deutsch</a> &mdash;
  <a href="README_es_ES.md">Espa&ntilde;ol</a> &mdash;
  <a href="README_ja_JP.md">&#26085;&#26412;&#35486;</a> &mdash;
  <a href="README_zh_CN.md">&#20013;&#25991;</a>
</p>

## Table des matières

[Captures](#captures) &bull;
[Fonctionnalités](#fonctionnalités) &bull;
[Pourquoi OpenVoxTuner ?](#pourquoi-openvoxtuner-) &bull;
[Structure du dépôt](#structure-du-dépôt) &bull;
[Licence](#licence) &bull;
[Soutenir le projet](#soutenir-le-projet) &bull;
[Licence développeur](#licence-développeur) &bull;
[Contribuer](#contribuer) &bull;
[Build](#build) &bull;
[Documentation](#documentation)

---

## Captures

<p align="center">
  <img src="../assets/screenshots/main_screen.png" width="80%" alt="Fenêtre principale OpenVoxTuner">
</p>

<details>
<summary><strong>Plus de captures...</strong></summary>

<p align="center">
  <img src="../assets/screenshots/curve_editor.png" width="45%" alt="Éditeur de courbe">
  <img src="../assets/screenshots/curve_pianoroll.png" width="45%" alt="Vue Piano Roll">
</p>
<p align="center">
  <img src="../assets/screenshots/harmony_types.png" width="80%" alt="Types d'harmonie">
</p>

</details>

## Fonctionnalités

### Correction de hauteur

- **Mode auto** — quantification sur 14 types de gamme (Majeur, Mineur, Pentatonique, Blues, Dorian, Phrygien, Lydien, Mixolydien, Locrien, Chromatique, Personnalisé...)
- **Mode graphique** — dessinez votre propre courbe de hauteur (éditeur graphique avec aimantation, grille, copier/coller, annuler/rétablir)
- **Mode de correction** — Modern (serré) ou Transparent (doux)
- **Vitesse & montant** — enveloppe de rattrapage + mélange sec/humide pour une réponse naturelle ou robotique
- **Humanize** — ajout de variations aléatoires subtiles pour un son plus naturel (0-50 cents)
- **Préservation du vibrato** — conserve le vibrato naturel du chanteur à travers la correction (0-100%)
- **Type de voix** — contraint la plage de détection (Universel, Basse, Baryton, Ténor, Alto, Soprano)
- **Mode de latence** — Monitoring direct, Basse latence, Qualité, Sécurité

### Détection de tonalité

- **Auto** — détection en temps réel depuis l'entrée audio (profils Krumhansl-Schmuckler)
- **OpenVoxKey** — pont compagnon via mémoire partagée IPC
- **Sidechain** — analyse de l'accompagnement via un bus sidechain dédié

### Effets

- **Traitement de formants** — 3 modes (Legacy, MultiFormant, Allpass) avec plusieurs stratégies de préservation ( LPC cross-synthesis disponible)
- **Réverbération** — réverb intégrée avec mix réglable
- **Gate de bruit** — gate d'entrée avec seuil (-80 à 0 dB)
- **Compresseur ascendant** — relève les passages calmes avant la détection de hauteur

### Moteur d'harmonie

- **Mode Use Voice** — transpose votre voix live en 1 à 4 voix d'harmonie avec pan stéréo
- **Mode Synth** — tonalités d'harmonie synthétisées (Choir, Organ) avec couleur réglable
- **22 types d'harmonie** — intervalles (3e/4e/5e/ octave en dessous/au-dessus), Vocal Stack, Power Chord, Drone, Unison...
- **Contrôles d'harmonie** — Gain Match (équilibrage RMS auto), Follow Lead, Attack par voix, Formant Shift (-5 à +5 st)
- Tracé de l'harmonie superposé dans l'éditeur de courbe

### Éditeur de courbe & visualiseur

- Éditeur graphique de courbe de hauteur avec glisser, aimantation gamme/grille, copier/coller, annuler/rétablir
- Visualisation de hauteur en temps réel avec tracés entrée/sortie/harmonie
- Clavier de piano avec surlignage automatique des notes
- Superposition de la waveform (affichage Line, Mirror ou Spectral)
- Export en image PNG (résolution 2x)

### Intégration ARA2

- Support ARA2 — lit la tonalité, la gamme et la signature depuis la timeline du DAW
- Règle de mesure sensible à la signature avec défilement auto
- Support multi-signatures (changement de signature en cours de projet)
- Superposition de forme d'onde synchronisée à la timeline du DAW

> ARA2 synchronise le plugin avec la timeline de votre DAW (tonalité/gamme, signature, playhead, forme d'onde). OpenVoxTuner reste un **effet temps réel** — il ne fournit pas d'édition hors-ligne note par note comme les éditeurs de hauteur dédiés.

### Comparaison & morphing A/B

- **Emplacements A/B** — sauvegarde et rappel de deux états complets du plugin
- **Curseur de morphing** — interpolation continue entre A et B (tous paramètres lisses)
- État persisté entre les sessions

### Divers

- Sortie de note MIDI (générée à partir de la hauteur détectée)
- Cible MIDI (MIDI entrant contrôle la hauteur de correction)
- Préréglages de courbe avec galerie à cartes
- Annuler/rétablir global (tous les paramètres automatisables)
- Thème Sombre/Clair
- Traitement stéréo, pitch shifting PSOLA à faible latence
- Mode standalone avec transport interne à 120 BPM

## Pourquoi OpenVoxTuner ?

- **Open source par conception** — chaque ligne de DSP, d'UI et de logique de préréglage est publique. Pas de boîte noire, pas de télémétrie, pas de fonctionnalité payante. Vous pouvez lire exactement comment votre voix est traitée.
- **AGPLv3 pour la liberté** — la licence garantit que le projet reste libre et ouvert. Chacun peut l'utiliser (même commercialement), et toute amélioration doit être partagée avec la communauté.
- **ARA2 natif** — la tonalité, la gamme et la signature sont lues directement depuis votre projet DAW, pour que le plugin suive votre arrangement sans configuration manuelle.
- **Conçu pour la voix réelle** — la détection YIN, le PSOLA préservant les formants et un éditeur de courbe graphique sont accordés aux nuances des performances chantées, pas seulement à des démos proof-of-concept.

### Développement assisté par IA

OpenVoxTuner utilise des assistants de codage IA pour accélérer le développement, toujours sous stricte supervision humaine. Chaque ligne de code est relue, testée et entièrement ouverte à l'audit de la communauté. Aucun code généré par IA non vérifié n'est fusionné.

## Structure du dépôt

```text
.
├─ Source/
│  ├─ dsp/                        # Modules DSP
│  │  ├─ IPitchDetector.h         # Interface abstraite du détecteur de hauteur
│  │  ├─ YinPitchDetector.*       # Algorithme YIN (actif)
│  │  ├─ ScaleQuantizer.*         # Moteur de quantification de gamme
│  │  ├─ PitchShifter.*           # Pitch shifter PSOLA
│  │  ├─ RetargetEnvelope.*       # Lissage de l'enveloppe de vitesse
│  │  ├─ FormantPreserver.*       # Filtre de compensation de formant
│  │  ├─ NoiseGate.h              # Gate de bruit d'entrée (RMS, hystérésis)
│  │  ├─ PitchCurve.*             # Modèle de données de courbe
│  │  ├─ PresetMorpher.h          # Moteur de morphing A/B
│  │  ├─ HarmonyEngine.*          # Moteur de synthèse d'harmonie
│  │  ├─ PitchDetector.*          # Référence YIN originale (non compilée)
│  │  └─ NoteUtils.h / IPitchShifter.h
│  ├─ ui/                         # Composants UI
│  │  ├─ PitchCurveEditor.*       # Composant éditeur de courbe
│  │  ├─ PitchVisualizer.*        # Visualisation de hauteur
│  │  ├─ PianoKeyboard.*          # Widget clavier de piano
│  │  ├─ ScaleKeyboardComponent.* # Affichage clavier de gamme
│  │  ├─ LookAndFeel.*            # Look and feel personnalisé
│  │  ├─ OVTTheme.h               # Couleurs de thème et rendu de waveform partagé
│  │  ├─ OVTFonts.h               # Helpers de polices
│  │  └─ OVTLanguages.h           # Traductions multilingues
│  ├─ external/presonus/          # Extensions PreSonus (Studio One)
│  ├─ resources/                  # Ressources binaires (BuildInfo.h.in)
│  ├─ PluginProcessor.*           # Processeur audio principal
│  └─ PluginEditor.*              # UI de l'éditeur principal
├─ scripts/                       # Scripts de build et de développement
│  ├─ build.ps1                   # Build Windows
│  ├─ build_installer.ps1         # Installateur Windows (Inno Setup)
│  ├─ build_macos_vst3.sh         # Build VST3 macOS
│  ├─ build_macos_au.sh           # Build AU macOS
│  ├─ build_macos_pkg.sh          # Installateur .pkg macOS
│  ├─ build_macos.sh              # Build universel macOS
│  └─ ... (aides à l'installation, symlink, release)
├─ test/                          # Tests unitaires (Catch2)
│  ├─ Main.cpp
│  └─ dsp/                        # Suites de tests par module
├─ docs/                        # Documentation
│  ├─ releases/                   # Notes de release (latest.json, v0.1.1.md)
│  ├─ architecture.md
│  ├─ default-parameters.md
│  └─ ...
├─ installer/                     # Ressources de l'installateur Windows
│  └─ OpenVoxTuner.iss            # Script Inno Setup
├─ .github/                       # CI/CD et modèles d'issues
│  ├─ workflows/                  # GitHub Actions (CI, release)
│  └─ ISSUE_TEMPLATE/             # Bug report / demande de fonctionnalité
├─ assets/                        # Ressources binaires
│  └─ icon.png
├─ external/ARA_SDK/              # Celemony ARA SDK (v2.2, submodule)
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ docs/implementation-roadmap.md
├─ .gitignore
├─ .gitattributes
└─ .gitmodules
```

## Licence

OpenVoxTuner est **gratuit pour tous** sous la licence [AGPLv3](../LICENSE) — musiciens, producteurs, studios, éducateurs. Aucune restriction sur l'usage commercial.

### Licences tierces

| Bibliothèque | Licence | Compatibilité |
|---------|---------|--------------|
| JUCE 8 | AGPLv3 | Même licence |
| ARA SDK | Apache 2.0 | Entièrement compatible |
| Extensions PreSonus | Domaine public | Entièrement compatible |
| Catch2 (tests) | Boost (BSL-1.0) | Entièrement compatible |

Toutes les licences tierces sont compatibles avec AGPLv3.

### Ce que l'AGPLv3 signifie pour vous

| Vous êtes... | Gratuit ? | Obligation ? |
|---|---|---|
| Musicien / Producteur | Oui | Aucune — faites juste de la musique |
| Studio (mix, master, prod) | Oui | Aucune — vous utilisez le plugin comme un outil |
| Éducateur / Étudiant | Oui | Aucune |
| Développeur (modifie & redistribue) | Oui | Vous devez partager votre source modifiée sous AGPLv3 |
| Société (fork en produit fermé) | Non | Vous avez besoin d'une licence commerciale |

> En pratique, si vous utilisez OpenVoxTuner pour faire de la musique — même professionnellement — la licence AGPLv3 est totalement gratuite, sans obligation.

### Soutenir le projet

OpenVoxTuner est gratuit pour tous. Si OpenVoxTuner vous fait gagner du temps ou aide votre musique, envisagez de soutenir le projet — même un petit don fait une énorme différence.

| Palier | Plateforme | Prix | Avantages |
|------|----------|-------|----------|
| **Gratuit** | — | 0 € | Plugin complet, toutes les fonctionnalités |
| **Buy me a coffee** | [Ko-fi](https://ko-fi.com/) | Ponctuel | Un simple merci ❤️ |
| **Sponsor** | [GitHub Sponsors](https://github.com/sponsors/) | Mensuel | Soutien au développement continu |
| **Supporter** | [Patreon](https://patreon.com/) | 5 €/mois | Discord privé + vote sur les fonctionnalités à venir |
| **Gold** | [Patreon](https://patreon.com/) | 20 €/mois | Tous les avantages Supporter + accès anticipé / builds bêta + nom dans les crédits |

Chaque contribution aide à garder le projet vivant et gratuit pour tous.

### Licence développeur

Une licence commerciale est disponible pour les développeurs ou sociétés souhaitant intégrer OpenVoxTuner dans un **produit fermé** sans l'obligation de copyleft AGPLv3.

**Ce qu'elle accorde :**
- Permission d'utiliser le DSP, les composants UI et les algorithmes d'OpenVoxTuner dans un logiciel propriétaire
- Pas d'obligation de copyleft — vous n'êtes **pas** tenu de publier votre code source
- Pas d'exigence de publication des dérivés sous AGPLv3

**Ce qu'elle inclut :**
- Support email prioritaire
- Fonctionnalités sur mesure et conseil DSP optionnels
- Licence perpétuelle pour la version achetée (mises à jour selon le palier)

**Contact :** ouvrez une issue sur GitHub.

## Contribuer

Les contributions sont les bienvenues ! Vous pouvez aider en :

- Corrigeant des bugs
- Améliorant les algorithmes DSP
- Ajoutant des traductions
- Améliorant l'UI
- Rédigeant de la documentation
- Testant la compatibilité avec les DAW

Voir [CONTRIBUTING.md](../CONTRIBUTING.md) pour les détails.

### Revue du code généré par IA

Certaines parties d'OpenVoxTuner sont écrites avec l'aide d'agents de codage IA, toujours sous supervision humaine. Tout le code est relu avant fusion, et les pull requests communautaires sont vivement encouragées pour auditer, améliorer ou remplacer les sections assistées par IA.

## Build

### Windows (Visual Studio)

Prérequis :
- Visual Studio 2022
- CMake >= 3.22
- JUCE 8 (chemin par défaut dans CMake : `C:/JUCE`)
- Git LFS (pour les ressources binaires)

```powershell
# Build Debug
.\scripts\build.ps1 -Configuration Debug

# Build Release
.\scripts\build.ps1 -Configuration Release

# Build de l'installateur Windows (nécessite Inno Setup)
.\scripts\build_installer.ps1
```

### macOS (VST3 / AU / pkg)

Prérequis :

```bash
xcode-select --install
brew install cmake ninja
```

Build VST3 :

```bash
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

Build AU :

```bash
./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install
```

Build de l'installateur `.pkg` macOS. Les releases officielles livrent un installateur **signé et notarisé** avec **VST3 + AU + Standalone** :

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,AU,STANDALONE
```

Pour créer un installateur plus léger sans l'AU, écrasez `--formats` (ex. `VST3,STANDALONE`).

Guides de build détaillés :
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## Formats

| Format   | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅    |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

## Documentation

- [Site web](https://openvoxtuner.eiffelbs.ovh) — page d'accueil avec aperçu des fonctionnalités et liens de téléchargement
- [Docs en ligne](https://ovtdocs.eiffelbs.ovh) — documentation complète (MkDocs Material)
- [docs/architecture.md](../docs/architecture.md) — vue d'ensemble de l'architecture logicielle
- [docs/default-parameters.md](../docs/default-parameters.md) — référence de tous les paramètres du plugin
- [docs/implementation-roadmap.md](../docs/implementation-roadmap.md) — feuille de route des fonctionnalités
- [docs/ARA_Specifications.md](../docs/ARA_Specifications.md) — spécifications techniques du support ARA2
- [docs/deployment-and-packaging-guide.md](../docs/deployment-and-packaging-guide.md) — workflow de release
- [docs/GITHUB_SETUP_AND_RELEASE.md](../docs/GITHUB_SETUP_AND_RELEASE.md) — configuration du dépôt GitHub
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md) — guide de build VST3 macOS
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md) — guide AU + installateur macOS
- [docs/preset-morphing-technical-strategy.md](../docs/preset-morphing-technical-strategy.md) — stratégie de morphing A/B
- [docs/releases/v0.1.1.md](../docs/releases/v0.1.1.md) — notes de release

## Licence

Voir [LICENSE](../LICENSE).

## Signalement d'issues

Utilisez les modèles d'issue GitHub :
- [Bug report](../.github/ISSUE_TEMPLATE/bug_report.md)
- [Feature request](../.github/ISSUE_TEMPLATE/feature_request.md)

## Historique d'étoiles

<a href="https://star-history.com/#EiffelBS/OpenVoxTuner&type=date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" width="100%" />
  </picture>
</a>
