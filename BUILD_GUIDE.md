# Guide de Build et d'Installation - OpenVoxTuner

## Structure du Projet

```
VST3/                                    # Racine du projet
??? Source/                              # Code source C++
??? build/                               # Dossier de build CMake (généré)
?   ??? OpenVoxTuner.sln                # Solution Visual Studio (générée)
?   ??? OpenVoxTuner_artefacts/         # Binaires compilés
?       ??? Debug/
?       ?   ??? VST3/OpenVoxTuner.vst3  # Plugin VST3 Debug
?       ?   ??? Standalone/OpenVoxTuner.exe
?       ??? Release/
?           ??? VST3/OpenVoxTuner.vst3  # Plugin VST3 Release
?           ??? Standalone/OpenVoxTuner.exe
??? installer/                           # Scripts Inno Setup
??? CMakeLists.txt                       # Configuration CMake
```

## Workflow de Développement

### Option 1 : Développement avec Visual Studio (Recommandé)

1. **Ouvrir la solution Visual Studio**
   ```powershell
   cd C:\Users\User\Documents\trae_projects\VST3\build
   .\OpenVoxTuner.sln
   ```

2. **Compiler dans Visual Studio**
   - Choisir la configuration : `Debug` ou `Release`
   - Build > Build Solution (Ctrl+Shift+B)
   - Le VST3 sera dans : `build\OpenVoxTuner_artefacts\Debug\VST3\OpenVoxTuner.vst3`

3. **Installer le VST3 pour tester**

   **MÉTHODE A : Installation manuelle (copie directe)**
   ```powershell
   # Depuis la RACINE du projet (VST3/), en mode Administrateur
   cd C:\Users\User\Documents\trae_projects\VST3
   .\install_vst3.ps1
   ```

   **MÉTHODE B : Créer un lien symbolique (meilleur pour le dev)**
   ```powershell
   # En mode Administrateur, depuis PowerShell
   cd "C:\Program Files\Common Files\VST3"

   # Supprimer l'ancien si présent
   Remove-Item "OpenVoxTuner.vst3" -Recurse -Force -ErrorAction SilentlyContinue

   # Créer un lien symbolique vers le build Debug
   New-Item -ItemType SymbolicLink -Path "OpenVoxTuner.vst3" `
            -Target "C:\Users\User\Documents\trae_projects\VST3\build\OpenVoxTuner_artefacts\Debug\VST3\OpenVoxTuner.vst3"
   ```

   **Avantage du lien symbolique** : Chaque rebuild dans Visual Studio met automatiquement à jour le plugin dans Program Files !

4. **Tester dans votre DAW**
   - Fermez complètement votre DAW
   - Relancez-le pour qu'il rescanne les plugins VST3
   - Chargez OpenVoxTuner

### Option 2 : Build et Installation Complète (Release)

Pour créer une version finale distribuable :

```powershell
# Depuis la racine du projet
cd C:\Users\User\Documents\trae_projects\VST3

# Build Release + Génération de l'installeur
.\build_installer.ps1

# L'installeur sera créé dans :
# build\installer\OpenVoxTuner_Windows_Installer.exe
```

Ensuite, lancez l'installeur pour une installation propre.

### Option 3 : Build via CMake + Scripts (sans Visual Studio)

```powershell
cd C:\Users\User\Documents\trae_projects\VST3

# Configuration + Build Release
.\build.ps1 -Configuration Release

# Build Debug
.\build.ps1 -Configuration Debug
```

## Problèmes Courants

### 1. "Accès refusé" lors de la copie du VST3

**Cause** : Program Files nécessite des droits administrateur.

**Solution** :
- Lancez PowerShell **en mode Administrateur** (clic droit > Exécuter en tant qu'administrateur)
- OU utilisez le lien symbolique (voir ci-dessus)
- OU utilisez l'installeur Inno Setup

### 2. Le DAW ne voit pas le plugin après installation

**Causes possibles** :
1. Le DAW n'a pas rescanné les plugins ? Fermez complètement et relancez
2. Le plugin est dans le mauvais dossier ? Vérifiez que `OpenVoxTuner.vst3` est bien dans `C:\Program Files\Common Files\VST3\`
3. Le bundle VST3 est incomplet ? Vérifiez que le dossier `OpenVoxTuner.vst3` contient bien `Contents\x86_64-win\OpenVoxTuner.vst3`

### 3. CMake Error "generator platform mismatch"

**Solution** :
```powershell
cd build
Remove-Item CMakeCache.txt -Force
Remove-Item CMakeFiles -Recurse -Force
cd ..
.\build.ps1
```

### 4. Modifications du code non prises en compte

**Si vous utilisez un lien symbolique** : Vérifiez que vous compilez en configuration Debug (ou changez le lien pour pointer vers Release).

**Sans lien symbolique** : Re-copiez le VST3 après chaque build :
```powershell
.\install_vst3.ps1  # En mode Admin
```

## Fichiers de Log

Les logs du plugin sont écrits dans :
```
C:\Users\User\Documents\OpenVoxTuner.log
```

Consultez ce fichier pour diagnostiquer les problèmes audio ou de paramètres.

## Structure d'un Bundle VST3

Un plugin VST3 est un **dossier** (pas un fichier), avec cette structure :

```
OpenVoxTuner.vst3/                          # Dossier principal (bundle)
??? Contents/
?   ??? x86_64-win/
?   ?   ??? OpenVoxTuner.vst3               # DLL native Windows 64-bit
?   ??? Resources/
?       ??? moduleinfo.json                 # Métadonnées VST3
??? desktop.ini                              # Pour l'icône Windows
??? Plugin.ico
```

C'est pour cette raison que vous devez copier **tout le dossier**, pas juste la DLL.

## Automatisation pour le Développement

Pour automatiser le workflow de développement, ajoutez une **Post-Build Event** dans Visual Studio :

1. Clic droit sur le projet `OpenVoxTuner` > Properties
2. Build Events > Post-Build Event
3. Command Line :
   ```batch
   powershell -ExecutionPolicy Bypass -File "$(SolutionDir)..\install_vst3.ps1"
   ```

Cela copiera automatiquement le VST3 après chaque build (nécessite VS en mode Admin).

## Notes sur la Structure build/

Oui, c'est normal que Visual Studio se trouve dans `build/` ! C'est la convention CMake :
- `build/` = dossier de build (généré, ignoré par git)
- Racine = sources + CMakeLists.txt

**Avantages** :
- Le dossier `build/` peut être supprimé et régénéré à tout moment sans perdre le code source
- Permet de créer plusieurs builds (Debug, Release, x86, x64) dans des sous-dossiers séparés
- Respecte la séparation sources / binaires

**N'ouvrez JAMAIS les fichiers source depuis `build/`** : ouvrez-les toujours depuis `Source/` à la racine.
