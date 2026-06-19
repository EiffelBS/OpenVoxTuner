# ⚡ ERREUR ALL_BUILD ? Lisez-moi !

## 🎯 Solution Rapide

Si Visual Studio affiche l'erreur :
```
Impossible de démarrer le programme
C:\Users\User\Documents\trae_projects\VST3\build\x64\Debug\ALL_BUILD
```

**Exécutez ce script** pour voir la solution :
```powershell
.\fix_all_build_error.ps1
```

**OU suivez ces 2 étapes dans Visual Studio :**

### 1. Définir le Projet de Démarrage
```
Explorateur de Solutions
  ↓
Clic droit sur "OpenVoxTuner_Standalone"
  ↓
"Définir comme projet de démarrage"
```

### 2. Lancer
```
Appuyez sur F5
```

---

## 📚 Documentation Complète

- **Guide de débogage complet** : [../DEBUG_GUIDE.md](../DEBUG_GUIDE.md)
- **Guide de build** : [../BUILD_GUIDE.md](../BUILD_GUIDE.md)
- **Solution rapide** : [QUICKFIX_ALL_BUILD_ERROR.md](QUICKFIX_ALL_BUILD_ERROR.md)

---

**C'est tout !** Le problème vient du fait que `ALL_BUILD` est un projet CMake qui compile tout mais ne produit pas d'exécutable.
