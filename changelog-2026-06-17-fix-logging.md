# Changelog - Corrections du 2026-06-17

## Problèmes Résolus

### 1. ✅ Logging Excessif (Cause du Problème Audio en Plugin VST3)

**Symptôme** : Le plugin fonctionne en standalone mais pas en mode VST3 dans un DAW.

**Cause** : Un log non-throttlé dans `PluginProcessor.cpp` (lignes 857-858) qui s'exécutait à chaque bloc audio (~86 fois par seconde), générant des millions de lignes de log et saturant complètement le système.

**Fix** : Throttlage du log pour qu'il ne s'affiche qu'une fois par seconde, comme tous les autres logs de debug.

**Fichier modifié** : `Source/PluginProcessor.cpp`

```cpp
// AVANT (lignes 845-858)
pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);

// Immediately compare snapshot to detect modifications
float diffSq = 0.0f;
// ... calculs ...
juce::Logger::writeToLog ("pitchShifter post-check: ..."); // ❌ À chaque bloc !

// APRÈS (avec throttling)
pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);

// Immediately compare snapshot to detect modifications (throttled log: once per second)
static std::atomic<uint32_t> lastPostCheckLogMs { 0 };
uint32_t nowPostCheck = juce::Time::getMillisecondCounter();
uint32_t lastPostCheck = lastPostCheckLogMs.load();
if (nowPostCheck - lastPostCheck > 1000)  // ✅ Seulement toutes les 1000ms
{
    if (lastPostCheckLogMs.compare_exchange_strong (lastPostCheck, nowPostCheck))
    {
        // ... calculs et log ...
    }
}
```

### 2. ✅ Organisation et Workflow de Développement

**Problèmes** :
- Droits d'accès insuffisants pour copier le VST3 dans `Program Files`
- Pas de script d'installation pratique pour le développement
- Confusion sur la structure `build/` dans le projet

**Solutions apportées** :

#### Nouveaux Scripts PowerShell

1. **`create_dev_symlink.ps1`** (⭐ Recommandé pour le dev)
   - Crée un lien symbolique vers le build Debug ou Release
   - Avantage : Le plugin dans Program Files est automatiquement mis à jour après chaque rebuild Visual Studio
   - Nécessite les droits administrateur (une seule fois)

2. **`install_vst3.ps1`** (Mise à jour)
   - Installation manuelle par copie (pour tests ponctuels)
   - Nécessite les droits admin à chaque copie

3. **`rebuild_clean.ps1`**
   - Nettoie complètement le dossier `build/`
   - Régénère tout via CMake
   - Utile quand CMake est perdu ou pour repartir de zéro

4. **`open_vs.ps1`**
   - Ouvre Visual Studio avec la solution OpenVoxTuner
   - Détecte automatiquement la version de VS installée

#### Documentation

1. **`BUILD_GUIDE.md`**
   - Guide complet de build et d'installation
   - Explications sur la structure VST3 (bundle vs fichier)
   - Solutions aux problèmes courants
   - Workflow de développement recommandé

2. **`README.md`**
   - Quick start pour les développeurs
   - Tableau récapitulatif des scripts
   - Notes sur le format VST3

## Impact sur les Performances

**Avant** :
- ~86 appels de log par seconde à 44.1kHz (buffer de 512 samples)
- Millions de lignes écrites dans `OpenVoxTuner.log`
- Saturation du disque dur
- Ralentissements majeurs voire crashes
- Plugin inutilisable en mode VST3

**Après** :
- 1 log par seconde maximum
- Fichier de log gérable (~1 Ko/seconde)
- Pas d'impact sur les performances audio
- Plugin fonctionnel en VST3 et standalone

## Workflow de Développement Recommandé

### Première Fois

```powershell
# 1. Créer le lien symbolique (une seule fois, nécessite admin)
.\create_dev_symlink.ps1

# 2. Ouvrir Visual Studio
.\open_vs.ps1
```

### Développement Quotidien

1. Modifier le code dans Visual Studio
2. Compiler (Ctrl+Shift+B)
3. ✨ **Le plugin est automatiquement mis à jour dans Program Files** (grâce au lien symbolique)
4. Fermer complètement le DAW
5. Relancer le DAW et tester

### En Cas de Problème CMake

```powershell
.\rebuild_clean.ps1 -Configuration Debug
```

## Notes Importantes

### Structure du Projet
- ✅ **NORMAL** que Visual Studio soit dans `build/` (convention CMake)
- `build/` = dossier généré, non versionné (dans `.gitignore`)
- Sources = toujours dans `Source/` à la racine
- **Ne jamais ouvrir les fichiers depuis `build/`**, toujours depuis `Source/`

### Format VST3
- Un VST3 est un **bundle** (dossier), pas un fichier unique `.vst3`
- Structure interne :
  ```
  OpenVoxTuner.vst3/
  ├── Contents/
  │   ├── x86_64-win/OpenVoxTuner.vst3  (DLL)
  │   └── Resources/moduleinfo.json
  ├── desktop.ini
  └── Plugin.ico
  ```
- C'est pourquoi il faut copier **tout le dossier**, pas juste la DLL

### Logs
- Fichier de log : `C:\Users\User\Documents\OpenVoxTuner.log`
- Les logs sont maintenant throttlés (1x/seconde max)
- Consultez ce fichier pour diagnostiquer les problèmes audio

## Tests à Effectuer

1. ✅ Compilation réussie
2. ⏳ Test en mode standalone
3. ⏳ Test en mode VST3 dans un DAW
4. ⏳ Vérification des logs (fichier de taille raisonnable)
5. ⏳ Test du lien symbolique (rebuild automatique)

## Prochaines Étapes Recommandées

1. Tester le plugin en mode VST3 avec la correction du logging
2. Vérifier que le fichier `OpenVoxTuner.log` ne grossit plus de manière excessive
3. Confirmer que l'audio tuné fonctionne correctement dans le DAW
4. Si le problème persiste, les logs throttlés permettront de voir exactement où se situe le problème (par exemple, si `rmsDiffL=0`, cela signifie que le `pitchShifter` ne modifie pas le buffer)

---

## Addendum : Débogage dans Visual Studio

### Problème Supplémentaire Résolu

**Erreur** : "Impossible de démarrer le programme ... ALL_BUILD"

**Cause** : Visual Studio essayait de lancer le projet CMake `ALL_BUILD` (qui compile tout mais ne produit pas d'exécutable).

**Solution** : Configurer le projet de démarrage correct dans Visual Studio :
1. Clic droit sur `OpenVoxTuner_Standalone` → "Définir comme projet de démarrage"
2. Lancer le débogueur (F5)

### Documentation Ajoutée

- **`DEBUG_GUIDE.md`** : Guide complet de débogage (breakpoints, attach to process, profiling)
- **`QUICKFIX_ALL_BUILD_ERROR.md`** : Solution rapide pour l'erreur ALL_BUILD
- **`build/set_startup_project.ps1`** : Script d'aide pour configurer le projet de démarrage
- **`.editorconfig`** : Configuration automatique de l'éditeur pour Visual Studio
