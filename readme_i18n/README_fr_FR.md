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
  <a href="#build">Build</a>
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

- [Captures](#captures)
- [Fonctionnalités](#fonctionnalités)
- [Pourquoi OpenVoxTuner ?](#pourquoi-openvoxtuner-)
- [Structure du dépôt](#structure-du-dépôt)
- [Licence](#licence)
- [Soutenir le projet](#soutenir-le-projet)
- [Licence développeur](#licence-développeur)
- [Contribuer](#contribuer)
- [Build](#build)
- [Documentation](#documentation)

---

## Captures

<!-- Images placeholder (propriétaire du projet a autorisé des images fictives, 2026-07-12).
     Remplacez les URL placehold.co par de vraies captures dans docs/screenshots/
     quand elles seront disponibles, par ex.
     <img src="docs/screenshots/main-view.png" width="80%" alt="Fenêtre principale OpenVoxTuner"> -->

<p align="center">
  <img src="https://placehold.co/960x540/15151f/e0e0e0?text=OpenVoxTuner+Main+View" width="80%" alt="Fenêtre principale OpenVoxTuner — placeholder">
</p>
<p align="center">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Live+Visualizer" width="45%" alt="Visualiseur live — placeholder">
  <img src="https://placehold.co/640x360/15151f/e0e0e0?text=Curve+Editor" width="45%" alt="Éditeur de courbe — placeholder">
</p>

## Fonctionnalités

### Correction de hauteur

- **Mode auto** — quantification sur la gamme avec sélection de la tonalité et de la gamme (Majeur, Mineur, Pentatonique, Chromatique, Personnalisé)
- **Mode graphique** — dessinez votre propre courbe de hauteur (éditeur style Melodyne avec aimantation, grille, copier/coller, annuler/rétablir)
- **Contrôle de la vitesse** — enveloppe de rattrapage pour une correction naturelle ou robotique
- **Contrôle du montant** — mélange entre le signal corrigé et le signal sec

### Effets

- **Décalage de formant** — préservation/transposition indépendante des formants (-12 à +12 demi-tons)
- **Réverbération** — effet de réverbération intégré avec niveau de mix réglable
- **Gate de bruit** — gate sur l'entrée avec seuil réglable (-80 à 0 dB), appliqué avant la détection de hauteur pour des résultats plus propres

### Comparaison & morphing A/B

- **Emplacements A/B** — sauvegarde et rappel de deux états complets du plugin (A et B)
- **Curseur de morphing** — interpolation continue entre les états A et B
- **Sauvegarde auto** — l'emplacement courant est automatiquement sauvegardé au changement
- Tous les paramètres se morphent en douceur (lerp continu pour les curseurs, bascule à 50 % pour les commutateurs)
- État A/B persisté entre les sessions

### Moteur d'harmonie

- **Mode Use Voice** — transpose votre voix live en notes d'harmonie (1 à 4 voix décalées avec pan stéréo)
- **Mode Synth** — tonalités d'harmonie synthétisées (Choir, Bright, Synth Lead, Strings, Guitar, type Vocoder) avec couleur réglable
- **Préréglages de type d'harmonie** — 3e/5e en dessous/au-dessus, Vocal Stack, Power Chord, Parallel 3rd, Drone
- **Volume & mix** pour un niveau d'harmonie et un wet/dry indépendants
- Tracé de l'harmonie superposé dans l'éditeur de courbe

### Détection de hauteur

- **YIN** — autocorrélation dans le domaine temporel (rapide, faible CPU, le seul détecteur utilisé)
- SWIPE' et PYIN ont été évalués puis supprimés (YIN conservé uniquement pour la vitesse et la stabilité)

### Intégration ARA2

- Support complet d'ARA2 (Audio Random Access) — intégration transparente à la timeline du DAW
- Extraction automatique de la tonalité/gamme depuis le contexte musical ARA
- Règle de mesure sensible à la signature (3/4, 4/4, 6/8, 12/8)
- Défilement automatique suivant le playhead du DAW pendant la lecture
- Support multi-signatures (changement de signature en cours de projet)

### Éditeur de courbe

- Éditeur graphique de courbe de hauteur avec glisser, ajouter, supprimer des points
- Aimantation à la gamme, aimantation à la grille
- Sélecteur de mesures (1, 2, 4, 8, 16, 32)
- Copier/coller et annuler/rétablir
- Superposition du tracé d'harmonie
- Ligne de curseur horizontale avec nom de note et lecture en Hz
- Lignes de référence des notes de gamme (comme le visualiseur live)
- Superposition de la waveform (Line ou Mirror, synchronisée avec le visualiseur live)
- Bascule de défilement auto (fonctionne dans tous les modes)

### Visualiseur live

- Visualisation de hauteur en temps réel avec tracés entrée/sortie/harmonie
- Clavier de piano avec surlignage automatique des notes
- Ligne de curseur horizontale avec nom de note et lecture en Hz
- Superposition de la waveform (types Line ou Mirror)
- Bloc de légende avec statistiques (en juste %, cents moyens)
- Export en image PNG (résolution 2x)

### Superposition de waveform

- Waveform capturée depuis l'entrée dans tous les modes (plugin, standalone, ARA)
- Deux types d'affichage sélectionnables dans le menu :
  - **Line** — contour simple de la waveform (40 % d'opacité)
  - **Mirror** — barres symétriques autour du centre (par défaut)
- Le type d'affichage s'applique uniformément au visualiseur live et à l'éditeur de courbe
- Préférence persistée entre les sessions

### Système de thèmes

- Thèmes Sombre et Clair avec palette de couleurs unifiée
- Bascule automatique des thèmes avec rafraîchissement complet de l'UI
- Couleurs cohérentes des menus contextuels (menu hamburger, préréglages, combos)
- Tooltips corrigés avec rendu rectangulaire propre

### Divers

- Sortie de note MIDI (générée à partir de la hauteur détectée)
- Traitement stéréo
- Pitch shifting PSOLA à faible latence
- Bascule Bypass (mode standalone)
- Mode standalone avec transport interne à 120 BPM

## Pourquoi OpenVoxTuner ?

- **Open source par conception** — chaque ligne de DSP, d'UI et de logique de préréglage est publique. Pas de boîte noire, pas de télémétrie, pas de fonctionnalité payante. Vous pouvez lire exactement comment votre voix est traitée.
- **AGPLv3 pour la liberté** — la licence garantit que le projet reste libre et ouvert. Chacun peut l'utiliser (même commercialement), et toute amélioration doit être partagée avec la communauté.
- **ARA2 natif** — l'intégration profonde au DAW signifie que tonalité, gamme et tempo sont lus directement depuis votre projet. Pas de configuration manuelle, pas de conjecture — OpenVoxTuner suit votre arrangement automatiquement.
- **Conçu pour la voix réelle** — la détection YIN, le PSOLA préservant les formants et un éditeur de courbe style Melodyne sont accordés aux nuances des performances chantées, pas seulement à des démos proof-of-concept.

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
├─ roadmap.md
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

**Contact :** ouvrez une issue sur GitHub ou écrivez à [license@openvoxtuner.com](mailto:license@openvoxtuner.com).

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

Build de l'installateur `.pkg` macOS. Les releases officielles livrent **VST3 + Standalone** (l'AU est omis car un AU non signé ne peut pas être chargé par un DAW) :

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

Pour inclure aussi le composant AU dans un build local, ajoutez-le à `--formats` (ex. `VST3,AU,STANDALONE`).

Guides de build détaillés :
- [docs/MACOS_VST3_BUILD_GUIDE.md](../docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](../docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## Formats

| Format   | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅*   |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

> \* Le composant AU est compilable depuis les sources, mais **non livré** dans les releases non signées — un AU non signé ne peut pas être chargé par un DAW sur macOS. Les releases distribuées incluent **VST3 + Standalone** sur les deux plateformes.

## Documentation

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
