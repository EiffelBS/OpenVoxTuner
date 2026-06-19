# Guide de Débogage - OpenVoxTuner

## Problème : "Impossible de démarrer le programme ... ALL_BUILD"

### Cause
Visual Studio essaie de lancer `ALL_BUILD`, qui est un projet CMake spécial sans exécutable. Il faut configurer le **projet de démarrage** correct.

---

## Solution : Configurer le Projet de Démarrage dans Visual Studio

### Méthode 1 : Via l'Explorateur de Solutions (Recommandé)

1. **Ouvrez** `OpenVoxTuner.sln` dans Visual Studio

2. Dans l'**Explorateur de solutions** (Solution Explorer), trouvez :
   - ?? `OpenVoxTuner_Standalone` (pour tester le plugin standalone)
   - OU ?? `OpenVoxTunerTests` (pour lancer les tests unitaires)

3. **Clic droit** sur `OpenVoxTuner_Standalone` ? **"Définir comme projet de démarrage"** (Set as Startup Project)

4. Le projet devrait maintenant apparaître **en gras** dans l'explorateur

5. **Lancez le débogueur** : `F5` ou `Debug > Start Debugging`

? **L'application standalone devrait maintenant se lancer !**

---

### Méthode 2 : Lancement Direct (Sans Changer le Projet de Démarrage)

Si vous ne voulez pas changer le projet de démarrage par défaut :

1. **Clic droit** sur `OpenVoxTuner_Standalone`
2. **Debug** > **Start New Instance**

Cette méthode lance directement le projet sans le définir comme projet de démarrage.

---

## Déboguer le Plugin VST3 dans un DAW

Pour déboguer le plugin VST3 chargé dans un DAW (Reaper, Studio One, etc.) :

### Étape 1 : Compiler en Debug

```powershell
# Assurez-vous de compiler en configuration Debug
# Dans Visual Studio : Configuration Manager > Active solution configuration > Debug
```

### Étape 2 : Installer le VST3 Debug

```powershell
# Depuis la racine du projet, en mode Admin
cd C:\Users\User\Documents\trae_projects\VST3
.\create_dev_symlink.ps1
# Choisissez "Debug" quand demandé
```

### Étape 3 : Configurer Visual Studio pour Attacher au DAW

1. **Lancez votre DAW** (par exemple, Reaper.exe)

2. **Chargez le plugin** OpenVoxTuner dans un projet

3. Dans **Visual Studio** :
   - Menu : `Debug > Attach to Process...` (ou `Ctrl+Alt+P`)
   - Cherchez le processus de votre DAW (ex: `reaper.exe`, `Studio One.exe`, etc.)
   - Cliquez sur **Attach**

4. **Placez des breakpoints** dans le code :
   - Ouvrez `PluginProcessor.cpp`
   - Cliquez dans la marge gauche à côté de la ligne où vous voulez stopper
   - Un point rouge apparaît

5. **Utilisez le plugin dans le DAW** ? Visual Studio s'arrêtera aux breakpoints

---

## Déboguer le Plugin VST3 Directement (Sans DAW)

JUCE permet de lancer un "plugin host" pour tester le VST3 sans DAW complet.

### Option A : Utiliser le Mode Standalone (Plus Simple)

Le mode standalone est **identique** au plugin VST3 niveau code, mais avec un wrapper standalone :

```
Debug > Start Debugging (F5)
```

Avantages :
- Pas besoin de DAW
- Debugging immédiat
- Tous les breakpoints fonctionnent

### Option B : AudioPluginHost de JUCE

JUCE fournit un host de test basique :

```powershell
# Si JUCE est installé dans C:\JUCE
cd C:\JUCE\extras\AudioPluginHost\Builds\VisualStudio2022
# Ouvrir AudioPluginHost.sln et compiler
```

Puis :
1. Lancez `AudioPluginHost.exe`
2. Menu : `Options > Edit the list of available plug-ins`
3. Scannez le dossier `C:\Program Files\Common Files\VST3`
4. Chargez OpenVoxTuner

Pour déboguer :
1. Visual Studio : `Debug > Attach to Process...`
2. Sélectionnez `AudioPluginHost.exe`

---

## Points de Breakpoint Utiles

### Pour Déboguer l'Audio

```cpp
// PluginProcessor.cpp

// 1. Début du processBlock
void OpenVoxTunerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // BREAKPOINT ICI pour voir si processBlock est appelé

// 2. Après détection de pitch
const float f0_in = computeInputPitch (buffer);
// BREAKPOINT ICI pour voir le pitch détecté

// 3. Après pitch shifting
pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);
// BREAKPOINT ICI pour vérifier que le buffer est modifié
```

### Pour Déboguer l'Interface

```cpp
// PluginEditor.cpp

PluginEditorComponent::PluginEditorComponent (OpenVoxTunerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // BREAKPOINT ICI pour voir si l'éditeur est créé
```

---

## Fenêtre de Sortie (Output Window)

Pour voir les logs en temps réel dans Visual Studio :

1. Menu : `View > Output` (ou `Ctrl+Alt+O`)
2. Dans le dropdown, sélectionnez : **Debug**
3. Les appels à `juce::Logger::writeToLog()` apparaissent ici

**Astuce** : Les logs sont aussi écrits dans `C:\Users\User\Documents\OpenVoxTuner.log`

---

## Fenêtre Espions (Watch Window)

Pendant le débogage, ajoutez des variables à surveiller :

1. Placez le curseur sur une variable (ex: `f0_in`)
2. Clic droit > **Add Watch**
3. La fenêtre "Watch" montre la valeur en temps réel

Variables utiles à surveiller :
- `f0_in` : Pitch détecté
- `ratio` : Ratio de pitch shifting
- `buffer.getNumSamples()` : Taille du bloc audio
- `buffer.getReadPointer(0)[0]` : Premier sample du buffer

---

## Problèmes Courants

### Le débogueur ne s'arrête pas aux breakpoints

**Cause** : Vous déboguez en configuration Release (optimisations activées)

**Solution** :
1. Configuration Manager > Active solution configuration > **Debug**
2. Rebuild

### Le plugin ne charge pas dans le DAW pendant le débogage

**Cause** : Le DAW a verrouillé la DLL

**Solution** :
1. Fermez complètement le DAW
2. Recompilez
3. Relancez le DAW

### "Symbols not loaded" dans Visual Studio

**Cause** : Les symboles de debug (.pdb) ne sont pas trouvés

**Solution** :
1. Vérifiez que vous compilez en **Debug**
2. Les .pdb doivent être à côté des .dll/.exe

### Performance très lente en Debug

**Normal** : Le mode Debug désactive les optimisations et ajoute des checks.

**Solution** : Pour tester les performances, utilisez **RelWithDebInfo** :
```powershell
.\build.ps1 -Configuration RelWithDebInfo
```

Cette configuration compile avec optimisations MAIS conserve les symboles de debug.

---

## Configurations de Build

| Configuration | Optimisation | Symboles Debug | Usage |
|---------------|--------------|----------------|-------|
| **Debug** | ? Non | ? Oui | Développement, debugging |
| **Release** | ? Oui | ? Non | Distribution finale |
| **RelWithDebInfo** | ? Oui | ? Oui | Profiling, debugging de performances |
| **MinSizeRel** | ? Taille | ? Non | Réduction de taille (rare) |

---

## Outils de Profiling

### Visual Studio Profiler

Pour analyser les performances audio :

1. Menu : `Debug > Performance Profiler...`
2. Cochez : **CPU Usage** et **Memory Usage**
3. Cliquez : **Start**
4. Utilisez le plugin
5. Cliquez : **Stop Collection**

### Analyseur de Graphe d'Appels

Pour voir quel code consomme le plus de CPU :

1. Menu : `Analyze > Performance Profiler...`
2. Cochez : **Instrumentation**
3. Lancez

Vous verrez un graphe montrant quelles fonctions prennent le plus de temps.

---

## Scripts Utiles

```powershell
# Définir le projet de démarrage (instructions interactives)
.\set_startup_project.ps1

# Ouvrir Visual Studio
..\open_vs.ps1

# Rebuild complet
..\rebuild_clean.ps1 -Configuration Debug
```

---

## Raccourcis Clavier Visual Studio

| Raccourci | Action |
|-----------|--------|
| `F5` | Démarrer le débogage |
| `Ctrl+F5` | Démarrer sans débogage |
| `F9` | Ajouter/Supprimer breakpoint |
| `F10` | Step Over (ligne suivante) |
| `F11` | Step Into (rentrer dans la fonction) |
| `Shift+F11` | Step Out (sortir de la fonction) |
| `Ctrl+Shift+B` | Build Solution |
| `Ctrl+Alt+P` | Attach to Process |
| `Ctrl+Alt+O` | Output Window |

---

## Ressources

- Logs du plugin : `C:\Users\User\Documents\OpenVoxTuner.log`
- Build artifacts : `build\OpenVoxTuner_artefacts\Debug\`
- Documentation JUCE : https://docs.juce.com/master/

---

**Bon débogage !** ????
