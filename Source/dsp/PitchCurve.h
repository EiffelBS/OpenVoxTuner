// PitchCurve.h
// Pitch curve editable : liste de points (time, pitch en Hz) avec interpolation.
// Mode "graphic" du plugin : l'utilisateur definit la hauteur desiree au fil du temps.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ScaleQuantizer.h" // pour l'enum Scale et les types lies a la gamme

// On utilise "ovtdsp" (autotune dsp) plutot que "dsp" pour eviter toute
// ambiguite avec le namespace "juce::dsp" apporte par JuceHeader.h.
namespace ovtdsp
{
    /**
     * Un point de la pitch curve.
     * time en secondes (relatif au playhead), pitch en Hz.
     * Operateur == requis par juce::Array::indexOf() (JUCE 8 utilise
     * juce::exactlyEqual qui necessite l'operateur).
     */
    struct PitchPoint
    {
        double time = 0.0;   // secondes
        float  pitch = 440.0f; // Hz

        bool operator== (const PitchPoint& other) const noexcept
        {
            return time == other.time && pitch == other.pitch;
        }
        bool operator!= (const PitchPoint& other) const noexcept
        {
            return ! (*this == other);
        }
    };

    /**
     * Pitch curve = liste triee par temps de PitchPoint.
     * Permet l'edition interactive et la serialisation.
     *
     * Convention :
     *   - Au moins 2 points (sinon on est en mode "auto")
     *   - Les points sont stockes tries par 'time' croissant
     *   - L'interpolation entre 2 points est lineaire
     *   - Avant le 1er point : on extrapole en tenant la valeur du 1er point
     *   - Apres le dernier point : idem (constante)
     */
    class PitchCurve
    {
    public:
        PitchCurve();

        // === Edition des points ===

        /// Ajoute ou remplace le point le plus proche du 'time' donne.
        /// @return index du point ajoute/modifie.
        int addOrUpdatePoint (double time, float pitch);

        /// Supprime le point le plus proche du 'time' (dans la tolerance).
        /// @return true si un point a ete supprime.
        bool removePointNear (double time, double toleranceSec = 0.05);

        /// Supprime tous les points.
        void clear() { points.clear(); }

        /// Nombre de points.
        int getNumPoints() const { return points.size(); }

        /// Acces direct.
        const PitchPoint& getPoint (int index) const { return points.getReference(index); }
        PitchPoint&       getPoint (int index)       { return points.getReference(index); }

        /// Modifie le pitch du point a l'index donne.
        /// Equivaut a points.getReference (index).pitch = pitch, mais garantit
        /// une ecriture directe dans le Array (evite toute ambiguite liee a
        /// une copie par valeur accidentelle).
        void setPointPitch (int index, float pitch) { points.getReference (index).pitch = pitch; }

        /// Modifie le temps et le pitch d'un point, maintient le tri par temps
        /// et met a jour l'index fourni pour qu'il pointe toujours sur le meme point.
        void setPointTimeAndPitch (int& index, double newTime, float newPitch)
        {
            if (index < 0 || index >= points.size()) return;
            auto& pt = points.getReference(index);
            pt.time = newTime;
            pt.pitch = newPitch;
            PitchPoint copy = pt;
            sortPoints();
            index = points.indexOf(copy);
        }

        void setMultiplePointsTimeAndPitch (juce::Array<int>& indices,
                                            const juce::Array<double>& newTimes,
                                            const juce::Array<float>& newPitches);

        // === Copie ===
        // La PitchCurve est copiee regulierement (UI -> processor), donc on
        // autorise la copie et l'affectation par defaut.
        PitchCurve (const PitchCurve&) = default;
        PitchCurve& operator= (const PitchCurve&) = default;

        // === Evaluation ===

        /// Donne le pitch de la courbe au temps donne.
        /// Si la courbe est vide, retourne defaultValue.
        /// Sinon, interpole lineairement entre les 2 points adjacents.
        float getPitchAt (double time, float defaultValue = 0.0f) const;

        // === Gamme / snapping ===

        /// Snap a frequency to the nearest note of an explicit interval set.
        /// "intervals" holds absolute semitone offsets within [0, 11] (one octave),
        /// already shifted by the musical key. This is the single source of truth
        /// shared with the on-screen scale display, so the interactive snap always
        /// matches the visible scale.
        static float snapToIntervals (float hz, const juce::Array<int>& intervals);

        // === Serialisation ===

        /// Serialise en XML pour sauvegarde dans l'etat du plugin.
        std::unique_ptr<juce::XmlElement> toXml() const;

        /// Recharge depuis XML.
        void fromXml (const juce::XmlElement& xml);

        // === Presets factory ===

        /// Reinitialise la courbe avec un preset adapte a un cas d'usage.
        /// @param presetName  "default", "spoken", "lyric", "rap", "robot"
        void loadPreset (const juce::String& presetName);

        // === Editing state (persisted alongside curve) ===
        void setStepMode (bool step) { stepMode = step; }
        bool isStepMode() const { return stepMode; }
        void setSnapEnabled (bool b) { snapEnabled = b; }
        bool isSnapEnabled() const { return snapEnabled; }
        void setSnapToGridEnabled (bool b) { snapToGridEnabled = b; }
        bool isSnapToGridEnabled() const { return snapToGridEnabled; }

    private:
        juce::Array<PitchPoint> points;
        bool stepMode = false;
        bool snapEnabled = true;
        bool snapToGridEnabled = true;

        void sortPoints();
    };
}
