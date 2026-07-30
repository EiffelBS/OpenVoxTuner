# Analyse de faisabilit\u00e9 : 8 Nouveaux Presets Pad pour les Harmonies

**Date :** 2026-07-28
**Statut :** Pr\u00e9d\u00e9ployment pour analyse

---

## 1. Analyse de l'Architecture Existant

### 1.1 Pipeline d'Harmonie Actuel

Le moteur d'harmonie (`HarmonyEngine.h/cpp`) fonctionne selon ce sch\u00e9ma :

```
Input f0 (Hz) + Scale Info
       |
       v
getHarmonyNotes() -> [f1, f2, f3, f4] (freqs)
       |
       v
Per-voice loop (per-block, per-sample):
  - Phase accumulation (sin-based oscillator)
  - Tone waveform selection (switch on toneMode 0-5)
  - Amplitude envelope (smoothstep attack, one-pole decay)
  - Stereo panning (fixed per voice: R, L, RR, LL)
  |
  v
Output buffer -> Gain matching -> Mix with dry signal
```

### 1.2 Contraintes Technique Majeure

**Le `HarmonyEngine::renderHarmonies()` op\u00e8re dans une boucle `for (int i = 0; i < numSamples; ++i)` par \u00e9chantillon.** Chaque \u00e9chantillon doit \u00eatre calcul\u00e9 en O(1) avec une complexit\u00e9 minimale.

**Ressources DSP disponibles :**
| Ressource | Disponibilit\u00e9 | Usage actuel |
|-----------|-----------------|--------------|
| `std::sin()` (oscillateur sinuso\u00efdal) | Oui (JUCE) | Fundamental + harmoniques |
| Harmoniques 2\u00e8me, 3\u00e8me, 4\u00e8me | Oui (`sin(n*p)`) | Tous les presets |
| Enveloppe d'amplitude | Oui (smoothstep+one-pole) | Attack/release global |
| LFO传统 | **Non** | Aucun module |
| Filtre IIR (LP/HP/BP/PEAK) | **Non** (sauf FormantPreserver) | Aucun filtre de tonalit\u00e9 |
| Effets (Delay/Reverb/Chorus/Distortion) | Reverb unique (post-pipeline) | Non utilis\u00e9 pour les tones |
| Modulation d'amplitude | Oui (ring mod sur vocoder) | Vocoder-like uniquement |
| Saturation | Oui (`tanh()`) | Synth Lead uniquement |
| Phase accumulation persistante | Oui (variable `phase` par voix) | Tous les presets |
| \u00c9tat par voix | `phases[]`, `amplitudes[]` | 2 vecteurs |

### 1.3 Param\u00e8tres Utilisables pour la Synth\u00e8se

Les seuls param\u00e8tres de modulation disponibles **en temps r\u00e9el par \u00e9chantillon** sont :
1. **Phase** (`phase` / `p`) : unique variable de temps interne \u00e0 l'oscillateur
2. **Harmoniques** `sin(2*p)`, `sin(3*p)`, `sin(4*p)` : 3 variantes
3. **Param\u00e8tre `color`** (0.0-1.0) : param\u00e8tre continu global par pr\u00e9set
4. **Amplitude enveloppe** (`amp`) : enveloppe LFO par note (pas par \u00e9chantillon)
5. **Saturation tanh()** : non-lin\u00e9arit\u00e9 utilisable
6. **\u00c9tat persistant par voix** : `phases[]` (1 vecteur double)

### 1.4 Limites Critiques

| Limite | D\u00e9tail | Impact |
|--------|---------|--------|
| **Pas de filtre** | Pas de bande-passante, pas de r\u00e9sonance | Impossible de cr\u00e9er un v\u00e9ritable "formant sweeping" |
| **Pas de LFO** | Pas d'oscillateur basse-fr\u00e9quence ind\u00e9pendant | Impossible de faire un "breathe" r\u00e9el (modulation < 1 Hz) |
| **Pas de delay** | Pas de m\u00e9moire d'\u00e9chantillon | Pas de chorus, phaser, delays |
| **Pas de wavetable** | Uniquement sin/n*sin | Impossible de cr\u00e9er des ondes complexes natives |
| **Par \u00e9chantillon** | Chaque \u00e9chantillon doit \u00eatre O(1) | Toute m\u00e9moire > 1 double par voix est un risque CPU |
| **Max 4 harmoniques** | sin(4*p) | Pas d'harmoniques sup\u00e9rieures \u00e0 4\u00e8me |

### 1.5 Strat\u00e9gie de Synth\u00e8se Propos\u00e9e

\u00c9tant donn\u00e9 ces contraintes, toutes les nouvelles sons doivent \u00eatre impl\u00e9ment\u00e9s **exclusivement** via :
- **Combinaisons pond\u00e9r\u00e9es** des harmoniques existantes (base, h2, h3, h4)
- **Modulation de phase** (d\u00e9rive de phase lente pour simuler un LFO)
- **Saturation tanh()** (chaleur, distorsion douce)
- **Modulation d'amplitude** (ring modulation, amplitude sweep)
- **Panoramique dynamique** (d\u00e9placement entre canaux gauche/droit via phase)
- **Interpolation entre harmoniques** (simuler du filtering par redistribution spectral)
- **\u00c9tats persistants** par voix (1 \u00e0 2 doubles max pour m\u00e9moire lente)

**Aucun nouveau module DSP externe n'est requis.** L'impl\u00e9mentation se fait dans `renderHarmonies()` seulement.

---

## 2. Les 8 Nouveaux Presets Pad Propos\u00e9s

### 2.1 N\u00b06 : \"Shimmer\"

**Identit\u00e9 sonore :** Pad ethereux avec un l\u00e9ger d\u00e9doublement d'octave au-dessus et une lueur r\u00e9sonnante. Comparable \u00e0 un shimpedal de synth\u00e9 (Roland Juno Shimmer, Prophet Shimmer).

**Cha\u00eene de synth\u00e8se :**
```
Shimmer = 0.45*f1 + 0.35*f1detuned + 0.12*h2 + 0.05*h3 + 0.03*(2f1)
f1detuned = sin(phase * (1 + slowPhaseWander))
slowPhaseWander = sin(phase * 0.008) * 0.003  // ultra lente
(2f1) = sin(2*f1_phase)  // 2\u00e8me "sub-octave" via phase doubling
```

**Param\u00e8tres techniques :**
- D\u00e9rive de phase lente (`sin(phase*0.008)*0.003`) simul\u00e9 via phase modulation
- 7e composante : octavisation haute fr\u00e9quence
- Pas de saturation
- Enveloppe l\u00e8chement plus long (release +20%)

**Int\u00e9gration :** 1 \u00e9tat persistant suppl\u00e9ment (`shimmerPhase`) dans `phases[]`

**Color parameter :** Module la largeur de d\u00e9rive de phase (`0.008` \u00b1 `0.004*color`)

---

### 2.2 N\u00b07 : \"Warm Sub-Pad\"

**Identit\u00e9 sonore :** Pad grave et enveloppant, centr\u00e9 sur les basse et fondamentale. Comparable \u00e0 un accesspad analogique (Yamaha CS-80 bass pad, Sequential Prophet pad grave).

**Cha\u00eene de synth\u00e8se :**
```
Warm = 0.55*f1 + 0.25*f1sub + 0.12*h2 + 0.06*tanh(0.8*f1)
f1sub = sin(0.5 * phase)  // sous-oscillateur octave en dessous
```

**Param\u00e8tres techniques :**
- Sous-oscillateur \u00e0 octave en dessous (`sin(phase/2)`)
- L\u00e9g\u00e8re saturation tanh sur la fondamentale pour chaleur
- \u00c9nergie principalement dans le bas du spectre (< 500 Hz)
- Panoramique large (L/R \u00e9tendu)

**Int\u00e9gration :** 0 \u00e9tat persistant suppl\u00e9mentaire (sub calcul\u00e9 directement)

**Color parameter :** Module le saturation (`tanh(0.8*color*f1)`) et la force du sub (`0.25*color`)

---

### 2.3 N\u00b08 : \"Glass\"

**Identit\u00e9 sonore :** Son cristallin, m\u00e9tallique, avec des harmoniques aigus nets. Comparable \u00e0 un glass harmonica, glockenspiel synth\u00e9tique, ou patch \"crystal\" de Korg Prophecy.

**Cha\u00eene de synth\u00e8se :**
```
Glass = 0.15*f1 + 0.10*h2 + 0.40*h3 + 0.25*h4 + 0.10*tanh(1.5*h3)
```

**Param\u00e8tres techniques :**
- Pond\u00e9ration extr\u00eamement orient\u00e9e vers les harmoniques 3 et 4
- Saturation l\u00e9g\u00e8re sur h3 pour accentuer le c\u00f4t\u00e9 m\u00e9tallique
- Attaque tr\u00e8s rapide (smoothstep court), release moyen
- Pas de modulation, son pur et direct

**Int\u00e9gration :** 0 \u00e9tat suppl\u00e9mentaire

**Color parameter :** D\u00e9place l'\u00e9quilibre h3/h4 (`0.40 \u00b1 0.15*color` sur h3, `0.25 \u00b1 0.15*color` sur h4)

---

### 2.4 N\u00b09 : \"Breath\"

**Identit\u00e9 sonore :** Pad inspir\u00e9 du souffle vocal, sans formants prononc\u00e9s, oscillant doucement. Comparable \u00e0 un breath pad de Native Instruments \"Rozart Voices\" ou \"Session Brass\" pad.

**Cha\u00eene de synth\u00e8se :**
```
Breath = (0.6*f1 + 0.2*h2 + 0.15*h3 + 0.05*h4) * breathEnv
breathEnv = 0.5 + 0.5 * sin(phaseSlow)
phaseSlow += phaseInc * breathSpeed  // accumulateur lent
breathSpeed = 0.0008 \u00b1 0.0004*color  // ~1-2 Hz \u00e0 44.1kHz
```

**Param\u00e8tres techniques :**
- Accumulateur de phase lent ind\u00e9pendant (`phaseBreath` \u00e0 ajouter)
- Enveloppe de souffle appliqu\u00e9e sur tout le signal
- Pond\u00e9ration douce privil\u00e9giant fondamentale et 2\u00e8me harmonique
- Pas de saturation

**Int\u00e9gration :** 1 \u00e9tat persistant suppl\u00e9mentaire (`phaseBreath` par voix, initialis\u00e9 al\u00e9atoirement pour \u00e9viter la synchronisation)

**Color parameter :** Module la vitesse du breath (`0.0008 \u00b1 0.0004*color`) et la profondeur de l'enveloppe

---

### 2.5 N\u00b010 : \"Analog Pad\"

**Identit\u00e9 sonore :** Pad analogique chaud \u00e0 onde triangulaire synth\u00e9tique, comparable \u00e0 un Juno-60 pad, Oberheim OB-6, ou Korg MS-20 pad.

**Cha\u00eene de synth\u00e8se :**
```
// Approximation triangulaire par somme de sin
TriangleApprox = f1 - 0.25*h2 + 0.11*h3 - 0.06*h4  // s\u00e9rie de Fourier r\u00e9duite
Analog = 0.7*TriangleApprox + 0.2*f1 + 0.1*f1detuned
f1detuned = sin(phase * (1 + detune))
detune = sin(phase*0.01) * 0.002  // d\u00e9rive de phase ultra lente
```

**Param\u00e8tres techniques :**
- Approximation de onde triangulaire (s\u00e9rie de Fourier tronqu\u00e9e \u00e0 4 termes)
- L\u00e9g\u00e8re d\u00e9rive de phase (synchro-drift caract\u00e9ristique des synth\u00e9s analogiques)
- Saturation tanh l\u00e9g\u00e8re (`tanh(1.1*Analog)`)
- Enveloppe moyenne (attack 35ms, release 80ms)

**Int\u00e9gration :** 0 \u00e9tat suppl\u00e9mentaire (l'allonge de phase est calcul\u00e9e \u00e0 partir de `phase` existant)

**Color parameter :** Module la quantit\u00e9 de d\u00e9rive (`0.002*color`) et de saturation

---

### 2.6 N\u00b011 : \"CinePad\"

**Identit\u00e9 sonor :** Grand pad orchestral/cin\u00e9matographique, large et immersif, comparable \u00e0 un pad de Hans Zimmer ou Two Steps From Hell.

**Cha\u00eene de synth\u00e8se :**
```
CinePad = 0.35*f1 + 0.25*f1detuned + 0.15*h2 + 0.12*h3 + 0.08*h4 + 0.05*tanh(0.6*h2)
f1detuned = sin(phase*(1+0.008)) + sin(phase*(1-0.008)) // double detune large
```

**Param\u00e8tres techniques :**
- Triple d\u00e9doublement (fondamentale + 2 d\u00e9saccords larges)
- Pond\u00e9ration \u00e9quilibr\u00e9e sur toutes les harmoniques
- L\u00e9g\u00e8re saturation sur h2 pour corpulence
- Enveloppe longue (attack 50ms, release 120ms)
- Panoramique large

**Int\u00e9gration :** 0 \u00e9tat suppl\u00e9mentaire (d\u00e9saccords statiques bas\u00e9s sur `phase`)

**Color parameter :** Module la largeur du d\u00e9saccord (`0.008 \u00b1 0.004*color`) et la saturation

---

### 2.7 N\u00b012 : \"Wobble\"

**Identit\u00e9 sonor :** Pad modul\u00e9 par un battement rapide caract\u00e9ristique (analogie : LFO d\u00e9saccord, style wobble bass mais en milieu/aigu). Comparable \u00e0 un viber wobble de Moog ou un flanger tr\u00e8s l\u00e9ger.

**Cha\u00eene de synth\u00e8se :**
```
// Wobble par interm\u00e9diaire de phase oscillante
WobbleBase = 0.5*f1 + 0.3*h2 + 0.15*h3 + 0.05*h4
wobblePhase = sin(phaseSlow)
wobblePhase += phaseInc * wobbleSpeed
wobbleSpeed = 0.006 \u00b1 0.003*color  // ~8-15 Hz \u00e0 44.1kHz

// Modulation de phase rapide
wobble = WobbleBase + 0.04 * wobblePhase * sin(2*f1_phase)
```

**Param\u00e8tres techniques :**
- Accumulateur de phase lent ind\u00e9pendant (`phaseWobble` \u00e0 ajouter)
- L'accumulateur module une petite quantit\u00e9 d'harmonique (PM l\u00e9ger)
- Vitesse ~8-15 Hz (audible mais pas tr\u00e8s rapide)
- Pas de saturation

**Int\u00e9gration :** 1 \u00e9tat persistant suppl\u00e9mentaire (`phaseWobble` par voix)

**Color parameter :** Module la vitesse (`0.006 \u00b1 0.003*color`) et la profondeur de modulation

---

### 2.8 N\u00b013 : \"ReverseReverb\"

**Identit\u00e9 sonor :** Pad avec un swell progressif caract\u00e9ristique (comme un reverse reverb). Le son monte en intensit\u00e9 de mani\u00e8re organique, simulant l'effet de reverb invers\u00e9e. Comparable \u00e0 un pad de Cocteau Twins ou Radiohead.

**Cha\u00eene de synth\u00e8se :**
```
ReverseBase = 0.5*f1 + 0.2*h2 + 0.15*h3 + 0.1*h4
reverseEnv = 1.0 - exp(-phaseRevRev)
reverseEnv = max(0.0, min(1.0, reverseEnv))
reverseEnv = smooth(reverseEnv)  // enveloppe douce

phaseRevRev += phaseInc * revSpeed
revSpeed = 0.003  // ~1 cycle par mesure (rapide)
```

**Param\u00e8tres techniques :**
- Accumulateur de phase lent ind\u00e9pendant (`phaseRevRev` par voix)
- Enveloppe exponentielle montante sur tous les \u00e9chantillons
- Pond\u00e9ration \u00e9quilibr\u00e9e sur fondamentale + harmoniques m\u00e9dium
- L\u00e9g\u00e8re saturation tanh

**Int\u00e9gration :** 1 \u00e9tat persistant suppl\u00e9mentaire (`phaseRevRev` par voix, initialis\u00e9 \u00e0 0 au restart)

**Color parameter :** Module la vitesse (`0.003 \u00b1 0.002*color`) et la saturation

---

## 3. V\u00e9rification de Faisabilit\u00e9 Technique

### 3.1 Tableau de Faisabilit\u00e9

| N\u00b0 | Preset | Nbre \u00c9tats Supp. | Complexit\u00e9 CPU | Faisabilit\u00e9 | Remarques |
|-----|--------|-------------------|-----------------|-------------|-----------|
| 6 | Shimmer | 1 (`shimmerPhase`) | Faible (\u2248+2 sin/cycle) | \u2705 Oui | Modulation de phase directe |
| 7 | Warm Sub-Pad | 0 | Tr\u00e8s faible | \u2705 Oui | Calcul direct `sin(phase/2)` |
| 8 | Glass | 0 | Tr\u00e8s faible | \u2705 Oui | Combinatoire simple |
| 9 | Breath | 1 (`phaseBreath`) | Faible (\u2248+2 sin/cycle) | \u2705 Oui | Accumulateur lent + multiplier |
| 10 | Analog Pad | 0 | Faible | \u2705 Oui | Approximation s\u00e9rie Fourier |
| 11 | CinePad | 0 | Faible | \u2705 Oui | Multiple detune statique |
| 12 | Wobble | 1 (`phaseWobble`) | Moyen (\u2248+3 sin/cycle) | \u26a0\ufe0f Attention | 3 appels sin additionnels |
| 13 | ReverseReverb | 1 (`phaseRevRev`) | Faible | \u2705 Oui | exp() + accumulateur |

### 3.2 \u00c9volution de la M\u00e9moire par Voix

| \u00c9tat | Avant | Apr\u00e8s ajout pad |
|-------|-------|----------------|
| `phases` (double) | 8 oct. | 12-16 oct. (si 1-2 \u00e9tats lent) |
| `amplitudes` (float) | 4 oct. | 4 oct. (inchang\u00e9) |
| `targetAmps` (float) | 4 oct. | 4 oct. (inchang\u00e9) |
| `attackSamplesRemaining` (int) | 4 oct. | 4 oct. (inchang\u00e9) |
| `attackTotalSamples` (int) | 4 oct. | 4 oct. (inchang\u00e9) |
| `attackStartAmp` (float) | 4 oct. | 4 oct. (inchang\u00e9) |
| `voicePrevGate` (uint8) | 1 oct. | 1 oct. (inchang\u00e9) |
| **Total par voix** | **~29 oct.** | **~33-37 oct.** (+14% max) |

**Risques m\u00e9moire :** N\u00e9gligeables. La m\u00e9moire par voix n'augmente que de 14% maximum.

### 3.3 Risques CPU

Le seul vrai risque est le **Wobble** (N\u00b012) qui ajoute 3 appels `sin()` par \u00e9chantillon suppl\u00e9mentaires.

**Estimation :**
- `sin()` sur x86_64 avec SSE/AVX: ~15-25 cycles
- 4 voix \u00d7 3 sin \u00d7 44100 \u00e9ch/s \u2248 5.3 millions de sin/s par bloc
- \u00c0 2.5 GHz CPU : ~0.2% de c\u0153ur suppl\u00e9mentaire par bloc
- **Impact :** N\u00e9gligeable sur tout CPU moderne m\u00eame 1\u00e8re g\u00e9n\u00e9ration

### 3.4 Int\u00e9gration dans l'Architecture Existante

**Fichiers \u00e0 modifier :**
| Fichier | Modifications |
|---------|--------------|
| `HarmonyEngine.h` | ajouter 1-2 `std::vector<double>` pour \u00e9tats lents (max 2) |
| `HarmonyEngine.cpp` | 1. `prepare()` : redimensionner vecteurs. 2. `renderHarmonies()` : switch addtionnel pour cas 6-13. 3. `setVoiceGate()` : reset \u00e9tats lents si besoin |
| `PluginProcessor.cpp` | rien (aucun nouveau param\u00e8tre, `toneMode` existe d\u00e9j\u00e0) |
| `PluginProcessor.h` | rien |
| `PluginEditor.cpp` | 1. Ajouter les noms dans le ComboBox Harmony Tone. 2. Traduction en 6 langues si n\u00e9cessaire |

**Nouveaux param\u00e8tres n\u00e9cessaires :** Aucun. Tous les pr\u00e9sets utilisent les param\u00e8tres existants (`harmony_tone` choice, `harmony_tone_color` continuous).

**Retrocompatibilit\u00e9 :** Tous les pr\u00e9sets existants (0-5) restent inchang\u00e9s. L'ajout de cas dans le `switch` est non-destructif.

**Conclusion :** L'impl\u00e9mentation est **entri\u00e8rement faisable sans aucun nouveau module DSP externe**. Tout se fait dans le `switch` de `renderHarmonies()`.

---

## 4. Planning de D\u00e9veloppement et de Tests

### 4.1 Phase 1 : Impl\u00e9mentation de Base (4 nouveaux presets)

**Packets :** Glass + Warm Sub-Pad + Analog Pad + CinePad (N\u2078, 7, 10, 11)

**Crit\u00e8re de s\u00e9curit\u00e9 :** 0 \u00e9tat suppl\u00e9mentaire par voix. Complexit\u00e9 CPU minimale.

T\u00e2ches :
1. Ajouter 4 `case` dans le `switch (toneMode)` de `renderHarmonies()` (HarmonyEngine.cpp)
2. Ajouter 4 pr\u00e9sets dans le ComboBox de PluginEditor.cpp
3. Traduire les 4 noms en 6 langues (OVTLanguages.h)
4. Tests unitaires de validation (RMS, clipping, CPU)

**Dur\u00e9e :** 1 session de d\u00e9veloppement

### 4.2 Phase 2 : Tests d'Identit\u00e9 Sound (Phase 1)

**Activit\u00e9 :** \u00c9coute comparative side-by-side sur \u00e9coutes haute qualit\u00e9 (casque/open-back) et environnements vari\u00e9s.

**M\u00e9thodologie :**
1. Cr\u00e9er un projet test dans un DAW : chanter une s\u00e9rie d'accords diatoniques (Am, C, F, G)
2. Pour chaque accord, passer en revue les 4 nouveaux presets \u00e0 volume \u00e9galis\u00e9
3. Noter pour chaque pr\u00e9set :
   - Distinction claire par rapport aux autres nouveaux presets (note 1-5)
   - Distinction claire par rapport aux pr\u00e9sets existants (Choir, Vocoder-like) (note 1-5)
   - Pl\u00e2it sonore g\u00e9n\u00e9ral (note 1-5)
   - Probl\u00e8mes perceptibles (clipping, resonances, sons \"morts\")

**Crit\u00e8re d'acceptation :** Tous les pr\u00e9sets doivent obtenir une note d'identit\u00e9 \u22653/5 par rapport \u00e0 tous les autres.

### 4.3 Phase 3 : Impl\u00e9mentation \u00e9volu\u00e9e (4 nouveaux presets)

**Packets :** Shimmer + Breath + Wobble + Reverse Reverb (N\u2076, 9, 12, 13)

**Crit\u00e8re :** 1 \u00e9tat suppl\u00e9mentaire par voix pour chaque. Validation CPU.

T\u00e2ches :
1. Ajouter les `std::vector<double>` n\u00e9cessaires dans HarmonyEngine.h
2. Initialiser et g\u00e9rer les nouveaux \u00e9tats dans `prepare()` et `renderHarmonies()`
3. Ajouter les `case` dans le `switch` (N\u2076, 9, 12, 13)
4. Ajouter les 4 nouveaux pr\u00e9sets dans le ComboBox et traduire
5. Tests de validation CPU avec profiling

**Dur\u00e9e :** 1-2 sessions de d\u00e9veloppement

### 4.4 Phase 4 : Tests Comparatifs Complets

**Activit\u00e9 :** M\u00eame proc\u00e9dure que Phase 2 mais sur les 12 presets (6 existants + 8 nouveaux).

**Sc\u00e9narios d'\u00e9coute :**
1. Voix masculine grave (Bass/Tenor) avec accords mineurs
2. Voix feminine aigu\u00eb (Alto/Soprano) avec accords majeurs
3. Monodie (note unique, pas d'accord) pour \u00e9valuer le caract\u00e8re du pad
4. Tempo rapide (chant r\u00e9p\u00e9titif) pour tester les attaques
5. Tempo lente (notes longues) pour tester les d\u00e9veloppements
6. Avec/br sans reverb pour v\u00e9rifier la compatibilit\u00e9 avec les effets

**Crit\u00e8re d'acceptation :** M\u00eame m\u00e9trique que Phase 2 (note d'identit\u00e9 \u22653/5 partout).

### 4.5 Phase 5 : Optimisation et Raffinements

T\u00e2ches :
1. Ajuster les pond\u00e9rations bas\u00e9es sur les retours d'\u00e9coute
2. \u00c9galiser le volume RMS entre tous les presets (gain match)
3. Ajuster les enveloppes si n\u00e9cessaire (certains presets peuvent b\u00e9n\u00e9ficier d'attack/release sp\u00e9cifiques)
4. V\u00e9rifier la compatibilit\u00e9 avec les diff\u00e9rents modes de d\u00e9tection de gamme

**Dur\u00e9e :** 1 session

### 4.6 Planning R\u00e9sum\u00e9

| Phase | Contenu | Dur\u00e9e Estim\u00e9e | D\u00e9pendances |
|-------|---------|-----------------|-------------|
| Phase 1 | Impl\u00e9m. (Glass, SubPad, Analog, CinePad) | 1 session | Aucune |
| Phase 2 | Tests identit\u00e9 Phase 1 | 1 session | Phase 1 termin\u00e9e |
| Phase 3 | Impl\u00e9m. \u00e9volu\u00e9e (Shimmer, Breath, Wobble, RevRvb) | 1-2 sessions | Phase 2 valid\u00e9e |
| Phase 4 | Tests complets 12 presets | 1-2 sessions | Phase 3 termin\u00e9e |
| Phase 5 | Optimisation/ajustements | 1 session | Phase 4 valid\u00e9e |

**Total estim\u00e9 :** 4-6 sessions de d\u00e9veloppement + 2-4 sessions de test

---

## 5. Contraintes Techniques et Limites \u00e0 Anticiper

### 5.1 Limites Inherent\u00e8s \u00e0 l'Architecture Sin/Cos

| Limite | Impact | Att\u00e9nuation |
|--------|--------|-------------|
| Pas de vraie forme d'onde | Impossible de g\u00e9n\u00e9rer saw, square, triangle r\u00e9elle | Approximation par s\u00e9rie de Fourier (limit\u00e9e \u00e0 4 termes) |
| Pas de filtre passe-bas/r\u00e9sonant | Impossible de cr\u00e9er un \"filter sweep\" v\u00e9ritable | Interpolation entre harmoniques simule un sweep spectral |
| Pas de delay/feedback | Pas de chorus, flanger, phaser, delay naturel | Simul\u00e9 par d\u00e9saccords de phase (chorus artificiel) |
| Pas de vrai LFO < 1Hz | Pas de m\u00e9lodious vibrato vraiment lent | Simul\u00e9 par accumulateur de phase \u00e0 tr\u00e8s basse vitesse |

### 5.2 Risques de Qualité Sonore

**Risque 1 : Confusion de timbre**
Les pr\u00e9sets utilisant les m\u00eames harmoniques avec pond\u00e9rations diff\u00e9rentes risque d'\u00eatre trop similaires (ex: Glass vs CinePad peuvent sonner proches).
- *Att\u00e9nuation :* Diff\u00e9rencier fortement les pond\u00e9rations et utiliser des effets secondaires (saturation pour CinePad, pas de saturation pour Glass)

**Risque 2 : Clipping/Overload**
La saturation tanh peut cr\u00e9er des pics si l'amplitude n'est pas bien contr\u00f4l\u00e9e.
- *Att\u00e9nuation :* Limiter l'amplitude d'entr\u00e9e de tanh (\u22641.5) et pr\u00e9server le clamp existant (lignes 417-419 de HarmonyEngine.cpp)

**Risque 3 : Synchronisation des accumulateurs**
Les accumulateurs lents (breath, shimmer, wobble, reverse) peuvent se synchroniser entre voix si initialis\u00e9s de mani\u00e8re d\u00e9terministe.
- *Att\u00e9nuation :* Initialiser al\u00e9atoirement (`random.nextDouble() * twoPi`) ou d\u00e9caler par index de voix (`phase * (1.0 + 0.001 * voiceIndex)`)

**Risque 4 : R\u00e9sonance fr\u00e9quente**
Certaines combinaisons d'harmoniques peuvent cr\u00e9er des battements ind\u00e9sirables (beat frequencies) entre voix邻近s de la m\u00eate harmonie.
- *Att\u00e9nuation :* Limiter les d\u00e9saccords \u00e0 < 10 cents et s'assurer que les pr\u00e9sets avec d\u00e9saccord (Shimmer, Analog, CinePad) ne sont pas utilis\u00e9s avec des harmonies serr\u00e9es (octaves)

### 5.3 Contraintes Audio Mat\u00e9rielles

| Contrainte | Impact |
|-----------|--------|
| Fr\u00e9quence d'\u00e9chantillonnage | \u00c0 44.1 kHz, l'harmonique 4 \u00e0 200 Hz = 800 Hz (OK). \u00c0 44.1 kHz, l'harmonique 4 \u00e0 700 Hz = 2800 Hz (encore OK, mais limite Nyquist si f0 > 1000 Hz) |
| Sortie st\u00e9r\u00e9o | Tous les presets utilisent le panoramique fixe existant. Aucun n\u00e9cessite un panoramique dynamique |
| Latence | Les nouveaux presets n'ajoutent quasi aucun Latence (pas de m\u00e9moire tampon) |

### 5.4 Sc\u00e9narios Utilis\u00e0 Proscrire

**\u00c9viter d'utiliser :**
- **Wobble** sur des intervalles tr\u00e8s serr\u00e9s (tiers, unisson) : les battements s'additionnent au lieu de se combiner harmonieusement
- **Glass** sur des notes trop aig\u00fces (> 800 Hz) : les harmoniques 3 et 4 approchent de la limite Nyquist
- **Shimmer** avec des harmonies d'octaves : la sous-octave haute peut cr\u00e9er un effet \"ring mod\" non d\u00e9sir\u00e9 avec les autres voix
- **Warm Sub-Pad** en sortie mono : le sous-oscillateur \u00e0 octave en dessous peut \u00eatre inaudible ou cr\u00e9er des probl\u00e8mes de phase

---

## 6. R\u00e9sum\u00e9 Ex\u00e9cutif

### Synth\u00e8se des Contraintes
- **Architecture actuelle :** Synth\u00e8se additive sinuso\u00efdale pure, sans filtre ni LFO externe
- **Contrainte principale :** Tout doit fonctionner dans une boucle par \u00e9chantillon O(1), avec < 2 doubles d'\u00e9tat suppl\u00e9mentaire par voix
- **Faisabilit\u00e9 :** \u2705 **100% faisable sans nouveau module DSP externe**

### R\u00e9sum\u00e9 des 8 Presets Propos\u00e9s

| N\u00b0 | Nom | Type | Complexit\u00e9 | \u00c9tats Supp. | Risque |
|-----|-----|------|-------------|-------------|--------|
| 6 | Shimmer | Pad ethereux octavi\u00e9 | Faible | 1 | Tr\u00e8s faible |
| 7 | Warm Sub-Pad | Pad grave chaud | Tr\u00e8s faible | 0 | Aucun |
| 8 | Glass | Pad cristallin aigu | Tr\u00e8s faible | 0 | Aucun |
| 9 | Breath | Pad souffle oscillant | Faible | 1 | Faible |
| 10 | Analog Pad | Pad triangulaire chaud | Faible | 0 | Tr\u00e8s faible |
| 11 | CinePad | Grand pad orchestral | Faible | 0 | Tr\u00e8s faible |
| 12 | Wobble | Pad modul\u00e9 battement | Moyen | 1 | Moyen |
| 13 | Reverse Reverb | Pad swell progressif | Faible | 1 | Faible |

### Plan D\u00e9ploiement Recommand\u00e9
1. **Imm\u00e9diat :** Impl\u00e9menter les 4 presets sans \u00e9tat suppl\u00e9mentaire (N\u2078, 7, 10, 11)
2. **Post-validation :** Impl\u00e9menter les 4 presets avec \u00e9tat suppl\u00e9mentaire (N\u2076, 9, 12, 13)
3. **Total du projet :** 4-6 sessions de d\u00e9veloppement + 2-4 sessions de tests

### Co\u00fbt Estim\u00e9 (en ressources de d\u00e9veloppement)
- Code : ~300-500 lignes de modifications (HarmonyEngine.h/cpp + PluginEditor.cpp + OVTLanguages.h)
- Tests : ~2-3 jours d'\u00e9coute comparative
- Risque global : **Faible** (\u00e0 condition de valider la phase 1 avant de passer \u00e0 la phase 3)

---

*Fin du rapport.*
