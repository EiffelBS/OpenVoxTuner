# Spécifications Techniques : Support ARA2 (Audio Random Access)

## 1. Mécanisme de secours (Fallback) pour les DAW non-ARA
Le plugin intègre une architecture "Hybride". Lorsqu'il est inséré dans un DAW ne supportant pas ARA2 (comme Ableton Live ou FL Studio), ou lorsqu'il est utilisé en mode Standalone, le plugin fonctionne comme un effet VST3 standard en temps réel.

**Implémentation technique :**
Dans la méthode `processBlock`, le plugin appelle `processBlockForARA()`. 
- Si l'hôte fournit un contexte ARA valide et que des régions audio sont assignées, le traitement ARA est effectué et la méthode retourne `true`. Le traitement temps réel (YIN, etc.) est alors bypassé.
- Si la méthode retourne `false`, le plugin poursuit son exécution séquentielle normale, capturant l'audio du bloc courant, effectuant l'analyse YIN en temps réel et appliquant la correction via `PitchShifter`.
L'expérience utilisateur est ainsi préservée à 100% dans tous les environnements.

## 2. Comportement Clip vs Piste (Hiérarchie ARA)
L'extension ARA permet d'insérer le plugin de deux manières distinctes selon le DAW :

### A. Instanciation sur un Clip / Événement (ex: Studio One)
- L'utilisateur glisse le plugin directement sur un événement audio dans la timeline.
- **Comportement ARA** : L'hôte crée une `ARARegionSequence` contenant exclusivement ce clip. Le plugin n'aura accès et n'analysera que la portion d'audio délimitée par ce clip. L'éditeur graphique affichera cette région spécifique.

### B. Instanciation sur une Piste complète (ex: Logic Pro, Cubase)
- L'utilisateur insère le plugin dans le rack d'effets de la piste vocale entière.
- **Comportement ARA** : L'hôte crée une `ARARegionSequence` englobant tous les clips présents sur cette piste. Le plugin aura accès à l'intégralité du contenu audio de la piste. L'éditeur graphique affichera la timeline complète de la piste avec l'ensemble des événements vocaux successifs.

**Note de conception :**
L'éditeur UI du plugin est conçu pour itérer sur l'objet racine `ARADocument` et agréger toutes les `ARARegionSequence` associées à l'instance courante. Ainsi, le plugin affichera de manière transparente et exacte le contenu que le DAW a décidé de lui assigner, garantissant une cohérence visuelle parfaite sans aucune manipulation de l'utilisateur.

## 3. Extraction de la Tonalité (Chord Track / Key Signature)
Lorsque le plugin est en mode ARA2, il interroge l'objet `ARAMusicalContext` fourni par l'hôte.
- Les données de `ARAKeySignature` (Tonalité) et `ARAChord` (Accords) sont extraites.
- Si le projet contient une tonalité définie (ex: Do Majeur), le module `ScaleQuantizer` du plugin se verrouille automatiquement sur cette gamme, désactivant le besoin pour l'utilisateur de la sélectionner manuellement dans l'interface.

## 4. Matrice de compatibilité et validation
- **Studio One (PreSonus)** : Support ARA2 natif (Clip & Track). Extraction des accords et tonalité 100% supportée.
- **Cubase / Nuendo (Steinberg)** : Support ARA2 VST3 natif. Extraction de la piste d'accords supportée.
- **Logic Pro (Apple)** : Support ARA2 AudioUnit. Instanciation sur piste recommandée.
- **Reaper (Cockos)** : Support ARA2 VST3. Excellent pour l'audio hors-ligne, extraction des accords limitée selon la configuration.
- **Ableton Live / FL Studio** : Fallback temps réel automatique (Traitement standard sans perte de fonctionnalité).
