# Architecture Multi-Moteurs de Pitch Shifting

Cette implémentation permet de basculer dynamiquement entre plusieurs moteurs de pitch-shifting à la volée.

## 1. Interface Commune : `IPitchShifter`
Tous les moteurs implémentent l'interface `IPitchShifter` (`Source/dsp/IPitchShifter.h`) qui garantit l'interchangeabilité.

```cpp
class IPitchShifter
{
public:
    virtual ~IPitchShifter() = default;
    virtual void prepare (double sampleRate, int maximumBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process (juce::AudioBuffer<float>& buffer, float ratio, float f0) = 0;
    virtual int getLatencySamples() const = 0;
};
```

## 2. Moteurs implémentés

### 2.1. RubberBand (`RubberBandPitchShifter`)
- **Qualité** : Très élevée.
- **Latence** : Modérée.
- **Licence** : GPL v2+.
- **Caractéristiques** : Moteur par défaut, très fluide, aucune désynchronisation à grand buffer. Adapté avec un système de FIFO car il requiert des blocs fixes de 512 échantillons.

### 2.2. SoundTouch (`SoundTouchPitchShifter`)
- **Qualité** : Élevée (bien pour la voix et la musique en général).
- **Latence** : Plus faible / Variable.
- **Licence** : LGPL.
- **Caractéristiques** : Compilation statique des fichiers sources inclus dans `external/soundtouch/source/SoundTouch`. Nécessite un tampon entrelacé pour le traitement.

### 2.3. PSOLA Legacy / Delay-Line Crossfade (`PitchShifter`)
- **Qualité** : Correcte (effet "robotique" / Chorus plus marqué, style delay-line).
- **Latence** : Faible (environ 50ms fixée).
- **Licence** : Custom (Maison).
- **Caractéristiques** : Historiquement basée sur PSOLA, l'implémentation a été entièrement réécrite pour utiliser un algorithme robuste de type "Delay-Line Crossfade" (proche de WSOLA). Elle utilise un buffer circulaire glissant avec deux têtes de lecture et un fenêtrage (Hann) synchrone sur la phase. Cela supprime totalement les coupures et les artefacts de pitch-tracking défaillants de l'ancien algorithme.

## 3. Comment ajouter un nouveau moteur
Pour ajouter un moteur "X" :
1. Créez les fichiers `Source/dsp/XPitchShifter.h` et `.cpp`.
2. Faites hériter `XPitchShifter` de `IPitchShifter`.
3. Implémentez les méthodes virtuelles.
4. Dans `Source/PluginProcessor.cpp`, ajoutez l'option à la création de `AudioParameterChoice` :
   ```cpp
   std::make_unique<juce::AudioParameterChoice> (
       "engine", "Engine", juce::StringArray { "RubberBand", "SoundTouch", "PSOLA (Legacy)", "MoteurX" }, 0)
   ```
5. Dans `PluginProcessor::processBlock`, ajoutez un `case 3:` au switch de sélection dynamique.
6. Dans `Source/PluginEditor.cpp`, ajoutez la ligne correspondante dans la ComboBox :
   ```cpp
   engineBox.addItem ("MoteurX", 4);
   ```

## 4. Sélection utilisateur
Le changement s'effectue via le paramètre "Engine" de l'UI (qui est un `juce::AudioParameterChoice`). Lorsqu'il change, le `PluginProcessor` libère l'ancien moteur, alloue le nouveau et appelle immédiatement `prepare()` avant de continuer le traitement du bloc.
