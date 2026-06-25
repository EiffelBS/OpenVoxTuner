#include <JuceHeader.h>
#include <ARA_Library/PlugIn/ARAPlug.h>

void testARA(juce::ARAMusicalContext* musicalContext) {
    if (musicalContext == nullptr) return;
    
    ARA::PlugIn::HostContentReader<ARA::kARAContentTypeKeySignatures> reader (musicalContext);
    auto count = reader.getEventCount();
    if (count > 0) {
        auto* keySig = reader.getEvent(0);
        if (keySig != nullptr) {
            int root = keySig->root;
        }
    }
}
