// OpenVoxKeyProcessor.cpp
// See OpenVoxKeyProcessor.h for the design overview.

#include "OpenVoxKeyProcessor.h"
#include "OpenVoxKeyEditor.h"

namespace
{
    // Maps a detected detector key (A-relative) + minor flag to the main
    // OpenVoxTuner Scale convention: Major -> 1 (Scale::Major),
    // Natural Minor -> 4 (Scale::NaturalMinor). This is exactly what the main
    // plug-in's applyDetectedKey() expects, so the published value can be
    // read and applied verbatim by the main instance.
    int scaleIndexForMode (bool minor) { return minor ? static_cast<int> (ovtdsp::Scale::NaturalMinor)
                                                      : static_cast<int> (ovtdsp::Scale::Major); }
}

//==============================================================================
OpenVoxKeyProcessor::OpenVoxKeyProcessor()
    : AudioProcessor (juce::AudioProcessor::BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       #if ! JucePlugin_IsMidiEffect
                          .withInput  ("Input",  juce::AudioChannelSet::mono(), false)
                          .withOutput ("Output", juce::AudioChannelSet::mono(), false)
                       #endif
                          )
{
    // Group letter the detection is published under (must match the main
    // OpenVoxTuner "Companion Group" selector: 0=A, 1=B, 2=C, 3=D).
    groupParam = new juce::AudioParameterChoice (
        "group", "Group",
        juce::StringArray { "A", "B", "C", "D" }, 0);
    addParameter (groupParam);

    // KeyDetector sliding window: ~3 s of history for a stable estimate.
    keyDetector.setWindowSeconds (3.0f);

    pitchDetector = std::make_unique<ovtdsp::YinPitchDetector>();
}

OpenVoxKeyProcessor::~OpenVoxKeyProcessor() = default;

//==============================================================================
void OpenVoxKeyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    if (pitchDetector != nullptr)
        pitchDetector->prepare (sampleRate / 4.0, samplesPerBlock);

    analysisFifo.setSize (1, analysisWindow, false, true, false);
    analysisFifo.clear();
    fifoWriteIndex = 0;
    fifoFillCount = 0;
    samplesSinceLastAnalysis = 0;
    analysisLinearBuffer.allocate (analysisWindow, true);
}

void OpenVoxKeyProcessor::releaseResources()
{
    if (pitchDetector != nullptr)
        pitchDetector->reset();
    keyDetector.reset();
}

void OpenVoxKeyProcessor::forcePublish() noexcept
{
    // Nothing meaningful to publish until a key has actually been detected.
    const int key = lastKey.load();
    if (key < 0)
        return;

    const int scaleIdx = scaleIndexForMode (lastMinor.load());
    const int grpIdx = getGroup();
    const juce::String grp = juce::StringArray { "A", "B", "C", "D" }[
        juce::jlimit (0, 3, grpIdx)];
    ovtdsp::KeyBridge::getInstance().publish (grp, key, scaleIdx);
}

//==============================================================================
void OpenVoxKeyProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    // Transparent effect: the audio is passed through unchanged (the host reads
    // the same channels we received). No processing touches the buffer.

    const float blockDur = static_cast<float> (numSamples) / static_cast<float> (currentSampleRate);

    // Track audio presence (for the editor's "searching" indicator): stamp the
    // timestamp whenever the input bus carries signal above a tiny noise floor.
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* d = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::abs (d[i]);
            if (a > peak) peak = a;
        }
    }
    if (peak > 1.0e-4f)
        lastAudioTime.store (juce::Time::getCurrentTime().toMilliseconds() / 1000.0);

    // Detect the pitch of the block, then feed it to the key detector and
    // publish the result to the shared bridge.
    const float f0 = computePitch (buffer);
    if (f0 > 0.0f)
    {
        keyDetector.addDetection (f0, 1.0f, blockDur);

        int detKey = -1; bool detMinor = false; float detConf = 0.0f;
        if (keyDetector.getEstimate (detKey, detMinor, detConf))
        {
            const int musicalKey = ovtdsp::KeyDetector::detectorKeyToMusical (detKey);
            const int scaleIdx   = scaleIndexForMode (detMinor);

            if (musicalKey != lastKey.load() || detMinor != lastMinor.load())
            {
                lastKey.store (musicalKey);
                lastMinor.store (detMinor);

                const int grpIdx = getGroup();
                const juce::String grp = juce::StringArray { "A", "B", "C", "D" }[
                    juce::jlimit (0, 3, grpIdx)];
                ovtdsp::KeyBridge::getInstance().publish (grp, musicalKey, scaleIdx);
            }
        }
    }
}

//==============================================================================
float OpenVoxKeyProcessor::computePitch (const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0 || pitchDetector == nullptr || currentSampleRate <= 0.0)
        return 0.0f;

    const int numSamples = buffer.getNumSamples();
    float* fifo = analysisFifo.getWritePointer (0);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : nullptr;

    // Fill the FIFO (mono downmix).
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = (inR != nullptr) ? 0.5f * (inL[i] + inR[i]) : inL[i];
        fifo[fifoWriteIndex] = s;
        fifoWriteIndex = (fifoWriteIndex + 1) % analysisWindow;
        if (fifoFillCount < analysisWindow)
            ++fifoFillCount;
    }

    samplesSinceLastAnalysis += numSamples;
    if (fifoFillCount < analysisWindow)
        return 0.0f; // not enough history yet
    if (samplesSinceLastAnalysis < analysisHopSize)
        return 0.0f; // throttle the (expensive) YIN analysis
    samplesSinceLastAnalysis = 0;

    if (analysisLinearBuffer.getData() == nullptr)
        return 0.0f;

    float* linear = analysisLinearBuffer.getData();

    constexpr int decimation = 4;
    const int decimatedWindow = analysisWindow / decimation;

    int idx = fifoWriteIndex;
    for (int i = 0; i < decimatedWindow; ++i)
    {
        float sum = 0.0f;
        for (int j = 0; j < decimation; ++j)
        {
            sum += fifo[idx];
            idx = (idx + 1) % analysisWindow;
        }
        linear[i] = sum / static_cast<float> (decimation);
    }

    const float newPitch = pitchDetector->detectPitch (linear, decimatedWindow);
    return (newPitch > 0.0f) ? newPitch : 0.0f;
}

//==============================================================================
const juce::String OpenVoxKeyProcessor::getName() const { return "OpenVoxKey"; }

int OpenVoxKeyProcessor::getNumPrograms()        { return 1; }
int OpenVoxKeyProcessor::getCurrentProgram()     { return 0; }
void OpenVoxKeyProcessor::setCurrentProgram (int) {}
const juce::String OpenVoxKeyProcessor::getProgramName (int) { return {}; }
void OpenVoxKeyProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
juce::AudioProcessorEditor* OpenVoxKeyProcessor::createEditor()
{
    return new OpenVoxKeyEditor (*this);
}

void OpenVoxKeyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("OpenVoxKey");
    if (groupParam != nullptr)
        state.setProperty ("group", groupParam->getIndex(), nullptr);
    juce::XmlElement::TextFormat format;
    if (auto xml = state.createXml())
    {
        juce::MemoryOutputStream os (destData, false);
        xml->writeTo (os, format);
    }
}

void OpenVoxKeyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream (data, static_cast<size_t> (sizeInBytes), false);
    auto xml = juce::XmlDocument::parse (stream.readString());
    if (xml == nullptr || ! xml->hasTagName ("OpenVoxKey"))
        return;

    if (groupParam != nullptr)
    {
        const int g = juce::jlimit (0, 3, xml->getIntAttribute ("group", 0));
        // AudioParameterChoice stores a normalised 0..1 value, so map the 0..3
        // index to g/3 (the same convention used by the editor's combo handler).
        groupParam->setValueNotifyingHost (static_cast<float> (g) / 3.0f);
    }
}

//=== Creation du plugin (point d'entree JUCE) ==============================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenVoxKeyProcessor();
}
