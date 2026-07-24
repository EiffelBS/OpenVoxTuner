// SidechainBusLayoutTest.cpp
// Unit tests for ovtdsp::isSidechainLayoutValid() used to validate the optional
// Sidechain input bus layout in OpenVoxTunerAudioProcessor::isBusesLayoutSupported.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/SidechainBusLayout.h"

class SidechainBusLayoutTest : public juce::UnitTest
{
public:
    SidechainBusLayoutTest() : juce::UnitTest ("SidechainBusLayout") {}

    void runTest() override
    {
        beginTest ("Mono sidechain is accepted alongside a stereo main bus");
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo()); // main
            layout.inputBuses.add (juce::AudioChannelSet::mono());   // sidechain
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (ovtdsp::isSidechainLayoutValid (layout),
                    "stereo main + mono sidechain must be valid");
        }

        beginTest ("Disabled (empty) sidechain is accepted");
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());  // main
            layout.inputBuses.add (juce::AudioChannelSet());          // sidechain disabled
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (ovtdsp::isSidechainLayoutValid (layout),
                    "disabled sidechain must be valid");
        }

        beginTest ("Stereo sidechain is accepted for Logic AU compatibility");
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());
            layout.inputBuses.add (juce::AudioChannelSet::stereo()); // sidechain stereo
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (ovtdsp::isSidechainLayoutValid (layout),
                     "stereo sidechain must be accepted (Logic AU compatibility)");
        }

        beginTest ("Layouts without a sidechain bus are always valid");
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());  // main only
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (ovtdsp::isSidechainLayoutValid (layout),
                    "a layout with no sidechain bus must be valid");
        }
    }
};

static SidechainBusLayoutTest sidechainBusLayoutTest;
