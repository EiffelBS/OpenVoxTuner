// PitchCurve.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ScaleQuantizer.h" // for the Scale enum and scale-related types

// We use "ovtdsp" (autotune dsp) rather than "dsp" to avoid any ambiguity
// with the "juce::dsp" namespace brought in by JuceHeader.h.
namespace ovtdsp
{
    /**
     * One point of the pitch curve.
     * time in seconds (relative to the playhead), pitch in Hz.
     * The == operator is required by juce::Array::indexOf() (JUCE 8 uses
     * juce::exactlyEqual which requires the operator).
     */
    struct PitchPoint
    {
        double time = 0.0;   // seconds
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
     * Pitch curve = list of PitchPoints sorted by time.
     * Enables interactive editing and serialization.
     *
     * Convention:
     *   - At least 2 points (otherwise we are in "auto" mode)
     *   - Points are stored sorted by ascending 'time'
     *   - Interpolation between 2 points is linear
     *   - Before the 1st point: extrapolate by holding the 1st point's value
     *   - After the last point: same (constant)
     */
    class PitchCurve
    {
    public:
        PitchCurve();

        // === Point editing ===

        /// Adds or replaces the point closest to the given 'time'.
        /// @return index of the added/modified point.
        int addOrUpdatePoint (double time, float pitch);

        /// Removes the point closest to 'time' (within tolerance).
        /// @return true if a point was removed.
        bool removePointNear (double time, double toleranceSec = 0.05);

        /// Removes all points.
        void clear() { points.clear(); }

        /// Number of points.
        int getNumPoints() const { return points.size(); }

        /// Direct access.
        const PitchPoint& getPoint (int index) const { return points.getReference(index); }
        PitchPoint&       getPoint (int index)       { return points.getReference(index); }

        /// Changes the pitch of the point at the given index.
        /// Equivalent to points.getReference (index).pitch = pitch, but
        /// guarantees a direct write into the Array (avoids any ambiguity
        /// from an accidental copy by value).
        void setPointPitch (int index, float pitch) { points.getReference (index).pitch = pitch; }

        /// Changes a point's time and pitch, keeps the sort by time, and
        /// updates the provided index so it always refers to the same point.
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

        // === Copying ===
        // The PitchCurve is copied regularly (UI -> processor), so default
        // copy and assignment are allowed.
        PitchCurve (const PitchCurve&) = default;
        PitchCurve& operator= (const PitchCurve&) = default;

        // === Evaluation ===

        /// Returns the curve's pitch at the given time.
        /// If the curve is empty, returns defaultValue.
        /// Otherwise, linearly interpolates between the 2 adjacent points.
        float getPitchAt (double time, float defaultValue = 0.0f) const;

        // === Scale / snapping ===

        /// Snap a frequency to the nearest note of an explicit interval set.
        /// "intervals" holds absolute semitone offsets within [0, 11] (one octave),
        /// already shifted by the musical key. This is the single source of truth
        /// shared with the on-screen scale display, so the interactive snap always
        /// matches the visible scale.
        static float snapToIntervals (float hz, const juce::Array<int>& intervals);

        // === Serialization ===

        /// Serializes to XML for saving in the plugin state.
        std::unique_ptr<juce::XmlElement> toXml() const;

        /// Reloads from XML.
        void fromXml (const juce::XmlElement& xml);

        // === Presets factory ===

        /// Resets the curve with a preset suited to a use case.
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



