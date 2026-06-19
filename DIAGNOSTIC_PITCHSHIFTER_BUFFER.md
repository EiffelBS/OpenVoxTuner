# 🔍 Diagnostic : Pourquoi le PitchShifter ne Modifie Pas le Buffer

## Problème

Le plugin détecte le pitch (`f0_in`), calcule les ratios, appelle `pitchShifter->process()`, **MAIS** on n'entend pas l'audio tuné en mode VST3 dans un DAW (alors que ça fonctionne en standalone).

## Changements Apportés

### 1. Ajout de Compteurs de Diagnostic

**Fichier** : `Source/PluginProcessor.h` (lignes 207-208)

Ajout de deux compteurs atomiques :
- `pitchShifterModifiedCount` : Nombre de fois où le buffer EST modifié
- `pitchShifterUnmodifiedCount` : Nombre de fois où le buffer N'EST PAS modifié

### 2. Surveillance du Buffer Après PitchShifter

**Fichier** : `Source/PluginProcessor.cpp` (lignes 846-872)

**Avant chaque appel** à `pitchShifter->process()` :
- Capture des 8 premiers samples du buffer (L+R)

**Après chaque appel** :
- Compare les samples avant/après
- Calcule le RMS de la différence
- Si `rmsL > 1e-9` ou `rmsR > 1e-9` → buffer modifié → incrémente `pitchShifterModifiedCount`
- Sinon → buffer non modifié → incrémente `pitchShifterUnmodifiedCount`

**Log throttlé (1x/seconde)** :
```
pitchShifter post-check: rmsDiffL=... rmsDiffR=... | modified=X unmodified=Y
```

## Ce Que Ce Log Va Révéler

### Scénario 1 : `modified=0, unmodified=~50`
**Diagnostic** : Le `pitchShifter` ne modifie **JAMAIS** le buffer.

**Causes possibles** :
1. Le `pitchShifter` n'a pas de grains actifs (pas d'output audio)
2. Le `pitchShifter` écrit dans le buffer mais avec des valeurs nulles
3. Bug dans l'algorithme PSOLA

**Action** : Investiguer dans `PitchShifter.cpp`, notamment :
- Vérifier que des grains sont créés (`gIdx >= 0`)
- Vérifier que `grains[j].active` est true
- Vérifier que `outL` et `outR` sont non-nuls
- Regarder les logs `"PitchShifter: created grain"` et `"PitchShifter.process"`

### Scénario 2 : `modified=~50, unmodified=0`
**Diagnostic** : Le `pitchShifter` modifie **TOUJOURS** le buffer.

**Causes possibles** :
1. Le buffer est modifié, mais le résultat est inaudible (gain trop faible, latence, etc.)
2. Problème de routing audio dans le DAW
3. Le DAW bypass le plugin silencieusement

**Action** :
- Vérifier le niveau de sortie du plugin dans le DAW
- Vérifier les valeurs de `rmsL` et `rmsR` (doivent être > 0.001 pour être audibles)
- Vérifier les paramètres du DAW (bypass, monitoring, etc.)

### Scénario 3 : `modified=X, unmodified=Y` (mix)
**Diagnostic** : Le `pitchShifter` modifie le buffer **parfois** (probablement quand f0 > 0).

**Explication probable** : Normal. Quand il n'y a pas de pitch détecté (`f0_in=0`), le `pitchShifter` ne génère pas de grains.

**Action** : Vérifier que les blocs **avec pitch détecté** sont bien modifiés (corrélation avec les logs `f0_in`).

## Prochaines Étapes

### 1. Recompiler et Réinstaller

```powershell
# Depuis C:\Users\User\Documents\trae_projects\VST3
.\build.ps1 -Configuration Debug -SkipConfigure

# Copier le VST3 (en mode Admin)
cd build
.\copy_vst3_to_program_files.ps1
```

### 2. Tester dans le DAW

1. Fermez **complètement** le DAW
2. Relancez-le
3. Chargez OpenVoxTuner sur une piste vocale
4. Activez la lecture avec du chant

### 3. Consulter les Logs

Ouvrez : `E:\Documents\OpenVoxTuner.log`

Cherchez les lignes `"pitchShifter post-check"` (apparaissent toutes les 1000ms).

**Exemples** :

✅ **Buffer modifié (normal)** :
```
pitchShifter post-check: rmsDiffL=0.123456789 rmsDiffR=0.098765432 | modified=50 unmodified=0
```

❌ **Buffer JAMAIS modifié (problème)** :
```
pitchShifter post-check: rmsDiffL=0.000000000 rmsDiffR=0.000000000 | modified=0 unmodified=50
```

### 4. Interprétation

#### Si `modified > 0`
Le `pitchShifter` fonctionne. Le problème est ailleurs :
- Vérifier le routing audio dans le DAW
- Vérifier les niveaux de sortie
- Vérifier les paramètres (bypass, amount, etc.)

#### Si `modified = 0`
Le `pitchShifter` ne génère **aucun** audio. Le problème est dans `PitchShifter.cpp` :
- Les grains ne sont pas créés
- Les grains sont créés mais pas actifs
- Les grains sont actifs mais produisent du silence

**Action** : Consulter les logs `"PitchShifter: created grain"` et `"PitchShifter.process"` pour voir si des grains sont créés.

---

## Logs à Surveiller

### Logs Existants

```
processBlock: f0_in=98.648 f0_out=97.999 amount=1.000 numSamples=264 buf0=-0.021254
calling pitchShifter->process: ratio=0.998700 formantRatio=1.000000 f0_in=98.648285
```

Ces logs montrent que :
- ✅ Le pitch est détecté (`f0_in=98.648`)
- ✅ Le ratio est calculé (`ratio=0.998700`)
- ✅ `pitchShifter->process()` est appelé

### Nouveau Log à Chercher

```
pitchShifter post-check: rmsDiffL=... rmsDiffR=... | modified=... unmodified=...
```

Ce log apparaît **1x/seconde** et montre si le buffer est effectivement modifié.

### Logs du PitchShifter (dans PitchShifter.cpp)

```
PitchShifter: created grain idx=0 center=960.000 outLength=480.000 F=1.000
PitchShifter.process: f0=98.648 pitchRatio=0.999 grainsActive=3 out0=0.012345
```

Ces logs montrent :
- Si des grains sont créés (`created grain`)
- Combien de grains sont actifs (`grainsActive=3`)
- La valeur du premier sample de sortie (`out0=0.012345`)

Si `out0` est toujours proche de zéro, le problème est dans l'algorithme PSOLA.

---

## Hypothèses

### Hypothèse 1 : Pas de Grains Créés (Mode VST3)

**Symptôme** : Pas de log `"PitchShifter: created grain"` en mode VST3, mais ces logs apparaissent en standalone.

**Cause possible** : Différence de timing ou de `sampleRate` entre standalone et VST3.

**Vérification** : Comparer les logs `prepareToPlay` entre standalone et VST3 :
```
prepareToPlay: sampleRate=48000 samplesPerBlock=264 pitchShifter_latency=960
```

### Hypothèse 2 : Grains Créés Mais Inaudibles

**Symptôme** : Des grains sont créés (`grainsActive > 0`), mais `out0` est très faible (< 1e-6).

**Cause possible** : Problème de gain dans l'algorithme PSOLA (ligne 179 de `PitchShifter.cpp`).

### Hypothèse 3 : Buffer Modifié Mais Écrasé Après

**Symptôme** : `rmsDiff > 0` juste après `pitchShifter->process()`, mais l'audio n'est pas entendue.

**Cause possible** : Un code après le `pitchShifter->process()` écrase le buffer (ex: mixing harmony avec gain=0).

**Vérification** : Ajouter un log **à la toute fin** de `processBlock()` pour voir les dernières valeurs du buffer.

---

## Compilation et Test

### Commandes Rapides

```powershell
# Rebuild
cd C:\Users\User\Documents\trae_projects\VST3
.\build.ps1 -Configuration Debug -SkipConfigure

# Installer
cd build
.\copy_vst3_to_program_files.ps1  # En mode Admin

# Tester
# -> Lancez le DAW, chargez le plugin, testez

# Consulter logs
notepad E:\Documents\OpenVoxTuner.log
```

### Vérifier la Version

Dans l'UI du plugin, vérifiez le build timestamp affiché pour confirmer que c'est bien la dernière version.

---

**Ce diagnostic va nous dire exactement où se situe le problème** : dans le PitchShifter (pas de grains ou grains silencieux) ou ailleurs (routing, mixing, DAW).
