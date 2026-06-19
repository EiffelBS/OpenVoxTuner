# ⚡ Quick Fix : Erreur "Impossible de démarrer ALL_BUILD"

## 🎯 Solution Rapide (2 étapes)

### 1. Dans Visual Studio : Définir le Projet de Démarrage

```
Explorateur de Solutions
  ↓
Trouver : OpenVoxTuner_Standalone
  ↓
Clic droit > "Définir comme projet de démarrage"
  ↓
Le projet devient GRAS
```

### 2. Lancer le Débogueur

```
Appuyez sur F5
OU
Debug > Start Debugging
```

✅ **C'est tout !** L'application devrait maintenant se lancer.

---

## 🔄 Alternative : Lancement Direct Sans Changer le Projet

```
Clic droit sur OpenVoxTuner_Standalone
  ↓
Debug > Start New Instance
```

---

## 📋 Projets Disponibles

| Projet | Description | Exécutable |
|--------|-------------|------------|
| `OpenVoxTuner_Standalone` | ⭐ Application standalone | ✅ Oui |
| `OpenVoxTuner_VST3` | Plugin VST3 | ❌ Non (DLL) |
| `OpenVoxTunerTests` | Tests unitaires | ✅ Oui |
| `ALL_BUILD` | Build complet (tous projets) | ❌ Non |
| `ZERO_CHECK` | Vérification CMake | ❌ Non |

**Note** : Seuls les projets avec ✅ peuvent être lancés directement.

---

## 🐛 Pour Déboguer le Plugin VST3 dans un DAW

Consultez : [DEBUG_GUIDE.md](DEBUG_GUIDE.md#déboguer-le-plugin-vst3-dans-un-daw)

Résumé rapide :
1. Compilez en **Debug**
2. Installez le VST3 : `.\create_dev_symlink.ps1`
3. Lancez le DAW et chargez le plugin
4. Visual Studio : `Debug > Attach to Process...` → Sélectionnez le DAW
5. Placez des breakpoints et utilisez le plugin

---

## 🔧 Scripts PowerShell Utiles

Depuis la **racine** du projet (`C:\Users\User\Documents\trae_projects\VST3\`) :

```powershell
# Instructions pour configurer le projet de démarrage
cd build
.\set_startup_project.ps1

# Ouvrir Visual Studio
.\open_vs.ps1

# Rebuild complet si problèmes
.\rebuild_clean.ps1 -Configuration Debug
```

---

## ❓ Pourquoi cette Erreur ?

CMake génère un projet spécial `ALL_BUILD` qui compile tous les projets, mais ne produit pas d'exécutable. Visual Studio le sélectionne par défaut comme projet de démarrage, d'où l'erreur.

**Solution** : Définir manuellement un projet exécutable comme projet de démarrage (ex: `OpenVoxTuner_Standalone`).

---

**Besoin d'aide ?** Consultez [DEBUG_GUIDE.md](DEBUG_GUIDE.md) pour un guide complet.
