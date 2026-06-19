# Changelog - 2026-06-14

## Modifications
- **DSP / Formant Shift** :
  - Séparation complète du Formant Shift de la chaîne d'Autotune : le Formant Shift est maintenant traité par une instance dédiée (`formantShifter`) qui intervient en post-traitement après l'Autotune. Cela résout le bug où le Formant Shift annulait la correction de justesse.
  - Isolement du Formant Shift par rapport au mécanisme de préservation des formants de l'autotune (`formantPreserver`).
  - Réduction de la plage du paramètre Formant de `[-12, +12]` à `[-5, +5]` pour éviter les dépassements de buffer (buffer overruns) et les clics audio.
  - Réécriture complète de la gestion des formants dans `PitchShifter::process` : utilisation de la technique WSOLA avec manipulation de la vitesse de lecture intra-grain (`currentFormantRatio`) et saut de phase (`virtualInputTime`) pour obtenir un véritable effet "ogre" ou "chipmunk" sans annuler la correction de justesse.

- **DSP / Graphic Mode** :
  - Résolution des artefacts audio (clics, scratchs) en mode Graphic : correction d'un bug majeur où la fenêtre de recherche d'alignement de phase (WSOLA) était restreinte à `0.5` période, l'empêchant de trouver la continuité de phase lors des changements de ratio brutaux dictés par la courbe. La fenêtre a été étendue à `1.2` période minimum.
  - Rétablissement de la vitesse de lecture intra-grain à `1.0f` pour l'Autotune afin de garantir un lissage parfait des formants naturels et de prévenir les sauts temporels hors limites.

- **DSP / Formant Shift & Autotune** :
  - Refonte majeure de l'algorithme `PitchShifter` pour résoudre la perte d'autotune et les "echos/scratchs" en fin de note.
  - Remplacement du pointeur d'écriture tournant (`writePos`) par un pointeur d'écriture absolu (`absoluteWritePos`) et passage de `virtualInputTime` en `double` au lieu de `float`. Cela élimine définitivement les bugs de dépassement de tampon (`wraparound`) qui causaient la répétition aléatoire d'anciens fragments audio (les "échos" entendus après l'arrêt du chant).
  - Suppression de l'égaliseur `FormantPreserver` et de la seconde instance `FormantShifter`. Le moteur granulaire effectue désormais le Formant Shifting et le Pitch Shifting en un seul et unique passage, de manière parfaitement mathématique et avec une qualité supérieure, ce qui réduit drastiquement la consommation CPU.

- **UI / Graphic Mode & Ergonomie** :
  - **Réorganisation spatiale inspirée de Pro-Q3** : Les outils spécifiques au mode d'édition graphique (`Snap to scale`, `Clear Curve`, `Reset Playhead`) ont été complètement retirés du bloc inférieur de l'interface. Ils sont désormais intégrés de manière horizontale dans la bannière supérieure (Top Bar), alignés sur la droite à côté du bouton Bypass.
  - Cette réorganisation a permis de supprimer un bloc d'interface entier en bas, offrant beaucoup plus d'espace de respiration (padding) aux boutons rotatifs de traitement (`Speed`, `Amount`, `Formant`) et au clavier de sélection de gamme (`Scale`).
  - **Affichage conditionnel renforcé** : Ces trois outils graphiques placés dans la barre supérieure apparaissent *exclusivement* lorsque l'onglet "Graphic (Advanced)" est actif. En mode "Auto (Live)", la barre supérieure redevient parfaitement épurée pour ne pas polluer l'attention de l'utilisateur.
  - **Correction d'affichage au redimensionnement** : Ajout d'une limite de taille minimale absolue (800x550) pour le plugin. Cela empêche les boutons (knobs) et la vue graphique de disparaître ou d'être écrasés lorsqu'on rétrécit trop la fenêtre depuis le DAW.
  - **Refonte graphique (Thème Studio One)** : L'interface utilisateur a été complètement modernisée pour correspondre à vos références (Fat Channel, Compressor, Vocal Tune). Les fonds sont désormais d'un gris/noir mat très professionnel (`#1A1A1A`), les éléments actifs (barres de progression, combobox) s'illuminent en bleu cyan (`#1A9AF0`), et les boutons inactifs sont grisés. Les potentiomètres (knobs) ont été redessinés dans un style "plat" (flat design) avec une pointe lumineuse.
  - **Bouton Formant Power** : Le bouton de Formant a été transformé en une véritable icône "Power" (cercle ouvert avec ligne verticale) qui s'illumine avec un halo jaune/doré (comme sur le Fat Channel) lorsqu'il est activé, et devient sombre lorsqu'il est coupé.
  - Le slider de Formant se grise automatiquement lorsque l'effet est désactivé, offrant un retour visuel intuitif.
  - Correction définitive de la visibilité et fonctionnalité du bouton `Reset Playhead` : il est désormais grisé (désactivé) lorsque le plugin est chargé en mode ARA (où la timeline est gérée exclusivement par le séquenceur). En mode plugin VST3 classique, il permet de réinitialiser la lecture locale en appliquant un offset temporel interne sans perdre la synchronisation de base avec l'hôte.

- **DSP / Détection de Pitch (Spikes)** :
  - **Amélioration majeure de la latence** : La latence interne du moteur a été ramenée de 60ms à **20ms**. Le plugin signale désormais dynamiquement cette valeur (`setLatencySamples`) au DAW pour que la compensation (PDC) soit parfaite. C'est le délai optimal pour conserver une excellente qualité d'Autotune et de Formant tout en permettant le jeu en direct ("Live").
  - Ajout d'un filtre médian (Median Filter de taille 5) directement dans le cœur du `PitchDetector` (algorithme YIN). Cela élimine complètement les "spikes" (sauts d'octave aléatoires d'une ou deux frames) qui causaient des erreurs de tracking et des artefacts métalliques dans le Pitch Shifter. La courbe verte dans l'éditeur sera désormais parfaitement lisse et stable, même sur les voix difficiles.
  - Ajustement de la visibilité des boutons : `Clear Curve` et `Reset Playhead` sont désormais exclusivement visibles dans l'onglet **Graphic**.

- **Presets de l'éditeur graphique (Tessitures vocales)** :
  - **Analyse des anciens presets** : Les anciens presets (`default`, `robot`, `spoken`, `lyric`) étaient calés sur des fréquences génériques (440 Hz / A4 ou 200 Hz). Ces valeurs étaient souvent trop aiguës de 1 à 2 octaves pour des voix parlées ou chantées masculines (Baryton/Basse), forçant le moteur DSP à "tirer" excessivement sur le signal et créant une perception artificielle.
  - **Refonte et ajout de presets réalistes** : Création d'un menu de presets complet et catégorisé par tessitures vocales humaines (Soprano, Mezzo, Alto, Ténor, Baryton, Basse).
  - Nouveaux presets plats (Robot) : ciblés sur **C3 (130 Hz)** pour voix grave, et **C4 (261 Hz)** pour voix aiguë.
  - Nouveaux presets parlés (Spoken) : oscillation naturelle pour **Homme (~120 Hz)** et **Femme (~220 Hz)**.
  - Nouveaux presets de mélodies : courbes expressives générées pour chaque tessiture vocale avec leurs limites de fréquences respectives (ex: Basse de E2 à C3, Soprano de C4 à G4).
  - Validation complète par tests unitaires : toutes les courbes chargent correctement et n'écrasent pas le comportement de l'éditeur (Snap-to-scale fonctionnel sur les nouvelles courbes).

- **Documentation** :
  - Mise à jour du `roadmap.md` pour refléter l'ajout de `Reset Playhead` et l'ajustement de `Clear Curve`.