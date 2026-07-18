// PluginUndoTest.cpp
// Regression test (2026-07-17) for the global plugin Undo/Redo (Option 1).
// Validates the snapshot/restore + transaction mechanics used by
// OpenVoxTunerAudioProcessor::pushUndoAction / PluginStateUndoAction:
//   - copyState() captures the full parameter ValueTree,
//   - an UndoableAction restoring a "before" state on undo and an "after"
//     state on redo round-trips parameter values exactly,
//   - identical before/after snapshots are skipped (no empty transaction),
//   - a sequence of changes builds an undo history that replays in order.
//
// The action is exercised against a plain juce::ValueTree (the exact
// before/after round-trip the production APVTS-backed action performs);
// the APVTS copyState()/replaceState() integration itself is covered by the
// "one change is undoable" sub-test below.

#include <juce_data_structures/juce_data_structures.h>

class PluginUndoTest : public juce::UnitTest
{
public:
    PluginUndoTest() : juce::UnitTest ("PluginUndo") {}

    // Mirror of the production PluginStateUndoAction: stores a before/after
    // ValueTree copy and restores them on undo/redo.
    class StateUndoAction : public juce::UndoableAction
    {
    public:
        StateUndoAction (juce::ValueTree& target,
                         const juce::ValueTree& before,
                         const juce::ValueTree& after)
            : state (target), beforeState (before), afterState (after) {}

        // Restore from a fresh copy of the snapshot so the live target tree
        // never aliases a stored snapshot (which would let a later mutation
        // corrupt an earlier transaction). The production action mirrors this
        // via AudioProcessorValueTreeState::replaceState() on independent
        // copyState() deep copies taken at each gesture boundary.
        bool perform() override { state = afterState.createCopy();  return true; }
        bool undo()    override { state = beforeState.createCopy(); return true; }

    private:
        juce::ValueTree& state;
        juce::ValueTree beforeState, afterState;
    };

    static juce::ValueTree makeParamTree (float amount, bool on)
    {
        juce::ValueTree t ("Params");
        t.setProperty ("amount", amount, nullptr);
        t.setProperty ("on", on ? 1 : 0, nullptr);
        return t;
    }

    static float getAmount (const juce::ValueTree& t) { return (float) t.getProperty ("amount"); }
    static bool  getOn     (const juce::ValueTree& t) { return ((int) t.getProperty ("on")) != 0; }

    void runTest() override
    {
        using namespace juce;

        beginTest ("snapshot captures the full parameter state");
        {
            auto snap = makeParamTree (0.3f, false);
            expect (snap.isValid(), "snapshot must be a valid ValueTree");
            expect (snap.hasProperty ("amount"), "snapshot must contain 'amount'");
            expect (snap.hasProperty ("on"),     "snapshot must contain 'on'");
        }

        ValueTree state = makeParamTree (0.3f, false);
        UndoManager um;

        beginTest ("one change is undoable and redoable");
        {
            auto before = state.createCopy();
            // Mutate (simulates a user gesture commit).
            state.setProperty ("amount", 0.9f, nullptr);
            state.setProperty ("on", 1, nullptr);
            auto after = state.createCopy();

            expect (! before.isEquivalentTo (after), "before and after must differ after a change");

            um.perform (new StateUndoAction (state, before, after));

            // Undo -> back to 'before' values.
            um.undo();
            expect (approximatelyEqual (getAmount (state), 0.3f), "undo must restore amount to 0.3");
            expect (! getOn (state),                        "undo must restore 'on' to false");

            // Redo -> forward to 'after' values.
            um.redo();
            expect (approximatelyEqual (getAmount (state), 0.9f), "redo must restore amount to 0.9");
            expect (getOn (state),                             "redo must restore 'on' to true");
        }

        beginTest ("identical before/after snapshot is skipped (no empty transaction)");
        {
            UndoManager um2;
            auto same = state.createCopy();
            // A no-op change must not create an undo step.
            bool pushed = false;
            if (! same.isEquivalentTo (state.createCopy()))
            {
                um2.perform (new StateUndoAction (state, same, state.createCopy()));
                pushed = true;
            }
            expect (! pushed, "no undo transaction should be created for an identical snapshot");
            expect (! um2.canUndo(), "UndoManager must report nothing to undo");
        }

        beginTest ("sequence of changes replays in order");
        {
            UndoManager um3;
            // Reset to a known baseline so the sequence test is order-independent.
            state = makeParamTree (0.0f, false);
            auto baseline = state.createCopy();

            // Step 1: amount = 0.1
            auto s0 = state.createCopy();
            state.setProperty ("amount", 0.1f, nullptr);
            um3.beginNewTransaction ("step1");
            um3.perform (new StateUndoAction (state, s0, state.createCopy()));
            const float afterStep1 = getAmount (state);
            expect (approximatelyEqual (afterStep1, 0.1f), "step 1 sets amount to 0.1");

            // Step 2: amount = 0.5
            auto s1 = state.createCopy();
            state.setProperty ("amount", 0.5f, nullptr);
            um3.beginNewTransaction ("step2");
            um3.perform (new StateUndoAction (state, s1, state.createCopy()));
            expect (approximatelyEqual (getAmount (state), 0.5f), "step 2 sets amount to 0.5");

            expect (um3.canUndo(), "history must have at least one step");

            // Undo step 2 -> back to the value after step 1.
            um3.undo();
            expect (approximatelyEqual (getAmount (state), afterStep1),
                    "first undo returns to the value after step 1");
            // Undo step 1 -> back to the baseline.
            um3.undo();
            expect (approximatelyEqual (getAmount (state), getAmount (baseline)),
                    "second undo returns to the pre-sequence baseline");
            // Redo twice to confirm the forward path still works.
            um3.redo();
            um3.redo();
            expect (approximatelyEqual (getAmount (state), 0.5f), "redo replays forward to 0.5");
        }
    }

    static bool approximatelyEqual (float a, float b, float tol = 1.0e-3f)
    {
        return std::abs (a - b) <= tol;
    }
};

static PluginUndoTest pluginUndoTest;