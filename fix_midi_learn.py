import re

with open('Source/PluginEditor.cpp', 'r') as f:
    c = f.read()

pattern = r'menu\.addSeparator\(\);\s*\n\s*// 7b\. MIDI Learn submenu.*?menu\.addSeparator\(\);'
replacement = '''menu.addSeparator();

        // MIDI Learn submenu — useful in standalone where host MIDI mapping isn't available.
        // In plugin/ARA the host provides its own MIDI learn, so we hide it to avoid confusion.
        const bool isStandalone = processorRef.isStandalone();
        if (isStandalone)
        {
            // 7b. MIDI Learn submenu
            {
                juce::PopupMenu midiLearnMenu;
                struct ParamEntry { const char* id; std::string name; };
                const ParamEntry params[] = {
                    {"speed", ovt::tr(ovt::Keys::kMidiLearnSpeed).toStdString()},
                    {"amount", ovt::tr(ovt::Keys::kMidiLearnAmount).toStdString()},
                    {"formant", ovt::tr(ovt::Keys::kMidiLearnFormant).toStdString()},
                    {"reverb_mix", ovt::tr(ovt::Keys::kMidiLearnReverbMix).toStdString()},
                    {"flex_tune", ovt::tr(ovt::Keys::kMidiLearnFlexTune).toStdString()},
                    {"humanize", ovt::tr(ovt::Keys::kMidiLearnHumanize).toStdString()},
                    {"vibrato_preserve", ovt::tr(ovt::Keys::kMidiLearnVibrato).toStdString()},
                    {"attack_aware", ovt::tr(ovt::Keys::kMidiLearnAttack).toStdString()},
                    {"attack_release", ovt::tr(ovt::Keys::kMidiLearnAttackRelease).toStdString()},
                    {"harmony_gain", ovt::tr(ovt::Keys::kMidiLearnHarmonyGain).toStdString()},
                    {"harmony_blend", ovt::tr(ovt::Keys::kMidiLearnHarmonyBlend).toStdString()},
                    {"harmony_tone_color", ovt::tr(ovt::Keys::kMidiLearnHarmonyTone).toStdString()}
                };
                for (const auto& p : params)
                {
                    midiLearnMenu.addItem (juce::String (p.name), [this, id = juce::String (p.id)] {
                        startMidiLearn (id);
                    });
                }
                menu.addSubMenu (ovt::tr(ovt::Keys::kMenuMidiLearn), midiLearnMenu);
            }
        }

        menu.addSeparator();'''

c = re.sub(pattern, replacement, c, flags=re.DOTALL)
with open('Source/PluginEditor.cpp', 'w') as f:
    f.write(c)
print('Done')