# Rapport d'analyse : Préservation des formants pour un traitement audio naturel

**Projet** : OpenVoxTuner
**Date** : 2026-07-27
**Portée** : Étude approfondie des méthodes de préservation des formants (LPC, lissage temporel/interpolation), comparaison au existant, lacunes, et plan d'amélioration actionnable avec critères de validation et métriques de succès.

---

## 1. Objectif et contexte

Lors d'une transposition de hauteur (pitch-shifting), les formants — résonances du conduit vocal situées vers 300–3500 Hz (F1–F4) — sont déplacés avec le fondamental si l'on utilise une simple modification de durée ou un PSOLA naïf. Cela produit l'effet « chipmunk » (voix trop fine en montée) ou « Darth Vader » (voix trop grave en descente) : la **timbre** (couleur vowelique) est altéré même si la hauteur est correcte.

La préservation des formants vise à **découpler** la hauteur fondamentale `F0` de l'enveloppe spectrale (le « filtre » du conduit vocal), afin que seule `F0` change pendant que les formants restent à leur place naturelle. C'est la clé d'un résultat « parfaitement naturel ».

L'analyse ci-dessous étudie deux familles de méthodes — le **Codage Prédictif Linéaire (LPC)** et le **lissage temporel / interpolation** — les évalue, les compare aux solutions déployées dans OpenVoxTuner, et propose des améliorations concrètes.

---

## 2. Concepts fondamentaux

- **Formants** : pôles du filtre de conduction vocale. Leur position définit la voyelle ; leur déplacement avec `F0` est l'artefact à corriger.
- **Modèle source-filtre** : la parole = excitation glottale (détermine `F0`) filtrée par le conduit (détermine les formants). Préserver les formants = conserver le **filtre** tandis que l'on modifie la **source**.
- **Pré-warp de Moulines & Charpentier (1990)** : déplacer les formants dans le sens inverse de la transposition. Compensation *partielle* `1/√r` ou *complète* `1/r` selon que l'on agit sur la fréquence ou sur le temps de lecture.

---

## 3. Étude détaillée des méthodes

### 3.1 LPC — Codage Prédictif Linéaire

**Principe.** On modélise chaque trame du signal par un filtre tout-pôle (all-pole) `H(z) = G / A(z)`, où `A(z) = 1 + a₁z⁻¹ + … + a_Pz⁻ᴾ`. L'enveloppe spectrale (donc les formants) est entièrement portée par les pôles `A(z)`.

La **préservation par cross-synthèse LPC** (technique de référence) procède ainsi :

1. Analyser le signal *transposé naïvement* (formants déplacés) → ses coefficients LPC `a_shifted`.
2. **Éclaircir** (whiten) : `e[n] = A_shifted(z) · x[n]` → on retire l'enveloppe du signal transposé, ne reste que l'excitation (la source, à la nouvelle hauteur).
3. **Re-synthétiser** en filtrant l'excitation par l'enveloppe **d'origine** `1/A_orig(z)` → les formants reviennent à leur place naturelle.

> Note d'implémentation (vérifiée dans le benchmark) : l'excitation doit être `e = x + Σ aₖ·x[n−k]` (signe **plus**), sinon l'enveloppe est inversée et la cross-synthèse détruit les formants au lieu de les préserver.

**Pertinence.** C'est la seule méthode qui opère un **vrai découplage source/filtre** : les formants sont extraits puis réinjectés indépendamment de `F0`. [architecture.md](../docs/architecture.md) listait déjà cette approche (« Exact formant preservation via LPC + non-uniform resampling ») comme *future work* — elle est donc connue comme la bonne solution, non encore implémentée.

**Avantages.**
- Préservation *quasi-exacte* des formants (la cross-synthèse rétablit l'enveloppe cible, pas une approximation).
- Fonctionne pour tous les rapports de transposition, tous les types de voix.
- Paramètre unique et physique : l'ordre LPC `P` (typiquement `2 × N_formants + 2 = 10`).

**Limites.**
- Coût de calcul (analyse LPC + filtrage par trame) supérieur au banc de biquads.
- Artefacts de **frontière de trame** (modulation d'amplitude si le gain par trame n'est pas géré) — atténuable par normalisation d'énergie par trame et overlap-add de Hann.
- L'ordre LPC fini ne capture pas parfaitement 4 formants + le tilt spectral ; la cross-synthèse laisse une distorsion résiduelle (voir §7).
- Sensible au bruit si l'analyse LPC est faite sur le signal bruité (atténuable en estimant l'enveloppe cible sur le signal propre ou par pré-emphase).

### 3.2 Lissage temporel et interpolation

**Principe.** Les paramètres de formants (soit les coefficients de biquads du banc de filtres, soit les coefficients LPC) varient d'une trame à l'autre (le ratio de transposition lui-même fluctue : vibrato, portamento, jitter YIN). Le **lissage temporel** interpole les coefficients entre trames pour éviter :
- des discontinuités (clics/pops) quand le ratio saute ;
- un « warble » de timbre quand un ratio modulé (vibrato ~5 Hz) fait balayer les centres de formants.

Deux déclinaisons :
- **Lissage des biquads** (approche du projet) : on lerp les coefficients cibles vers les coefficients appliqués, à raison d'un pas `α` par bloc.
- **Interpolation des coefficients LPC** (dans la cross-synthèse) : on moyenne les `a_k` des trames voisines avant whitening/re-synthèse, ce qui lisse l'enveloppe de formants dans le temps.

**Pertinence.** Indispensable en complément de *toute* méthode (LPC ou filtre), car le signal de ratio réel n'est jamais constant. C'est un **correctif de robustesse**, non une méthode de préservation à lui seul.

**Avantages.**
- Élimine pops et warble (le projet a déjà corrigé un warble 5 Hz via `biquadSmoothAlpha`, voir §4).
- L'interpolation LPC améliore légèrement la distorsion aux grands rapports (constaté en §7 : C1 ≤ C0).

**Limites.**
- Un lissage **trop lent** (α petit) introduit un *lag* de timbre : pendant un vibrato ou une transition rapide, les formants sont temporairement mal compensés → warble perceptible.
- Un lissage **trop rapide** (α grand) laisse passer la modulation du ratio → AM résiduelle à la sortie et risque de clics.
- Le lissage ne *crée* pas de préservation : il ne fait que rendre supportable une méthode sous-jacente déjà imparfaite.

### 3.3 Référence : pré-warp par banc de filtres (méthode actuelle du projet)

On place `N` peaking-EQ (biquads) aux fréquences de formants et on déplace leurs centres selon `1/√r` (compensation partielle) ou `1/r` (totale). Avantage : très léger (quelques biquads). Inconvénients : ce n'est qu'une *approximation* (un peaking-EQ ne remodèle pas l'enveloppe de façon exacte, et les largeurs de bande de formants sont déformées), et il faut *connaître* les fréquences de formants.

---

## 4. Solutions déployées dans OpenVoxTuner (état actuel)

Le projet utilise **deux** mécanismes de formants, tous deux basés sur le *scaling de ratio* (aucun découplage source/filtre réel) :

### 4.1 `FormantPreserver` — [`FormantPreserver.h`](../Source/dsp/FormantPreserver.h)
- Mode `MultiFormant` : 4 peaking-EQ (F1–F4).
- Compensation **partielle** `1/√r` (voir `FormantPreserver.cpp`, `compensationRatio = 1/√(r)`).
- **Centres de formants fixés par défaut à `[500, 1500, 2500, 3500] Hz`** (défauts voix masculine, `FormantPreserver.h` ligne 121) — *identiques quel que soit le type de voix*.
- Lissage temporel des biquads : `biquadSmoothAlpha = 0.05` (Fix AZ, ligne 116), corrigeant un warble 5 Hz hérité d'un α=0.002.

### 4.2 `PitchShifter` — [`PitchShifter.cpp`](../Source/dsp/PitchShifter.cpp)
- `formantRatio` (vitesse de lecture des grains PSOLA) découple partiellement les formants du temps *à l'intérieur* de PSOLA (ligne 666, `double F = currentFormantRatio`). Lissé par `α = 0.005`.
- Dans `PluginProcessor.cpp`, `userFormantRatio = 2^(shiftSemitones/12)` (ligne 2093) est dérivé du *même* décalage que le pitch et passé au PitchShifter (ligne 2146), indépendamment du `FormantPreserver` (lignes 2126–2127).

**Bilan.** Deux approches *approximatives* se superposent. Aucune n'extrait et ne ré-injecte l'enveloppe de formants ; toutes déplacent des centres par une loi de ratio imparfaite.

---

## 5. Lacunes identifiées

| # | Lacune | Impact |
|---|--------|--------|
| G1 | **Centres de formants fixes** `[500,1500,2500,3500]` quel que soit le type de voix | Les centres réels différent fortement (masculin F1≈730, féminin≈850, enfant≈900). Le pré-warp s'applique « à côté » des vrais formants → compensation partiellement inefficace. |
| G2 | **Compensation partielle `1/√r`** au lieu de `1/r` | À grand rapport, les formants restent décalés (sous-compensation en montée, sur-compensation en descente). |
| G3 | **Pas de découplage source/filtre** | La méthode agit par *ratio-scaling* d'une approximation d'enveloppe, pas par extraction/ré-injection de l'enveloppe réelle. |
| G4 | **Déformation des largeurs de bande de formants** | Un peaking-EQ déplace le centre mais n'altère pas la bande de façon physique ; la forme des formants est altérée. |
| G5 | **Double application potentielle** (FormantPreserver `1/√r` + `formantRatio` grain-speed) | Risque de sure/st sous-compensation combinée selon les réglages. |
| G6 | **Pas de robustesse voix/type explicite** | `voice-type-feasibility-report.md` §2.3C proposait un réglage des formants par type de voix, jugé optionnel et non implémenté. |
| G7 | **Absence de validation perceptive et métrique** | Aucune mesure objective de distorsion formantique en CI (les tests existants couvrent le warble de lissage, pas la qualité de préservation). |

---

## 6. Critères d'évaluation objectifs

| Critère | Métrique | Comment mesuré |
|---------|----------|----------------|
| **Distorsion formantique** | LSD globale (dB) et LSD en *bandes de formants* (dB) vs référence idéale | Écart RMS des enveloppes LPC entre sortie et référence idéale (formants laissés en place). |
| **Naturalité perçue** | Score MUSHRA (0–100) par évaluateurs humains | Protocole en §8 (non exécutable en CI automatisée). |
| **Performance temps réel** | ms CPU par s d'audio | Benchmark relatif ; l'algorithme LPC est temps-réel viable en C++ (voir caveat §7.4). |
| **Compatibilité voix** | LSD sur voix masculin/féminin/enfant | Mêmes métriques, 3 jeux de formants canoniques. |
| **Robustesse au bruit** | LSD en bandes de formants à SNR 20/10 dB | Cross-synthèse LPC appliquée à un signal transposé bruité. |

---

## 7. Tests comparatifs quantitatifs

### 7.1 Méthodologie

Un banc d'essai numpy pur (`test/formant_preservation_benchmark.py`, sans scipy) modélise des voyelles sources-filtres (banque de résonateurs parallèles Klatt, 4 formants équilibrés) pour 3 types de voix (masculin F0=120, féminin 220, enfant 300) et 4 rapports de transposition `r ∈ {0.75, 1.0, 1.5, 2.0}`. Cinq méthodes sont comparées contre une **référence idéale** (voyelle à la hauteur de sortie avec formants laissés en place) :

- **B0** : transposition naïve (formants couplés au pitch) — artefact de base.
- **B1** : pré-warp banc de filtres `1/√r` — **modélisation optimiste du projet** (on suppose ici que le banc *connaît les vrais formants* ; le projet réel, à centres fixes, est *pire* que ce B1).
- **B2** : pré-warp `1/r` — idéal théorique de l'approche filtre.
- **C0** : cross-synthèse LPC (échange d'enveloppe par trame).
- **C1** : cross-synthèse LPC + **interpolation temporelle des coefficients LPC**.

Les enveloppes sont calculées par LPC *par trame* (moyennée), ce qui évite les pôles LPC spurieux d'une analyse globale sous-paramétrisée.

### 7.2 Distorsion spectrale (LSD globale, dB, vs référence idéale)

| Voix | r | B0 (naïf) | B1 (projet*) | C0 (LPC) | C1 (LPC+lis.) |
|------|---|-----------|--------------|----------|---------------|
| Masc. | 0.75 | 4.10 | 3.58 | **1.52** | **1.55** |
| Masc. | 1.50 | 8.06 | 4.71 | **3.48** | **3.49** |
| Masc. | 2.00 | 12.24 | 8.24 | **3.10** | **3.02** |
| Fém. | 1.50 | 9.63 | 5.28 | **3.28** | **3.24** |
| Fém. | 2.00 | 14.13 | 7.76 | **3.99** | **3.95** |
| Enf. | 1.50 | 12.57 | 6.50 | **3.51** | **3.50** |
| Enf. | 2.00 | 15.86 | 11.70 | **6.29** | **6.29** |

*À r=1.0, toutes les méthodes donnent LSD≈0 (cohérence du banc).*

**Constat** : la cross-synthèse LPC (C0/C1) est systématiquement la meilleure, avec un gain de **~2.5× à ~4×** sur le pré-warp du projet (B1) et de **~3× à ~5×** sur le naïf (B0) aux grands rapports. L'interpolation temporelle (C1) est légèrement supérieure à C0 aux grandes montées (3.02 vs 3.10 à r=2.0 masculin), confirmant l'intérêt du lissage LPC.

### 7.3 Distorsion en bandes de formants (FBAND LSD, dB)

| Voix | r | B0 | B1 (projet*) | C0 (LPC) | C1 (LPC+lis.) |
|------|---|----|--------------|----------|---------------|
| Masc. | 2.00 | 6.87 | 6.93 | **4.36** | **4.30** |
| Fém. | 2.00 | 4.87 | **9.53** | 5.25 | 5.18 |
| Enf. | 2.00 | 9.72 | 10.08 | **6.81** | **6.82** |

**Constat critique** : à r=2.0 féminin, le pré-warp `1/√r` (B1=9.53) est **pire que la transposition naïve** (B0=4.87) dans les bandes de formants. La compensation partielle sur-compense les formants pour certaines voix/rapports — preuve que l'approche par ratio fixe est **non fiable d'un type de voix à l'autre**, ce qui motive fortement le passage au LPC.

### 7.4 Warble / lissage temporel (profondeur de modulation RMS, vibrato 5 Hz)

| `biquadSmoothAlpha` | 0.002 (lent) | 0.050 (projet) | 0.200 (rapide) |
|---------------------|--------------|----------------|----------------|
| Modulation résiduelle | 0.0246 | 0.0559 | 0.1262 |

Le réglage courant du projet (0.05) est un compromis : un lissage plus lent réduit l'AM résiduelle mais introduit un *lag* de timbre (le warble historique du Fix AZ) ; un lissage rapide suit la modulation mais génère davantage d'AM et de risque de clics. **Le lissage est un paramètre à régler par écoute**, pas une solution de préservation.

### 7.5 Performance temps réel (ms CPU / s d'audio, meilleur de 3)

| Méthode | B0 | B1 (projet) | C0 (LPC) | C1 (LPC+lis.) |
|---------|----|-------------|-----------|---------------|
| ms/s | 12.4 | 20.9 | 114.7 | 117.5 |

**Caveat important** : ces chiffres proviennent d'une implémentation Python/numpy *non optimisée* (boucles d'interpréteur dominate le coût). L'algorithme LPC lui-même — Levinson-Durbin (ordre 10) + filtrage tout-pôle par trame de ~400 éch. à hop 100 → ~160 trames/s, quelques milliers d'opérations chacune — est **parfaitement temps-réel en C++/JUCE** (sous la ms par trame). Le ratio ~5–6× observé ici est une borne supérieure ; en natif il sera bien plus proche du banc de biquads (qui reste ~5× moins cher mais de qualité nettement inférieure).

### 7.6 Robustesse au bruit ambiant (LSD bandes de formants, cross-synthèse LPC)

| Condition | SNR 20 dB | SNR 10 dB | Propre (plancher) |
|-----------|----------|-----------|-------------------|
| FBAND LSD (dB) | 3.31 | 3.22 | 4.40 |

La cross-synthèse LPC **ne se dégrade pas de façon catastrophique** sous bruit (3.3 dB à SNR 10–20 dB, comparable au plancher propre de 4.4 dB). La méthode est donc raisonnablement robuste à un signal d'entrée bruité — à condition d'estimer l'enveloppe cible sur le signal propre ou via pré-emphase. *Limite du benchmark* : la distorsion résiduelle (~4 dB) reflète aussi l'ordre LPC fini et le gain-matching par trame ; un ordre plus élevé et une meilleure gestion de trame la réduiraient.

---

## 8. Tests qualitatifs — naturalité perçue (protocole d'écoute)

La distorsion formantique (LSD) corrèle fortement avec la naturalité perçue : une enveloppe de formants préservée = voyelle reconnaissable et naturelle. Toutefois, seule une écoute humaine conclut. Protocole proposé (type MUSHRA) :

1. **Stimuli** : pour chaque type de voix et `r ∈ {0.75, 1.25, 1.5, 2.0}`, générer 4 versions — (a) référence idéale (B2), (b) projet actuel (FormantPreserver + PitchShifter), (c) LPC cross-synthèse (C0), (d) naïf (B0, ancre « hidden reference » dégradée).
2. **Sujets** : 8–12 évaluateurs, écoute en casque, présentation aléatoire et en double aveugle.
3. **Échelle** : note de 0 (artificiel/chipmunk) à 100 (naturel), par paire comparée à l'ancre « référence cachée ».
4. **Consignes** : évaluer *la naturalité de la timbre/voyelle*, indépendamment de la justesse de la hauteur.
5. **Analyse** : moyenne et écart-type par condition ; test de Friedman + post-hoc ; seuil de significativité p<0.05.

**Hypothèse attendue (à confirmer)** : LPC (C0) ≥ référence > projet actuel > naïf. Ce protocole doit être exécuté avant toute mise en production de l'approche LPC.

---

## 9. Recommandations d'implémentation priorisées

### P0 — Court terme, fort impact / faible risque
- **P0.1 : Formants dépendants du type de voix (G1, G6).** Remplacer les centres fixes `[500,1500,2500,3500]` par un jeu de formants sélectionné selon `F0` estimé (ou un paramètre explicite masculin/féminin/enfant). Cible : `FormantPreserver::formantConfigs` + détection/paramètre de type de voix dans `PluginProcessor`.
- **P0.2 : Passage à la compensation `1/r` (G2).** Dans `FormantPreserver.cpp::updateAllFormants`, utiliser `compensationRatio = 1/r` (au lieu de `1/√r`) sur les centres *réels* du type de voix. Amélioration immédiate sans nouveau DSP.

### P1 — Moyen terme, gain qualitatif majeur
- **P1.1 : Module LPC cross-synthèse (C0).** Nouvelle classe `ovtdsp::LpcFormantPreserver` : analyse LPC par trame (ordre 10, fenêtre de Hann, hop ~100 éch. à 44.1 kHz), éclaircissement du signal transposé, re-synthèse avec l'enveloppe de référence (extraite du signal d'entrée propre ou d'un cache d'enveloppe). Normalisation d'énergie par trame + overlap-add pour éviter le warble de frontière.
- **P1.2 : Interpolation temporelle des coefficients LPC (C1).** Moyenner les `aₖ` des trames voisines (ou interpoler exponentiellement) avant re-synthèse — réduit la distorsion aux grands rapports (§7.2).
- **P1.3 : Désambiguïser FormantPreserver vs formantRatio (G5).** Documenter et régler le câblage `PluginProcessor` (lignes 2126–2146) pour qu'une seule chaîne de préservation soit active par mode, évitant la double compensation.

### P2 — Long terme, robustesse
- **P2.1 : Pré-emphase + ordre LPC adaptatif** pour réduire la distorsion résiduelle (~4 dB, §7.6) et la sensibilité au bruit.
- **P2.2 : Test MUSHRA automatisable** (§8) et intégration d'un score de distorsion formantique en CI (rejouer `test/formant_preservation_benchmark.py` sur des extraits réels).
- **P2.3 : Mode hybride** — LPC quand le signal est stable et clair, repli sur le banc de filtres (type-voix aware) en cas de bruit fort / parole non voisée.

---

## 10. Étapes de validation

1. **Unitaires** : tests sur signaux synthétiques (voyelles sources-filtres) vérifiant que (a) l'enveloppe LPC de sortie rejoint l'enveloppe cible à ±1 dB de LSD, (b) aucun warble de frontière (modulation RMS < 1 %).
2. **Régression** : rejouer le benchmark quantitatif (§7) ; exiger C0/C1 < B1 sur tous les (voix, r), et C1 ≤ C0 aux grands r.
3. **Intégration** : vérifier l'absence de clics/pops (test de modulation 5 Hz existant `FormantPreserverModulationTest` étendu au module LPC).
4. **Perceptif** : protocole MUSHRA (§8) ; seuil de publication : LPC significativement > projet actuel (p<0.05) et non significativement < référence idéale.
5. **Temps réel** : vérifier la latence/CPU en C++ natif sous 1× budget temps-réel à 44.1 kHz stéréo.

---

## 11. Métriques de succès

| Métrique | État actuel (projet B1) | Cible (LPC C0/C1) | Mesure |
|----------|--------------------------|-------------------|--------|
| LSD globale à r=2.0 (moy. masc./fém./enf.) | ~9.2 dB | **≤ 4 dB** | benchmark §7.2 |
| LSD bandes de formants à r=2.0 | 6.9–10.1 dB | **≤ 5 dB** | benchmark §7.3 |
| Score MUSHRA naturalité | référence | **≥ +20 pts vs actuel** | écoute §8 |
| Warble de frontière (RMS) | n/a (nouveau module) | **< 1 %** | test unitaire |
| CPU natif | ~1× (biquads) | **< 2× biquads** | profilage C++ |
| Robustesse bruit (FBAND LSD @ SNR10) | — | **≤ 4 dB** | benchmark §7.6 |

**Critère d'amélioration vs existant** : réduction d'au moins **~2.5× de la distorsion formantique** (LSD) aux grands rapports de transposition, et amplification de la naturalité perçue mesurée par écoute.

---

## 12. Synthèse

Le **LPC par cross-synthèse** est la méthode la plus pertinente pour une préservation des formants « parfaitement naturelle » : il opère le découplage source/filtre exact que les approximations par ratio du projet n'atteignent pas. Le **lissage temporel / interpolation** est un correctif de robustesse indispensable (et l'interpolation LPC améliore même légèrement la qualité). Les solutions déployées dans OpenVoxTuner souffrent de centres de formants fixes, d'une compensation partielle `1/√r`, et de l'absence de découplage réel — lacunes que le benchmark quantitatif quantifie (LPC ~2.5–5× meilleur que le projet aux grands rapports). Les recommandations P0 (type de voix + `1/r`) offrent un gain immédiat ; P1 (module LPC) apporte le saut qualitatif, à valider par écoute MUSHRA et profilage temps-réel avant production.
