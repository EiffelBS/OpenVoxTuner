// FactoryPresets.cpp
// Built-in (factory) pitch-curve preset registry. See FactoryPresets.h.

#include "FactoryPresets.h"

namespace ovtdsp
{
    const std::vector<FactoryPresetInfo>& getFactoryPresets()
    {
        static const std::vector<FactoryPresetInfo> list =
        {
            { "default",        "Default",         "Basic",           "Flat A3 reference tone." },
            { "robot_c3",      "Robot (C3)",      "Robotic",         "Monotone robotic voice at C3." },
            { "robot_c4",      "Robot (C4)",      "Robotic",         "Monotone robotic voice at C4." },
            { "spoken_male",   "Spoken (Male)",   "Vocal Character", "Natural spoken-male contour." },
            { "spoken_female", "Spoken (Female)", "Vocal Character", "Natural spoken-female contour." },
            { "bass",          "Bass",           "Voice Type",      "Low male bass melody." },
            { "baritone",      "Baritone",       "Voice Type",      "Baritone melody." },
            { "tenor",         "Tenor",          "Voice Type",      "Tenor melody." },
            { "alto",          "Alto",           "Voice Type",      "Alto melody." },
            { "mezzo",         "Mezzo",          "Voice Type",      "Mezzo-soprano melody." },
            { "soprano",       "Soprano",        "Voice Type",      "Soprano melody." },
        };
        return list;
    }
}
