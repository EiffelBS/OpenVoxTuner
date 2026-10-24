// PluginEditorInternal.h
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Helpers shared by the editor TUs that PluginEditor.cpp was split into.
// They are internal to the editor, but internal LINKAGE is per translation
// unit: a static copy in each TU would compile and then diverge silently.
// An inline definition in this header is what keeps exactly one copy.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginEditor.h"

// Helper: ensure a PopupMenu uses our custom LookAndFeel for correct background colours
inline void applyMenuLookAndFeel (juce::PopupMenu& m, OvtLookAndFeel& lf)
{
    m.setLookAndFeel (&lf);
}
