// PluginEditor.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/OVTFonts.h"
#include "ui/OVTTheme.h"
#include "ui/OVTLanguages.h"
#include "ui/PresetGallery.h"
#include "ovt_assets_binary_data.h"
#include <map>

// Cached vector logo (assets/icon.svg embedded as a binary resource). Parsed
// once and reused so we don't re-parse the SVG on every repaint.
static juce::Drawable* getBrandLogo()
{
    static std::unique_ptr<juce::Drawable> logo;
    if (logo == nullptr && ovtAssets::icon_svgSize > 0)
    {
        auto svgXml = juce::XmlDocument::parse (ovtAssets::icon_svg);
        if (svgXml != nullptr)
            logo = juce::Drawable::createFromSVG (*svgXml);
    }
    return logo.get();
}

// === Theme colors ("autotune" style: dark + pink/purple accent) ===

// "Advanced" banner look-and-feel: a plain rectangular handle (square corners),
// subtle fill (the block background, slightly darker), no border. It is aligned
// to the block's right edge but inset from the rounded corners (see resized()),
// so the reactive zone is a single clean rectangle with no overflow / point.
void VerticalTextButtonLF::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool /*shouldDrawButtonAsDown*/)
{
    auto b = button.getLocalBounds().toFloat();
    juce::Colour fill = backgroundColour;
    if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.18f);
    g.setColour (fill);
    g.fillRoundedRectangle (b, 4.0f);
}

// Draw the "Advanced" handle for the Correction block: a single centred chevron
// that points right when collapsed (click to expand) and left when expanded
// (click to collapse). Accent colour so the handle is clearly visible. No extra
// glyphs: the chevron alone communicates expand/collapse.
void VerticalTextButtonLF::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                           bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/)
{
    const bool expanded = button.getToggleState();
    const int w = button.getWidth();
    const int h = button.getHeight();

    // Chevron (centred): points right when collapsed (click to expand), left when
    // expanded (click to collapse).
    g.setColour (shouldDrawButtonAsHighlighted ? ebs::accent().brighter (0.2f) : ebs::accent());
    const float cx = static_cast<float> (w) * 0.5f;
    const float cy = static_cast<float> (h) * 0.5f;
    const float s  = 5.0f;
    juce::Path chevron;
    if (expanded)
        chevron.addTriangle (cx + s, cy - s, cx + s, cy + s, cx - s, cy);  // points left
    else
        chevron.addTriangle (cx - s, cy - s, cx - s, cy + s, cx + s, cy);  // points right
    g.fillPath (chevron);
}

// Helper: ensure a PopupMenu uses our custom LookAndFeel for correct background colours
static void applyMenuLookAndFeel (juce::PopupMenu& m, OvtLookAndFeel& lf)
{
    m.setLookAndFeel (&lf);
}

// === Modern / Transparent graphic switch (studio-console style) ===
// Two-position horizontal switch placed below the Speed/Amount knobs. It is
// backed by the existing (otherwise invisible) correctionModeButton so the
// parameter value, undo gesture and external sync (preset load, etc.) are all
// handled by the existing wiring. A light Timer keeps the visual in sync when
// the parameter changes from elsewhere.
class CorrectionModeSwitch : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    explicit CorrectionModeSwitch (juce::Button& backingButton)
        : button (backingButton)
        , pillPos (backingButton.getToggleState() ? 1.0f : 0.0f)
        , targetPillPos (pillPos)
    {
        setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
        setWantsKeyboardFocus (false);
        startTimerHz (60); // 60 Hz for smooth animation
    }

    ~CorrectionModeSwitch() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = area.getHeight() * 0.5f;

        // Track background (dark, subtle border).
        g.setColour (ebs::bgPanel());
        g.fillRoundedRectangle (area, radius);
        g.setColour (ebs::text().withAlpha (0.18f));
        g.drawRoundedRectangle (area, radius, 1.0f);

        const float half = area.getWidth() * 0.5f;
        auto leftRect  = area.withWidth (half);
        auto rightRect = area.withTrimmedLeft (half);

        // Sliding accent pill - animated position.
        const float pillW = half - 4.0f;
        const float pillX = leftRect.getX() + 2.0f + pillPos * half;
        auto pillRect = juce::Rectangle<float> (pillX, area.getY() + 2.0f, pillW, area.getHeight() - 4.0f);
        g.setColour (ebs::accent());
        g.fillRoundedRectangle (pillRect, pillRect.getHeight() * 0.5f);

        // Labels: active = white, inactive = dimmed.
        const float fontH = juce::jmax (9.0f, area.getHeight() * 0.40f);
        g.setFont (juce::Font (fontH, juce::Font::bold));
        const bool isTrans = targetPillPos > 0.5f;
        g.setColour (isTrans ? ebs::text().withAlpha (0.55f) : juce::Colours::white);
        g.drawText (ovt::tr (ovt::Keys::kLabelModernBtn), leftRect,
                    juce::Justification::centred);
        g.setColour (isTrans ? juce::Colours::white : ebs::text().withAlpha (0.55f));
        g.drawText (ovt::tr (ovt::Keys::kLabelTransparentBtn), rightRect,
                    juce::Justification::centred);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        // Toggle the backing button (fires its onClick + ButtonAttachment, which
        // writes the parameter and triggers the editor's undo gesture).
        const bool newState = ! button.getToggleState();
        button.setToggleState (newState, juce::sendNotification);
        targetPillPos = newState ? 1.0f : 0.0f;
    }

    bool hitTest (int, int) override { return true; }

private:
    void timerCallback() override
    {
        // Sync with external parameter changes.
        const bool t = button.getToggleState();
        const float target = t ? 1.0f : 0.0f;
        if (std::abs (target - targetPillPos) > 0.001f)
            targetPillPos = target;

        // Animate pillPos toward targetPillPos.
        constexpr float kSmoothing = 0.18f; // ~8-10 frames at 60Hz = smooth but snappy
        if (std::abs (pillPos - targetPillPos) > 0.001f)
        {
            pillPos += (targetPillPos - pillPos) * kSmoothing;
            repaint();
        }
    }

    juce::Button& button;
    float pillPos = 0.0f;      // current animated position [0..1]
    float targetPillPos = 0.0f; // target position [0..1]
};

// === Live / Curve Editor tab switch (iPhone-style) ===
// Replaces the standard JUCE TabbedButtonBar with an animated pill switch.
// Two-position horizontal switch controlling the tabbed component's current
// index (0 = Live, 1 = Curve Editor). A light Timer keeps the visual in sync
// when the tab changes from elsewhere.
class TabSwitch : public juce::Component,
                  private juce::Timer
{
public:
    explicit TabSwitch (juce::TabbedComponent& tabs)
        : tabbed (tabs)
        , pillPos (tabs.getCurrentTabIndex() == 1 ? 1.0f : 0.0f)
        , targetPillPos (pillPos)
    {
        setWantsKeyboardFocus (false);
        startTimerHz (60);
    }

    ~TabSwitch() override { stopTimer(); }

    /// Minimum width required to render the two labels (Live + translated
    /// "Curve Editor") at the standard font size (40% of @p height) without
    /// truncating any translation, including the long ones (FR: "Editeur de
    /// courbe", DE: "Kurveneditor", ES: "Editor de curva", JP/CN full-width).
    /// The layout must call this and allocate at least that many pixels
    /// horizontally - the Live/Curve tabs are critical navigation, so
    /// widening the pill is always preferable to shrinking the label font.
    static int getPreferredWidth (float height)
    {
        const float fontH = juce::jmax (9.0f, height * 0.50f);
        // Bypass the platform font scale (createFontRaw) so the pill
        // label visually matches the Modern/Transparent pill below the
        // Speed/Amount knobs (which paints at the raw 40%-of-height
        // point size).  The multiplier is 0.50 (vs the Modern pill's
        // 0.40) so the "Live" / "Curve Editor" navigation labels are
        // noticeably larger, as befits a primary navigation element.
        juce::Font f = ovt::createFontRaw (fontH, true);
        const float liveW  = juce::GlyphArrangement::getStringWidth (f, ovt::tr (ovt::Keys::kTabLive));
        const float curveW = juce::GlyphArrangement::getStringWidth (f, ovt::tr (ovt::Keys::kTabCurveEditor));
        // Two labels side by side, each with horizontal padding (6px each side)
        // plus a small outer breathing room.
        const float labelW = juce::jmax (liveW, curveW) + 12.0f;
        return juce::roundToInt (labelW * 2.0f + 8.0f);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = area.getHeight() * 0.5f;

        g.setColour (ebs::bgPanel());
        g.fillRoundedRectangle (area, radius);
        g.setColour (ebs::text().withAlpha (0.18f));
        g.drawRoundedRectangle (area, radius, 1.0f);

        const float half = area.getWidth() * 0.5f;
        auto leftRect  = area.withWidth (half);
        auto rightRect = area.withTrimmedLeft (half);

        // Sliding accent pill.
        const float pillW = half - 4.0f;
        const float pillX = leftRect.getX() + 2.0f + pillPos * half;
        auto pillRect = juce::Rectangle<float> (pillX, area.getY() + 2.0f, pillW, area.getHeight() - 4.0f);
        g.setColour (ebs::accent());
        g.fillRoundedRectangle (pillRect, pillRect.getHeight() * 0.5f);

        // Use ~50%-of-height font size, in the plugin typeface family.
        // Bypass the platform font scale (createFontRaw) so the label
        // visually matches the Modern/Transparent pill below the
        // Speed/Amount knobs.  The multiplier is 0.50 (vs the Modern
        // pill's 0.40) so the "Live" / "Curve Editor" navigation labels
        // are noticeably larger, as befits a primary navigation element.
        // The pill is allocated enough width (see getPreferredWidth() and
        // the layout at the call site) so that even the longest translation
        // of "Curve Editor" fits without truncation - Live/Curve Editor
        // are critical navigation, so widening the pill beats shrinking
        // the label.
        const float fontH = juce::jmax (9.0f, area.getHeight() * 0.50f);
        g.setFont (ovt::createFontRaw (fontH, true));

        const juce::String liveText  = ovt::tr (ovt::Keys::kTabLive);
        const juce::String curveText = ovt::tr (ovt::Keys::kTabCurveEditor);
        const bool isRight = targetPillPos > 0.5f;
        g.setColour (isRight ? ebs::text().withAlpha (0.55f) : juce::Colours::white);
        g.drawText (liveText, leftRect, juce::Justification::centred);
        g.setColour (isRight ? juce::Colours::white : ebs::text().withAlpha (0.55f));
        g.drawText (curveText, rightRect, juce::Justification::centred);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        const int newIdx = (tabbed.getCurrentTabIndex() == 0) ? 1 : 0;
        tabbed.setCurrentTabIndex (newIdx);
        targetPillPos = (newIdx == 1) ? 1.0f : 0.0f;
    }

    bool hitTest (int, int) override { return true; }

private:
    void timerCallback() override
    {
        const float target = (tabbed.getCurrentTabIndex() == 1) ? 1.0f : 0.0f;
        if (std::abs (target - targetPillPos) > 0.001f)
            targetPillPos = target;

        constexpr float kSmoothing = 0.18f;
        if (std::abs (pillPos - targetPillPos) > 0.001f)
        {
            pillPos += (targetPillPos - pillPos) * kSmoothing;
            repaint();
        }
    }

    juce::TabbedComponent& tabbed;
    float pillPos = 0.0f;
    float targetPillPos = 0.0f;
};

const juce::Colour OpenVoxTunerAudioProcessorEditor::kBgDark     = juce::Colour::fromString("#FF121318"); // Deep background
const juce::Colour OpenVoxTunerAudioProcessorEditor::kBgPanel    = juce::Colour::fromString("#FF14151C"); // Dark panels
const juce::Colour OpenVoxTunerAudioProcessorEditor::kAccent     = juce::Colour::fromString("#FF1A9AF0"); // Light blue (Vocal Tune)
const juce::Colour OpenVoxTunerAudioProcessorEditor::kAccentSoft = juce::Colour::fromString("#401A9AF0"); // Soft blue
const juce::Colour OpenVoxTunerAudioProcessorEditor::kText       = juce::Colour::fromString("#FFE1E1E6"); // Off-white text

// === Scale modes names for displaying slider values ===
static const juce::StringArray kScaleNames = {
    "Chromatic", "Major", "Melodic Minor", "Harmonic Minor", "Natural Minor", 
    "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Phrygian", 
    "Lydian", "Mixolydian", "Locrian", "Custom"
};
// === Note names for the Key slider ===
static const juce::StringArray kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

namespace
{
    juce::File getUserPresetsDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("OpenVoxTuner")
                       .getChildFile ("Presets");
        dir.createDirectory();
        return dir;
    }

    // Plugin Presets live in a separate "Presets/Plugin" folder so their files
    // never collide with Curve Preset files (which live directly under
    // "Presets"). The two preset kinds are fully orthogonal.
    juce::File getUserPluginPresetsDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("OpenVoxTuner")
                       .getChildFile ("Presets")
                       .getChildFile ("Plugin");
        dir.createDirectory();
        return dir;
    }

    juce::String sanitizePresetFileStem (juce::String name)
    {
        name = name.trim();
        if (name.isEmpty())
            return "Preset";

        juce::String out;
        out.preallocateBytes (name.getNumBytesAsUTF8());

        for (auto c : name)
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                out << juce::String::charToString (c);
            else if (c == ' ' || c == '-' || c == '_')
                out << "_";
        }

        out = out.trimCharactersAtEnd ("_").trimCharactersAtStart ("_");
        if (out.isEmpty())
            out = "Preset";

        return out;
    }

    juce::String normaliseVersionString (juce::String v)
    {
        v = v.trim();
        if (v.startsWithChar ('v') || v.startsWithChar ('V'))
            v = v.substring (1).trim();
        return v;
    }

    int compareVersionStrings (juce::String latest, juce::String current)
    {
        latest = normaliseVersionString (latest);
        current = normaliseVersionString (current);

        juce::StringArray a, b;
        a.addTokens (latest, ".", "");
        b.addTokens (current, ".", "");
        const int count = juce::jmax (a.size(), b.size());

        for (int i = 0; i < count; ++i)
        {
            const int av = (i < a.size()) ? a[i].getIntValue() : 0;
            const int bv = (i < b.size()) ? b[i].getIntValue() : 0;
            if (av > bv) return 1;
            if (av < bv) return -1;
        }

        return 0;
    }
}

struct OpenVoxTunerUpdateCheckState
{
    std::atomic<bool> finished { false };
    std::atomic<bool> cancelled { false };
    std::atomic<bool> updateAvailable { false };
    juce::String latestVersion;
    juce::String releaseUrl;
    juce::String statusText;
};

bool OpenVoxTunerAudioProcessorEditor::isVersionNewer (const juce::String& latest, const juce::String& current)
{
    return compareVersionStrings (latest, current) > 0;
}

void OpenVoxTunerAudioProcessorEditor::startUpdateCheck()
{
    updateCheckState = std::make_shared<OpenVoxTunerUpdateCheckState>();

    juce::String currentVersion;
   #if defined (JucePlugin_VersionString)
    currentVersion = JucePlugin_VersionString;
   #elif defined (JucePlugin_Version)
    currentVersion = juce::String (JucePlugin_Version);
   #else
    currentVersion = "0.0.0";
   #endif

    const juce::String updateInfoUrl = OVT_UPDATE_INFO_URL;
    auto state = updateCheckState;

    std::thread ([state, currentVersion, updateInfoUrl]
    {
        if (state == nullptr || state->cancelled.load())
            return;

        state->statusText = "Checking...";
        state->releaseUrl = "https://github.com/EiffelBS/OpenVoxTuner/releases/latest";

        juce::String response;
        try
        {
            response = juce::URL (updateInfoUrl).readEntireTextStream (false);
        }
        catch (...)
        {
            response.clear();
        }

        if (! state->cancelled.load() && response.isNotEmpty())
        {
            auto parsed = juce::JSON::parse (response);
            juce::String latestVersion;
            juce::String releaseUrl;

            if (auto* obj = parsed.getDynamicObject())
            {
                latestVersion = obj->getProperty ("version").toString();
                releaseUrl = obj->getProperty ("url").toString();
            }

            if (releaseUrl.isEmpty())
                releaseUrl = "https://github.com/EiffelBS/OpenVoxTuner/releases/latest";

            if (latestVersion.isNotEmpty())
            {
                state->latestVersion = latestVersion;
                state->releaseUrl = releaseUrl;
                state->updateAvailable.store (OpenVoxTunerAudioProcessorEditor::isVersionNewer (latestVersion, currentVersion));
                state->statusText = state->updateAvailable.load() ? "Update available" : "Up to date";
            }
            else
            {
                state->statusText = "Update check failed";
            }
        }
        else if (! state->cancelled.load())
        {
            state->statusText = "Update check failed";
        }

        state->finished.store (true);
    }).detach();
}

// === HelpOverlayComponent ===
OpenVoxTunerAudioProcessorEditor::HelpOverlayComponent::HelpOverlayComponent (
    OpenVoxTunerAudioProcessorEditor& o)
    : owner (o)
{
    setInterceptsMouseClicks (true, false);
    setVisible (false);
}

void OpenVoxTunerAudioProcessorEditor::HelpOverlayComponent::mouseDown (const juce::MouseEvent&)
{
    owner.helpOverlayVisible = false;
    setVisible (false);
}

void OpenVoxTunerAudioProcessorEditor::HelpOverlayComponent::paint (juce::Graphics& g)
{
    // Semi-transparent dark backdrop
    g.setColour (juce::Colour (0xcc000000));
    g.fillRect (getLocalBounds());

    const int centerX = getWidth() / 2;
    const int centerY = getHeight() / 2;

    // Shortcuts list
    struct Shortcut { const char* key; const char* descKey; };
    const Shortcut shortcuts[] = {
        { "Mouse Wheel",            ovt::Keys::kHelpMouseWheel },
        { "Ctrl / Cmd + Wheel",     ovt::Keys::kHelpCtrlWheel },
        { "Click + Drag",           ovt::Keys::kHelpClickDrag },
        { "Double-click",           ovt::Keys::kHelpDoubleClick },
        { "Right-click / Alt+Click",ovt::Keys::kHelpRightClick },
        { "Ctrl / Cmd + C",         ovt::Keys::kHelpCopy },
        { "Ctrl / Cmd + V",         ovt::Keys::kHelpPaste },
        { "Delete / Backspace",     ovt::Keys::kHelpDelete },
        { "Ctrl / Cmd + Z",         ovt::Keys::kHelpUndo },
        { "Ctrl / Cmd + Y",         ovt::Keys::kHelpRedo },
        { "Ctrl / Cmd + Shift + Z", ovt::Keys::kHelpRedo },
        { "?",                      ovt::Keys::kHelpToggleHelp },
    };
    const int numShortcuts = 12;
    const int rowsPerCol = 6;
    const int numCols = (numShortcuts + rowsPerCol - 1) / rowsPerCol; // 2 columns

    // Measure the widest key / description so the box is sized to fit the
    // current language and nothing is ever truncated (long keys like
    // "Right-click / Alt+Click" previously overflowed the fixed 94px column).
    const juce::Font legendFont = ovt::fontLegend();
    const juce::Font titleFont  = ovt::fontLabel();
    const int padX = 24;           // inner horizontal padding
    const int colGap = 24;         // gap between the key and its description
    const int colGapBetween = 32;  // gap between the two columns
    int maxKeyW = 0, maxDescW = 0;
    for (int i = 0; i < numShortcuts; ++i)
    {
        maxKeyW  = juce::jmax (maxKeyW,  static_cast<int> (juce::GlyphArrangement::getStringWidth (legendFont, shortcuts[i].key)));
        maxDescW = juce::jmax (maxDescW, static_cast<int> (juce::GlyphArrangement::getStringWidth (legendFont, ovt::tr (shortcuts[i].descKey))));
    }
    const int colW = maxKeyW + colGap + maxDescW;
    const int contentW = numCols * colW + (numCols - 1) * colGapBetween;

    // Also account for the title / close-hint width so long translations do
    // not get clipped either.
    const int titleW = static_cast<int> (juce::GlyphArrangement::getStringWidth (titleFont, ovt::tr (ovt::Keys::kHelpTitle)));
    const int closeW = static_cast<int> (juce::GlyphArrangement::getStringWidth (ovt::fontLegendHint(), ovt::tr (ovt::Keys::kHelpCloseHint)));

    // Final panel dimensions: leave room for inner padding, the header band,
    // the divider and the close hint.
    const int boxW = juce::jmax (contentW + padX * 2,
                                 juce::jmax (titleW + 64, juce::jmax (closeW + 48, 360)));
    const int boxH = 380;
    const int boxX = centerX - boxW / 2;
    const int boxY = centerY - boxH / 2;

    // Panel background
    g.setColour (ebs::bgPanel());
    g.fillRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 8.0f);
    g.setColour (ebs::accentSoft());
    g.drawRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 8.0f, 1.5f);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (titleFont);
    g.drawText (ovt::tr(ovt::Keys::kHelpTitle), boxX + 16, boxY + 12, boxW - 32, 24,
                juce::Justification::centred);

    // Scope subtitle: the listed shortcuts are Curve-Editor scoped (they only
    // fire when the Curve Editor has keyboard focus), so state that explicitly
    // instead of implying they are global.
    g.setColour (ebs::textDim());
    g.setFont (ovt::fontLegendHint());
    g.drawText (ovt::tr(ovt::Keys::kHelpScope), boxX + 16, boxY + 38, boxW - 32, 16,
                juce::Justification::centred);

    // Divider
    g.setColour (ebs::accentSoft());
    g.fillRect ((float) (boxX + 16), (float) (boxY + 58), (float) (boxW - 32), 1.0f);

    g.setFont (legendFont);
    int sy = boxY + 70;
    for (int i = 0; i < numShortcuts; ++i)
    {
        const int col = i / rowsPerCol;
        const int row = i % rowsPerCol;
        const int x = boxX + padX + col * (colW + colGapBetween);
        const int y = sy + row * 22;

        g.setColour (ebs::accent());
        g.drawText (shortcuts[i].key, x, y, maxKeyW, 18, juce::Justification::centredLeft);
        g.setColour (ebs::text());
        g.drawText (ovt::tr(shortcuts[i].descKey), x + maxKeyW + colGap, y, maxDescW, 18,
                    juce::Justification::centredLeft);
    }

    // Close hint
    g.setColour (ebs::textDim());
    g.setFont (ovt::fontLegendHint());
    g.drawText (ovt::tr(ovt::Keys::kHelpCloseHint), boxX, boxY + boxH - 24, boxW, 20,
                juce::Justification::centred);
}

OpenVoxTunerAudioProcessorEditor::OpenVoxTunerAudioProcessorEditor (OpenVoxTunerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Reset the editor-shutdown flag set by the previous editor instance's
    // destructor.  The processor outlives the editor, so without this
    // reset the flag would stay stuck at `true` after every close/reopen
    // cycle, short-circuiting updateHostTransport() and starving the
    // visualizer of data.
    processorRef.notifyEditorResumed();

    setLookAndFeel (&customLookAndFeel);

    // Restore theme preference from plugin state
    auto* themeParam = processorRef.getParameters().getParameter ("ui_theme");
    if (themeParam != nullptr)
        ebs::currentTheme() = (themeParam->getValue() > 0.5f) ? ebs::Theme::Light : ebs::Theme::Dark;

    applyThemeToAllComponents();

    // Restore language preference from plugin state
    auto* langParam = processorRef.getParameters().getParameter ("ui_language");
    if (langParam != nullptr)
    {
        const int langIdx = juce::roundToInt (langParam->getValue() * 5.0f);
        switch (langIdx)
        {
            case 0: ovt::currentLanguage() = ovt::Language::English;  break;
            case 1: ovt::currentLanguage() = ovt::Language::French;   break;
            case 2: ovt::currentLanguage() = ovt::Language::German;   break;
            case 3: ovt::currentLanguage() = ovt::Language::Spanish;  break;
            case 4: ovt::currentLanguage() = ovt::Language::Japanese; break;
            case 5: ovt::currentLanguage() = ovt::Language::Chinese;  break;
            default: ovt::currentLanguage() = ovt::Language::English; break;
        }
    }

    // Restore waveform overlay preference from APVTS.
    if (auto* wfParam = processorRef.getParameters().getParameter ("ui_show_waveform"))
    {
        showWaveform = wfParam->getValue() > 0.5f;
        processorRef.setWaveformCaptureEnabled (showWaveform);
    }

    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 700);
    tooltipWindow->setLookAndFeel (&customLookAndFeel);
    // Sync the TooltipWindow with the user-configured delay (Normal=700ms
    // by default; the user can pick Slow=1500ms or Fast=200ms via the
    // wrench / Interface / Tooltip Delay submenu).
    setTooltipDelayChoice (tooltipDelayChoice);

    updateButton.setButtonText (ovt::tr(ovt::Keys::kLabelUpdates));
    updateButton.setTooltip (ovt::tr(ovt::Keys::kTooltipCheckUpdates));
    updateButton.onClick = [this]
    {
        if (updateCheckState != nullptr && updateCheckState->updateAvailable.load())
        {
            if (updateCheckState->releaseUrl.isNotEmpty())
                juce::URL (updateCheckState->releaseUrl).launchInDefaultBrowser();
        }
        else
        {
            juce::URL ("https://github.com/EiffelBS/OpenVoxTuner/releases").launchInDefaultBrowser();
        }
    };
    updateButton.setColour (juce::TextButton::buttonColourId, ebs::bgPanel());
    updateButton.setColour (juce::TextButton::textColourOffId, ebs::text());
    updateButton.setColour (juce::TextButton::textColourOnId, ebs::accent());
    addAndMakeVisible (updateButton);

    startUpdateCheck();

    // === Configuration of the 4 sliders (rotary knobs) ===
    auto setupKnob = [this] (juce::Slider& s, juce::Label* l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        s.setColour (juce::Slider::rotarySliderFillColourId,    ebs::accent());
        s.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
        s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
        s.setColour (juce::Slider::textBoxTextColourId,         ebs::text());
        s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        if (l != nullptr)
        {
            l->setText (name, juce::dontSendNotification);
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::textColourId, ebs::text());
            l->setFont (ovt::fontLabel());
            addAndMakeVisible (*l);
        }
    };

    setupKnob (speedSlider,  &speedLabel,  "Speed (ms)");
    translatableLabels.push_back ({ &speedLabel, ovt::Keys::kLabelSpeed    });
    setupKnob (amountSlider, &amountLabel, "Amount");
    translatableLabels.push_back ({ &amountLabel, ovt::Keys::kLabelAmount    });
    speedSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSpeed));
    amountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipAmount));

    // "Advanced" expand/collapse banner for the Correction block: reveals the
    // Flex / Humanize / Vibrato / Attack-Aware correction knobs to the right of
    // the Speed / Amount knobs when expanded.
    advancedButton.setLookAndFeel (&advancedButtonLF);
    // Subtle, borderless banner: same family as the block background but a little
    // darker; the expanded/collapsed state is shown only by the direction chevron,
    // not by a strong colour change.
    advancedButton.setColour (juce::TextButton::buttonColourId,   ebs::bgPanel().darker (0.5f));
    advancedButton.setColour (juce::TextButton::buttonOnColourId, ebs::bgPanel().darker (0.5f));
    advancedButton.setColour (juce::TextButton::textColourOffId,  ebs::text());
    advancedButton.setColour (juce::TextButton::textColourOnId,   ebs::text());
    advancedButton.setTooltip (ovt::tr (ovt::Keys::kTooltipAdvanced));
    advancedButton.setClickingTogglesState (true);
    // Restore the persisted expand/collapse state from the processor (saved
    // across sessions in the plugin state XML).
    advancedExpanded = processorRef.getAdvancedExpanded();
    advancedButton.setToggleState (advancedExpanded, juce::dontSendNotification);
    advancedButton.onClick = [this]
    {
        advancedExpanded = advancedButton.getToggleState();
        processorRef.setAdvancedExpanded (advancedExpanded);
        resized();
        repaint();  // the bottom-block frames are painted in paint(), so redraw them
    };
    addAndMakeVisible (advancedButton);
    setupKnob (formantSlider, nullptr, "");
    // Pivot knob: fill grows from the central 0 value, not from the minimum.
    formantSlider.getProperties().set ("centred", true);
    // Formant knob: no value textbox; the live value is shown in JUCE's popup
    // display while dragging. The TooltipWindow can't be used for this: it only
    // appears after the mouse is stationary, which never happens during a
    // rotate-drag, so the value tooltip would never show.
    formantSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    formantSlider.setPopupDisplayEnabled (true, false, this);
    formantSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " st"; };

    // === Harmony UI ===
    // Harmony enable toggle (use same visual style as Formant)
    harmonyEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyBtn));
    harmonyEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    harmonyEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    harmonyEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyEn));
    addAndMakeVisible (harmonyEnableButton);

    // Harmony "Follow Lead" toggle (power-style): when on, the harmony voices move with
    // the lead correction character (vibrato preservation, humanize, flex, attack-aware)
    // instead of staying locked to the scale grid. Default on.
    harmonyFollowLeadButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyFollow));
    harmonyFollowLeadButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    harmonyFollowLeadButton.setColour (juce::ToggleButton::tickColourId,  ebs::accent());
    harmonyFollowLeadButton.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyFollow));
    addAndMakeVisible (harmonyFollowLeadButton);

    // Harmony "Gain Match" toggle (power-style): when on, the harmony mix is scaled
    // by 1/sqrt(1+N) where N = number of active harmony voices, so the total output
    // RMS is roughly equal to the dry input RMS. Compensates for the additive
    // volume boost that is most noticeable on Unison and Unison+Octaves. Default on.
    // The dry signal is untouched; the user-facing harmony volume knob still
    // controls the overall harmony level (post-compensation).
    harmonyGainMatchButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyGainMatch));
    harmonyGainMatchButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    harmonyGainMatchButton.setColour (juce::ToggleButton::tickColourId,  ebs::accent());
    harmonyGainMatchButton.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyGainMatch));
    addAndMakeVisible (harmonyGainMatchButton);

    // Harmony type combo - index 3 = "3rd Below + Above" (default).
    // IMPORTANT: this list MUST contain ALL 22 choices, matching the
    // "harmony_type" AudioParameterChoice (0..21). The ComboBoxParameterAttachment
    // maps the parameter's normalized value via `round(norm * (numItems-1))`, so a
    // box with fewer items than the parameter shifts every index above the missing
    // ones (e.g. Power Chord = param 17 but box index 15 when "Unison" items are
    // absent), making the displayed selection wrong.
    harmonyTypeBox.addItemList (juce::StringArray {
        "None",
        "3rd Below", "3rd Above", "3rd Below + Above",
        "4th Below", "4th Above", "4th Below + Above",
        "5th Below", "5th Above", "5th Below + Above",
        "3rd Below + 5th Above", "5th Below + 3rd Above",
        "Octave Below", "Octave Above", "Octave Below + Above",
        "Vocal Stack (3 voices)", "Vocal Stack (4 voices)",
        "Power Chord", "Parallel 3rd", "Drone",
        "Unison (2 voices)", "Unison + Octaves (4 voices)"
    }, 1);
    harmonyTypeBox.setSelectedItemIndex (3, juce::dontSendNotification);
    harmonyTypeBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    harmonyTypeBox.setColour (juce::ComboBox::textColourId, ebs::text());
    harmonyTypeBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    addAndMakeVisible (harmonyTypeBox);

    // Harmony knobs (Volume, Blend) - use same rotary knob style as main knobs
    setupKnob (harmonyGainSlider, nullptr, "");
    harmonyGainSlider.setRange (0.0, 1.0, 0.01);
    harmonyGainSlider.setValue (1.0);
    harmonyGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyGainSlider.setPopupDisplayEnabled (true, false, this);
    harmonyGainSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    harmonyGainSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipVolume));

    setupKnob (harmonyBlendSlider, nullptr, "");
    harmonyBlendSlider.setRange (0.0, 1.0, 0.01);
    harmonyBlendSlider.setValue (0.5);
    harmonyBlendSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyBlendSlider.setPopupDisplayEnabled (true, false, this);
    harmonyBlendSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    harmonyBlendSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipBlend));
    harmonyAttackSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyAttack));

    // Harmony Attack knob: per-voice fade-in duration (ms). Placed under Blend.
    setupKnob (harmonyAttackSlider, nullptr, "");
    harmonyAttackSlider.setRange (1.0, 300.0, 1.0);
    harmonyAttackSlider.setValue (35.0);
    harmonyAttackSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyAttackSlider.setPopupDisplayEnabled (true, false, this);
    harmonyAttackSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };

    setupKnob (harmonyFormantSlider, nullptr, "");
    // Pivot knob: fill grows from the central 0 value, not from the minimum.
    harmonyFormantSlider.getProperties().set ("centred", true);
    harmonyFormantSlider.setRange (-5.0, 5.0, 0.1);
    harmonyFormantSlider.setValue (0.0);
    harmonyFormantSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyFormantSlider.setPopupDisplayEnabled (true, false, this);
    harmonyFormantSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " st"; };
    harmonyFormantSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyFormant));

    // Title labels above the Harmony knobs (no value display, just names).
    auto setupTitleLabel = [this] (juce::Label& l, const juce::String& name) {
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, ebs::text());
        l.setFont (ovt::fontLabelSmall());
        addAndMakeVisible (l);
    };
    setupTitleLabel (harmonyGainLabel, "Volume");
    setupTitleLabel (harmonyBlendLabel, "Blend");
    setupTitleLabel (harmonyAttackLabel, "Attack");
    setupTitleLabel (harmonyFormantLabel, "Formant");

    // Use Voice controls
    useVoiceButton.setButtonText (ovt::tr(ovt::Keys::kLabelUseVoice));
    useVoiceButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    useVoiceButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    addAndMakeVisible (useVoiceButton);

    shiftedVoicesBox.addItemList ({ "1", "2", "3", "4" }, 1);
    shiftedVoicesBox.setSelectedId (4, juce::dontSendNotification);
    shiftedVoicesBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    shiftedVoicesBox.setColour (juce::ComboBox::textColourId, ebs::text());
    shiftedVoicesBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    addAndMakeVisible (shiftedVoicesBox);

    harmonyToneBox.addItemList ({ "Choir", "Organ" }, 1);
    harmonyToneBox.setSelectedItemIndex (0, juce::dontSendNotification);
    harmonyToneBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    harmonyToneBox.setColour (juce::ComboBox::textColourId, ebs::text());
    harmonyToneBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    addAndMakeVisible (harmonyToneBox);

    harmonyToneColorLabel.setText ("Tone", juce::dontSendNotification);
    harmonyToneColorLabel.setJustificationType (juce::Justification::centred);
    harmonyToneColorLabel.setColour (juce::Label::textColourId, ebs::text());
    harmonyToneColorLabel.setFont (ovt::fontLabelSmall());
    addAndMakeVisible (harmonyToneColorLabel);

    harmonyToneColorSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    harmonyToneColorSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    harmonyToneColorSlider.setRange (0.0, 1.0, 0.01);
    harmonyToneColorSlider.setValue (0.5, juce::dontSendNotification);
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderFillColourId, ebs::accent());
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    harmonyToneColorSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    harmonyToneColorSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipToneColor));
    addAndMakeVisible (harmonyToneColorSlider);

    // Key and Scale are discrete values -> ComboBox.
    auto setupCombo = [this] (juce::ComboBox& b, juce::Label& l, const juce::String& name,
                              const juce::StringArray& items, int initialIndex)
    {
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, ebs::text());
        l.setFont (ovt::fontLabel());
        addAndMakeVisible (l);

        for (int i = 0; i < items.size(); ++i)
            b.addItem (items[i], i + 1);
        b.setSelectedItemIndex (initialIndex, juce::dontSendNotification);
        b.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
        b.setColour (juce::ComboBox::textColourId,       ebs::text());
        b.setColour (juce::ComboBox::outlineColourId,    ebs::accentSoft());
        b.setColour (juce::ComboBox::arrowColourId,      ebs::accent());
        addAndMakeVisible (b);
    };

    setupCombo (keyBox,   keyLabel,   "Root", kNoteNames,  0);
    translatableLabels.push_back ({ &keyLabel, ovt::Keys::kLabelRoot    });
    setupCombo (scaleBox, scaleLabel, "Scale", kScaleNames, 0);
    translatableLabels.push_back ({ &scaleLabel, ovt::Keys::kLabelScale    });

    // Key/Scale DETECTION source combo (Auto / OpenVoxKey / Sidechain). The old
    // "Manual" choice is gone: the power button below drives manual vs. detected.
    // The "Key Src" label is repurposed as the detection-row label (next to the
    // power button); the source combo has no label of its own.
    keySourceBox.addItemList (juce::StringArray { "Auto", "OpenVoxKey", "Sidechain" }, 1);
    keySourceBox.setSelectedItemIndex (0, juce::dontSendNotification);
    keySourceBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    keySourceBox.setColour (juce::ComboBox::textColourId,       ebs::text());
    keySourceBox.setColour (juce::ComboBox::outlineColourId,    ebs::accentSoft());
    keySourceBox.setColour (juce::ComboBox::arrowColourId,      ebs::accent());
    keySourceBox.setTooltip (ovt::tr (ovt::Keys::kTooltipKeySource));
    addAndMakeVisible (keySourceBox);

    // Line-1 label "Key/Scale Detection", sitting next to the power button.
    keySourceLabel.setText (ovt::tr (ovt::Keys::kLabelKeyDetect), juce::dontSendNotification);
    keySourceLabel.setJustificationType (juce::Justification::centredLeft);
    keySourceLabel.setColour (juce::Label::textColourId, ebs::text());
    keySourceLabel.setFont (ovt::fontLabel());
    addAndMakeVisible (keySourceLabel);
    // Clicking the "Key/Scale Detection" label toggles the power button, matching
    // the behaviour of the other Power buttons (whose label is drawn inside them).
    keySourceLabel.addMouseListener (this, false);
    translatableLabels.push_back ({ &keySourceLabel, ovt::Keys::kLabelKeyDetect    });

    // Companion group (A/B/C/D) - shown only when the source is OpenVoxKey.
    setupCombo (companionGroupBox, companionGroupLabel, "Group",
                juce::StringArray { "A", "B", "C", "D" }, 0);
    translatableLabels.push_back ({ &companionGroupLabel, ovt::Keys::kLabelCompanionGroup    });

    // Power-icon toggle for Key/Scale detection (on/off). No text: the label
    // next to it carries the "Key/Scale Detection" wording.
    keyDetectPowerButton.setButtonText (juce::String());
    keyDetectPowerButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    keyDetectPowerButton.setColour (juce::ToggleButton::tickColourId,  ebs::accent());
    keyDetectPowerButton.setTooltip (ovt::tr (ovt::Keys::kTooltipKeyDetect));
    addAndMakeVisible (keyDetectPowerButton);
    keyDetectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (processorRef.getParameters(), "key_detect", keyDetectPowerButton);

    // Show the Companion Group control only when detection is on AND the source is
    // OpenVoxKey; re-layout so the second row appears / disappears. Expanding the
    // block already calls resized(); the power button and combo also trigger it.
    keySourceBox.onChange = [this] {
        companionGroupBox.setVisible (keyDetectPowerButton.getToggleState()
                                       && keySourceBox.getSelectedItemIndex() == 1);
        resized();
    };
    keyDetectPowerButton.onClick = [this] { resized(); };

    latencyModeLabel.setText ("", juce::dontSendNotification);
    latencyModeLabel.setJustificationType (juce::Justification::centred);
    latencyModeLabel.setColour (juce::Label::textColourId, ebs::text());
    latencyModeLabel.setFont (ovt::fontVersion());
    latencyModeLabel.setVisible (false);

    latencyModeBox.addItemList ({ "Direct Monitoring", "Low Latency", "Quality", "Safe" }, 1);
    latencyModeBox.setSelectedItemIndex (2, juce::dontSendNotification);
    latencyModeBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    latencyModeBox.setColour (juce::ComboBox::textColourId, ebs::text());
    latencyModeBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    latencyModeBox.setColour (juce::ComboBox::arrowColourId, ebs::accent());
    addAndMakeVisible (latencyModeBox);

    // === Hamburger menu button (gear icon) ===
    menuButton.onClick = [this]
    {
        juce::PopupMenu menu;

        // 1. Latency submenu (4 modes: Direct Monitoring / Low Latency / Quality / Safe)
        juce::PopupMenu latencyMenu;
        latencyMenu.addItem (ovt::tr(ovt::Keys::kMenuDirectMonitoring), true, latencyModeBox.getSelectedId() == 1, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (0.0f);
    });
        latencyMenu.addItem (ovt::tr(ovt::Keys::kMenuLowLatency), true, latencyModeBox.getSelectedId() == 2, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (1.0f / 3.0f);
    });
        latencyMenu.addItem (ovt::tr(ovt::Keys::kMenuQuality), true, latencyModeBox.getSelectedId() == 3, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (2.0f / 3.0f);
    });
        latencyMenu.addItem (ovt::tr(ovt::Keys::kMenuSafe), true, latencyModeBox.getSelectedId() == 4, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (3.0f / 3.0f);
    });
        menu.addSubMenu (ovt::tr(ovt::Keys::kMenuLatency), latencyMenu);

        menu.addSeparator();

        // MIDI OUT toggle
        {
            bool midiOn = processorRef.getParameters().getParameter ("midi_out_enable")->getValue() > 0.5f;
            menu.addItem (ovt::tr(ovt::Keys::kMenuMidiOut), true, midiOn, [this] {
                if (auto* p = processorRef.getParameters().getParameter ("midi_out_enable"))
                    p->setValueNotifyingHost (1.0f - p->getValue());
    });
        }

        // Tuning follows MIDI IN toggle : an incoming held MIDI note
        // drives the correction target (the voice is tuned to the note).
        {
            bool midiTargetOn = processorRef.getParameters().getParameter ("midi_target_enable")->getValue() > 0.5f;
            menu.addItem (ovt::tr(ovt::Keys::kMenuMidiTarget), true, midiTargetOn, [this] {
                if (auto* p = processorRef.getParameters().getParameter ("midi_target_enable"))
                    p->setValueNotifyingHost (1.0f - p->getValue());
    });
        }

        menu.addSeparator();

        // Interface submenu
        {
            juce::PopupMenu interfaceMenu;

            // Theme toggle
            {
                juce::PopupMenu themeMenu;
                const bool isDark = ebs::isDark();
                themeMenu.addItem (ovt::tr(ovt::Keys::kMenuDarkTheme), true, isDark, [this] {
                    ebs::currentTheme() = ebs::Theme::Dark;
                    if (auto* p = processorRef.getParameters().getParameter ("ui_theme"))
                        p->setValueNotifyingHost (0.0f);
                    applyThemeToAllComponents();
    });
                themeMenu.addItem (ovt::tr(ovt::Keys::kMenuLightTheme), true, !isDark, [this] {
                    ebs::currentTheme() = ebs::Theme::Light;
                    if (auto* p = processorRef.getParameters().getParameter ("ui_theme"))
                        p->setValueNotifyingHost (1.0f);
                    applyThemeToAllComponents();
    });
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuTheme), themeMenu);
            }

            // Language selector
            {
                juce::PopupMenu langMenu;
                const auto currentLang = ovt::currentLanguage();
                auto addLangItem = [&](ovt::Language lang, int langIdx)
                {
                    const juce::String label = ovt::languageDisplayName (lang);
                    langMenu.addItem (label, true, (currentLang == lang), [this, lang, langIdx] {
                        ovt::currentLanguage() = lang;
                        if (auto* p = processorRef.getParameters().getParameter ("ui_language"))
                            p->setValueNotifyingHost ((float) langIdx / 5.0f);
                        repaint();
                        refreshLabels();
                        resized();
    });
                };
                addLangItem (ovt::Language::English,  0);
                addLangItem (ovt::Language::French,   1);
                addLangItem (ovt::Language::German,   2);
                addLangItem (ovt::Language::Spanish,  3);
                addLangItem (ovt::Language::Japanese, 4);
                addLangItem (ovt::Language::Chinese,  5);
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuLanguage), langMenu);
            }

            interfaceMenu.addSeparator();

            // Show Waveform toggle
            interfaceMenu.addItem (ovt::tr(ovt::Keys::kMenuShowWaveform), true, showWaveform, [this] {
                showWaveform = ! showWaveform;
                if (auto* p = processorRef.getParameters().getParameter ("ui_show_waveform"))
                    p->setValueNotifyingHost (showWaveform ? 1.0f : 0.0f);
                processorRef.setWaveformCaptureEnabled (showWaveform);
                if (! showWaveform)
                {
                    if (pitchVisualizer != nullptr)
                        pitchVisualizer->setWaveformOverlay (nullptr, 0, 44100.0);
                    if (curveEditor != nullptr)
                        curveEditor->setWaveformOverlay (nullptr, 0, 44100.0);
                }
    });

            // Waveform display type submenu
            {
                juce::PopupMenu waveformMenu;
                const int currentType = processorRef.getWaveformDisplayType();
                waveformMenu.addItem (ovt::tr(ovt::Keys::kMenuWaveformLine), true, currentType == 0, [this] { setWaveformDisplayType (0);    });
                waveformMenu.addItem (ovt::tr(ovt::Keys::kMenuWaveformMirror), true, currentType == 1, [this] { setWaveformDisplayType (1);    });
                waveformMenu.addItem (ovt::tr(ovt::Keys::kMenuWaveformSpectral), true, currentType == 2, [this] { setWaveformDisplayType (2);    });
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuWaveformDisplay), waveformMenu, true);
            }

            interfaceMenu.addSeparator();

            // Auto-Center Pitch Display (Live tab only)
            if (pitchVisualizer != nullptr)
            {
                const bool acEnabled = pitchVisualizer->isAutoCenter();
                interfaceMenu.addItem ("Auto-Center Pitch", true, acEnabled, [this] {
                    if (pitchVisualizer != nullptr)
                    {
                        pitchVisualizer->setAutoCenter (! pitchVisualizer->isAutoCenter());
                        if (auto* p = processorRef.getParameters().getParameter ("ui_auto_center"))
                            p->setValueNotifyingHost (pitchVisualizer->isAutoCenter() ? 1.0f : 0.0f);
                    }
                });
            }

            interfaceMenu.addSeparator();

            // Tooltip delay submenu. Slow=1500ms (more contemplative,
            // useful when demoing the plugin), Normal=700ms (JUCE default),
            // Fast=200ms (snappy, for power users). The choice is applied
            // live to the global TooltipWindow so the user sees the effect
            // on the very next hover, no editor restart required.
            {
                juce::PopupMenu tooltipMenu;
                auto addDelayItem = [&](int idx, const char* key)
                {
                    tooltipMenu.addItem (ovt::tr (key), true,
                                         (tooltipDelayChoice == idx),
                                         [this, idx] { setTooltipDelayChoice (idx); });
                };
                addDelayItem (0, ovt::Keys::kLabelTooltipDelaySlow);
                addDelayItem (1, ovt::Keys::kLabelTooltipDelayNormal);
                addDelayItem (2, ovt::Keys::kLabelTooltipDelayFast);
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuTooltipDelay), tooltipMenu);
            }

            interfaceMenu.addSeparator();

            // Keyboard Shortcuts
            interfaceMenu.addItem (ovt::tr(ovt::Keys::kMenuKeyboardShortcuts), [this] { toggleHelpOverlay();    });

            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuInterface), interfaceMenu);
        }

        menu.addSeparator();

        const bool isStandalone = processorRef.isStandaloneWrapper();

        // Advanced submenu
        {
            juce::PopupMenu advancedMenu;

            // Export as Image...
            advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuExportImage), [this] {
                const bool isCurveEditor = (tabbedComponent.getCurrentTabIndex() == 1);
                auto* pitchViz = dynamic_cast<ui::PitchVisualizer*> (tabbedComponent.getTabContentComponent (0));

                if ((isCurveEditor && curveEditor == nullptr) || (!isCurveEditor && pitchViz == nullptr))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                        ovt::tr(ovt::Keys::kDlgExport), ovt::tr(ovt::Keys::kDlgExportNotFound));
                    return;
                }

                auto defaultDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
                auto downloadsDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                        .getChildFile ("Downloads");
                if (downloadsDir.isDirectory())
                    defaultDir = downloadsDir;

                juce::Component::SafePointer<ui::PitchVisualizer> pitchVizSafe (pitchViz);
                juce::Component::SafePointer<ui::PitchCurveEditor> curveEditorSafe (curveEditor.get());
                auto chooserPtr = std::make_shared<juce::FileChooser>(
                    ovt::tr(ovt::Keys::kDlgExportPng),
                    defaultDir,
                    "*.png");
                chooserPtr->launchAsync (
                    juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                    [chooserPtr, isCurveEditor, pitchVizSafe, curveEditorSafe] (const juce::FileChooser& fc) {
                        auto file = fc.getResult();
                        if (file != juce::File{})
                        {
                            if (file.getFileExtension() != ".png")
                                file = file.withFileExtension (".png");

                            bool ok = false;
                            if (isCurveEditor && curveEditorSafe != nullptr)
                                ok = curveEditorSafe->exportAsImage (file);
                            else if (pitchVizSafe != nullptr)
                                ok = pitchVizSafe->exportAsImage (file);

                            if (ok)
                                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                    ovt::tr(ovt::Keys::kDlgExport), ovt::tr(ovt::Keys::kDlgImageSaved) + file.getFullPathName());
                            else
                                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                    ovt::tr(ovt::Keys::kDlgExport), ovt::tr(ovt::Keys::kDlgImageFailed));
                        }
    });
    });

            // Formant Mode and Pitch Detection are hidden for now - may be
            // re-enabled later when additional modes become available.

            // MIDI Learn (standalone only)
            if (isStandalone)
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
                advancedMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuMidiLearn), midiLearnMenu);
            }

            // Chord Detection (ovtchord): real-time chord-aware tuning from the
            // MIDI input ("Tuning follows MIDI IN") and/or the sidechain bus.
            {
                bool chordOn = processorRef.getParameters().getParameter ("chord_detect_enable")->getValue() > 0.5f;
                advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuChordDetection), true, chordOn, [this] {
                    if (auto* p = processorRef.getParameters().getParameter ("chord_detect_enable"))
                        p->setValueNotifyingHost (1.0f - p->getValue());
                });
            }

            // Reset to Default
            advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuResetDefault), [this] {
                juce::PopupMenu confirmMenu;
                confirmMenu.addItem (ovt::tr(ovt::Keys::kMenuCancel), []{    });
                confirmMenu.addSeparator();
                confirmMenu.addItem (ovt::tr(ovt::Keys::kMenuConfirmReset), [this] {
                    auto& paramTree = processorRef.getParameters().state;
                    for (int i = 0; i < paramTree.getNumChildren(); ++i)
                    {
                        auto id = paramTree.getChild(i).getProperty ("id").toString();
                        if (auto* param = processorRef.getParameters().getParameter (id))
                            param->setValueNotifyingHost (param->getDefaultValue());
                    }
    });
                applyMenuLookAndFeel (confirmMenu, customLookAndFeel);
                confirmMenu.showMenuAsync (juce::PopupMenu::Options()
                    .withTargetComponent (&menuButton)
                    .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards));
    });

            // Bypass (standalone only)
            if (processorRef.isStandaloneWrapper())
            {
                bool bypassOn = processorRef.getParameters().getParameter ("bypass")->getValue() > 0.5f;
                advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuBypass), true, bypassOn, [this] {
                    if (auto* p = processorRef.getParameters().getParameter ("bypass"))
                        p->setValueNotifyingHost (1.0f - p->getValue());
    });
            }

            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuAdvanced), advancedMenu);
        }

        menu.addSeparator();

        // Check for Updates
        menu.addItem (ovt::tr(ovt::Keys::kMenuCheckUpdates), [this] {
            updateButton.onClick();
    });

       #if JUCE_DEBUG
        menu.addItem (ovt::tr(ovt::Keys::kMenuDebugWindow), [this] { debugWindowButton.onClick();    });
       #endif


        applyMenuLookAndFeel (menu, customLookAndFeel);
        auto opts = juce::PopupMenu::Options();
        if (! pendingMenuScreenPos.isOrigin())
        {
            opts = opts.withTargetScreenArea (juce::Rectangle<int> (
                pendingMenuScreenPos.x, pendingMenuScreenPos.y, 1, 1));
            pendingMenuScreenPos = {};
        }
        else
        {
            opts = opts.withTargetComponent (&menuButton)
                       .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards);
        }
        menu.showMenuAsync (opts);
    };
    addAndMakeVisible (menuButton);

    // === A/B Comparison buttons ===
    auto setupABButton = [this] (ABTextButton& btn, const juce::String& label, int slotIdx) {
        btn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcccccc));
        btn.setTooltip (label == "A" ? ovt::tr(ovt::Keys::kTooltipAbSlotA) : ovt::tr(ovt::Keys::kTooltipAbSlotB));
        btn.onClick = [this, slotIdx] {
            auto& clickedSlot = (slotIdx == 0) ? slotA : slotB;

            // Auto-save the CURRENT slot before loading the other one.
            // Only save if the current state matches this slot's original state
            // (morph slider at the correct endpoint), not a morphed blend.
            // Always commit the CURRENT slot's state when switching away, so
            // edits made on it are never lost. Capture the LIVE parameters
            // directly. Do NOT snap the morph first: snapping fires
            // onMorphSliderChanged, which would re-apply the slot's stale
            // stored state via morphSource and overwrite the live edit before
            // capture (exactly the "slot A reset to defaults" bug seen).
            {
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
                auto* lp = processorRef.getParameters().getParameter ("vibrato_preserve");
                auto* lh = processorRef.getParameters().getParameter ("humanize");
                auto* lf = processorRef.getParameters().getParameter ("harmony_formant");
                auto* lt = processorRef.getParameters().getParameter ("harmony_type");
                juce::Logger::writeToLog (
                    "[DIAG] pre-save live: vibrato=" + juce::String (lp ? lp->getValue() : -1.0f, 4)
                    + " humanize=" + juce::String (lh ? lh->getValue() : -1.0f, 4)
                    + " formant=" + juce::String (lf ? lf->getValue() : -1.0f, 4)
                    + " type=" + juce::String (lt ? (int) std::round (lt->getValue() * 21.0f) : -1));
#endif
                auto& currentSlot = isSlotAActive ? slotA : slotB;
                const int currentIdx = isSlotAActive ? 0 : 1;
                saveSlot (currentSlot, currentIdx);
            }

            // Initialize the clicked slot if empty (first click)
            if (! clickedSlot.hasData)
                saveSlot (clickedSlot, slotIdx);

            // Clear old morph state before loading
            morphSource.reset();
            morphTarget.reset();
            morphUndoState.reset();
            // Drop the external-automation exclusion baseline. loadSlot() below
            // overwrites the parameters with the slot's saved values; without
            // clearing this map the next slider move would treat every changed
            // parameter as "externally driven" and exclude it from the morph,
            // which (once excluded, a parameter's baseline is never refreshed)
            // progressively kills the slider after a few A<->B toggles.
            lastMorphIntendedValues.clear();

            // Load the clicked slot
            loadSlot (clickedSlot);
            isSlotAActive = (slotIdx == 0);
            processorRef.setAbActiveSlot (slotIdx);

            // Set up morph state: source = A (slider left), target = B (slider right).
            // This is always the same regardless of which slot is active.
            if (slotA.morphState != nullptr && slotB.morphState != nullptr)
            {
                morphSource = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
                morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            }

            // Position slider: A=left (0), B=right (1)
            processorRef.setMorphAmount (slotIdx == 0 ? 0.0f : 1.0f);

            // Reflect the (now active) slot's assigned plugin preset in the
            // preset selector. If the slot still matches its preset exactly
            // (no user edits since it was loaded) show that preset; otherwise
            // it is considered "User" (detached from the preset).
            auto& activeSlot = isSlotAActive ? slotA : slotB;
            if (slotMatchesPreset (activeSlot))
            {
                currentPluginPresetName = activeSlot.presetName;
                presetComboBox.setText (activeSlot.presetName, juce::dontSendNotification);
            }
            else
            {
                currentPluginPresetName = "User";
                presetComboBox.setText ("User", juce::dontSendNotification);
            }

            updateABButtonStates();
        };
        addAndMakeVisible (btn);
    };
    setupABButton (buttonA, "A", 0);
    setupABButton (buttonB, "B", 1);
    // Right-click on A/B saves the current state into that slot (detaches it
    // from any named plugin preset, since it is now an explicit manual state).
    buttonA.onRightClick = [this] { saveSlot (slotA, 0); slotA.presetName = ""; updateABButtonStates(); };
    buttonB.onRightClick = [this] { saveSlot (slotB, 1); slotB.presetName = ""; updateABButtonStates(); };
    // Button A always has valid data (it's the default state)
    buttonA.hasValidData = true;
    buttonA.isActive = true;

    // === Morph Slider ===
    morphSlider.setName ("Morph");   // kept for component identification/debugging
    morphSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morphSlider.setRange (0.0, 1.0, 0.01);
    morphSlider.setColour (juce::Slider::thumbColourId, ebs::accent());
    morphSlider.setColour (juce::Slider::trackColourId, ebs::accent().withAlpha (0.7f));
    morphSlider.setColour (juce::Slider::backgroundColourId, ebs::bgPanel());
    morphSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphDrag));
    // Morph is now a host-automatable parameter ("morph_amount"). The slider is
    // bound to it via this attachment, so DAW automation and the slider stay in
    // sync. The actual morph application is driven from timerCallback().
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (processorRef.getParameters(), "morph_amount", morphSlider);
    addAndMakeVisible (morphSlider);
    morphSlider.addMouseListener (this, false); // for right-click context menu

    morphSliderLabel.setText (ovt::tr(ovt::Keys::kLabelMorph), juce::dontSendNotification);
    morphSliderLabel.setFont (ovt::fontLegendHint());
    morphSliderLabel.setColour (juce::Label::textColourId, ebs::textDim());
    morphSliderLabel.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphLabel));
    morphSliderLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (morphSliderLabel);

    // === Slider ranges ===
    speedSlider.setRange (0.0, 200.0, 1.0);
    speedSlider.setValue (50.0);
    amountSlider.setRange (0.0, 1.0, 0.01);
    amountSlider.setValue (1.0);

    // === Bindings for the Key/Scale ComboBoxes to their parameters ===
    // ComboBoxAttachment keeps the combo box and the host parameter in sync in BOTH
    // directions: parameter -> combo box (so morph/automation correctly drives the displayed
    // scale/key) and combo box -> parameter on user selection. It uses the ComboBox Listener
    // mechanism (addListener / comboBoxChanged) to write the parameter on a genuine user
    // selection; this is robust and idempotent with whatever we do below.
    // IMPORTANT: when the attachment drives the combo (morph / host automation) it updates the
    // displayed index with sendNotificationSync. JUCE guards the Listener with an internal
    // ignoreCallbacks flag, but the onChange callback is NOT guarded, so onChange ALSO fires in
    // that case. Therefore the onChange handlers below must NOT write the parameter (that would
    // fight the morph / automation, since the index is read back from the combo display which can
    // momentarily lag the morph target during a crossfade). onChange only mirrors the per-note
    // custom flags / piano keys for whatever scale/key the combo currently shows.
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "key", keyBox);
    // The ComboBoxAttachment writes the "key" parameter on user selection (via its Listener).
    // Here we only re-sync the per-note custom flags / piano keys for the new key. We must not
    // write the parameter here (see the note above about morph/automation driving the combo).
    keyBox.onChange = [this] {
        const int scaleIdx = scaleBox.getSelectedItemIndex();
        if (scaleIdx >= 0 && scaleIdx != 13)
            scaleBox.onChange(); // Re-sync intervals/piano for the new key
    };

    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "scale", scaleBox);
    // The ComboBoxAttachment writes the "scale" parameter on user selection (via its Listener),
    // so the engine and the curve editor snap always use the selected scale. This onChange
    // handler must NOT also write the parameter: it fires whenever the attachment drives the
    // combo (morph / host automation) via sendNotificationSync, and writing back would fight the
    // morph (the index comes from the combo display, which can lag the morph target during a
    // crossfade). Here we only keep the per-note custom flags / piano keys in sync with the
    // selected preset scale.
    scaleBox.onChange = [this] {
        const int idx = scaleBox.getSelectedItemIndex();
        if (idx < 0)
            return;
        if (idx != 13) // Not Custom (Custom keeps the user's custom note flags)
        {
            // Suppress onUserInteraction callbacks on the piano keys during the
            // programmatic sync, so selecting a preset from the combo does NOT
            // re-trigger the "switch to Custom" logic (which would set the combo
            // back to "Custom" and cancel the user's preset selection).
            scaleKeyboard.setUpdatingFromScaleCombo (true);
            const int keyIdx = processorRef.getKeyIndex();

            ovtdsp::ScaleQuantizer tempQuantizer;
            tempQuantizer.setKey (keyIdx);
            tempQuantizer.setScale (static_cast<ovtdsp::Scale> (juce::jlimit (0, 13, idx)));
            auto intervals = tempQuantizer.getScaleIntervals ();

            for (int i = 0; i < 12; ++i)
            {
                auto* p = processorRef.getParameters().getParameter ("custom" + juce::String (i));
                if (p != nullptr)
                {
                    float targetVal = intervals.contains (i) ? 1.0f : 0.0f;
                    if (p->getValue() != targetVal)
                        p->setValueNotifyingHost (targetVal);
                }
            }
            scaleKeyboard.setUpdatingFromScaleCombo (false);
        }
    };
    latencyModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "latency_mode", latencyModeBox);
    detectorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "pitch_detector", detectorBox);
    keySourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "key_source", keySourceBox);
    companionGroupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "companion_group", companionGroupBox);

    // Reverb attachments
    reverbEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "reverb_enable", reverbEnableButton);
    reverbMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "reverb_mix", reverbMixSlider);

    noiseGateEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getParameters(), "noise_gate_enable", noiseGateEnableButton);
    noiseGateThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getParameters(), "noise_gate_threshold", noiseGateThresholdSlider);

    // Upward Compressor attachments
    upwardCompEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getParameters(), "upward_comp_enable", upwardCompEnableButton);
    upwardCompAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getParameters(), "upward_comp_amount", upwardCompAmountSlider);

    // FlexTune / Humanize / Vibrato / Correction Mode attachments
    // 2026-07-24 (Deprecation): FlexTune and Attack-Aware attachments
    // are disabled (see PluginEditor.h). The APVTS parameters still
    // exist for preset compatibility, but the UI sliders/buttons are
    // not attached and not visible.
    // flexTuneAttachment = std::make_unique<...> (...);  // DEPRECATED
    humanizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "humanize", humanizeSlider);
    vibratoPreserveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "vibrato_preserve", vibratoPreserveSlider);
    // attackAwareAttachment = std::make_unique<...> (...);   // DEPRECATED
    // attackReleaseAttachment = std::make_unique<...> (...); // DEPRECATED
    correctionModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "correction_mode", correctionModeButton);

    // === Voice Type combo (Correction block, Advanced area, row 2) ===
    // Same visual style as the other dropdowns (Scale, Key, etc.).
    voiceTypeBox.addItemList ({ "Universal", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 1);
    voiceTypeBox.setSelectedItemIndex (0, juce::dontSendNotification);
    voiceTypeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a36));
    voiceTypeBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xffcccccc));
    voiceTypeBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x441A9AF0));
    voiceTypeBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff1A9AF0));
    voiceTypeBox.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff191b1e));
    voiceTypeBox.setColour (juce::PopupMenu::textColourId, juce::Colour (0xffcccccc));
    voiceTypeBox.setTooltip ("Constrain pitch detection to a vocal register. Universal (default) covers the full voice range. Select Bass / Baritone / Tenor / Alto / Soprano to reduce octave errors and CPU for that register.");
    voiceTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "voice_type", voiceTypeBox);
    addAndMakeVisible (voiceTypeBox);

    voiceTypeLabel.setText (ovt::tr (ovt::Keys::kVoiceTypeLabel), juce::dontSendNotification);
    voiceTypeLabel.setJustificationType (juce::Justification::left);
    voiceTypeLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    voiceTypeLabel.setFont (ovt::fontLegendHint());
    voiceTypeLabel.setTooltip (ovt::tr (ovt::Keys::kTooltipVoiceType));
    addAndMakeVisible (voiceTypeLabel);

    // Register the label so refreshLabels() updates it on language switch.
    translatableLabels.push_back ({ &voiceTypeLabel, ovt::Keys::kVoiceTypeLabel });

    // === Formant Strategy combo (Advanced area, row 3) ===
    // Selects the formant-preservation method. Each strategy has a distinct
    // character; the tooltip below summarises them in plain language.
    formantStrategyBox.addItemList ({
        ovt::tr (ovt::Keys::kFormantStrategySubtle),
        ovt::tr (ovt::Keys::kFormantStrategyBalanced),
        ovt::tr (ovt::Keys::kFormantStrategyMarked),
        ovt::tr (ovt::Keys::kFormantStrategyReactive),
        ovt::tr (ovt::Keys::kFormantStrategyPrecise) }, 1);
    formantStrategyBox.setSelectedItemIndex (4, juce::dontSendNotification);  // Precise is default
    formantStrategyBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a36));
    formantStrategyBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xffcccccc));
    formantStrategyBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x441A9AF0));
    formantStrategyBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff1A9AF0));
    formantStrategyBox.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff191b1e));
    formantStrategyBox.setColour (juce::PopupMenu::textColourId, juce::Colour (0xffcccccc));
    formantStrategyBox.setTooltip (ovt::tr (ovt::Keys::kTooltipFormantStrategy));
    formantStrategyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "formant_strategy", formantStrategyBox);
    addAndMakeVisible (formantStrategyBox);

    formantStrategyLabel.setText (ovt::tr (ovt::Keys::kLabelFormantStrategy), juce::dontSendNotification);
    formantStrategyLabel.setJustificationType (juce::Justification::left);
    formantStrategyLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    formantStrategyLabel.setFont (ovt::fontLegendHint());
    formantStrategyLabel.setTooltip (ovt::tr (ovt::Keys::kLabelFormantStrategy));
    addAndMakeVisible (formantStrategyLabel);

    // Register for live language switch.
    translatableLabels.push_back ({ &formantStrategyLabel, ovt::Keys::kLabelFormantStrategy });

    // UI updates (visibility of custom buttons, etc.) are handled in timerCallback.

    // Helper for creating Drawables from full SVG XML (with viewBox).
    // Uses a placeholder color (#010101) that gets replaced by the desired state color.
    auto createDrawableSVG = [](const juce::String& svgXml, juce::Colour strokeColor) -> std::unique_ptr<juce::Drawable> {
        auto baseXml = juce::XmlDocument::parse(svgXml);
        if (baseXml == nullptr) return std::make_unique<juce::DrawablePath>();
        auto d = juce::Drawable::createFromSVG(*baseXml);
        if (d == nullptr) return std::make_unique<juce::DrawablePath>();
        // Replace placeholder color with the desired state color
        d->replaceColour(juce::Colour(0xff010101), strokeColor);
        return d;
    };

    auto setupIconButton = [&](juce::DrawableButton& btn, const juce::String& svgXml, bool isToggle, const juce::String& tooltip) {
        auto normal   = createDrawableSVG(svgXml, juce::Colours::grey);
        auto over     = createDrawableSVG(svgXml, juce::Colours::lightgrey);
        auto down     = createDrawableSVG(svgXml, juce::Colours::white);

        if (isToggle) {
            auto normalOn = createDrawableSVG(svgXml, ebs::accent());
            auto overOn   = createDrawableSVG(svgXml, ebs::accent().brighter(0.2f));
            auto downOn   = createDrawableSVG(svgXml, juce::Colours::white);
            btn.setImages(normal.get(), over.get(), down.get(), nullptr,
                          normalOn.get(), overOn.get(), downOn.get(), nullptr);
            btn.setClickingTogglesState(true);
        } else {
            btn.setImages(normal.get(), over.get(), down.get());
        }

        btn.setTooltip(tooltip);
        btn.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha(0.2f));
        btn.setColour(juce::DrawableButton::textColourId, juce::Colours::white);
        btn.setColour(juce::DrawableButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible(btn);
    };

    // === SVG icons as full XML strings (Lucide-style, 24x24 viewBox) ===
    // Each uses stroke="#010101" as a placeholder for per-state coloring.

    static const char* svgScale = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="7" cy="18" r="3"/><path d="M10 18V4l11-2v15"/></svg>)";

    static const char* svgGrid = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/></svg>)";

    static const char* svgStep = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20v-5h6v-6h6V4h4"/></svg>)";

    static const char* svgClear = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 6h18"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/><line x1="10" y1="11" x2="10" y2="17"/><line x1="14" y1="11" x2="14" y2="17"/></svg>)";

    static const char* svgPower = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2v10"/><path d="M18.36 6.64a9 9 0 1 1-12.73 0"/></svg>)";

    static const char* svgGear = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>)";

    // Zoom/scroll/reset icons (shared with the Visualizer toolbar).
    static const char* svgZoomIn = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="11" y1="8" x2="11" y2="14"/><line x1="8" y1="11" x2="14" y2="11"/></svg>)";
    static const char* svgZoomOut = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="8" y1="11" x2="14" y2="11"/></svg>)";
    static const char* svgScrollUp = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="18 15 12 9 6 15"/></svg>)";
    static const char* svgScrollDown = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"/></svg>)";
    static const char* svgResetView = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>)";

    // Transport (standalone only).
    static const char* svgPlay = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="6 4 20 12 6 20 6 4"/></svg>)";
    static const char* svgStop = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="6" width="12" height="12" rx="1"/></svg>)";
    // "Return to start" (rewind): a left-pointing triangle plus a vertical bar,
    // the classic DAW skip-to-beginning glyph.
    static const char* svgRewind = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="6" y1="5" x2="6" y2="19"/><polygon points="19 5 9 12 19 19"/></svg>)";
    // Curve Editor "Options": hamburger (3 horizontal bars) - clearly distinct
    // from the plugin's own gear (menuButton) and icon-only (no text label).
    static const char* svgHamburger = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/></svg>)";

    // Plugin Preset selector (top banner, centred): previous / next arrows and
    // a save (download-to-disk) icon. Distinct from the Curve Presets gallery.
    static const char* svgPresetPrev = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 18 9 12 15 6"/></svg>)";
    static const char* svgPresetNext = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 18 15 12 9 6"/></svg>)";
    static const char* svgPresetSave = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/></svg>)";

    // Global plugin Undo / Redo icons (curved-left arrows). Mirror the Curve
    // Editor's own undo/redo buttons, but operate on the processor-wide
    // UndoManager (every automatable parameter).
    static const char* svgUndo = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="9 14 4 9 9 4"/><path d="M4 9h11a5 5 0 0 1 0 10h-4"/></svg>)";
    static const char* svgRedo = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="15 14 20 9 15 4"/><path d="M20 9H9a5 5 0 0 0 0 10h4"/></svg>)";

    // Preset Gallery : a 2x2 grid of rounded tiles.
    static const char* svgPresetGrid = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg>)";

    // Setup Toolbar Buttons
    // Curve Editor "Options" button: icon-only hamburger, with a distinct
    // accent-tinted background so it stands out from the neutral zoom/scroll/
    // snap buttons in this section.
{
    auto optsNormal = createDrawableSVG (svgHamburger, juce::Colours::white);
    auto optsOver   = createDrawableSVG (svgHamburger, ebs::accent());
    auto optsDown   = createDrawableSVG (svgHamburger, juce::Colours::white);
    optionsButton.setImages (optsNormal.get(), optsOver.get(), optsDown.get());
    optionsButton.setColour (juce::DrawableButton::backgroundColourId, ebs::accent().withAlpha (0.22f));
    optionsButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.4f));
    optionsButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
    optionsButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (optionsButton);

    optionsButton.onClick = [this] { showCurveOptionsMenu(); };
    optionsButton.setTooltip (ovt::tr(ovt::Keys::kTooltipCurveOptions));

    // Preset Gallery toolbar button: icon-only grid, opens the browsable gallery.
    {
        auto galNormal = createDrawableSVG (svgPresetGrid, juce::Colours::white);
        auto galOver   = createDrawableSVG (svgPresetGrid, ebs::accent());
        auto galDown   = createDrawableSVG (svgPresetGrid, juce::Colours::white);
        presetGalleryButton.setImages (galNormal.get(), galOver.get(), galDown.get());
        presetGalleryButton.setColour (juce::DrawableButton::backgroundColourId, ebs::accent().withAlpha (0.22f));
        presetGalleryButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.4f));
        presetGalleryButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        presetGalleryButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (presetGalleryButton);

        presetGalleryButton.onClick = [this] { showPresetGallery(); };
        presetGalleryButton.setTooltip (ovt::tr(ovt::Keys::kTooltipPresetGallery));
    }
}

    setupIconButton(snapButton, svgScale, true, "Snap to scale");
    snapButton.setToggleState(true, juce::dontSendNotification);
    snapButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setSnapEnabled(snapButton.getToggleState());
    };
    snapButton.setTooltip(ovt::tr(ovt::Keys::kTooltipSnapToScale));

    setupIconButton(snapGridButton, svgGrid, true, "Snap to grid");
    snapGridButton.setToggleState(false, juce::dontSendNotification);
    snapGridButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setSnapToGridEnabled(snapGridButton.getToggleState());
    };
    snapGridButton.setTooltip(ovt::tr(ovt::Keys::kTooltipSnapToGrid));

    setupIconButton(stepModeButton, svgStep, true, "Step mode (staircase interpolation)");
    stepModeButton.setToggleState(true, juce::dontSendNotification);
    stepModeButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setStepModeEnabled(stepModeButton.getToggleState());
    };
    stepModeButton.setTooltip(ovt::tr(ovt::Keys::kTooltipStepMode));

    // Curve Editor view controls (mirror the Visualizer toolbar: zoom / scroll / reset).
    setupIconButton(zoomInButton, svgZoomIn, false, ovt::tr(ovt::Keys::kTooltipZoomIn));
    zoomInButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->zoomIn(); };

    setupIconButton(zoomOutButton, svgZoomOut, false, ovt::tr(ovt::Keys::kTooltipZoomOut));
    zoomOutButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->zoomOut(); };

    setupIconButton(scrollUpButton, svgScrollUp, false, ovt::tr(ovt::Keys::kTooltipScrollUp));
    scrollUpButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->scrollUp(); };

    setupIconButton(scrollDownButton, svgScrollDown, false, ovt::tr(ovt::Keys::kTooltipScrollDown));
    scrollDownButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->scrollDown(); };

    setupIconButton(resetViewButton, svgResetView, false, ovt::tr(ovt::Keys::kTooltipResetView));
    resetViewButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->resetView(); };

    // "Measures" control: number of measures shown in the curve editor time window.
    // Moved here from the embedded editor controls so it does not cover the ruler.
    measuresLabel.setText (ovt::tr(ovt::Keys::kLabelMeasures), juce::dontSendNotification);
    measuresLabel.setJustificationType (juce::Justification::left);
    measuresLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    measuresLabel.setFont (ovt::fontMeasuresLabel());
    addAndMakeVisible (measuresLabel);

    measuresComboBox.addItemList ({ "1", "2", "4", "8", "16", "32" }, 1);
    measuresComboBox.setSelectedItemIndex (2, juce::dontSendNotification); // default "4"
    measuresComboBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a36));
    measuresComboBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xffcccccc));
    measuresComboBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x441A9AF0));
    measuresComboBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff1A9AF0));
    measuresComboBox.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff191b1e));
    measuresComboBox.setColour (juce::PopupMenu::textColourId, juce::Colour (0xffcccccc));
    measuresComboBox.onChange = [this] {
        if (curveEditor != nullptr)
            curveEditor->setMeasuresVisible (measuresComboBox.getText().getIntValue());
    };
    addAndMakeVisible (measuresComboBox);

    // === Global plugin Undo/Redo buttons (Option 1) ===
    // Icon-only DrawableButtons (curved-arrow glyphs) that operate on the
    // processor-wide UndoManager (every automatable parameter). Wired to
    // performGlobalUndo() / performGlobalRedo(); the keyboard shortcuts
    // Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z do the same (see keyPressed()).
    // They sit in the header between button B and the Options gear.
    {
        auto undoNorm = createDrawableSVG (svgUndo, juce::Colours::white);
        auto undoOver = createDrawableSVG (svgUndo, ebs::accent());
        auto undoDown = createDrawableSVG (svgUndo, juce::Colours::white);
        undoButton.setImages (undoNorm.get(), undoOver.get(), undoDown.get());
        undoButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        undoButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        undoButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        undoButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        undoButton.setTooltip (ovt::tr(ovt::Keys::kTooltipUndo));
        undoButton.onClick = [this] { performGlobalUndo(); };
        addAndMakeVisible (undoButton);

        auto redoNorm = createDrawableSVG (svgRedo, juce::Colours::white);
        auto redoOver = createDrawableSVG (svgRedo, ebs::accent());
        auto redoDown = createDrawableSVG (svgRedo, juce::Colours::white);
        redoButton.setImages (redoNorm.get(), redoOver.get(), redoDown.get());
        redoButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        redoButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        redoButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        redoButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        redoButton.setTooltip (ovt::tr(ovt::Keys::kTooltipRedo));
        redoButton.onClick = [this] { performGlobalRedo(); };
        addAndMakeVisible (redoButton);
    }

    // === Plugin Preset selector (separate from Curve Presets) ===
    // Centred in the top banner: [<] [Combo: name] [>] [save]. Mirrors the
    // standard plugin-preset bar. Applies the full parameter state (no
    // curve) via the processor's global-undo-aware apply.
    {
        auto prevNorm = createDrawableSVG (svgPresetPrev, juce::Colours::white);
        auto prevOver = createDrawableSVG (svgPresetPrev, ebs::accent());
        auto prevDown = createDrawableSVG (svgPresetPrev, juce::Colours::white);
        presetPrevButton.setImages (prevNorm.get(), prevOver.get(), prevDown.get());
        presetPrevButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        presetPrevButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        presetPrevButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        presetPrevButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        presetPrevButton.setTooltip (ovt::tr(ovt::Keys::kTooltipPluginPresetPrev));
        presetPrevButton.onClick = [this] {
            const int idx = pluginPresetNames.indexOf (currentPluginPresetName);
            const int n = pluginPresetNames.size();
            if (n == 0) return;
            const int prev = (idx <= 0) ? n - 1 : idx - 1;
            applyPluginPresetByName (pluginPresetNames[prev]);
        };
        addAndMakeVisible (presetPrevButton);

        auto nextNorm = createDrawableSVG (svgPresetNext, juce::Colours::white);
        auto nextOver = createDrawableSVG (svgPresetNext, ebs::accent());
        auto nextDown = createDrawableSVG (svgPresetNext, juce::Colours::white);
        presetNextButton.setImages (nextNorm.get(), nextOver.get(), nextDown.get());
        presetNextButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        presetNextButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        presetNextButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        presetNextButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        presetNextButton.setTooltip (ovt::tr(ovt::Keys::kTooltipPluginPresetNext));
        presetNextButton.onClick = [this] {
            const int idx = pluginPresetNames.indexOf (currentPluginPresetName);
            const int n = pluginPresetNames.size();
            if (n == 0) return;
            const int next = (idx < 0 || idx >= n - 1) ? 0 : idx + 1;
            applyPluginPresetByName (pluginPresetNames[next]);
        };
        addAndMakeVisible (presetNextButton);

        auto saveNorm = createDrawableSVG (svgPresetSave, juce::Colours::white);
        auto saveOver = createDrawableSVG (svgPresetSave, ebs::accent());
        auto saveDown = createDrawableSVG (svgPresetSave, juce::Colours::white);
        presetSaveButton.setImages (saveNorm.get(), saveOver.get(), saveDown.get());
        presetSaveButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        presetSaveButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        presetSaveButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        presetSaveButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        presetSaveButton.setTooltip (ovt::tr(ovt::Keys::kTooltipPluginPresetSave));
        presetSaveButton.onClick = [this] { promptSavePluginPreset(); };
        addAndMakeVisible (presetSaveButton);

        presetComboBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a36));
        presetComboBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        presetComboBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x441A9AF0));
        presetComboBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff1A9AF0));
        presetComboBox.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff191b1e));
        presetComboBox.setColour (juce::PopupMenu::textColourId, juce::Colours::white);
        presetComboBox.onChange = [this] {
            const int idx = presetComboBox.getSelectedItemIndex();
            if (idx >= 0 && idx < pluginPresetNames.size())
                applyPluginPresetByName (pluginPresetNames[idx]);
        };
        addAndMakeVisible (presetComboBox);

        refreshPluginPresetList();
    }

    // Standalone transport: a single Play/Pause toggle plus a "Return to start"
    // (rewind) button. The toggle shows the Play glyph when stopped and the Stop
    // glyph when playing (see syncTransportButtons).
    {
        auto playNorm = createDrawableSVG (svgPlay,  juce::Colours::grey);
        auto playOver = createDrawableSVG (svgPlay,  juce::Colours::lightgrey);
        auto playDown = createDrawableSVG (svgPlay,  juce::Colours::white);
        auto stopNorm = createDrawableSVG (svgStop,  ebs::accent());
        auto stopOver = createDrawableSVG (svgStop,  ebs::accent().brighter (0.2f));
        auto stopDown = createDrawableSVG (svgStop,  juce::Colours::white);
        // Normal images = Play, "on" images = Stop. Clicking does NOT auto-toggle
        // (we drive the displayed state from the processor in syncTransportButtons).
        playButton.setImages (playNorm.get(), playOver.get(), playDown.get(), nullptr,
                              stopNorm.get(), stopOver.get(), stopDown.get(), nullptr);
        playButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        playButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        playButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        playButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        playButton.onClick = [this] {
            processorRef.setTransportPlaying (! processorRef.isTransportPlaying());
            syncTransportButtons();
        };
        addAndMakeVisible (playButton);
    }

    setupIconButton(rewindButton, svgRewind, false, ovt::tr(ovt::Keys::kTooltipRewind));
    rewindButton.onClick = [this] {
        processorRef.resetTransportTime();
        if (curveEditor != nullptr)
        {
            curveEditor->clearInputTrace();
            curveEditor->returnToStart();
        }
    };

    setupIconButton(bypassButton, svgPower, true, "Bypass audio processing");
    bypassButton.setTooltip (ovt::tr(ovt::Keys::kTooltipBypassIcon));
    addAndMakeVisible (bypassButton);

    // MIDI Out icon (clicking toggles the attached toggle button)
    setupIconButton(midiOutButton, svgPower, true, "Enable MIDI Out");
    addAndMakeVisible (midiOutButton);

    // Menu button (gear icon for options)
    setupIconButton (menuButton, svgGear, false, "OpenVoxTuner options");

    // Toggle buttons with text (attached to parameters)
    bypassToggleButton.setButtonText (ovt::tr(ovt::Keys::kLabelBypassBtn));
    bypassToggleButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    bypassToggleButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    bypassToggleButton.setTooltip (ovt::tr(ovt::Keys::kTooltipBypass));
    addAndMakeVisible (bypassToggleButton);

    midiToggleButton.setButtonText (ovt::tr(ovt::Keys::kLabelMidiOutBtn));
    midiToggleButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    midiToggleButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    midiToggleButton.setTooltip (ovt::tr(ovt::Keys::kTooltipMidiOutIcon));
    addAndMakeVisible (midiToggleButton);

    // Hide legacy icon buttons from top bar (kept for backward compatibility)
    bypassButton.setVisible (false);
    midiOutButton.setVisible (false);

    // Debug window button near bypass
   #if JUCE_DEBUG
    debugWindowButton.setButtonText (ovt::tr(ovt::Keys::kLabelDebug));
    debugWindowButton.setTooltip (ovt::tr(ovt::Keys::kTooltipDebugWindow));
    addAndMakeVisible (debugWindowButton);
    debugWindowButton.onClick = [this]() {
        // Create a simple modeless window if not already
        if (debugWindow != nullptr)
        {
            debugWindow->setVisible (true);
            debugWindow->toFront (true);
            return;
        }

        class ClosableDebugWindow : public juce::DocumentWindow
        {
        public:
            ClosableDebugWindow (const juce::String& name, juce::Colour backgroundColour, int requiredButtons)
                : juce::DocumentWindow (name, backgroundColour, requiredButtons) {}

            void closeButtonPressed() override
            {
                setVisible (false);
            }
        };

        auto* w = new ClosableDebugWindow ("Harmony Debug",
                                           juce::Colours::black,
                                           juce::DocumentWindow::closeButton);

        // Content component
        class DebugContent : public juce::Component, public juce::Timer
        {
        public:
            DebugContent (OpenVoxTunerAudioProcessor& p) : proc (p)
            {
                attackSlider.setRange (0.0, 200.0, 1.0);
                attackSlider.setValue (5.0);
                releaseSlider.setRange (0.0, 1000.0, 1.0);
                releaseSlider.setValue (proc.getHarmonyOutputLevel() > 0.0f ? 80.0 : 80.0);

                addAndMakeVisible(attackSliderLabel);
                addAndMakeVisible(releaseSliderLabel);
                addAndMakeVisible(attackSlider);
                addAndMakeVisible(releaseSlider);

                attackSliderLabel.setText ("Attack ms", juce::dontSendNotification);
                releaseSliderLabel.setText ("Release ms", juce::dontSendNotification);

                applyButton.setButtonText ("Apply");
                addAndMakeVisible (applyButton);
                applyButton.onClick = [this] {
                    proc.setHarmonyEnvelopeTimes ((float) attackSlider.getValue(), (float) releaseSlider.getValue());
                };

                clearButton.setButtonText ("Force Clear");
                addAndMakeVisible (clearButton);
                clearButton.onClick = [this] {
                    proc.clearHarmonyCache();
                };

                dumpButton.setButtonText ("Dump VST3 Info");
                addAndMakeVisible (dumpButton);
                dumpButton.onClick = [this] {
                    proc.dumpVST3BundleInfo();
                };

                testGrainButton.setButtonText ("Test Grain");
                addAndMakeVisible (testGrainButton);
                // Attach the button to the debug parameter so the host will propagate the change
                testGrainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.getParameters(), "dbg_test_grain", testGrainButton);

                midiLog.setMultiLine(true);
                midiLog.setReadOnly(true);
                addAndMakeVisible(midiLog);

                startTimerHz (10);
            }
            ~DebugContent() override
            {
                stopTimer();
            }
            void resized() override
            {
                auto r = getLocalBounds().reduced(8);
                attackSliderLabel.setBounds (r.removeFromTop(18));
                attackSlider.setBounds (r.removeFromTop(24));
                releaseSliderLabel.setBounds (r.removeFromTop(18));
                releaseSlider.setBounds (r.removeFromTop(24));
                auto bottom = r.removeFromTop(28);
                applyButton.setBounds (bottom.removeFromLeft(80));
                clearButton.setBounds (bottom.removeFromLeft(90));
                dumpButton.setBounds (bottom.removeFromLeft(100));
                testGrainButton.setBounds (bottom.removeFromLeft(120));
                midiLog.setBounds (r);
            }
            void timerCallback() override
            {
                // update midi log
                juce::String s;
                for (int ch = 1; ch <= 9; ++ch)
                {
                    int note = proc.getLastSentMidiNoteForChannel(ch);
                    if (note >= 0) s += "Ch" + juce::String(ch) + ":" + juce::String(note) + "\n";
                }
                midiLog.setText(s, juce::dontSendNotification);
            }
        private:
            OpenVoxTunerAudioProcessor& proc;
            juce::Label attackSliderLabel, releaseSliderLabel;
            juce::Slider attackSlider, releaseSlider;
            juce::TextButton applyButton, clearButton;
            juce::TextButton dumpButton;
            juce::TextButton testGrainButton;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> testGrainAttachment;
            juce::TextEditor midiLog;
        };

        w->setContentOwned (new DebugContent (processorRef), true);
        w->setUsingNativeTitleBar (true);
        w->setResizable (true, false);
        w->centreWithSize (400, 320);
        w->setVisible (true);
        debugWindow.reset (w);
    };
   #else
    debugWindowButton.setVisible (false);
    debugWindowButton.setEnabled (false);
   #endif

    formantEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelFormantBtn));
    formantEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    formantEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    addAndMakeVisible (formantEnableButton);

    // Reverb controls (post-processing effect, same style as Formant)
    setupKnob (reverbMixSlider, &reverbMixLabel, "Mix");
    reverbMixSlider.setRange (0.0, 1.0, 0.01);
    reverbMixSlider.setEnabled (false); // disabled until reverb is toggled on
    // No value textbox; the live value is shown in JUCE's popup display while dragging.
    reverbMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    reverbMixSlider.setPopupDisplayEnabled (true, false, this);
    reverbMixSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

    reverbEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelReverbBtn));
    reverbEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    reverbEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    reverbEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipReverbEn));
    addAndMakeVisible (reverbEnableButton);

    // Noise Gate
    noiseGateEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelNoiseGate));
    noiseGateEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipNoiseGate));
    noiseGateEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    noiseGateEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    addAndMakeVisible (noiseGateEnableButton);

    noiseGateThresholdSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    noiseGateThresholdSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    noiseGateThresholdSlider.setPopupDisplayEnabled (true, false, this);
    noiseGateThresholdSlider.textFromValueFunction = [] (double v) { return juce::String (v, 0) + " dB"; };
    noiseGateThresholdSlider.setRange (-80.0, 0.0, 1.0);
    noiseGateThresholdSlider.setColour (juce::Slider::rotarySliderFillColourId, ebs::accent());
    noiseGateThresholdSlider.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    noiseGateThresholdSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    noiseGateThresholdSlider.setColour (juce::Slider::textBoxTextColourId, ebs::text());
    noiseGateThresholdSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    noiseGateThresholdSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    noiseGateThresholdSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipThreshold));
    addAndMakeVisible (noiseGateThresholdSlider);

    noiseGateThresholdLabel.setText (ovt::tr(ovt::Keys::kLabelThreshold), juce::dontSendNotification);
    noiseGateThresholdLabel.setColour (juce::Label::textColourId, ebs::text());
    noiseGateThresholdLabel.setJustificationType (juce::Justification::centred);
    noiseGateThresholdLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    noiseGateThresholdLabel.setVisible (false); // Hidden - same style as Formant/Reverb (no label)

    // Upward Compressor controls (input effect, before tuning).
    // Same visual style as the other effect knobs: power toggle on top, a single
    // "amount" knob with no visible value (only JUCE's drag popup shows it).
    upwardCompEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelUpwardComp));
    upwardCompEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    upwardCompEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    upwardCompEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipUpwardCompEn));
    addAndMakeVisible (upwardCompEnableButton);

    upwardCompAmountSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    upwardCompAmountSlider.setRange (0.0, 1.0, 0.01);
    upwardCompAmountSlider.setEnabled (false); // disabled until upward comp is toggled on
    // No value textbox; the live value is shown in JUCE's popup display while dragging.
    upwardCompAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    upwardCompAmountSlider.setPopupDisplayEnabled (true, false, this);
    upwardCompAmountSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    upwardCompAmountSlider.setColour (juce::Slider::rotarySliderFillColourId, ebs::accent());
    upwardCompAmountSlider.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    upwardCompAmountSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    upwardCompAmountSlider.setColour (juce::Slider::textBoxTextColourId, ebs::text());
    upwardCompAmountSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    upwardCompAmountSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    upwardCompAmountSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipUpwardCompAmount));
    addAndMakeVisible (upwardCompAmountSlider);

    // 2026-07-24 (Deprecation): FlexTune and Attack-Aware UI setup
    // is disabled. The slider/label members still exist (in
    // PluginEditor.h) for future re-implementation, but they are
    // not added to the visible UI and not attached to parameters.
    // The setupKnob calls are commented out to keep the code
    // available as reference.
    // ----------------------------------------------------------------
    // FlexTune / Humanize knobs
    // setupKnob (flexTuneSlider, &flexTuneLabel, "FlexTune");
    // translatableLabels.push_back ({ &flexTuneLabel, ovt::Keys::kLabelFlex    });
    // flexTuneSlider.setRange (0.0, 100.0, 1.0);
    // flexTuneSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipFlexTune));

    setupKnob (humanizeSlider, &humanizeLabel, "Humanize");
    translatableLabels.push_back ({ &humanizeLabel, ovt::Keys::kLabelHumanize    });
    humanizeSlider.setRange (0.0, 50.0, 1.0);
    humanizeSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipHumanize));
    // The live value is shown in JUCE's popup display while dragging (see below).

    // FlexTune and Humanize: no textbox, smaller inline labels
    // flexTuneLabel.setText ("Flex", juce::dontSendNotification);
    // flexTuneSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    // flexTuneSlider.setPopupDisplayEnabled (true, false, this);
    // flexTuneSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " cents"; };
    humanizeLabel.setText ("Humanize", juce::dontSendNotification);
    humanizeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    humanizeSlider.setPopupDisplayEnabled (true, false, this);
    humanizeSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " cents"; };

    // Vibrato preservation knob (0..1 -> displayed as 0..100 %).
    setupKnob (vibratoPreserveSlider, &vibratoPreserveLabel, "Vibrato");
    translatableLabels.push_back ({ &vibratoPreserveLabel, ovt::Keys::kLabelVibrato    });
    vibratoPreserveSlider.setRange (0.0, 1.0, 0.01);
    vibratoPreserveSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipVibrato));
    vibratoPreserveLabel.setText ("Vibrato", juce::dontSendNotification);
    vibratoPreserveSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    vibratoPreserveSlider.setPopupDisplayEnabled (true, false, this);
    vibratoPreserveSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };

    // 2026-07-24 (Deprecation): Attack release knob setup is disabled.
    // setupKnob (attackReleaseSlider, &attackReleaseLabel, "Attack Rel");
    // translatableLabels.push_back ({ &attackReleaseLabel, ovt::Keys::kLabelAttackRelease    });
    // attackReleaseSlider.setRange (10.0, 300.0, 1.0);
    // attackReleaseSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipAttackRelease));
    // attackReleaseLabel.setText ("Rel", juce::dontSendNotification);
    // attackReleaseSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    // attackReleaseSlider.setPopupDisplayEnabled (true, false, this);
    // attackReleaseSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };

    // Correction Mode toggle button
    correctionModeButton.setButtonText (ovt::tr(ovt::Keys::kLabelModernBtn));
    correctionModeButton.setClickingTogglesState (true);
    correctionModeButton.setColour (juce::TextButton::buttonColourId, ebs::accentSoft());
    correctionModeButton.setColour (juce::TextButton::buttonOnColourId, ebs::accent());
    correctionModeButton.setColour (juce::TextButton::textColourOffId, ebs::text());
    correctionModeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    correctionModeButton.setTooltip (ovt::tr(ovt::Keys::kTooltipCorrection));
    correctionModeButton.onClick = [this] {
        bool isTransparent = correctionModeButton.getToggleState();
        correctionModeButton.setButtonText (isTransparent ? ovt::tr(ovt::Keys::kLabelTransparentBtn) : ovt::tr(ovt::Keys::kLabelModernBtn));
    };
    addAndMakeVisible (correctionModeButton);

    // Graphic Modern/Transparent switch below the Speed/Amount knobs (console style).
    // Backed by correctionModeButton so undo + parameter sync are reused.
    modeSwitch = std::make_unique<CorrectionModeSwitch> (correctionModeButton);
    addAndMakeVisible (*modeSwitch);

    // Attack-Aware correction toggle button - power-icon style like Gate / Reverb / Formant.
    // 2026-07-24 (Deprecation): Attack-Aware button setup is disabled.
    // attackAwareButton.setButtonText (ovt::tr(ovt::Keys::kLabelAttackBtn));
    // attackAwareButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    // attackAwareButton.setTooltip (ovt::tr(ovt::Keys::kTooltipAttack));
    // addAndMakeVisible (attackAwareButton);

    addAndMakeVisible (scaleKeyboard);

    // === Bidirectional attachments to AudioParameters ===
    auto& tree = processorRef.getParameters();
    speedAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "speed",  speedSlider);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "amount", amountSlider);
    formantAttachment= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "formant", formantSlider);
    formantEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "formant_enable", formantEnableButton);
    // Attach the textual toggle buttons to parameters
    bypassToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "bypass", bypassToggleButton);
    midiToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "midi_out_enable", midiToggleButton);
    // also keep legacy names for backward compatibility
    bypassAttachment = nullptr;
    midiOutAttachment = nullptr;

    // Harmony attachments
    harmonyEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_enable", harmonyEnableButton);
    harmonyTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, "harmony_type", harmonyTypeBox);
    harmonyGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_gain", harmonyGainSlider);
    harmonyBlendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_blend", harmonyBlendSlider);
    harmonyAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_attack", harmonyAttackSlider);
    harmonyFollowLeadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_follow_lead", harmonyFollowLeadButton);
    harmonyGainMatchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_gain_match", harmonyGainMatchButton);
    harmonyFormantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_formant", harmonyFormantSlider);
    // Keep the Follow Lead and Gain Match sub-toggles in sync with the parent
    // Harmony switch. onStateChange fires on a direct click, so we re-run
    // refreshLabels() (which now also disables those two sub-toggles based on
    // the Harmony state). This guarantees the sub-toggles are greyed out the
    // instant Harmony is turned off, and stays consistent with preset load /
    // automation / language switch - all paths already call refreshLabels().
    harmonyEnableButton.onStateChange = [this] {
        refreshLabels();
    };
    useVoiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_use_voice", useVoiceButton);
    shiftedVoicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, "harmony_shifted_voices", shiftedVoicesBox);
    harmonyToneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, "harmony_tone", harmonyToneBox);
    harmonyToneColorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_tone_color", harmonyToneColorSlider);
    for (int i = 0; i < 12; ++i)
    {
        const juce::String id = "custom" + juce::String (i);
        customAttachments[i] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, id, scaleKeyboard.getButton(i));

        scaleKeyboard.getButton(i).onUserInteraction = [this] {
            // Switch to Custom mode silently: push the new scale value
            // through the AudioParameterChoice (NOT just the raw atomic
            // pointer), because the audio callback's syncParameters()
            // reads the AudioParameterChoice (line 2261 - `scaleChoiceParam
            // ->getIndex()`) when rebuilding the scale intervals. Storing
            // into the atomic alone leaves scaleChoiceParam pointing at the
            // previous preset, so on the next block syncParameters() pushes
            // the previous preset's intervals back into the quantizer and
            // the piano keyboard - and the toggle we just applied is
            // visually invisible (the user-reported bug on 2026-07-17:
            // "je clique sur la touche D => rien ne se passe (touche dans
            // la gamme précédente C Natural Minor)").
            //
            // setValueNotifyingHost fires valueChanged, which the
            // ComboBoxAttachment listens to in order to update the combo
            // display (no need for a separate setSelectedItemIndex call).
            // The scaleBox.onChange callback also fires; in Custom mode
            // (the target of this switch) it short-circuits immediately
            // ("Custom keeps the user's custom note flags" comment in
            // onChange), so the toggles we just set are NOT clobbered.
            auto* scaleParam = dynamic_cast<juce::AudioParameterChoice*>(
                processorRef.getParameters().getParameter ("scale"));
            if (scaleParam != nullptr)
            {
                const int numChoices = scaleParam->choices.size();
                const int customIdx  = numChoices - 1; // Custom is the last entry
                if (scaleParam->getIndex() != customIdx && numChoices > 1)
                {
                    const float normalized = static_cast<float> (customIdx)
                                           / static_cast<float> (numChoices - 1);
                    scaleParam->setValueNotifyingHost (normalized);
                }
            }
        };
    }

    // Key/scale ComboBox binding was already done above via ComboBoxAttachment.
    // === Pitch visualizer ===
    pitchVisualizer = std::make_unique<ui::PitchVisualizer>();
    pitchVisualizer->onRightClick = [this] {
        // Store the current mouse position so the wrench menu opens at the cursor.
        pendingMenuScreenPos = juce::Desktop::getInstance().getMousePosition();
        menuButton.triggerClick();
    };

    // Restore auto-center pitch preference from APVTS.
    if (auto* acParam = processorRef.getParameters().getParameter ("ui_auto_center"))
        pitchVisualizer->setAutoCenter (acParam->getValue() > 0.5f);

    // Restore visualizer zoom range from APVTS.
    {
        float fMinHz = 50.0f, fMaxHz = 1500.0f;
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmin"))
            fMinHz = p->convertFrom0to1 (p->getValue());
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmax"))
            fMaxHz = p->convertFrom0to1 (p->getValue());
        pitchVisualizer->setZoomRange (fMinHz, fMaxHz);
    }

    // Save visualizer zoom to APVTS when it changes.
    pitchVisualizer->onZoomChanged = [this] (float fMinHz, float fMaxHz) {
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmin"))
            p->setValueNotifyingHost (p->convertTo0to1 (fMinHz));
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmax"))
            p->setValueNotifyingHost (p->convertTo0to1 (fMaxHz));
    };

    curveEditor = std::make_unique<ui::PitchCurveEditor>();
    curveEditor->addListener (this);
    curveEditor->setViewRange (16.0, 50.0f, 1000.0f);
    curveEditor->onRightClick = [this] (const juce::MouseEvent& e) {
        showPresetsMenu (&e);
    };
    // Ruler-click seek: forward the requested playhead time to the transport.
    // In "Follow host" mode (VST3/AU, not ARA, not looping) the DAW owns the
    // timeline, so a ruler click cannot move the DAW playhead - seeking there
    // would only create a permanent offset that desyncs from the DAW loop.
    // We ignore the seek in that mode (the playhead snaps back to the DAW on
    // the next block). In Loop Playhead (Measures) / Standalone / ARA the seek
    // is honoured.
    curveEditor->onSeek = [this] (double t) {
#if OVT_ARA_ENABLED
        if (processorRef.isBoundToARA())
            processorRef.seekToTime (t);          // ARA: host follows the plug-in
        else
#endif
        if (processorRef.isPlayheadLooping())
            processorRef.seekToTime (t);          // Loop / Standalone: local timeline
        // else: Follow host -> DAW is master, ignore (no desync)
    };

    // Initialize tabs
      tabbedComponent.setOutline(0);
      // Tab content areas always use dark background regardless of theme
      const juce::Colour tabContentColour = juce::Colour (0xff14151c);
      tabbedComponent.addTab(ovt::tr(ovt::Keys::kTabLive), tabContentColour, pitchVisualizer.get(), false);
      tabbedComponent.addTab(ovt::tr(ovt::Keys::kTabCurveEditor), tabContentColour, curveEditor.get(), false);
    addAndMakeVisible(tabbedComponent);
    tabbedComponent.setColour (juce::TabbedComponent::backgroundColourId, ebs::bgDark());
    tabbedComponent.setColour (juce::TabbedComponent::outlineColourId, ebs::accentSoft());

    // Replace the standard tab bar with an iPhone-style animated pill switch.
    tabbedComponent.getTabbedButtonBar().setVisible (false);
    tabSwitch = std::make_unique<TabSwitch> (tabbedComponent);
    addAndMakeVisible (*tabSwitch);

    // Initialize piano keyboards with chromatic intervals so they display
    // all keys on first paint, before the first timerCallback fires.
    {
        juce::Array<int> chromatic;
        for (int i = 0; i < 12; ++i) chromatic.add (i);
        pitchVisualizer->setScaleIntervals (chromatic);
        curveEditor->setScaleIntervals (chromatic);
        scaleKeyboard.setActiveScaleIntervals (chromatic);
    }

    // Make sure tools are drawn over the tabbed component
    optionsButton.toFront(false);
    snapButton.toFront(false);
    snapGridButton.toFront(false);
    stepModeButton.toFront(false);
    zoomInButton.toFront(false);
    zoomOutButton.toFront(false);
    scrollUpButton.toFront(false);
    scrollDownButton.toFront(false);
    resetViewButton.toFront(false);
    playButton.toFront(false);
    rewindButton.toFront(false);
    measuresLabel.toFront(false);
    measuresComboBox.toFront(false);

    // Add overlay LAST so it renders on top of all other components
    addAndMakeVisible (helpOverlay);

    // Force curve sync on first timer tick (covers both setStateInformation
    // before/after createEditor, and editor recreation on UI show/hide).
    processorRef.getPendingCurveRestore().store (true);
    
    // Sync immediately so the curve is correct on the very first paint
    // (avoids a 33ms flash of the "default" preset).
    if (curveEditor != nullptr)
    {
        curveEditor->setCurve (processorRef.getPitchCurve());
        processorRef.getPendingCurveRestore().store (false);
    }
    // Restore A/B slots from the processor (persisted across project reload).
    {
        auto restoreSlot = [this] (ABState& slot, int slotIdx)
        {
            if (! processorRef.hasAbSlotData (slotIdx)) return;
            const auto* ms = processorRef.getAbSlotMorphState (slotIdx);
            if (ms == nullptr) return;

            // Backward-compatibility: remap old Vocoder-like index (5) to Organ (1)
            auto remapTone = *ms;
            if (remapTone.harmonyTone == 5) remapTone.harmonyTone = 1;
            // Old indices 1..4 (Bright/SynthLead/Strings/Guitar) were removed -
            // silently keep them as-is (will map to wrong preset, but user will notice).

            slot.morphState = std::make_unique<ovtdsp::MorphState> (remapTone);
            slot.hasData = true;
            slot.name = "filled";
        };

        restoreSlot (slotA, 0);
        restoreSlot (slotB, 1);
        // Restore the active slot so the editor reopens on the same slot the
        // user left. The main APVTS holds that slot's values, so keeping the
        // active-slot flag in sync prevents slot A from appearing to contain
        // slot B's content after a restart.
        isSlotAActive = (processorRef.getAbActiveSlot() == 0);
        // Snap the live parameters to the ACTIVE slot's saved MorphState ONLY
        // when the restored state is a "frozen morph" (morph_amount mid-range).
        // Otherwise the global parameters restored from `.settings` are
        // authoritative and MUST NOT be overwritten by the active slot's values
        // — otherwise every global change (Vibrato, Humanize, harmony type /
        // formant) made before quitting is silently lost on restart.
        const float restoredMorph = processorRef.getMorphAmount();
        const bool frozenMorph = (restoredMorph > 0.02f && restoredMorph < 0.98f);
        {
            auto& activeSlot = isSlotAActive ? slotA : slotB;
            if (frozenMorph && activeSlot.hasData && activeSlot.morphState != nullptr)
                loadSlot (activeSlot);
        }
        processorRef.setMorphAmount (isSlotAActive ? 0.0f : 1.0f);
        lastMorphValue = isSlotAActive ? 0.0f : 1.0f;
        updateABButtonStates();
    }
    
    // Initialize tab from processor parameter
    float initialMode = processorRef.getParameters().getParameter("mode")->getValue();
    tabbedComponent.setCurrentTabIndex(initialMode > 0.5f ? 1 : 0);
    
    // Tab change callback
    // Parameter update is done in timerCallback() to avoid complex loops.

    // Default size: AFTER creating children, otherwise setSize triggers
    // resized() which accesses pitchVisualizer/curveEditor still nullptr.
    // Min size only: no max size constraint so the plugin can use all
    // available space in ARA mode (DAW may allocate arbitrarily large views).
    setSize (900, 650);
    setResizable (true, true);
    setResizeLimits (1100, 600, 99999, 99999);

    // Refresh labels with current language translations
    refreshLabels();

    // === Global plugin Undo/Redo (Option 1) ===
    // Seed the live snapshot with the current state and wire the gesture
    // listeners. The UndoManager itself lives in the processor.
    undoLiveSnapshot_ = processorRef.getParameters().copyState();
    registerUndoGestureListeners();

    // Timer to update the visualizer (~30 fps).
    startTimerHz (30);
}

OpenVoxTunerAudioProcessorEditor::~OpenVoxTunerAudioProcessorEditor()
{
    if (updateCheckState != nullptr)
        updateCheckState->cancelled.store (true);

    // Signal the processor to stop touching the AU playhead BEFORE we
    // stop our own timer.  A MessageQueue timer callback may already be
    // queued (it is dispatched asynchronously), so without this flag the
    // callback could reach updateHostTransport() -> getPlayHead()->
    // getPosition() after the AU's internal state has been torn down,
    // crashing the host (Live 12 AU, macOS 26).
    processorRef.notifyEditorShuttingDown();

    stopTimer();

    // These are independent top-level Desktop components. Hide and destroy
    // them before the editor's child components and LookAndFeel are torn down.
    midiImportFileChooser.reset();
    if (debugWindow != nullptr)
    {
        debugWindow->setVisible (false);
        debugWindow->setContentOwned (nullptr, true);
        debugWindow.reset();
    }
    if (galleryWindow != nullptr)
    {
        galleryWindow->setVisible (false);
        galleryWindow->setContentOwned (nullptr, true);
        galleryWindow.reset();
    }

    // -------------------------------------------------------------------
    // Clear LookAndFeel references BEFORE the customLookAndFeel member is
    // destroyed (declaration order in the header puts it first in the
    // private section, so C++ destroys it LAST after this destructor
    // returns). Every child Component that was added to the editor or
    // to any of its sub-components received a LookAndFeel propagation
    // when addAndMakeVisible() was called: JUCE walks the parent's
    // lookAndFeel weak-ref down to each new child. Those weak-refs
    // remain in the child even if we setLookAndFeel(nullptr) on the
    // parent alone - each Component owns its own LookAndFeel pointer.
    // We therefore must clear them recursively, in the REVERSE order
    // of the visual hierarchy (most-deeply-nested children first), so
    // that by the time customLookAndFeel's destructor runs (after the
    // implicit member-destruction phase that follows this body), no
    // surviving weak-ref still points at the (about-to-die) object.
    // Otherwise the JUCE leak/assertion detector in Debug mode reports
    // `refCount.value = 2` and aborts.
    // -------------------------------------------------------------------

    // Drop the LookAndFeel on the deepest children first (Slider, Label,
    // Button, ComboBox, custom widgets...). This is done via a recursive
    // lambda so any future component added to the editor is covered
    // without needing to update this list.
    // juce::Component exposes its children via the `getChildren()` range
    // (Component::Children), which is a safe iterable even during
    // teardown (it skips children being deleted).
    std::function<void(juce::Component&)> clearLookAndFeelRecursive =
        [&clearLookAndFeelRecursive](juce::Component& c)
    {
        // Recurse first (post-order: deepest first), then null out the
        // LookAndFeel on the current component. This ensures children
        // are fully released before their parent.
        for (auto* child : c.getChildren())
            clearLookAndFeelRecursive (*child);
        c.setLookAndFeel (nullptr);
    };
    clearLookAndFeelRecursive (*this);

    // Now safe to release the explicit teardown of components that own
    // resources beyond the LookAndFeel pointer.
    if (tooltipWindow != nullptr)
    {
        tooltipWindow->setLookAndFeel (nullptr);
        tooltipWindow.reset();
    }
    if (curveEditor != nullptr)
    {
        curveEditor->removeListener();
        curveEditor->setLookAndFeel (nullptr);
    }
    setLookAndFeel (nullptr);
}

void OpenVoxTunerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Main background (flat, no gradient)
    g.setColour (ebs::bgDark());
    g.fillAll();

    // Draw the bottom blocks backgrounds (left = scale, middle = knobs, right = harmony)
    g.setColour (ebs::bgPanel());
    g.fillRoundedRectangle (block1Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block2Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block3Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block4Bounds.toFloat(), 6.0f);

    g.setColour (ebs::accentSoft().withAlpha(0.3f));
    g.drawRoundedRectangle (block1Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block2Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block3Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block4Bounds.toFloat(), 6.0f, 1.0f);

    // Top banner with title (opaque, matches tab bar background).
    g.setColour (ebs::bgDark());
    g.fillRect (0, 0, getWidth(), 50);

    // --- LOGO DRAWING (assets/icon.svg) ---
    juce::Rectangle<float> logoArea (20.0f, 10.0f, 30.0f, 30.0f);
    if (auto* logo = getBrandLogo())
        logo->drawWithin (g, logoArea, juce::Justification::centred, 1.0f);

    // --- TITLE TEXT ---
    // Drawn with GlyphArrangement so "OpenVox" (accent) and "Tuner" (white) sit
    // flush together with no measurement/truncation mismatch. The previous code
    // measured each word's width, cast it to int, and passed it to drawText, which
    // then added an ellipsis ("OpenV...Tun...") because the box was 1px too narrow.
    const juce::Font titleFont = ebs::fontTitle();
    const float titleY = 8.0f;
    // Vertically centre the title against the logo (logoArea centre Y).
    const float baseline = logoArea.getCentreY()
                         + (titleFont.getAscent() - titleFont.getDescent()) * 0.5f;
    float x = 60.0f;

    juce::GlyphArrangement ga;
    ga.addLineOfText (titleFont, "OpenVox", x, baseline);
    g.setColour (ebs::accent());
    ga.draw (g);
    x += juce::GlyphArrangement::getStringWidth (titleFont, "OpenVox");

    ga.clear();
    ga.addLineOfText (titleFont, "Tuner", x, baseline);
    g.setColour (ebs::text());
    ga.draw (g);
    x += juce::GlyphArrangement::getStringWidth (titleFont, "Tuner");

    // Version string (debug builds only - hides the version number in release).
#if defined (JUCE_DEBUG)
    g.setColour (ebs::textDim());
    g.setFont (ovt::fontVersion());
#  if defined (JucePlugin_VersionString)
    g.drawText (juce::String ("v") + JucePlugin_VersionString, x + 8, titleY, 160, 36, juce::Justification::left);
#  elif defined (JucePlugin_Version)
    g.drawText (juce::String ("v") + juce::String (JucePlugin_Version), x + 8, titleY, 160, 36, juce::Justification::left);
#  else
    g.drawText ("v0.1.1", x + 8, titleY, 160, 36, juce::Justification::left);
#  endif
#endif

    // CPU usage meter (debug builds only). Sits to the LEFT of the A/B slot
    // buttons in the top banner, so it is out of the way of the preset
    // selector and the version string. Hidden entirely in release builds.
#if defined (JUCE_DEBUG)
    {
        const int cpuW = 64;
        const int cpuH = 16;
        const int cpuGap = 8;
        // Anchor to the right edge of button A (buttonA has valid bounds once
        // resized() has run, which always precedes paint()).
        const int cpuX = buttonA.getX() - cpuW - cpuGap;
        const int cpuY = buttonA.getY() + (buttonA.getHeight() - cpuH) / 2;

        const int cpuPct = static_cast<int> (currentCpuUsage * 100.0f);
        const juce::String cpuText = ovt::tr(ovt::Keys::kLabelCpu) + juce::String (cpuPct) + "%";

        // Background
        g.setColour (ebs::cpuBg());
        g.fillRoundedRectangle ((float) cpuX, (float) cpuY, (float) cpuW, (float) cpuH, 4.0f);

        // Colour based on usage
        juce::Colour cpuColour;
        if (cpuPct < 30)      cpuColour = juce::Colour (0xff4caf50);
        else if (cpuPct < 60) cpuColour = juce::Colour (0xffffc107);
        else if (cpuPct < 85) cpuColour = juce::Colour (0xffff9800);
        else                  cpuColour = juce::Colour (0xfff44336);

        g.setColour (cpuColour);
        const int barW = static_cast<int> ((cpuW - 8) * juce::jmin (1.0f, currentCpuUsage));
        g.fillRoundedRectangle ((float) (cpuX + 4), (float) (cpuY + cpuH - 4),
                                (float) barW, 2.0f, 1.0f);

        g.setColour (ebs::cpuText());
        g.setFont (ovt::fontLegendHint());
        g.drawText (cpuText, cpuX, cpuY, cpuW, cpuH, juce::Justification::centred);
    }
#endif

    // === Chord badge (ARA or live ovtchord) ===
    // A small pill shown when the host provides lead-sheet chord info via ARA,
    // or when real-time chord detection (ovtchord, MIDI/sidechain) is active in
    // non-ARA mode. Anchored between the preset selector and the A/B cluster.
    {
        juce::String badgeText;
        bool outOfScale = false;
        bool showBadge = false;

        if (processorRef.isBoundToARA_custom())
        {
            int araRoot = -999, araBass = -999;
            juce::String araChordName;
            processorRef.getAraChordAt (processorRef.getInterpolatedTransportTime(), araRoot, araBass, araChordName);
            if (araRoot != -999)
            {
                auto cofToPc = [] (int cof) { return ((cof * 7) % 12 + 12) % 12; };
                // Show just the chord symbol (e.g. "C7", "Csus4"); fall back to
                // root (+bass) if no name could be derived.
                if (araChordName.isNotEmpty())
                    badgeText = araChordName;
                else
                {
                    badgeText = ovtdsp::noteInOctaveName (cofToPc (araRoot));
                    if (araBass != -999 && araBass != araRoot)
                        badgeText += "/" + juce::String (ovtdsp::noteInOctaveName (cofToPc (araBass)));
                }
                outOfScale = processorRef.isAraChordOutOfScale (processorRef.getInterpolatedTransportTime());
                showBadge = true;
            }
        }
        else
        {
            // Non-ARA: show the live chord detected by ovtchord (MIDI/sidechain).
            processorRef.getLiveChord (badgeText, outOfScale);
            showBadge = badgeText.isNotEmpty();
        }

        // When the chord contains notes outside the selected scale, the
        // chord-context override widens the allowed notes (bypass). Show that
        // with a warning colour instead of the normal accent.
        if (showBadge && presetSaveButton.getRight() > 0)
        {
            const juce::Colour badgeColour = outOfScale ? juce::Colour (0xffffb300) : ebs::accent();

            const juce::Font badgeFont = ovt::fontLegendHint();
            const int badgeH = 22;
            const int badgeW = (int) juce::GlyphArrangement::getStringWidth (badgeFont, badgeText) + 18;
            const int badgeX = presetSaveButton.getRight() + 8;
            const int badgeY = 14;
            const juce::Rectangle<int> badgeRect (badgeX, badgeY, badgeW, badgeH);

            g.setColour (badgeColour.withAlpha (0.18f));
            g.fillRoundedRectangle (badgeRect.toFloat(), badgeH * 0.5f);
            g.setColour (badgeColour.withAlpha (0.7f));
            g.drawRoundedRectangle (badgeRect.toFloat(), badgeH * 0.5f, 1.0f);
            g.setColour (badgeColour);
            g.setFont (badgeFont);
            g.drawText (badgeText, badgeRect, juce::Justification::centred);
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // === Top banner (title + hamburger menu) ===
    auto titleArea = bounds.removeFromTop (50);

    // Hamburger menu button in the top-right corner.
    const int menuW = 48;
    auto menuArea = titleArea.removeFromRight (menuW).reduced (6, 10);
    menuButton.setBounds (menuArea);

    // Layout: [buttonA] [morphSlider] [buttonB] [undo] [redo] [menuButton] in
    // the top-right. The global Undo/Redo icon buttons sit between button B
    // and the Options gear.
    const int btnSize = 28;
    const int btnGap = 4;
    const int morphW = 80;

    // Position from right to left: menu, redo, undo, B, morph, A
    int x = menuArea.getX() - btnSize - btnGap; // redo
    redoButton.setBounds (x, 11, btnSize, btnSize);
    x -= btnSize + btnGap;                       // undo
    undoButton.setBounds (x, 11, btnSize, btnSize);
    x -= btnSize + btnGap;                       // buttonB
    buttonB.setBounds (x, 11, btnSize, btnSize);
    x -= morphW + btnGap;
    morphSlider.setBounds (x, 13, morphW, btnSize - 4);
    morphSliderLabel.setBounds (x, 2, morphW, 10);
    x -= btnSize + btnGap;
    buttonA.setBounds (x, 11, btnSize, btnSize);

    // === Plugin Preset selector (separate from Curve Presets) ===
    // Centred in the top banner, between the left edge of the title area and
    // the right-hand control cluster (button A / morph / B / undo / redo /
    // gear). Layout: [<] [Combo: name] [>] [save].
    {
        const int selBtn = 24;
        const int selGap = 4;
        const int comboW = 180;
        const int totalW = selBtn + selGap + comboW + selGap + selBtn + selGap + selBtn;
        const int bannerLeft  = titleArea.getX();
        const int bannerRight = x; // right edge of buttonA cluster
        const int availW = bannerRight - bannerLeft;
        int selX = bannerLeft + juce::jmax (0, (availW - totalW) / 2);
        const int selY = 13;

        presetPrevButton.setBounds (selX, selY, selBtn, selBtn);
        selX += selBtn + selGap;
        presetComboBox.setBounds (selX, selY, comboW, selBtn);
        selX += comboW + selGap;
        presetNextButton.setBounds (selX, selY, selBtn, selBtn);
        selX += selBtn + selGap;
        presetSaveButton.setBounds (selX, selY, selBtn, selBtn);
    }

    // Hide all old top-bar controls (they still work via attachments/handlers).
    latencyModeLabel.setBounds (0, 0, 0, 0);
    latencyModeBox.setBounds (0, 0, 0, 0);
    detectorLabel.setBounds (0, 0, 0, 0);
    detectorBox.setBounds (0, 0, 0, 0);
    midiToggleButton.setBounds (0, 0, 0, 0);
    bypassToggleButton.setBounds (0, 0, 0, 0);
    bypassButton.setBounds (0, 0, 0, 0);
    midiOutButton.setBounds (0, 0, 0, 0);
    updateButton.setBounds (0, 0, 0, 0);
    debugWindowButton.setBounds (0, 0, 0, 0);

    // === Visualizer (top) and Graphic Editor (middle) ===
    const int pad = 10;
    // Reserve the bottom area for the control blocks (Speed/Amount + Effects +
    // Scale/Keyboard + Harmony). The Correction block no longer needs extra
    // height: its advanced knobs (Flex / Humanize / Vibrato / Attack-Aware) are
    // revealed to the SIDE via the "Advanced" banner, so we keep the bottom
    // strip compact.
    auto centerArea = bounds.removeFromTop (bounds.getHeight() - 190);
    auto tabArea = centerArea.reduced (pad);
    // Reduce the gap between the title banner and the Live/Curve Editor tabs.
    tabArea.setY (centerArea.getY() + 4);
    tabbedComponent.setBounds (tabArea);
    
    // Graphic Mode specific tools aligned over the tab bar row.
    auto tabBounds = tabbedComponent.getBounds();
    auto toolsArea = tabBounds.removeFromTop(30).reduced(2, 4); // height is 22

    // iPhone-style Live / Curve Editor tab switch, placed on the left of the
    // toolbar row instead of the old JUCE TabbedButtonBar. Same height as the
    // Modern/Transparent switch below the Speed/Amount knobs.
    if (tabSwitch != nullptr)
    {
        const int switchH = 30;
        // Width is computed from the translated labels so the longest
        // translation of "Curve Editor" (FR "Editeur de courbe",
        // DE "Kurveneditor", ES "Editor de curva", JP/CN full-width)
        // never gets truncated - the Live/Curve tabs are critical
        // navigation, so we widen the pill rather than shrink the font.
        const int switchW = TabSwitch::getPreferredWidth (static_cast<float> (switchH));
        auto switchRect = toolsArea.removeFromLeft (switchW);
        switchRect = switchRect.withSizeKeepingCentre (switchW, switchH);
        tabSwitch->setBounds (switchRect);
        toolsArea.removeFromLeft (6); // gap
    }

    int iconSize = toolsArea.getHeight(); // 22

    // Toolbar left group (curve-editor mode): standalone transport + measures.
    const bool isStandalone = processorRef.isStandaloneWrapper();
    const int leftGap = 8;
    if (isStandalone)
    {
        // Standalone transport: a "Return to start" (rewind) button placed
        // before the Play/Pause toggle, matching the classic DAW order.
        rewindButton.setBounds (toolsArea.removeFromLeft(iconSize));
        toolsArea.removeFromLeft(4);
        playButton.setBounds (toolsArea.removeFromLeft(iconSize));
        toolsArea.removeFromLeft(leftGap);
    }
    // "Measures" control (number of measures shown in the curve editor time window).
    // Kept on the toolbar row so it no longer covers the ruler.
    const int measuresLabelW = static_cast<int> (juce::GlyphArrangement::getStringWidth (measuresLabel.getFont(), measuresLabel.getText())) + 10;
    measuresLabel.setBounds (toolsArea.removeFromLeft (measuresLabelW));
    toolsArea.removeFromLeft(4);
    measuresComboBox.setBounds (toolsArea.removeFromLeft (56));
    toolsArea.removeFromLeft(leftGap);

    // Toolbar (right to left): Options | Gallery | zoom / scroll / reset | snap / grid / step
    // Options and Gallery are icon-only buttons; leave a clear gap before
    // the reset (X) button so they don't visually touch.
    optionsButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    presetGalleryButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);

    resetViewButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    scrollDownButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    scrollUpButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    zoomOutButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    zoomInButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);

    stepModeButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    snapGridButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    snapButton.setBounds (toolsArea.removeFromRight(iconSize));

    // === Bottom bar: blocks ===
    auto bottomArea = bounds.reduced (pad);

    // Block widths. The Correction block shows Speed + Amount by default and
    // WIDENS when the "Advanced" banner is expanded to reveal the correction
    // knobs (Flex / Humanize / Vibrato / Attack-Aware) on the right.
    const int knobBlockWidth   = 220;  // Speed + Amount (prominent big knobs)
    const int advancedWidth    = 160;  // revealed correction knobs when expanded
    const int effectBlockWidth = 200;
    const int scaleBlockWidth  = 236;  // narrowed to fit the Root combo (see Block 1)
    const int blockSpacing     = 10;
    const int bannerW          = 22;   // vertical "Advanced" banner width

    // Clamp the Correction width so the other blocks always keep some room.
    const int fixedOthers   = effectBlockWidth + scaleBlockWidth + 3 * blockSpacing;
    const int availForBlock = juce::jmax (knobBlockWidth,
                                          bottomArea.getWidth() - fixedOthers);
    const int correctionWidth = juce::jmin (knobBlockWidth + (advancedExpanded ? advancedWidth : 0),
                                            availForBlock);

    auto leftBlock = bottomArea.removeFromLeft (correctionWidth);
    bottomArea.removeFromLeft (blockSpacing);
    auto effectBlock = bottomArea.removeFromLeft (effectBlockWidth);
    bottomArea.removeFromLeft (blockSpacing);
    auto middleBlock = bottomArea.removeFromLeft (scaleBlockWidth);
    bottomArea.removeFromLeft (blockSpacing);
    auto rightBlock = bottomArea; // remaining (Harmony)

    block2Bounds = leftBlock;     // Correction: Speed, Amount (+ advanced knobs)
    block4Bounds = effectBlock;   // Effects: Gate, Reverb, Formant
    block1Bounds = middleBlock;   // Key, Scale, Keyboard
    block3Bounds = rightBlock;    // Harmony controls

    // --- Block 2 : Correction (Speed + Amount, optionally Advanced knobs) ---
    auto b2 = block2Bounds.reduced (10);

    // "Advanced" vertical banner pinned to the right edge of the (possibly
    // widened) Correction block. Clicking it expands / collapses the block. It
    // is a plain rectangular handle inset from the block's rounded corners.
    auto bannerRect = block2Bounds.withTrimmedLeft (block2Bounds.getWidth() - bannerW);
    // Make the reactive (clickable) zone half the block height and round its
    // corners so it reads as a clean handle, not a full-height grip.
    const int halfTrim = static_cast<int> (block2Bounds.getHeight() * 0.25f);
    bannerRect = bannerRect.withTrimmedTop (halfTrim).withTrimmedBottom (halfTrim);
    advancedButton.setBounds (bannerRect);

    // Content area excludes the banner strip on the right.
    auto content = b2.withTrimmedRight (bannerW);

    // Speed + Amount: the two prominent big knobs on the left. A strip is
    // reserved at the bottom of this area for the Modern/Transparent graphic
    // switch, which shrinks the two knobs vertically.
    const int baseContentW = knobBlockWidth - bannerW - 20;  // width reserved for Speed + Amount
    auto baseArea = content.removeFromLeft (baseContentW);
    auto advancedArea = content;                              // Flex / Humanize / Vibrato / Attack

    // Reserve the switch strip at the bottom of the base area, then let the
    // knobs fill the remaining top space (so they get smaller).
    const int switchH   = 30;
    const int switchGap = 8;
    auto switchRect = baseArea.removeFromBottom (switchH);
    baseArea.removeFromBottom (switchGap);

    const int knobPadding = 6;
    const int bigHalf = (baseArea.getWidth() - knobPadding) / 2;

    auto bSpeed = baseArea.removeFromLeft (bigHalf);
    speedLabel.setBounds (bSpeed.removeFromTop (18));
    speedSlider.setBounds (bSpeed);
    baseArea.removeFromLeft (knobPadding);

    auto bAmount = baseArea;
    amountLabel.setBounds (bAmount.removeFromTop (18));
    amountSlider.setBounds (bAmount);

    // Modern / Transparent studio-console switch, spanning the base area width.
    if (modeSwitch != nullptr)
        modeSwitch->setBounds (switchRect);

    // Advanced correction knobs (revealed when the block is expanded). They are
    // compact, laid out in a 2x2 grid that FILLS the advanced area width (no empty
    // side margins, no overlap), and have NO value textbox (value shows in the
    // popup while dragging), so the "Advanced" zone stays small.
    const int advLabelH = 13;
    const int advGapX   = 8;    // gap between the two columns
    const int advGapY   = 6;    // gap between the rows
    const int knobMax   = 52;   // cap so advanced knobs stay smaller than Speed/Amount
    const int advInsetR = 5;    // pixels to shift advanced content away from main block

    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> cell)
    {
        l.setBounds (cell.removeFromTop (advLabelH));
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setPopupDisplayEnabled (true, false, this);
        const int d = juce::jmin (cell.getWidth() - 4, cell.getHeight() - 4, knobMax);
        const int kx = cell.getX() + (cell.getWidth() - d) / 2;
        const int ky = cell.getY() + (cell.getHeight() - d) / 2;
        s.setBounds (kx, ky, d, d);
    };

    if (advancedArea.getWidth() > 4)
    {
        // 2026-07-27 (Layout update with Voice Type selector): the advanced
        // area now uses 2 rows: row 1 = Vibrato + Humanize side-by-side
        // (2 small knobs centered), row 2 = Voice Type combo box (full width).
        // This matches the proposal in docs/voice-type-feasibility-report.md.

        // Shift all content 5px right to create breathing room from the main block.
        advancedArea.removeFromLeft (advInsetR);

        // Row 1: Vibrato + Humanize (2 columns).
        const int advRow1H = (int)((advancedArea.getHeight() - 2 * advGapY) * 0.42f);
        auto row1Area = advancedArea.removeFromTop (advRow1H);
        advancedArea.removeFromTop (advGapY);

        const int colW = (row1Area.getWidth() - advGapX) / 2;
        auto vibCell   = row1Area.removeFromLeft (colW);
        row1Area.removeFromLeft (advGapX);
        auto humanCell = row1Area;

        placeKnob (vibratoPreserveSlider, vibratoPreserveLabel, vibCell);
        placeKnob (humanizeSlider,         humanizeLabel,        humanCell);
        // 2026-07-24 (Deprecation): FlexTune and Attack-Aware knobs
        // are no longer placed in the visible UI. Their slider/label
        // members are still kept (in PluginEditor.h) for future
        // re-implementation, but the placeKnob calls are commented
        // out so the two visible knobs (Vibrato, Humanize) take the
        // full advanced area in a 1x2 column.

        // Row 2: Voice Type label + combo. Narrower combos centered in the
        // advanced area so they stay away from the main (Speed/Amount) block.
        const int comboW  = 130;   // narrower than full area width (180)
        const int comboL  = (int)((advancedArea.getWidth() - comboW) / 2); // centered
        advancedArea.removeFromLeft (comboL);
        voiceTypeLabel.setBounds (advancedArea.removeFromTop (advLabelH));
        voiceTypeBox.setBounds (advancedArea.removeFromTop (22).withWidth (comboW));
        // Adjust remaining area width to keep row 3 aligned.
        advancedArea.removeFromLeft (advancedArea.getWidth() - comboW - comboL);
        advancedArea.removeFromTop (advGapY);

        // Row 3: Formant Strategy label + combo.
        formantStrategyLabel.setBounds (advancedArea.removeFromTop (advLabelH));
        formantStrategyBox.setBounds (advancedArea.removeFromTop (22).withWidth (comboW));
    }
    else
    {
        // Collapsed: hide the advanced knobs so they don't overlap other blocks.
        // 2026-07-24 (Deprecation): FlexTune bounds set to (0,0,0,0) is
        // no longer needed (not visible), but kept for safety.
        flexTuneSlider.setBounds (0, 0, 0, 0);   flexTuneLabel.setBounds (0, 0, 0, 0);
        humanizeSlider.setBounds (0, 0, 0, 0);   humanizeLabel.setBounds (0, 0, 0, 0);
        vibratoPreserveSlider.setBounds (0, 0, 0, 0); vibratoPreserveLabel.setBounds (0, 0, 0, 0);
        // 2026-07-24 (Deprecation): Attack-Aware bounds no longer needed
        // (not visible), but kept for safety.
        attackReleaseSlider.setBounds (0, 0, 0, 0);       attackAwareButton.setBounds (0, 0, 0, 0);
        // Voice Type is hidden when Advanced is collapsed
        voiceTypeBox.setBounds (0, 0, 0, 0);
        voiceTypeLabel.setBounds (0, 0, 0, 0);
        // Formant Strategy is hidden when Advanced is collapsed
        formantStrategyBox.setBounds (0, 0, 0, 0);
        formantStrategyLabel.setBounds (0, 0, 0, 0);
    }

    // --- Block 4 : Effects - 2 rows (Gate + Reverb on top, Formant below) ---
    // The effect knobs use a FIXED, modest size (they are secondary controls, not
    // the primary Speed / Amount knobs) so they no longer stretch to fill the row
    // height. The value is shown in the popup display while dragging (no textbox).
    auto b4 = block4Bounds.reduced (10);
    const int effectPowerH = 18;
    const int effKnobD     = 54;
    const int rowGap       = 8;
    const int rowH         = (b4.getHeight() - rowGap) / 2;

    // Row 1 : Noise Gate + Upward Comp (two columns)
    auto row1 = b4.removeFromTop (rowH);
    b4.removeFromTop (rowGap);
    const int effectColW = (row1.getWidth() - 8) / 2;

    // Noise Gate (column 1): power toggle on top, fixed-size knob below
    auto gateCol = row1.removeFromLeft (effectColW);
    noiseGateEnableButton.setBounds (gateCol.removeFromTop (effectPowerH));
    {
        const int kx = gateCol.getX() + juce::jmax (0, (gateCol.getWidth() - effKnobD) / 2);
        noiseGateThresholdSlider.setBounds (kx, gateCol.getY() + 2, effKnobD, effKnobD);
    }
    row1.removeFromLeft (8);

    // Upward Comp (column 2): power toggle on top, fixed-size knob below
    auto ucCol = row1;
    upwardCompEnableButton.setBounds (ucCol.removeFromTop (effectPowerH));
    {
        const int kx = ucCol.getX() + juce::jmax (0, (ucCol.getWidth() - effKnobD) / 2);
        upwardCompAmountSlider.setBounds (kx, ucCol.getY() + 2, effKnobD, effKnobD);
    }

    // Row 2 : Reverb + Formant (two columns)
    auto row2 = b4;
    const int effectColW2 = (row2.getWidth() - 8) / 2;

    // Reverb (column 1): power toggle on top, fixed-size knob below
    auto reverbCol = row2.removeFromLeft (effectColW2);
    reverbEnableButton.setBounds (reverbCol.removeFromTop (effectPowerH));
    {
        const int kx = reverbCol.getX() + juce::jmax (0, (reverbCol.getWidth() - effKnobD) / 2);
        reverbMixSlider.setBounds (kx, reverbCol.getY() + 2, effKnobD, effKnobD);
    }
    row2.removeFromLeft (8);

    // Formant (column 2): power toggle on top, fixed-size knob below
    auto formantCol = row2;
    formantEnableButton.setBounds (formantCol.removeFromTop (effectPowerH));
    {
        const int kx = formantCol.getX() + juce::jmax (0, (formantCol.getWidth() - effKnobD) / 2);
        formantSlider.setBounds (kx, formantCol.getY() + 2, effKnobD, effKnobD);
    }

    // Harmony controls block (rightmost block)
    // Split into left/right columns FIRST, then fill each column.
    // Left: enable button + type + voices + toggles
    // Right: Volume/Blend/Formant knobs
    {
        auto h = block3Bounds.reduced(10);

        auto powerButtonWidth = [&] (juce::Button& b) -> int
        {
            const float radius    = 18.0f * 0.3f;
            const float iconWidth = radius * 2.0f;
            float textW = 0.0f;
            if (b.getButtonText().isNotEmpty())
                textW = juce::GlyphArrangement::getStringWidth (ebs::fontComboBox(), b.getButtonText()) + 8.0f;
            return static_cast<int> (std::ceil (iconWidth + textW + 4.0f));
        };

        // Split left/right columns from the full height.
        auto leftCol = h.removeFromLeft ((int) std::round (h.getWidth() * 0.58f));
        h.removeFromLeft (8);
        auto rightCol = h;

        // --- Left column: enable button, type, voices, toggles ---
        {
            auto enableRow = leftCol.removeFromTop (18);
            const int harmonyEnableW = powerButtonWidth (harmonyEnableButton);
            harmonyEnableButton.setBounds (enableRow.removeFromLeft (harmonyEnableW).reduced (0, 0));
        }

        harmonyTypeBox.setBounds (leftCol.removeFromTop(26));

        auto uvRow = leftCol.removeFromTop(22);
        useVoiceButton.setBounds (uvRow.removeFromLeft(120).reduced(2));
        harmonyToneColorLabel.setBounds (uvRow.removeFromRight(34));

        auto selectorRow = leftCol.removeFromTop(28);
        auto selectorBox = selectorRow.removeFromLeft (juce::jmax (90, leftCol.getWidth() - 34));
        shiftedVoicesBox.setBounds (selectorBox.reduced (0, 2));
        harmonyToneBox.setBounds (selectorBox.reduced (0, 2));
        harmonyToneColorSlider.setBounds (selectorRow.withSizeKeepingCentre (28, 24));

        auto flRow = leftCol.removeFromTop (20);
        const int flW = powerButtonWidth (harmonyFollowLeadButton);
        harmonyFollowLeadButton.setBounds (flRow.removeFromLeft (flW).reduced (0, 1));

        auto gmRow = leftCol.removeFromTop (20);
        const int gmW = powerButtonWidth (harmonyGainMatchButton);
        harmonyGainMatchButton.setBounds (gmRow.removeFromLeft (gmW).reduced (0, 1));

        // --- Right column: Volume + Blend side-by-side, Formant below ---
        const int harmLabelH = 14;
        const int harmKnobD  = 44;
        const int harmRowH   = harmLabelH + harmKnobD + 2;
        const int harmRowGap = 8;
        const int harmKnobGap = 8;
        auto knobColW = (rightCol.getWidth() - harmKnobGap) / 2;

        auto knobRow1 = rightCol.removeFromTop (harmRowH);
        {
            const int kx = knobRow1.getX() + juce::jmax (0, (knobColW - harmKnobD) / 2);
            harmonyGainLabel.setBounds (kx, knobRow1.getY(), harmKnobD, harmLabelH);
            harmonyGainSlider.setBounds (kx, knobRow1.getY() + harmLabelH + 2, harmKnobD, harmKnobD);
        }
        {
            const int kx = knobRow1.getX() + knobColW + harmKnobGap + juce::jmax (0, (knobColW - harmKnobD) / 2);
            harmonyBlendLabel.setBounds (kx, knobRow1.getY(), harmKnobD, harmLabelH);
            harmonyBlendSlider.setBounds (kx, knobRow1.getY() + harmLabelH + 2, harmKnobD, harmKnobD);
        }

        rightCol.removeFromTop (harmRowGap);
        auto knobRow2 = rightCol.removeFromTop (harmRowH);
        {
            // Formant stays under the Volume knob (left column).
            const int kxL = knobRow2.getX() + juce::jmax (0, (knobColW - harmKnobD) / 2);
            harmonyFormantLabel.setBounds (kxL, knobRow2.getY(), harmKnobD, harmLabelH);
            harmonyFormantSlider.setBounds (kxL, knobRow2.getY() + harmLabelH + 2, harmKnobD, harmKnobD);
        }
        {
            // Harmony Attack (per-voice fade-in) is placed directly under the
            // Blend knob (right column), as requested.
            const int kxR = knobRow2.getX() + knobColW + harmKnobGap + juce::jmax (0, (knobColW - harmKnobD) / 2);
            harmonyAttackLabel.setBounds (kxR, knobRow2.getY(), harmKnobD, harmLabelH);
            harmonyAttackSlider.setBounds (kxR, knobRow2.getY() + harmLabelH + 2, harmKnobD, harmKnobD);
        }
    }

    // --- Block 1 : Key, Scale, Keyboard (middle) ---
    auto b1 = block1Bounds.reduced(10);

    auto topRow = b1.removeFromTop(44); // Key + Scale row (20 label + 24 combobox)

    // Left: Key (Root) - only ever shows up to 2 chars (e.g. "C", "C#"), so keep it narrow.
    auto bKey = topRow.removeFromLeft(56);
    keyLabel.setBounds(bKey.removeFromTop(20));
    keyBox.setBounds(bKey);

    topRow.removeFromLeft(10); // spacer

    // Middle: Scale Box (fixed narrower width)
    int desiredScaleWidth = juce::jmin(140, topRow.getWidth());
    auto bScale = topRow.removeFromLeft(desiredScaleWidth);
    scaleLabel.setBounds(bScale.removeFromTop(20));
    scaleBox.setBounds(bScale);

    b1.removeFromTop(6); // spacer

    // Keyboard (mini-piano) - placed right after Key / Scale (they are related).
    scaleKeyboard.setBounds(b1.removeFromTop(55).withSizeKeepingCentre(180, 55));

    b1.removeFromTop(6); // spacer

    // --- Key/Scale DETECTION ---
    // In ARA mode the host owns the tonality; hide the whole detector section.
    const bool isARA = processorRef.isBoundToARA_custom();

    if (!isARA)
    {
        // Line 1: a power-style toggle (on/off) + the "Key/Scale Detection" label.
        // "Manual" is no longer a source choice: when the toggle is off you set the
        // key/scale by hand; when on, the source combo below drives detection.
        {
            auto detRow = b1.removeFromTop (22);
            const int pw = 18;   // match Gate / Reverb power buttons
            keyDetectPowerButton.setBounds (detRow.removeFromLeft (pw));
            detRow.removeFromLeft (6);
            keySourceLabel.setBounds (detRow);
        }
        const bool detectOn = keyDetectPowerButton.getToggleState();
        // The group combo is only relevant when detection is on AND the source is OpenVoxKey.
        companionGroupBox.setVisible (detectOn && keySourceBox.getSelectedItemIndex() == 1);

        if (detectOn)
        {
            // Line 2: detection source combo, directly under the power/label row
            // (no gap) and kept at the same 24px height as the Key/Scale combos.
            auto srcRow = b1.removeFromTop (24);
            if (companionGroupBox.isVisible())
            {
                const int grpW = 46;
                auto grpRow = srcRow.removeFromRight (grpW);
                keySourceBox.setBounds (srcRow);
                companionGroupLabel.setBounds (0, 0, 0, 0);   // mini combo: hide the label
                companionGroupBox.setBounds (grpRow);
            }
            else
            {
                keySourceBox.setBounds (srcRow);
                companionGroupLabel.setBounds (0, 0, 0, 0);
                companionGroupBox.setBounds (0, 0, 0, 0);
            }
        }
        else
        {
            keySourceBox.setBounds (0, 0, 0, 0);
            companionGroupLabel.setBounds (0, 0, 0, 0);
            companionGroupBox.setBounds (0, 0, 0, 0);
        }
    }
    else
    {
        // ARA mode: hide all Key/Scale Detection controls
        keyDetectPowerButton.setBounds (0, 0, 0, 0);
        keySourceLabel.setBounds (0, 0, 0, 0);
        keySourceBox.setBounds (0, 0, 0, 0);
        companionGroupLabel.setBounds (0, 0, 0, 0);
        companionGroupBox.setBounds (0, 0, 0, 0);
    }
}

void OpenVoxTunerAudioProcessorEditor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();

    // JUCE's StandaloneFilterWindow is created requesting only the minimise and close
    // title-bar buttons (see juce_StandaloneFilterWindow.h), so the maximise button is
    // absent even though the window is resizable. Re-add it once we are attached to the
    // top-level Standalone window so it can be maximised like a normal desktop app.
    // In a DAW host the top-level component is not a DocumentWindow, so the cast yields
    // nullptr and this is a safe no-op.
    if (auto* w = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))
        w->setTitleBarButtonsRequired (juce::DocumentWindow::minimiseButton
                                        | juce::DocumentWindow::maximiseButton
                                        | juce::DocumentWindow::closeButton, false);
}

void OpenVoxTunerAudioProcessorEditor::timerCallback()
{
    // If the editor is being destroyed, do nothing.  A MessageQueue
    // timer callback may already be in flight when the destructor
    // runs, and the AU's internal state may already be torn down.
    if (processorRef.isEditorShuttingDown())
        return;

    currentCpuUsage = processorRef.getCpuUsage();

    // One-shot diagnostic dump on the first tick, after all restore/attachment
    // messages have settled, to verify what is actually restored at startup.
    if (! startupDiagLogged)
    {
        startupDiagLogged = true;
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
        auto* htype = processorRef.getParameters().getParameter ("harmony_type");
        const float hval = htype != nullptr ? htype->getValue() : -1.0f;
        const int hIdx = (hval >= 0.0f) ? (int) std::round (hval * 21.0f) : -1;
        const ovtdsp::MorphState* slotBms = processorRef.getAbSlotMorphState (1);
        const int slotBType = slotBms != nullptr ? slotBms->harmonyType : -999;
        const ovtdsp::MorphState* slotAms = processorRef.getAbSlotMorphState (0);
        auto f4 = [] (float v) { return juce::String (v, 4); };
        juce::Logger::writeToLog (
            "[DIAG] startup: activeSlot=" + juce::String (processorRef.getAbActiveSlot())
            + " morph=" + juce::String (processorRef.getMorphAmount(), 2)
            + " slotA_hasData=" + juce::String (slotA.hasData ? 1 : 0)
            + " slotB_hasData=" + juce::String (slotB.hasData ? 1 : 0)
            + " slotB_type=" + juce::String (slotBType)
            + " harmony_type_param=" + juce::String (hIdx)
            + " harmony_type_val=" + juce::String (hval, 4)
            + " slotA_type=" + juce::String (slotAms != nullptr ? slotAms->harmonyType : -999)
            + " slotA_vibrato=" + (slotAms ? f4 (slotAms->vibratoPreserve) : juce::String ("null"))
            + " slotA_humanize=" + (slotAms ? f4 (slotAms->humanize) : juce::String ("null"))
            + " slotA_formant=" + (slotAms ? f4 (slotAms->harmonyFormant) : juce::String ("null")));
#endif
    }

    // Apply deferred parameter changes from the audio thread (key/scale detection).
    // Must happen on the UI thread to avoid deadlocks in some hosts.
    processorRef.flushPendingParameterChanges();

    // Read host transport (playhead position, playing state, time signature).
    // getPlayHead()->getPosition() can deadlock Cubase and Live VST3 from
    // the audio thread, so we read it here and cache in atomics.  Wrapped
    // in try/catch because on macOS a torn-down AU can throw Objective-C++
    // exceptions out of JUCE internals.
    try
    {
        processorRef.updateHostTransport();
    }
    catch (...)
    {
    }

    // Read ARA metadata (key/bar signatures) from the host on the UI thread.
    // HostContentReader can deadlock the audio thread in Cubase/Live VST3.
    try
    {
        processorRef.updateAraMetadata();
    }
    catch (...)
    {
    }

    // Repaint the chord badge only when the displayed chord or its out-of-scale
    // state changes (the badge is drawn in paint(); a repaint per tick would
    // redraw the whole banner). In ARA mode the chord is looked up at the
    // current playhead so the badge follows the chord track; in non-ARA mode it
    // reflects the live ovtchord detection (MIDI/sidechain).
    {
        int araRoot = -999, araBass = -999;
        juce::String araChordName;
        processorRef.getAraChordAt (processorRef.getInterpolatedTransportTime(), araRoot, araBass, araChordName);
        const bool outOfScale = processorRef.isAraChordOutOfScale (processorRef.getInterpolatedTransportTime());

        juce::String liveSymbol;
        bool liveOutOfScale = false;
        processorRef.getLiveChord (liveSymbol, liveOutOfScale);

        if (araRoot != lastAraChordBadgeRoot
            || outOfScale != lastAraChordBadgeOutOfScale
            || liveSymbol != lastLiveChordBadgeSymbol)
        {
            lastAraChordBadgeRoot = araRoot;
            lastAraChordBadgeOutOfScale = outOfScale;
            lastLiveChordBadgeSymbol = liveSymbol;
            repaint();
        }
    }

    refreshVisualizer();

    // Forward waveform data to both visualizer and curve editor when enabled
    if (showWaveform)
    {
        juce::AudioBuffer<float> waveform;
        double sr = 44100.0;
        processorRef.copyAraWaveform (waveform, sr);
        if (waveform.getNumSamples() > 0)
        {
            const float* data = waveform.getReadPointer (0);
            if (pitchVisualizer != nullptr)
                pitchVisualizer->setWaveformOverlay (data, waveform.getNumSamples(), sr);
            if (curveEditor != nullptr)
                curveEditor->setWaveformOverlay (data, waveform.getNumSamples(), sr);
        }
    }

    // Sync waveform display type (only when it changes)
    {
        const int dispType = processorRef.getWaveformDisplayType();
        if (dispType != lastWaveformDisplayType)
        {
            lastWaveformDisplayType = dispType;
            if (pitchVisualizer != nullptr)
                pitchVisualizer->setDisplayType (dispType);
            if (curveEditor != nullptr)
                curveEditor->setDisplayType (dispType);
        }
    }

    // Morph: apply whenever the DAW (or the slider) changes the automatable
    // "morph_amount" parameter. Driven here so DAW automation works even without
    // user interaction with the slider.
    {
        const float m = processorRef.getMorphAmount();
        if (m != lastMorphValue)
            onMorphSliderChanged (m);
    }

    // Keep the Harmony sub-toggles (Follow Lead / Gain Match / Use Voice) in
    // sync with the Harmony switch even when Harmony changes WITHOUT a direct
    // click - e.g. preset load or DAW automation. The ButtonAttachment updates
    // the Harmony button with dontSendNotification, so onStateChange does not
    // fire in those cases. When the Harmony enable state changes, re-run
    // refreshLabels() (which disables those sub-toggles based on the Harmony
    // state) instead of only mirroring the two Follow Lead / Gain Match
    // buttons here. refreshLabels() is cheap enough to run once per state
    // change, and it keeps every Harmony-dependent control consistent.
    {
        const bool harmonyOn = harmonyEnableButton.getToggleState();
        if (harmonyOn != lastHarmonyEnabled)
        {
            lastHarmonyEnabled = harmonyOn;
            refreshLabels();
        }
    }

    if (updateCheckState != nullptr)
    {
        if (! updateCheckState->finished.load())
        {
            updateButton.setButtonText (ovt::tr(ovt::Keys::kStatusChecking));
            updateButton.setEnabled (false);
        }
        else
        {
            updateButton.setEnabled (true);
            if (updateCheckState->updateAvailable.load())
            {
                updateButton.setButtonText (ovt::tr(ovt::Keys::kStatusUpdatePrefix) + updateCheckState->latestVersion);
                updateButton.setColour (juce::TextButton::buttonColourId, ebs::accent().darker (0.2f));
                updateButton.setTooltip (ovt::tr(ovt::Keys::kTooltipUpdateAvailable));
            }
            else
            {
                updateButton.setButtonText (ovt::tr(ovt::Keys::kStatusUpToDate));
                updateButton.setColour (juce::TextButton::buttonColourId, ebs::bgPanel());
                updateButton.setTooltip (updateCheckState->statusText.isNotEmpty() ? updateCheckState->statusText : ovt::tr(ovt::Keys::kTooltipUpdateReleases));
            }
        }
    }

    // Sync tab <-> "mode" parameter
    int tabIndex = tabbedComponent.getCurrentTabIndex();

    // Visibility of Curve Editor Mode specific buttons
    bool isCurveEditorMode = (tabIndex == 1);
    
    // (clearHarmonyTraces is now done in the tab-change detection block
    // below - doing it on every tick would wipe the buffer the draw
    // routine needs to render the harmony blue lines.)
    
    optionsButton.setVisible (isCurveEditorMode);
    snapButton.setVisible (isCurveEditorMode);
    snapGridButton.setVisible (isCurveEditorMode);
    stepModeButton.setVisible (isCurveEditorMode);
    zoomInButton.setVisible (isCurveEditorMode);
    zoomOutButton.setVisible (isCurveEditorMode);
    scrollUpButton.setVisible (isCurveEditorMode);
    scrollDownButton.setVisible (isCurveEditorMode);
    resetViewButton.setVisible (isCurveEditorMode);

    // Measures control + standalone transport live on the toolbar row.
    measuresLabel.setVisible (isCurveEditorMode);
    measuresComboBox.setVisible (isCurveEditorMode);
    playButton.setVisible (isCurveEditorMode && processorRef.isStandaloneWrapper());
    rewindButton.setVisible (isCurveEditorMode && processorRef.isStandaloneWrapper());

    // Gray out Formant slider if disabled
    bool isFormantEnabled = formantEnableButton.getToggleState();
    formantSlider.setEnabled (isFormantEnabled);

    // Gray out Reverb slider if disabled
    bool isReverbEnabled = reverbEnableButton.getToggleState();
    reverbMixSlider.setEnabled (isReverbEnabled);

    // Gray out Upward Comp slider if disabled
    bool isUpwardCompEnabled = upwardCompEnableButton.getToggleState();
    upwardCompAmountSlider.setEnabled (isUpwardCompEnabled);

    // Harmony controls enable/disable
    bool isHarmonyEnabled = harmonyEnableButton.getToggleState();
    harmonyTypeBox.setEnabled (isHarmonyEnabled);
    harmonyGainSlider.setEnabled (isHarmonyEnabled);
    harmonyBlendSlider.setEnabled (isHarmonyEnabled);
    harmonyAttackSlider.setEnabled (isHarmonyEnabled);
    harmonyFormantSlider.setEnabled (isHarmonyEnabled);
    // Use Voice is a sub-toggle of Harmony: it must be disabled whenever
    // Harmony is off (otherwise its state is silently read on re-enable).
    useVoiceButton.setEnabled (isHarmonyEnabled);
    // Follow Lead and Gain Match are sub-toggles of Harmony: they must be
    // disabled whenever Harmony itself is off, otherwise the user can still
    // flip them (and the engine would silently read the stale values on the
    // next Harmony re-enable). Centralising this in refreshLabels() also
    // keeps them in sync when Harmony changes via preset load / automation /
    // language switch, not just on a direct click. We also call
    // setInterceptsMouseClicks(false) so the custom PowerButton rendering
    // (which ignores isEnabled() for the glow) cannot be toggled by a click,
    // and setAlpha to make the disabled state visually obvious.
    harmonyFollowLeadButton.setEnabled (isHarmonyEnabled);
    harmonyGainMatchButton.setEnabled (isHarmonyEnabled);
    harmonyFollowLeadButton.setInterceptsMouseClicks (isHarmonyEnabled, isHarmonyEnabled);
    harmonyGainMatchButton.setInterceptsMouseClicks (isHarmonyEnabled, isHarmonyEnabled);
    const bool useVoice = useVoiceButton.getToggleState();
    shiftedVoicesBox.setEnabled (isHarmonyEnabled && useVoice);
    harmonyToneBox.setEnabled (isHarmonyEnabled && !useVoice);
    harmonyToneColorSlider.setEnabled (isHarmonyEnabled && !useVoice);

    // Show bypass only in standalone wrapper (DAW has host bypass)
    bypassToggleButton.setVisible (processorRef.isStandaloneWrapper());

    // Show only the relevant selector to keep the UI compact and readable
    shiftedVoicesBox.setVisible (isHarmonyEnabled && useVoice);
    harmonyToneBox.setVisible (isHarmonyEnabled && !useVoice);
    harmonyToneColorSlider.setVisible (isHarmonyEnabled && !useVoice);
    harmonyToneColorLabel.setVisible (isHarmonyEnabled && !useVoice);

    // "Reset Playhead" is now in the Curve Editor Options menu; it is disabled
    // there when bound to ARA (the host owns the timeline in ARA mode).

    auto* modeParam = processorRef.getParameters().getParameter("mode");
    
    // If user clicked a tab
    if (tabIndex != static_cast<int>(modeParam->getValue())) {
        modeParam->setValueNotifyingHost(tabIndex == 1 ? 1.0f : 0.0f);
        // Just-entered the Curve Editor: start with a fresh harmony buffer
        // so the first few blue lines appear from a clean state instead of
        // mixing with stale samples from a previous tab visit.
        if (tabIndex == 1 && curveEditor != nullptr)
            curveEditor->clearHarmonyTraces();
    }
    
    // Update edit state and playhead
    if (curveEditor != nullptr) {
        curveEditor->setEditorEnabled(tabIndex == 1);
        // Pass the extrapolated transport time so the playhead moves
        // smoothly at 60 fps even though the worker only refreshes the
        // host PPQ at 30 Hz (see getInterpolatedTransportTime in the
        // processor header).  When the host is paused, the extrapolation
        // returns the frozen PPQ, so the playhead does not drift.
        curveEditor->setPlayheadTime(processorRef.getInterpolatedTransportTime(), processorRef.getIsPlaying(), processorRef.isPlayheadLooping());
        // Propagate time signature (Feature 1) - read from processor
        int num = processorRef.getCurrentTimeSigNumerator();
        int den = processorRef.getCurrentTimeSigDenominator();
        curveEditor->setTimeSignature (num, den);
        // Sync controls with persisted parameters (first time only).
        if (!measuresSyncDone) {
            // Restore Measures combo from the persisted parameter.
            auto* measuresRaw = processorRef.getParameters().getRawParameterValue("editor_measures");
            float measures = measuresRaw ? measuresRaw->load() : 4.0f;
            const int mVal = static_cast<int> (measures);
            if (measuresComboBox.getText().getIntValue() != mVal) {
                for (int i = 0; i < measuresComboBox.getNumItems(); ++i) {
                    if (measuresComboBox.getItemText(i).getIntValue() == mVal) {
                        measuresComboBox.setSelectedItemIndex (i, juce::dontSendNotification);
                        break;
                    }
                }
            }
            curveEditor->setMeasuresVisible (mVal);

            // Restore Auto-Scroll from the persisted parameter. In Standalone the
            // playhead always loops on the Measures window, so auto-scroll (which
            // follows the playhead by panning the view) is disabled/ignored.
            if (processorRef.isStandaloneWrapper())
                curveEditor->setAutoScroll (false);
            else
            {
                auto* scrollRaw = processorRef.getParameters().getRawParameterValue("auto_scroll");
                bool scrollOn = scrollRaw ? (scrollRaw->load() > 0.5f) : true;
                curveEditor->setAutoScroll (scrollOn);
            }

            measuresSyncDone = true;
        }
        // Persist control states -> AudioProcessor parameters (saved/loaded by the host).
        auto* measuresParam = processorRef.getParameters().getRawParameterValue("editor_measures");
        if (measuresParam) {
            int curVal = measuresComboBox.getText().getIntValue();
            if (curVal > 0)
                const_cast<std::atomic<float>*>(measuresParam)->store(static_cast<float>(curVal));
        }
        auto* scrollParam = processorRef.getParameters().getRawParameterValue("auto_scroll");
        if (scrollParam)
            const_cast<std::atomic<float>*>(scrollParam)->store(curveEditor->getAutoScroll() ? 1.0f : 0.0f);
        // Reflect standalone transport state on the toolbar buttons.
        syncTransportButtons();
        // Sync curve from processor to editor after state restore or editor recreation
        if (processorRef.getPendingCurveRestore().load() && curveEditor != nullptr)
        {
            curveEditor->setCurve (processorRef.getPitchCurve());
            processorRef.getPendingCurveRestore().store (false);
            syncEditButtons();
            resetMorph(); // state changed - invalidate any active morph
        }
        // Also sync on every timer tick for safety (e.g. model changes via capture/clear)
        syncEditButtons();
    }

    // Update harmony visuals: forward harmony frequencies to the visualizer
    if (pitchVisualizer != nullptr)
    {
        bool harmonyOn = true;
        auto* rawHarmonyEnable = processorRef.getParameters().getRawParameterValue("harmony_enable");
        if (rawHarmonyEnable != nullptr)
            harmonyOn = (rawHarmonyEnable->load() > 0.5f);

        juce::Array<float> freqsToSend;
        processorRef.copyHarmonyFrequencies (freqsToSend);

        if (! harmonyOn)
            freqsToSend.clear();

        const juce::Array<float> liveFreqs = freqsToSend;
        if (freqsToSend.size() > 0)
        {
            // Detect DAW transport jump (loop wrap, seek) for the harmony
            // traces too. The red input trace is already cleared in
            // refreshVisualizer() the same way - without this, the blue
            // lines from the previous loop iteration remain visible after
            // the playhead wraps back to 0, because they still fall inside
            // the curve editor's timeVisible window.
            const double nowLoop = processorRef.getLoopTransportTime();
            const double loopDelta = std::abs (nowLoop - lastHarmonyTransportTime);
            if (loopDelta > 0.5 && lastHarmonyTransportTime > 0.0 && curveEditor != nullptr)
                curveEditor->clearHarmonyTraces();
            lastHarmonyTransportTime = nowLoop;

        }

        // Keep the Live visualizer on the real snapshot.
        pitchVisualizer->setHarmonyFrequencies (liveFreqs);

        // Use exactly the same timeline as the red input trace in
        // refreshVisualizer(). getInterpolatedTransportTime() may use the
        // host's un-offset PPQ, while getLoopTransportTime() uses the curve
        // editor's actual wrapped/offset timeline. Mixing those clocks is the
        // source of the diagonal blue segments after reset, looping or seek.
        if (curveEditor != nullptr && curveEditor->getShowHarmoniesTrace())
        {
            juce::Array<float> sortedFreqs = freqsToSend;
            sortedFreqs.sort();
            curveEditor->addHarmonySamples (processorRef.getLoopTransportTime(), sortedFreqs);
        }



        // MIDI status label removed; debug window shows detailed MIDI log.
    }

    // === Global plugin Undo/Redo (Option 1) ===
    // Maintain the "before" baseline for discrete clicks/toggles/combos. We
    // only refresh it when NOT applying an undo (to avoid clobbering the real
    // pre-change state) and NOT during a slider drag (the drag-start snapshot
    // is authoritative). DAW automation updates params every block but never
    // fires the gesture handlers below, so it is never pushed to the history.
    if (! applyingUndo_ && ! undoGestureActive_)
        undoLiveSnapshot_ = processorRef.getParameters().copyState();

    // Reflect UndoManager state on the toolbar buttons.
    undoButton.setEnabled (processorRef.getUndoManager().canUndo());
    redoButton.setEnabled (processorRef.getUndoManager().canRedo());
}

void OpenVoxTunerAudioProcessorEditor::setWaveformDisplayType (int type)
{
    processorRef.setWaveformDisplayType (type);
    if (pitchVisualizer != nullptr)
        pitchVisualizer->setDisplayType (type);
    if (curveEditor != nullptr)
        curveEditor->setDisplayType (type);
}

void OpenVoxTunerAudioProcessorEditor::refreshVisualizer()
{
    if (pitchVisualizer == nullptr) return;
    const float hzIn  = processorRef.getCurrentInputPitch();
    const float hzOut = processorRef.getCurrentOutputPitch();
    pitchVisualizer->pushInputPitch  (hzIn);
    pitchVisualizer->pushOutputPitch (hzOut);
    const bool midiActive = processorRef.isMidiTargetActive();
    pitchVisualizer->setMidiFollowActive (midiActive);
    if (curveEditor != nullptr)
    {
        curveEditor->setMidiFollowActive (midiActive);
        curveEditor->setMidiTargetHz (processorRef.getCurrentMidiTargetHz());
    }
    if (curveEditor != nullptr)
    {
        curveEditor->getPianoKeyboard().setCurrentPitches (hzIn, hzOut);
        // Detect DAW transport jump (loop, seek) and clear trace
        const double now = processorRef.getLoopTransportTime();
        const double delta = std::abs (now - lastTransportTime);
        if (delta > 0.5 && lastTransportTime > 0.0) // >0.5s jump = not normal playback
            curveEditor->clearInputTrace();
        lastTransportTime = now;
        if (hzIn > 0.0f && curveEditor->getShowInputTrace())
            curveEditor->addInputTraceSample (now, hzIn);
    }

    // Note info for the header display.
    const ovtdsp::NoteInfo info = ovtdsp::describePitch (hzIn, hzOut);
    pitchVisualizer->setNoteInfo (info);

    // Update the scale intervals (for the background lines).
    // We read them from the parameter tree -> ScaleQuantizer.
    // Note: the quantizer is rebuilt in syncParameters() every block,
    // so we can just retrieve its current intervals.
    if (curveEditor != nullptr)
    {
        // Use the processor's ScaleQuantizer directly for authoritative intervals.
        // This avoids duplicating the scale table and prevents mismatches.
        juce::Array<int> intervals;
        processorRef.copyScaleIntervals (intervals);
        pitchVisualizer->setScaleIntervals (intervals);
        curveEditor->setScaleIntervals (intervals);
        scaleKeyboard.setActiveScaleIntervals (intervals);

        const int keyIdx = processorRef.getKeyIndex();
        const int scaleIdx = processorRef.getScaleIndex();
        
        if (scaleIdx == 13)
        {
            // Custom scale: also sync custom intervals from the scale keyboard
            juce::Array<int> customIntervals;
            for (int i = 0; i < 12; ++i)
                if (scaleKeyboard.getButton(i).getToggleState())
                    customIntervals.add (i);
            curveEditor->setCustomIntervals (customIntervals);
            curveEditor->setKeyAndScale (keyIdx, ovtdsp::Scale::Custom);
        }
        else
        {
            curveEditor->setKeyAndScale (keyIdx, static_cast<ovtdsp::Scale> (juce::jlimit (0, 13, scaleIdx)));
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::pitchCurveChanged()
{
    // The curve editor notified a change: we copy the curve to
    // the processor (thread-safe via getter).
    if (curveEditor == nullptr) return;
    processorRef.getPitchCurve() = curveEditor->getCurve();
}

void OpenVoxTunerAudioProcessorEditor::toggleHelpOverlay()
{
    helpOverlayVisible = ! helpOverlayVisible;
    helpOverlay.setVisible (helpOverlayVisible);
    if (helpOverlayVisible)
        helpOverlay.setBounds (getLocalBounds());
    repaint();
}

// =============================================================================
// MIDI Import (drag-and-drop + menu)
// =============================================================================

bool OpenVoxTunerAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.toLowerCase().endsWith (".mid") || f.toLowerCase().endsWith (".midi"))
            return true;
    return false;
}

void OpenVoxTunerAudioProcessorEditor::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    juce::File midiFile;
    for (const auto& f : files)
    {
        juce::File candidate (f);
        if (candidate.existsAsFile()
            && (f.toLowerCase().endsWith (".mid") || f.toLowerCase().endsWith (".midi")))
        {
            midiFile = candidate;
            break;
        }
    }

    if (! midiFile.existsAsFile())
        return;

    handleMidiFileImport (midiFile);
}

void OpenVoxTunerAudioProcessorEditor::handleMidiFileImport (const juce::File& midiFile)
{
    // Ensure we are on the Curve Editor tab
    if (tabbedComponent.getCurrentTabIndex() != 1)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            ovt::tr (ovt::Keys::kMidiErrorWrongTab));
        return;
    }

    // Analyze the file
    auto info = ovtdsp::MidiImporter::analyzeFile (midiFile);

    if (! info.isValid)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            info.errorMessage);
        return;
    }

    // Single non-percussion channel: import directly with "highest note" strategy
    if (info.channels.size() == 1)
    {
        auto curve = ovtdsp::MidiImporter::importFrom (
            midiFile,
            ovtdsp::MidiExtractStrategy::HighestNote,
            info.channels.getFirst().channel);

        applyMidiImport (curve, midiFile.getFileName());
        return;
    }

    // Multiple channels: show selection dialog
    showMidiChannelDialog (midiFile, info);
}

void OpenVoxTunerAudioProcessorEditor::applyMidiImport (
    const ovtdsp::PitchCurve& newCurve,
    const juce::String& sourceName)
{
    if (curveEditor == nullptr)
        return;

    if (newCurve.getNumPoints() < 2)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            ovt::tr (ovt::Keys::kMidiErrorTooFew));
        return;
    }

    curveEditor->importMidiCurve (newCurve);
}

// =============================================================================
// MIDI Import: Multi-channel selection dialog
// =============================================================================

class MidiChannelSelectorComponent : public juce::Component
{
public:
    MidiChannelSelectorComponent (
        const ovtdsp::MidiImportInfo& info,
        std::function<void (ovtdsp::MidiExtractStrategy, int)> onConfirm)
        : importInfo (info), confirmCallback (std::move (onConfirm))
    {
        setSize (420, 260);

        // Strategy combo
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyHighest), 1);
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyLowest),  2);
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyLoudest), 3);
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyChannel), 4);
        strategyCombo.setSelectedId (1, juce::dontSendNotification);
        strategyCombo.onChange = [this] { updateChannelVisibility(); };
        addAndMakeVisible (strategyCombo);

        // Channel combo (populated from info)
        for (const auto& ch : info.channels)
        {
            channelCombo.addItem (
                "Channel " + juce::String (ch.channel)
                + " (" + juce::String (ch.numNotes) + " notes, "
                + ovtdsp::noteInOctaveName (ovtdsp::midiToNoteInOctave (ch.minNote))
                + juce::String (ovtdsp::midiToOctave (ch.minNote)) + " - "
                + ovtdsp::noteInOctaveName (ovtdsp::midiToNoteInOctave (ch.maxNote))
                + juce::String (ovtdsp::midiToOctave (ch.maxNote)) + ")",
                ch.channel);
        }
        if (info.channels.size() > 0)
            channelCombo.setSelectedId (info.channels.getFirst().channel,
                                        juce::dontSendNotification);
        addAndMakeVisible (channelCombo);

        updateChannelVisibility();

        // Buttons
        importButton.setButtonText (ovt::tr (ovt::Keys::kMidiBtnImport));
        importButton.onClick = [this] { onImportClicked(); };
        addAndMakeVisible (importButton);

        cancelButton.setButtonText (ovt::tr (ovt::Keys::kMenuCancel));
        cancelButton.onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };
        addAndMakeVisible (cancelButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff2d2d2d));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (14.0f);
        g.drawText (ovt::tr (ovt::Keys::kMidiStrategy),
                    16, 24, 80, 24, juce::Justification::centredLeft);
        g.drawText (ovt::tr (ovt::Keys::kMidiChannel),
                    16, 64, 80, 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        strategyCombo.setBounds (100, 24, 300, 24);
        channelCombo.setBounds (100, 64, 300, 24);
        cancelButton.setBounds (230, 210, 80, 30);
        importButton.setBounds (320, 210, 80, 30);
    }

private:
    void updateChannelVisibility()
    {
        bool showChannel = (strategyCombo.getSelectedId() == 4);
        channelCombo.setVisible (showChannel);
    }

    void onImportClicked()
    {
        auto strategy = static_cast<ovtdsp::MidiExtractStrategy> (
            strategyCombo.getSelectedId() - 1);
        int channel = channelCombo.getSelectedId();
        confirmCallback (strategy, channel);

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }

    ovtdsp::MidiImportInfo importInfo;
    std::function<void (ovtdsp::MidiExtractStrategy, int)> confirmCallback;
    juce::ComboBox strategyCombo, channelCombo;
    juce::TextButton importButton, cancelButton;
};

void OpenVoxTunerAudioProcessorEditor::showMidiChannelDialog (
    const juce::File& midiFile,
    const ovtdsp::MidiImportInfo& info)
{
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = ovt::tr (ovt::Keys::kMidiDialogTitle)
                     + ": " + midiFile.getFileName();

    auto* selectorComp = new MidiChannelSelectorComponent (
        info,
        [editorSafe = juce::Component::SafePointer<OpenVoxTunerAudioProcessorEditor> (this), midiFile]
        (ovtdsp::MidiExtractStrategy s, int ch)
        {
            if (editorSafe != nullptr)
            {
                auto curve = ovtdsp::MidiImporter::importFrom (midiFile, s, ch);
                editorSafe->applyMidiImport (curve, midiFile.getFileName());
            }
        });
    selectorComp->setSize (420, 260);
    opts.content.set (selectorComp, true);
    opts.useNativeTitleBar = true;
    opts.escapeKeyTriggersCloseButton = true;
    opts.resizable = false;
    opts.launchAsync();
}
// === Preset Morphing ===

void OpenVoxTunerAudioProcessorEditor::resetMorph()
{
    if (morphSource != nullptr)
    {
        ovtdsp::applyInterpolatedState (processorRef.getParameters(),
                                        *morphSource, *morphSource, 0.0f);
        auto resetCurve = morphSource->curve;
        if (curveEditor != nullptr)
        {
            curveEditor->setCurve (resetCurve);
            curveEditor->setGhostCurve (nullptr);
        }
    }
    processorRef.setMorphAmount (0.0f);
    lastMorphValue = 0.0f;
    morphSource.reset();
    morphTarget.reset();
    morphUndoState.reset();
    lastMorphIntendedValues.clear();
}

void OpenVoxTunerAudioProcessorEditor::onMorphSliderChanged (float value)
{
    // Ignore morph application during slot switching - the caller manages state.
    if (switchingSlot)
    {
        lastMorphValue = value;
        return;
    }

    // Auto-capture source on first movement if not set
    if (morphSource == nullptr)
    {
        // Fresh morph: forget any previous external-automation exclusions so
        // the new crossfade starts with a clean baseline.
        lastMorphIntendedValues.clear();
        morphSource = std::make_unique<ovtdsp::MorphState> (
            ovtdsp::captureState (processorRef.getParameters(), processorRef.getPitchCurve(), "Current"));
        morphSourceName = "Current";
    }
    // Auto-capture target from Slot B (or Slot A if B is empty) on first movement
    if (morphTarget == nullptr)
    {
        if (slotB.hasData && slotB.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            morphTargetName = "Slot B";
        }
        else if (slotA.hasData && slotA.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
            morphTargetName = "Slot A";
        }
        else
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (
                ovtdsp::captureState (processorRef.getParameters(), processorRef.getPitchCurve(), "Target"));
            morphTargetName = "Target";
        }
    }

    // Capture pre-morph state on first movement (for undo)
    if (lastMorphValue < 0.01f && value > 0.01f && morphUndoState == nullptr)
    {
        morphUndoState = std::make_unique<ovtdsp::MorphState> (
            ovtdsp::captureState (processorRef.getParameters(), processorRef.getPitchCurve(), "Pre-morph"));
    }

    // Detect parameters currently driven by external automation (DAW lanes or
    // UI) so the morph crossfade does not overwrite them. A parameter is
    // considered externally driven when its live value differs from the value
    // the morph last applied to it.
    juce::AudioProcessorValueTreeState& params = processorRef.getParameters();
    juce::StringArray excluded;
    {
        const juce::StringArray ids = ovtdsp::getMorphParameterIds();
        for (const auto& id : ids)
        {
            auto* p = params.getParameter (id);
            if (p == nullptr)
                continue;
            auto it = lastMorphIntendedValues.find (id);
            if (it != lastMorphIntendedValues.end()
                && std::abs (p->getValue() - it->second) > 1.0e-4f)
                excluded.add (id);
        }
    }

    // Apply interpolated state (skipping externally-driven parameters)
    ovtdsp::applyInterpolatedState (params,
                                    *morphSource, *morphTarget, value, &excluded);

    // Record the values the morph just applied so the next frame can detect
    // whether a parameter was changed externally and must remain excluded.
    {
        const juce::StringArray ids = ovtdsp::getMorphParameterIds();
        for (const auto& id : ids)
        {
            if (excluded.contains (id))
                continue;
            auto* p = params.getParameter (id);
            if (p != nullptr)
                lastMorphIntendedValues[id] = p->getValue();
        }
    }

    // Morph the parameters only: the pitch curve is NOT crossfaded (a curve
    // blend would resample and add spurious intermediate points). The displayed
    // curve snaps to the nearest slot's curve as the slider moves, so no extra
    // points are ever introduced by the morph.
    if (curveEditor != nullptr)
    {
        const ovtdsp::PitchCurve& nearestCurve = (value < 0.5f) ? morphSource->curve : morphTarget->curve;
        curveEditor->setCurve (nearestCurve);
        // Show ghost curve (target) when morphing, clear when at source
        curveEditor->setGhostCurve (value > 0.01f ? &morphTarget->curve : nullptr);
    }

    lastMorphValue = value;
}

void OpenVoxTunerAudioProcessorEditor::undoMorph()
{
    if (morphUndoState == nullptr) return;

    // Restore the pre-morph state
    ovtdsp::applyInterpolatedState (processorRef.getParameters(),
                                    *morphUndoState, *morphUndoState, 0.0f);
    if (curveEditor != nullptr)
    {
        curveEditor->setCurve (morphUndoState->curve);
        curveEditor->setGhostCurve (nullptr);
    }
    processorRef.setMorphAmount (0.0f);
    lastMorphValue = 0.0f;
    morphUndoState.reset();
}

void OpenVoxTunerAudioProcessorEditor::showMorphContextMenu()
{
    juce::PopupMenu menu;

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphSetSource), [this] {
        morphSource = std::make_unique<ovtdsp::MorphState> (
            ovtdsp::captureState (processorRef.getParameters(),
                                 processorRef.getPitchCurve(), "Current"));
        morphSourceName = "Current";
        processorRef.setMorphAmount (0.0f);
        lastMorphValue = 0.0f;
    });

    menu.addSeparator();

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphSetTargetA), [this] {
        if (slotA.hasData && slotA.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
            morphTargetName = "Slot A";
            processorRef.setMorphAmount (0.0f);
            lastMorphValue = 0.0f;
        }
    });

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphSetTargetB), [this] {
        if (slotB.hasData && slotB.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            morphTargetName = "Slot B";
            processorRef.setMorphAmount (0.0f);
            lastMorphValue = 0.0f;
        }
    });

    menu.addSeparator();

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphAtoB), [this] {
        if (slotA.hasData && slotA.morphState != nullptr && slotB.hasData && slotB.morphState != nullptr)
        {
            morphSource = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
            morphSourceName = "Slot A";

            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            morphTargetName = "Slot B";

            // Fresh morph baseline: drop the external-automation exclusion map so
            // the new crossfade is not poisoned by stale intended values.
            lastMorphIntendedValues.clear();
            processorRef.setMorphAmount (0.0f);
            lastMorphValue = 0.0f;
        }
    });

    menu.addSeparator();

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphUndo), morphUndoState != nullptr, false, [this] {
        undoMorph();
    });

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphReset), [this] {
        if (morphSource != nullptr)
        {
            ovtdsp::applyInterpolatedState (processorRef.getParameters(),
                                            *morphSource, *morphSource, 0.0f);
            auto resetCurve = morphSource->curve;
            if (curveEditor != nullptr)
            {
                curveEditor->setCurve (resetCurve);
                curveEditor->setGhostCurve (nullptr);
            }
        }
        processorRef.setMorphAmount (0.0f);
        lastMorphValue = 0.0f;
    });

    applyMenuLookAndFeel (menu, customLookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options());
}

void OpenVoxTunerAudioProcessorEditor::refreshDrawableButtonIcons()
{
    // The DrawableButton colour IDs are already re-applied in
    // applyThemeToAllComponents(). This method is a hook for
    // future SVG icon recreation if needed.
    applyThemeToAllComponents();
}

bool OpenVoxTunerAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Global plugin Undo/Redo (Option 1): Ctrl/Cmd+Z = undo,
    // Ctrl/Cmd+Shift+Z (or Ctrl/Cmd+Y) = redo. These are editor-global and
    // distinct from the Curve Editor's own undo (which only covers curve
    // points and only fires when the Curve Editor has focus).
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'Z')
        {
            if (key.getModifiers().isShiftDown())
                performGlobalRedo();
            else
                performGlobalUndo();
            return true;
        }
        if (key.getKeyCode() == 'Y')
        {
            performGlobalRedo();
            return true;
        }
    }
    return false;
}

// === Global plugin Undo/Redo (Option 1) ===

void OpenVoxTunerAudioProcessorEditor::performGlobalUndo()
{
    auto& um = processorRef.getUndoManager();
    if (! um.canUndo())
        return;
    applyingUndo_ = true;
    um.undo();
    applyingUndo_ = false;
    // Re-sync the live baseline and every dependent control to the restored
    // state. refreshLabels() also re-disables Harmony sub-toggles etc.
    undoLiveSnapshot_ = processorRef.getParameters().copyState();
    refreshLabels();
}

void OpenVoxTunerAudioProcessorEditor::performGlobalRedo()
{
    auto& um = processorRef.getUndoManager();
    if (! um.canRedo())
        return;
    applyingUndo_ = true;
    um.redo();
    applyingUndo_ = false;
    undoLiveSnapshot_ = processorRef.getParameters().copyState();
    refreshLabels();
}

void OpenVoxTunerAudioProcessorEditor::commitUndoGesture (const juce::ValueTree& before)
{
    // `before` is the pre-change snapshot; `after` is the current live state.
    // Skip programmatic refreshes that round-trip identical values.
    const juce::ValueTree after = processorRef.getParameters().copyState();
    if (before.isValid() && after.isValid() && ! before.isEquivalentTo (after))
        processorRef.pushUndoAction (before, after);
    undoLiveSnapshot_ = after;
}

void OpenVoxTunerAudioProcessorEditor::registerUndoGestureListeners()
{
    // Register the editor as listener on every control that mutates the
    // plugin state, so a single set of handlers records ALL gestures.
    // Sliders: snapshot at drag-start, commit at drag-end.
    speedSlider.addListener (this);
    amountSlider.addListener (this);
    formantSlider.addListener (this);
    morphSlider.addListener (this);
    reverbMixSlider.addListener (this);
    // 2026-07-24 (Deprecation): FlexTune listener disabled (UI hidden).
    // flexTuneSlider.addListener (this);
    humanizeSlider.addListener (this);
    vibratoPreserveSlider.addListener (this);
    // 2026-07-24 (Deprecation): Attack-Aware listeners disabled.
    // attackReleaseSlider.addListener (this);
    harmonyGainSlider.addListener (this);
    harmonyBlendSlider.addListener (this);
    harmonyAttackSlider.addListener (this);
    harmonyToneColorSlider.addListener (this);
    noiseGateThresholdSlider.addListener (this);

    // Toggles + combo boxes: commit on change using the last pre-change
    // snapshot (the live baseline captured by the timer).
    formantEnableButton.addListener (this);
    bypassToggleButton.addListener (this);
    midiToggleButton.addListener (this);
    reverbEnableButton.addListener (this);
    noiseGateEnableButton.addListener (this);
    // 2026-07-24 (Deprecation): Attack-Aware button listener disabled.
    // attackAwareButton.addListener (this);
    correctionModeButton.addListener (this);
    harmonyEnableButton.addListener (this);
    harmonyFollowLeadButton.addListener (this);
    harmonyGainMatchButton.addListener (this);
    useVoiceButton.addListener (this);
    keyDetectPowerButton.addListener (this);

    keyBox.addListener (this);
    scaleBox.addListener (this);
    latencyModeBox.addListener (this);
    detectorBox.addListener (this);
    keySourceBox.addListener (this);
    companionGroupBox.addListener (this);
    harmonyTypeBox.addListener (this);
    shiftedVoicesBox.addListener (this);
    harmonyToneBox.addListener (this);
    measuresComboBox.addListener (this);

    // The 12 custom-scale toggle buttons (part of the scale keyboard).
    for (int i = 0; i < 12; ++i)
        scaleKeyboard.getButton (i).addListener (this);
}

// juce::Slider::Listener
void OpenVoxTunerAudioProcessorEditor::sliderValueChanged (juce::Slider*)
{
    // Deliberately empty: the live snapshot is refreshed by the 30 fps timer
    // and committed on sliderDragEnded, so per-tick value changes need no
    // action here.
}

void OpenVoxTunerAudioProcessorEditor::sliderDragStarted (juce::Slider*)
{
    undoGestureActive_ = true;
    undoBeforeDrag_ = processorRef.getParameters().copyState();
}

void OpenVoxTunerAudioProcessorEditor::sliderDragEnded (juce::Slider*)
{
    if (! undoGestureActive_)
        return;
    undoGestureActive_ = false;
    commitUndoGesture (undoBeforeDrag_);
}

// juce::Button::Listener
void OpenVoxTunerAudioProcessorEditor::buttonClicked (juce::Button*)
{
    commitUndoGesture (undoLiveSnapshot_);
}

// juce::ComboBox::Listener
void OpenVoxTunerAudioProcessorEditor::comboBoxChanged (juce::ComboBox*)
{
    commitUndoGesture (undoLiveSnapshot_);
}

// Wrap a non-gesture state change (preset load, programmatic apply) in a
// single undo transaction: snapshot before, run the change, snapshot after.
void OpenVoxTunerAudioProcessorEditor::pushUndoAroundCall (std::function<void()> fn)
{
    if (applyingUndo_)
        return; // never record while replaying undo/redo
    const juce::ValueTree before = processorRef.getParameters().copyState();
    fn();
    commitUndoGesture (before);
}

void OpenVoxTunerAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    // Clicking the "Key/Scale Detection" label toggles its power button.
    if (event.eventComponent == &keySourceLabel)
    {
        keyDetectPowerButton.triggerClick();
        return;
    }

    // Right-click on morph slider opens context menu
    if (event.mods.isRightButtonDown())
    {
        auto* source = event.eventComponent;
        if (source == &morphSlider)
        {
            showMorphContextMenu();
        }
    }
    // Double-click on morph slider snaps to center (0.5)
    else if (event.getNumberOfClicks() >= 2)
    {
        auto* source = event.eventComponent;
        if (source == &morphSlider)
        {
            processorRef.setMorphAmount (0.5f);
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::refreshLabels()
{
    // Update registered labels
    for (auto& tl : translatableLabels)
    {
        if (tl.label != nullptr)
            tl.label->setText (ovt::tr (tl.key), juce::dontSendNotification);
    }

    // Update scale combo box items
    {
        const char* scaleKeys[] = {
            ovt::Keys::kScaleChromatic, ovt::Keys::kScaleMajor,
            ovt::Keys::kScaleMelodicMinor, ovt::Keys::kScaleHarmonicMinor,
            ovt::Keys::kScaleNaturalMinor, ovt::Keys::kScaleMajorPentatonic,
            ovt::Keys::kScaleMinorPentatonic, ovt::Keys::kScaleBlues,
            ovt::Keys::kScaleDorian, ovt::Keys::kScalePhrygian,
            ovt::Keys::kScaleLydian, ovt::Keys::kScaleMixolydian,
            ovt::Keys::kScaleLocrian, ovt::Keys::kScaleCustom
        };
        const int currentScale = scaleBox.getSelectedItemIndex();
        scaleBox.clear (juce::dontSendNotification);
        for (int i = 0; i < 14; ++i)
            scaleBox.addItem (ovt::tr (scaleKeys[i]), i + 1);
        if (currentScale >= 0)
            scaleBox.setSelectedItemIndex (currentScale, juce::dontSendNotification);
    }

    // Update harmony combo box items (all 22)
    {
        const char* harmonyKeys[] = {
            ovt::Keys::kHarmonyNone, ovt::Keys::kHarmony3rdBelow,
            ovt::Keys::kHarmony3rdAbove, ovt::Keys::kHarmony3rdBelowAbove,
            ovt::Keys::kHarmony4thBelow, ovt::Keys::kHarmony4thAbove,
            ovt::Keys::kHarmony4thBelowAbove, ovt::Keys::kHarmony5thBelow,
            ovt::Keys::kHarmony5thAbove, ovt::Keys::kHarmony5thBelowAbove,
            ovt::Keys::kHarmony3rdBelow5thAbove, ovt::Keys::kHarmony5thBelow3rdAbove,
            ovt::Keys::kHarmonyOctaveBelow, ovt::Keys::kHarmonyOctaveAbove,
            ovt::Keys::kHarmonyOctaveBelowAbove, ovt::Keys::kHarmonyVocalStack3,
            ovt::Keys::kHarmonyVocalStack4, ovt::Keys::kHarmonyPowerChord,
            ovt::Keys::kHarmonyParallel3rd, ovt::Keys::kHarmonyDrone,
            ovt::Keys::kHarmonyUnison2, ovt::Keys::kHarmonyUnisonOctaves4
        };
        const int currentHarmony = harmonyTypeBox.getSelectedItemIndex();
        harmonyTypeBox.clear (juce::dontSendNotification);
        for (int i = 0; i < 22; ++i)
            harmonyTypeBox.addItem (ovt::tr (harmonyKeys[i]), i + 1);
        if (currentHarmony >= 0)
            harmonyTypeBox.setSelectedItemIndex (juce::jmin (currentHarmony, 21), juce::dontSendNotification);
    }

    // Update Voice Type combo box items (6). Labels already include the note
    // range suffix (e.g. "Bass (E2-E4)") so the user sees the range directly
    // in the menu without needing a tooltip.
    {
        const char* voiceTypeKeys[] = {
            ovt::Keys::kVoiceTypeUniversal, ovt::Keys::kVoiceTypeBass,
            ovt::Keys::kVoiceTypeBaritone, ovt::Keys::kVoiceTypeTenor,
            ovt::Keys::kVoiceTypeAlto, ovt::Keys::kVoiceTypeSoprano
        };
        const int currentVoiceType = voiceTypeBox.getSelectedItemIndex();
        voiceTypeBox.clear (juce::dontSendNotification);
        for (int i = 0; i < 6; ++i)
            voiceTypeBox.addItem (ovt::tr (voiceTypeKeys[i]), i + 1);
        if (currentVoiceType >= 0)
            voiceTypeBox.setSelectedItemIndex (juce::jmin (currentVoiceType, 5), juce::dontSendNotification);
    }

    // Update "Use Voice" toggle button text
    useVoiceButton.setButtonText (ovt::tr (ovt::Keys::kLabelUseVoice));

    // Update button text
    updateButton.setButtonText (ovt::tr(ovt::Keys::kLabelUpdates));
    harmonyEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyBtn));
    harmonyFollowLeadButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyFollow));
    harmonyGainMatchButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyGainMatch));
    formantEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelFormantBtn));
    reverbEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelReverbBtn));
    bypassToggleButton.setButtonText (ovt::tr(ovt::Keys::kLabelBypassBtn));
    midiToggleButton.setButtonText (ovt::tr(ovt::Keys::kLabelMidiOutBtn));
    correctionModeButton.setButtonText (correctionModeButton.getToggleState() ? ovt::tr(ovt::Keys::kLabelTransparentBtn) : ovt::tr(ovt::Keys::kLabelModernBtn));
    harmonyToneColorLabel.setText (ovt::tr(ovt::Keys::kLabelTone), juce::dontSendNotification);
    noiseGateEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelNoiseGate));
    noiseGateThresholdLabel.setText (ovt::tr(ovt::Keys::kLabelThreshold), juce::dontSendNotification);

    // Refresh all translatable tooltips
    advancedButton.setTooltip (ovt::tr (ovt::Keys::kTooltipAdvanced));
    voiceTypeBox.setTooltip (ovt::tr (ovt::Keys::kTooltipVoiceType));
    voiceTypeLabel.setTooltip (ovt::tr (ovt::Keys::kTooltipVoiceType));
    formantStrategyBox.setTooltip (ovt::tr (ovt::Keys::kTooltipFormantStrategy));
    formantStrategyLabel.setTooltip (ovt::tr (ovt::Keys::kLabelFormantStrategy));
    harmonyFollowLeadButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyFollow));
    harmonyGainMatchButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyGainMatch));
    updateButton.setTooltip (ovt::tr(ovt::Keys::kTooltipCheckUpdates));
    menuButton.setTooltip (ovt::tr(ovt::Keys::kTooltipMenuOptions));
    bypassButton.setTooltip (ovt::tr (ovt::Keys::kTooltipBypassIcon));
    midiOutButton.setTooltip (ovt::tr(ovt::Keys::kTooltipMidiOutIcon));
    bypassToggleButton.setTooltip (ovt::tr (ovt::Keys::kTooltipBypass));
    midiToggleButton.setTooltip (ovt::tr (ovt::Keys::kTooltipMidiOutIcon));
    correctionModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    harmonyEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyEn));
    reverbEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipReverbEn));
    formantEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipFormant));
    formantSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipFormant));
    reverbMixSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipReverbEn));
    noiseGateEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipNoiseGate));
    noiseGateThresholdSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipThreshold));
    flexTuneSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipFlexTune));
    humanizeSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHumanize));
    vibratoPreserveSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipVibrato));
    attackReleaseSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipAttackRelease));
    attackAwareButton.setTooltip (ovt::tr (ovt::Keys::kTooltipAttack));
    attackAwareButton.setButtonText (ovt::tr (ovt::Keys::kLabelAttackBtn));
    keySourceBox.setTooltip (ovt::tr (ovt::Keys::kTooltipKeySource));
    companionGroupBox.setTooltip (ovt::tr (ovt::Keys::kTooltipCompanionGroup));
    speedSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSpeed));
    amountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipAmount));
    harmonyGainSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipVolume));
    harmonyBlendSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipBlend));
    harmonyAttackSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyAttack));
    harmonyFormantSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyFormant));
    harmonyToneColorSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipToneColor));
    buttonA.setTooltip (ovt::tr(ovt::Keys::kTooltipAbSlotA));
    buttonB.setTooltip (ovt::tr(ovt::Keys::kTooltipAbSlotB));
    morphSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphDrag));
    morphSliderLabel.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphLabel));
    morphSliderLabel.setText (ovt::tr(ovt::Keys::kLabelMorph), juce::dontSendNotification);

    // Refresh drawable button tooltips (already use ovt::tr but need refresh)
    optionsButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCurveOptions));
    snapButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSnapToScale));
    snapGridButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSnapToGrid));
    stepModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipStepMode));
    zoomInButton.setTooltip (ovt::tr (ovt::Keys::kTooltipZoomIn));
    zoomOutButton.setTooltip (ovt::tr (ovt::Keys::kTooltipZoomOut));
    scrollUpButton.setTooltip (ovt::tr (ovt::Keys::kTooltipScrollUp));
    scrollDownButton.setTooltip (ovt::tr (ovt::Keys::kTooltipScrollDown));
    resetViewButton.setTooltip (ovt::tr (ovt::Keys::kTooltipResetView));
    rewindButton.setTooltip (ovt::tr (ovt::Keys::kTooltipRewind));

    // Refresh tooltips for components that were created in the constructor
    // and were NOT being updated on language change. Without these calls
    // the user would see the tooltip in the language that was active at
    // first launch, even after switching to a different one via
    // wrench/Interface/Language. Affects: Upward Compress (enable + amount
    // knob), Modern/Transparent pill switch, and the plugin preset
    // prev/next/save + gallery buttons.
    upwardCompEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipUpwardCompEn));
    upwardCompAmountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipUpwardCompAmount));
    correctionModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    if (modeSwitch != nullptr)
        modeSwitch->setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    presetPrevButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPluginPresetPrev));
    presetNextButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPluginPresetNext));
    presetSaveButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPluginPresetSave));
    presetGalleryButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPresetGallery));
    undoButton.setTooltip (ovt::tr (ovt::Keys::kTooltipUndo));
    redoButton.setTooltip (ovt::tr (ovt::Keys::kTooltipRedo));

    // "Measures" control label follows the active language.
    measuresLabel.setText (ovt::tr (ovt::Keys::kLabelMeasures), juce::dontSendNotification);

    // Update tab names
    tabbedComponent.setTabName (0, ovt::tr(ovt::Keys::kTabLive));
    tabbedComponent.setTabName (1, ovt::tr(ovt::Keys::kTabCurveEditor));

    // Refresh curve editor embedded labels
    if (curveEditor != nullptr)
        curveEditor->refreshTranslations();

    // Re-run the layout so width-dependent labels (e.g. the "Measures" control)
    // are re-sized to the new language text instead of staying clipped at the
    // previous (shorter/longer) width.
    resized();

    repaint();
}

void OpenVoxTunerAudioProcessorEditor::applyPresetUiStateFromXml (const juce::XmlElement& xml)
{
    const int keyIdx = xml.getIntAttribute ("key", keyBox.getSelectedItemIndex());
    const int scaleIdx = xml.getIntAttribute ("scale", scaleBox.getSelectedItemIndex());
    const bool snapScale = xml.getBoolAttribute ("snapScale", snapButton.getToggleState());
    const bool snapGrid = xml.getBoolAttribute ("snapGrid", snapGridButton.getToggleState());
    const bool stepMode = xml.getBoolAttribute ("stepMode", stepModeButton.getToggleState());

    keyBox.setSelectedItemIndex (juce::jlimit (0, 11, keyIdx), juce::sendNotificationSync);
    scaleBox.setSelectedItemIndex (juce::jlimit (0, 13, scaleIdx), juce::sendNotificationSync);

    snapButton.setToggleState (snapScale, juce::dontSendNotification);
    snapGridButton.setToggleState (snapGrid, juce::dontSendNotification);
    stepModeButton.setToggleState (stepMode, juce::dontSendNotification);

    if (curveEditor != nullptr)
    {
        curveEditor->setSnapEnabled (snapScale);
        curveEditor->setSnapToGridEnabled (snapGrid);
        curveEditor->setStepModeEnabled (stepMode);
    }
}

void OpenVoxTunerAudioProcessorEditor::loadCustomPresetFromFile (const juce::File& file)
{
    if (curveEditor == nullptr) return;
    if (! file.existsAsFile()) return;

    pushUndoAroundCall ([this, &file] {
        resetMorph(); // cancel any active morph when loading a preset

        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument (file).getDocumentElement());
        if (xml == nullptr) return;

        const juce::XmlElement* root = xml.get();
        const juce::XmlElement* curveXml = nullptr;

        if (xml->hasTagName ("OVT_PRESET"))
            curveXml = xml->getChildByName ("PITCH_CURVE");
        else if (xml->hasTagName ("PITCH_CURVE"))
            curveXml = xml.get();

        if (curveXml == nullptr) return;

        ovtdsp::PitchCurve newCurve;
        newCurve.fromXml (*curveXml);
        curveEditor->setCurve (newCurve);
        syncEditButtons();

        if (root->hasTagName ("OVT_PRESET"))
            applyPresetUiStateFromXml (*root);
        else
        {
            // Reflect the effective Step Mode (forced ON in Piano Roll mode) rather
            // than the raw preset value.
            stepModeButton.setToggleState (curveEditor->isStepModeEnabled(), juce::dontSendNotification);
            applyPresetUiStateFromXml (*root);
        }
    });
}

void OpenVoxTunerAudioProcessorEditor::promptSaveCustomPreset()
{
    if (curveEditor == nullptr) return;

    auto* w = new juce::AlertWindow (ovt::tr(ovt::Keys::kDlgSavePreset),
                                     ovt::tr(ovt::Keys::kDlgSavePresetDesc),
                                     juce::AlertWindow::NoIcon);
    w->addTextEditor ("name", "", "Name:");
    w->addButton (ovt::tr(ovt::Keys::kDlgSave), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ovt::tr(ovt::Keys::kMenuCancel), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        std::unique_ptr<juce::AlertWindow> cleanup (w);
        if (result == 0 || curveEditor == nullptr)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         ovt::tr(ovt::Keys::kDlgInvalidName),
                                                         ovt::tr(ovt::Keys::kDlgEmptyName),
                                                         this);
            return;
        }

        const auto dir = getUserPresetsDirectory();
        const auto fileStem = sanitizePresetFileStem (name);
        const auto file = dir.getChildFile (fileStem + ".xml");

        if (file.existsAsFile())
        {
            auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                                   ovt::tr(ovt::Keys::kDlgOverwrite),
                                                                   ovt::tr(ovt::Keys::kDlgOverwriteDesc),
                                                                   ovt::tr(ovt::Keys::kMenuCancel), ovt::tr(ovt::Keys::kDlgOverwriteBtn),
                                                                   this);

            juce::NativeMessageBox::showAsync (opts, [this, file, name] (int result)
            {
                if (result != 2)
                    return;
                writeCustomPresetFile (name, file);
    });
            return;
        }

        writeCustomPresetFile (name, file);
    }), true);
}

void OpenVoxTunerAudioProcessorEditor::writeCustomPresetFile (const juce::String& name, const juce::File& file)
{
    if (curveEditor == nullptr || name.isEmpty() || file == juce::File())
        return;

    auto root = std::make_unique<juce::XmlElement> ("OVT_PRESET");
    root->setAttribute ("name", name);
    root->setAttribute ("key", keyBox.getSelectedItemIndex());
    root->setAttribute ("scale", scaleBox.getSelectedItemIndex());
    root->setAttribute ("snapScale", snapButton.getToggleState() ? 1 : 0);
    root->setAttribute ("snapGrid", snapGridButton.getToggleState() ? 1 : 0);
    root->setAttribute ("stepMode", stepModeButton.getToggleState() ? 1 : 0);

    auto curveXml = curveEditor->getCurve().toXml();
    if (curveXml != nullptr)
        root->addChildElement (curveXml.release());

    const bool ok = file.replaceWithText (root->toString());
    if (ok)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                     ovt::tr(ovt::Keys::kDlgPresetSaved),
                                                     ovt::tr(ovt::Keys::kDlgPresetSavedDesc) + name,
                                                     this);
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     ovt::tr(ovt::Keys::kDlgSaveFailed),
                                                     ovt::tr(ovt::Keys::kDlgSaveFailedDesc),
                                                     this);
    }
}

void OpenVoxTunerAudioProcessorEditor::deleteCustomPresetFile (const juce::File& file, std::function<void (bool)> onDone,
                                                              juce::Component* parentComp)
{
    if (! file.existsAsFile())
    {
        if (onDone) onDone (false);
        return;
    }

    // When called from the preset gallery (a separate DocumentWindow above the
    // editor), parent the dialog to the gallery so it appears in front of it
    // instead of behind. Defaults to the editor when no parent is supplied.
    juce::Component* dialogParent = (parentComp != nullptr) ? parentComp : this;

    auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                           ovt::tr(ovt::Keys::kDlgDeletePreset),
                                                           ovt::tr(ovt::Keys::kDlgDeletePresetDesc) + file.getFileNameWithoutExtension(),
                                                           ovt::tr(ovt::Keys::kMenuCancel), ovt::tr(ovt::Keys::kDlgDelete),
                                                           dialogParent);

    juce::NativeMessageBox::showAsync (opts, [this, file, onDone, dialogParent] (int result)
    {
        // Debug: display the result value
        // Debug: show the actual result value
        if (result != 1) // Delete button id is 1 in makeOptionsYesNo
        {
            if (onDone) onDone (false);
            return;
        }

        // Ensure file is writable before attempting deletion
        file.setReadOnly(false);
        const bool deleted = file.deleteFile();
        if (!deleted || file.existsAsFile())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                             ovt::tr(ovt::Keys::kDlgDeleteFailed),
                                                             ovt::tr(ovt::Keys::kDlgDeleteFailedDesc),
                                                             dialogParent);
            if (onDone) onDone (false);
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                             ovt::tr(ovt::Keys::kDlgPresetDeleted),
                                                             ovt::tr(ovt::Keys::kDlgPresetDeletedDesc) + file.getFileNameWithoutExtension(),
                                                             dialogParent);
            // Refresh the gallery (if open) or fall back to the preset menu.
            if (onDone)
                onDone (true);
            else
                showPresetsMenu();
        }
    });
}

// === A/B Comparison ===

/**
 * Saves the current plugin state into an A/B slot using a direct MorphState
 * snapshot. Persists the MorphState to the processor for project reload.
 */
void OpenVoxTunerAudioProcessorEditor::saveSlot (ABState& slot, int slotIndex)
{
    // Use a safe curve reference - pitchCurve may be null during init.
    const ovtdsp::PitchCurve emptyCurve;
    const auto& curve = processorRef.hasPitchCurve() ? processorRef.getPitchCurve() : emptyCurve;

    slot.morphState = std::make_unique<ovtdsp::MorphState> (
        ovtdsp::captureState (processorRef.getParameters(), curve,
                             slotIndex == 0 ? "Slot A" : "Slot B"));
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
    if (slotIndex == 0)
        juce::Logger::writeToLog (
            "[DIAG] saveSlot(A): type=" + juce::String (slot.morphState->harmonyType)
            + " vibrato=" + juce::String (slot.morphState->vibratoPreserve, 4)
            + " humanize=" + juce::String (slot.morphState->humanize, 4)
            + " formant=" + juce::String (slot.morphState->harmonyFormant, 4));
#endif
    slot.hasData = true;
    slot.name = "filled";

    processorRef.setAbSlotMorphState (slotIndex, *slot.morphState);
}

/**
 * Loads a saved A/B slot by setting UI sliders directly, which triggers
 * the SliderAttachment to update the processor parameter.
 * This "slider-first" approach works reliably because SliderAttachment
 * listens to slider changes (Slider::Listener) and syncs them to params.
 * For discrete/boolean params without sliders, we use setValueNotifyingHost.
 */
void OpenVoxTunerAudioProcessorEditor::loadSlot (const ABState& slot)
{
    if (! slot.hasData || slot.morphState == nullptr) return;

    switchingSlot = true;

    auto& params = processorRef.getParameters();
    const auto& saved = *slot.morphState;

    // Set continuous parameter sliders directly. The SliderAttachment will
    // see the value change and call param->setValueNotifyingHost to sync.
    auto setSlider = [&params] (const juce::String& id, float normValue, juce::Slider& slider)
    {
        if (auto* p = params.getParameter (id))
            slider.setValue (p->convertFrom0to1 (normValue), juce::sendNotificationSync);
    };
    setSlider ("speed",              saved.speed,              speedSlider);
    setSlider ("amount",             saved.amount,             amountSlider);
    setSlider ("formant",            saved.formant,            formantSlider);
    setSlider ("harmony_gain",       saved.harmonyGain,        harmonyGainSlider);
    setSlider ("harmony_blend",      saved.harmonyBlend,       harmonyBlendSlider);
    setSlider ("harmony_attack",     saved.harmonyAttack,      harmonyAttackSlider);
    setSlider ("harmony_formant",    saved.harmonyFormant,     harmonyFormantSlider);
    setSlider ("harmony_tone_color", saved.harmonyToneColor,   harmonyToneColorSlider);
    setSlider ("reverb_mix",         saved.reverbMix,          reverbMixSlider);
    setSlider ("noise_gate_threshold", saved.noiseGateThreshold, noiseGateThresholdSlider);
    setSlider ("upward_comp_amount",   saved.upwardCompAmount,   upwardCompAmountSlider);
    setSlider ("flex_tune",          saved.flexTune,           flexTuneSlider);
    setSlider ("humanize",           saved.humanize,           humanizeSlider);
    setSlider ("vibrato_preserve",    saved.vibratoPreserve,    vibratoPreserveSlider);
    setSlider ("attack_release",      saved.attackRelease,      attackReleaseSlider);
    attackAwareButton.setToggleState (saved.attackAware > 0.5f, juce::dontSendNotification);

    // Backward-compatibility: old Vocoder-like (index 5) -> Organ (index 1)
    int curTone = saved.harmonyTone;
    if (curTone == 5) curTone = 1; // Vocoder-like was the last item (index 5) in the old 6-item list
    // Note: old indices 1..4 were removed (Bright, SynthLead, Strings, Guitar) -
    // any project using those will silently map to Choir (0), which is the default.

    // Discrete parameters: use setValueNotifyingHost (ComboBox attachments sync)
    auto setChoice = [&params] (const juce::String& id, float normValue)
    {
        if (auto* p = params.getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normValue));
    };
    setChoice ("key",                      (float) saved.key / 11.0f);
    setChoice ("scale",                    (float) saved.scale / 13.0f);
    setChoice ("harmony_type",             (float) saved.harmonyType / 21.0f);
    setChoice ("harmony_tone",             (float) curTone / 1.0f);
    setChoice ("harmony_shifted_voices",   (float) (saved.harmonyShiftedVoices - 1) / 3.0f);
    setChoice ("latency_mode",             (float) saved.latencyMode / 3.0f);
    setChoice ("editor_measures",          (float) (saved.editorMeasures - 1) / 31.0f);
    setChoice ("voice_type",               (float) saved.voiceType / 5.0f);
    // Continuous-style morph params (key_source / companion_group are
    // normalised 0..1; key_detect steps). They were captured by captureState
    // but never re-applied here, so a slot switch left them at the previous
    // slot's values.
    setChoice ("key_source",               saved.keySource);
    setChoice ("companion_group",          saved.companionGroup);
    setChoice ("key_detect",               saved.keyDetect);

    // Boolean parameters: use setValueNotifyingHost (Button attachments sync)
    setChoice ("formant_enable",    saved.formantEnable ? 1.0f : 0.0f);
    setChoice ("bypass",            saved.bypass ? 1.0f : 0.0f);
    setChoice ("harmony_enable",    saved.harmonyEnable ? 1.0f : 0.0f);
    setChoice ("harmony_use_voice", saved.harmonyUseVoice ? 1.0f : 0.0f);
    setChoice ("harmony_follow_lead", saved.harmonyFollowLead ? 1.0f : 0.0f);
    setChoice ("reverb_enable",     saved.reverbEnable ? 1.0f : 0.0f);
    setChoice ("noise_gate_enable", saved.noiseGateEnable ? 1.0f : 0.0f);
    setChoice ("upward_comp_enable", saved.upwardCompEnable ? 1.0f : 0.0f);
    setChoice ("correction_mode",   saved.correctionMode ? 1.0f : 0.0f);

    // Restore the pitch curve
    if (curveEditor != nullptr)
        curveEditor->setCurve (saved.curve);

    switchingSlot = false;
}

/** Update A/B button visual states and morph slider visibility. */
void OpenVoxTunerAudioProcessorEditor::updateABButtonStates()
{
    buttonA.hasValidData = true; // A is always valid (default state)
    buttonB.hasValidData = slotB.hasData;
    buttonA.isActive = isSlotAActive;
    buttonB.isActive = ! isSlotAActive;
    buttonA.repaint();
    buttonB.repaint();

    const bool bothFilled = slotA.hasData && slotB.hasData;
    morphSlider.setVisible (bothFilled);
    morphSliderLabel.setVisible (bothFilled);
    if (! bothFilled && morphSource != nullptr)
    {
        processorRef.setMorphAmount (0.0f);
        lastMorphValue = 0.0f;
        morphSource.reset();
        morphTarget.reset();
        morphUndoState.reset();
    }
}

// === Plugin Presets (separate from Curve Presets) ===

namespace
{
    // Ordered factory plugin-preset names. "Default" is first and contains
    // the plugin's factory-default parameter values.
    const juce::StringArray& getFactoryPluginPresetNames()
    {
        static const juce::StringArray names {
            "Default",
            "Pure Correction",
            "Transparent / Doubling",
            "Studio Stack",
            "Live Scene",
            "Spoken Voice / Podcast",
            "Creative Robot",
            "Broadcast Voice",
            "Breathy Vocals",
            "Lead Vocal"
        };
        return names;
    }

    // Returns the parameter overrides (id -> value) for a factory preset, or
    // nullptr for "Default" (which uses the factory defaults verbatim).
    const std::map<juce::String, float>* getFactoryPluginPresetOverrides (const juce::String& name)
    {
        static const std::map<juce::String, float> correctionPure = {
            { "amount", 1.0f }, { "flex_tune", 0.0f }, { "humanize", 0.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", false },
            { "vibrato_preserve", 0.0f }, { "attack_aware", 0.0f },
            { "reverb_enable", 0.0f }, { "noise_gate_enable", 0.0f }
        };
        static const std::map<juce::String, float> transparentDub = {
            { "amount", 0.85f }, { "flex_tune", 25.0f }, { "humanize", 15.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", false },
            { "reverb_enable", 0.0f }, { "correction_mode", 0.0f }
        };
        static const std::map<juce::String, float> studioStack = {
            { "amount", 0.9f }, { "flex_tune", 15.0f }, { "humanize", 20.0f },
            { "harmony_enable", 1.0f }, { "harmony_type", 17.0f }, // Unison + Octaves (4 voices)
            { "harmony_gain_match", 1.0f }, { "harmony_follow_lead", 1.0f },
            { "harmony_gain", 0.75f }, { "harmony_blend", 0.5f },
            { "formant_enable", 1.0f }, { "formant_mode", 1.0f }
        };
        static const std::map<juce::String, float> liveScene = {
            { "amount", 0.9f }, { "flex_tune", 10.0f }, { "humanize", 30.0f },
            { "harmony_enable", 0.0f }, { "attack_aware", 1.0f },
            { "latency_mode", 0.0f }, // Direct Monitoring
            { "reverb_enable", 0.0f }, { "noise_gate_enable", 0.0f }
        };
        static const std::map<juce::String, float> spokenPodcast = {
            { "amount", 0.7f }, { "speed", 40.0f }, { "flex_tune", 35.0f },
            { "humanize", 25.0f }, { "vibrato_preserve", 0.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", 1.0f },
            { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "reverb_enable", 0.0f }, { "noise_gate_enable", 1.0f },
            { "noise_gate_threshold", -45.0f },
            // Spoken/quiet material: lift weak passages before tuning so the
            // pitch detector and correction see a more consistent level.
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.6f }
        };
        static const std::map<juce::String, float> creativeRobot = {
            { "amount", 1.0f }, { "flex_tune", 0.0f }, { "humanize", 0.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", 1.0f },
            { "formant_mode", 1.0f }, { "formant", 3.0f }, // strong formant shift
            { "vibrato_preserve", 0.0f }
        };
        // Broadcast Voice: strong, consistent level for streaming/radio. A firm
        // gate cuts room tone between phrases and the upward compressor lifts
        // any dip so the loudness stays even and present on air.
        static const std::map<juce::String, float> broadcastVoice = {
            { "amount", 0.9f }, { "speed", 30.0f }, { "flex_tune", 20.0f },
            { "humanize", 20.0f }, { "harmony_enable", 0.0f },
            { "formant_enable", 1.0f }, { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "reverb_enable", 0.0f }, { "noise_gate_enable", 1.0f },
            { "noise_gate_threshold", -50.0f },
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.8f }
        };
        // Breathy Vocals: airy, weak indie/lo-fi voice. No gate (keep the natural
        // breath), just a gentle upward lift so the quiet, breathy parts stay
        // intelligible and sit in tune without losing their character.
        static const std::map<juce::String, float> breathyVocals = {
            { "amount", 0.8f }, { "speed", 50.0f }, { "flex_tune", 30.0f },
            { "humanize", 35.0f }, { "vibrato_preserve", 0.3f },
            { "harmony_enable", 0.0f }, { "formant_enable", 1.0f },
            { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "reverb_enable", 1.0f }, { "reverb_mix", 0.25f }, { "noise_gate_enable", 0.0f },
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.6f }
        };
        // Lead Vocal: the flagship "thickened lead" sound. A light gate tames
        // room tone, "-3 / +3" harmony (3rd below + above) fattens the lead
        // without smearing it, a touch of reverb adds space, and a moderate
        // upward compression keeps the lead consistent and present.
        static const std::map<juce::String, float> leadVocal = {
            { "amount", 0.9f }, { "speed", 40.0f }, { "flex_tune", 15.0f },
            { "humanize", 20.0f }, { "vibrato_preserve", 0.2f },
            { "harmony_enable", 1.0f }, { "harmony_type", 3.0f }, // 3rd below + above (-3 / +3)
            { "harmony_gain_match", 1.0f }, { "harmony_follow_lead", 1.0f },
            { "harmony_gain", 0.6f }, { "harmony_blend", 0.5f },
            { "harmony_tone_color", 0.5f },
            { "formant_enable", 1.0f }, { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "reverb_enable", 1.0f }, { "reverb_mix", 0.25f },
            { "noise_gate_enable", 1.0f }, { "noise_gate_threshold", -40.0f },
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.5f }
        };

        if (name == "Pure Correction")       return &correctionPure;
        if (name == "Transparent / Doubling") return &transparentDub;
        if (name == "Studio Stack")          return &studioStack;
        if (name == "Live Scene")            return &liveScene;
        if (name == "Spoken Voice / Podcast") return &spokenPodcast;
        if (name == "Creative Robot")        return &creativeRobot;
        if (name == "Broadcast Voice")       return &broadcastVoice;
        if (name == "Breathy Vocals")        return &breathyVocals;
        if (name == "Lead Vocal")            return &leadVocal;
        return nullptr; // "Default" -> factory defaults
    }
}

void OpenVoxTunerAudioProcessorEditor::refreshPluginPresetList()
{
    pluginPresetNames.clear();
    pluginPresetNames.addArray (getFactoryPluginPresetNames());

    const auto dir = getUserPluginPresetsDirectory();
    const auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
        pluginPresetNames.add (f.getFileNameWithoutExtension());

    // "User" marks a non-preset (modified) state, shown when no named preset matches.
    if (! pluginPresetNames.contains ("User"))
        pluginPresetNames.add ("User");

    presetComboBox.clear();
    presetComboBox.addItemList (pluginPresetNames, 1);
}

void OpenVoxTunerAudioProcessorEditor::applyPluginPresetByName (const juce::String& name)
{
    if (name == "User" || name.isEmpty())
        return;

    const juce::ValueTree state = buildPluginPresetStateByName (name);
    if (! state.isValid())
        return;

    processorRef.applyPluginPresetState (state);
    currentPluginPresetName = name;
    presetComboBox.setText (name, juce::dontSendNotification);

    // Record the assigned preset on the active A/B slot so that switching
    // slots later can show the preset name (or "User" if it was edited).
    (isSlotAActive ? slotA : slotB).presetName = name;
}

juce::ValueTree OpenVoxTunerAudioProcessorEditor::buildPluginPresetStateByName (const juce::String& name)
{
    if (name == "User" || name.isEmpty())
        return {};

    // Factory preset: build the state from the default tree + overrides.
    const auto& factoryNames = getFactoryPluginPresetNames();
    if (factoryNames.contains (name))
    {
        juce::ValueTree state = processorRef.getDefaultPluginState().createCopy();
        if (const auto* ov = getFactoryPluginPresetOverrides (name))
        {
            for (const auto& [id, val] : *ov)
            {
                juce::ValueTree paramChild = state.getChildWithProperty ("id", id);
                if (paramChild.isValid())
                    paramChild.setProperty ("value", val, nullptr);
            }
        }
        return state;
    }

    // Custom preset: parse the XML file from the Plugin presets folder.
    const auto file = getUserPluginPresetsDirectory().getChildFile (name + ".xml");
    if (! file.existsAsFile())
        return {};

    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument (file).getDocumentElement());
    if (xml == nullptr)
        return {};

    const juce::XmlElement* paramsXml = nullptr;
    if (xml->hasTagName ("OVT_PLUGIN_PRESET"))
    {
        paramsXml = xml->getChildByName (processorRef.getParameters().state.getType());
        if (paramsXml == nullptr)
            paramsXml = xml->getChildByName ("PARAMETERS");
    }
    else if (xml->hasTagName (processorRef.getParameters().state.getType()))
    {
        paramsXml = xml.get();
    }

    if (paramsXml == nullptr)
        return {};

    return juce::ValueTree::fromXml (*paramsXml);
}

bool OpenVoxTunerAudioProcessorEditor::slotMatchesPreset (const ABState& slot)
{
    if (slot.presetName.isEmpty())
        return false;

    const juce::ValueTree expected = buildPluginPresetStateByName (slot.presetName);
    if (! expected.isValid())
        return false;

    // Compare only the parameters the preset defines (the preset's children).
    // The stored "value" is the parameter's normalized 0..1 value, which is
    // exactly what AudioProcessorParameter::getValue() returns, so a direct
    // tolerance comparison is correct. UI language / theme / morph / mode are
    // intentionally excluded (presets never touch them).
    const auto& params = processorRef.getParameters();
    for (int i = 0; i < expected.getNumChildren(); ++i)
    {
        const auto child = expected.getChild (i);
        const auto idVar = child.getProperty ("id", juce::var());
        if (idVar.isVoid())
            continue;
        const juce::String id = idVar;
        if (auto* p = params.getParameter (id))
        {
            const float liveVal   = p->getValue();
            const float presetVal = static_cast<float> (child.getProperty ("value", 0.0));
            if (std::abs (liveVal - presetVal) > 1.0e-3f)
                return false;
        }
    }
    return true;
}

void OpenVoxTunerAudioProcessorEditor::promptSavePluginPreset()
{
    auto* w = new juce::AlertWindow (ovt::tr(ovt::Keys::kDlgSavePluginPreset),
                                      ovt::tr(ovt::Keys::kDlgSavePluginPresetDesc),
                                      juce::AlertWindow::NoIcon);
    w->addTextEditor ("name", "", "Name:");
    w->addButton (ovt::tr(ovt::Keys::kDlgSave), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ovt::tr(ovt::Keys::kMenuCancel), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result) {
        if (result == 0)
        {
            delete w;
            return;
        }

        const juce::String name = w->getTextEditorContents ("name").trim();
        delete w;

        if (name.isEmpty())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         ovt::tr(ovt::Keys::kDlgInvalidName),
                                                         ovt::tr(ovt::Keys::kDlgEmptyName),
                                                         this);
            return;
        }

        const auto file = getUserPluginPresetsDirectory()
                             .getChildFile (sanitizePresetFileStem (name) + ".xml");

        if (file.existsAsFile())
        {
            auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                                   ovt::tr(ovt::Keys::kDlgOverwrite),
                                                                   ovt::tr(ovt::Keys::kDlgOverwriteDesc),
                                                                   ovt::tr(ovt::Keys::kMenuCancel),
                                                                   ovt::tr(ovt::Keys::kDlgOverwriteBtn),
                                                                   this);
            juce::NativeMessageBox::showAsync (opts, [this, file, name] (int result) {
                if (result != 2)
                    return;
                writePluginPresetFile (name, file);
                refreshPluginPresetList();
                currentPluginPresetName = name;
                presetComboBox.setText (name, juce::dontSendNotification);
            });
            return;
        }

        writePluginPresetFile (name, file);
        refreshPluginPresetList();
        currentPluginPresetName = name;
        presetComboBox.setText (name, juce::dontSendNotification);
    }), true);
}

void OpenVoxTunerAudioProcessorEditor::writePluginPresetFile (const juce::String& name, const juce::File& file)
{
    // A Plugin Preset captures parameters.state (no pitch curve) plus a few
    // UI-only preferences. Serialised as <OVT_PLUGIN_PRESET> with a
    // <PARAMETERS> child so it never collides with Curve Preset XML shape.
    auto stateXml = processorRef.getPluginPresetState().createXml();
    if (stateXml == nullptr)
        return;

    juce::XmlElement root ("OVT_PLUGIN_PRESET");
    root.setAttribute ("name", name);
    root.setAttribute ("advancedExpanded", advancedExpanded ? 1 : 0);
    // stateXml already carries the real parameters.state root type (e.g.
    // "OpenVoxTuner") with <PARAM id=.. value=..> children, so restoreState()
    // via replaceState() keeps the tree type consistent.
    root.addChildElement (stateXml.release());

    const bool ok = file.replaceWithText (root.toString());
    if (ok)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                     ovt::tr(ovt::Keys::kDlgPresetSaved),
                                                     ovt::tr(ovt::Keys::kDlgPresetSavedDesc) + name,
                                                     this);
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     ovt::tr(ovt::Keys::kDlgSaveFailed),
                                                     ovt::tr(ovt::Keys::kDlgSaveFailedDesc),
                                                     this);
    }
}

void OpenVoxTunerAudioProcessorEditor::deletePluginPresetFile (const juce::File& file)
{
    file.deleteFile();
    refreshPluginPresetList();
    if (currentPluginPresetName == file.getFileNameWithoutExtension())
    {
        currentPluginPresetName = "User";
        presetComboBox.setText ("User", juce::dontSendNotification);
    }
}

// === MIDI Learn ===
void OpenVoxTunerAudioProcessorEditor::startMidiLearn (const juce::String& parameterId)
{
    learnState.isLearning = true;
    learnState.parameterId = parameterId;
    learnState.assignedCc = -1;

    // Show a brief dialog prompting the user to move a MIDI controller.
    juce::AlertWindow::showMessageBoxAsync (
        juce::AlertWindow::InfoIcon,
        ovt::tr(ovt::Keys::kDlgMidiLearn),
        ovt::tr(ovt::Keys::kDlgMidiLearnDesc),
        ovt::tr(ovt::Keys::kDlgOk));
}

void OpenVoxTunerAudioProcessorEditor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (! learnState.isLearning || ! message.isController()) return;

    const int cc = message.getControllerNumber();
    auto* param = processorRef.getParameters().getParameter (learnState.parameterId);
    if (param != nullptr)
    {
        // Store the CC assignment (could be persisted to state).
        learnState.assignedCc = cc;
        learnState.isLearning = false;
    }
}

void OpenVoxTunerAudioProcessorEditor::applyFactoryPreset (const juce::String& name)
{
    if (curveEditor == nullptr)
        return;
    pushUndoAroundCall ([this, &name] {
        resetMorph(); // cancel any active morph when loading a preset
        ovtdsp::PitchCurve newCurve;
        newCurve.loadPreset (name);
        curveEditor->setCurve (newCurve);

        // A preset replaces the current editor state, so commit it to the active
        // A/B slot. Otherwise switching slots would discard the preset and reload
        // the slot's stale stored curve (the preset only touches the editor curve,
        // not the slot's MorphState).
        if (isSlotAActive)
            saveSlot (slotA, 0);
        else
            saveSlot (slotB, 1);

        // Keep the morph slider aligned with the active slot so the displayed
        // curve matches the slot the preset was applied to (and the auto-save on
        // the next slot switch captures it correctly).
        processorRef.setMorphAmount (isSlotAActive ? 0.0f : 1.0f);

        syncEditButtons();
    });
}

juce::PopupMenu OpenVoxTunerAudioProcessorEditor::buildPresetsMenu()
{
    auto loadFactory = [this] (const juce::String& name) {
        applyFactoryPreset (name);
    };

    juce::PopupMenu factory;
    factory.addItem ("Default",   [loadFactory] { loadFactory ("default");    });
    factory.addSeparator();
    factory.addItem ("Robot (C3)", [loadFactory] { loadFactory ("robot_c3");    });
    factory.addItem ("Robot (C4)", [loadFactory] { loadFactory ("robot_c4");    });
    factory.addSeparator();
    factory.addItem ("Spoken Voice (Male)",   [loadFactory] { loadFactory ("spoken_male");    });
    factory.addItem ("Spoken Voice (Female)", [loadFactory] { loadFactory ("spoken_female");    });
    factory.addSeparator();
    factory.addItem ("Bass",     [loadFactory] { loadFactory ("bass");    });
    factory.addItem ("Baritone", [loadFactory] { loadFactory ("baritone");    });
    factory.addItem ("Tenor",    [loadFactory] { loadFactory ("tenor");    });
    factory.addItem ("Alto",     [loadFactory] { loadFactory ("alto");    });
    factory.addItem ("Mezzo",    [loadFactory] { loadFactory ("mezzo");    });
    factory.addItem ("Soprano",  [loadFactory] { loadFactory ("soprano");    });

    juce::PopupMenu custom;
    const auto dir = getUserPresetsDirectory();
    const auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
        custom.addItem (f.getFileNameWithoutExtension(), [this, f] { loadCustomPresetFromFile (f);    });

    custom.addSeparator();
    custom.addItem (ovt::tr (ovt::Keys::kMenuSavePresetAs), [this] { promptSaveCustomPreset();    });

    juce::PopupMenu deleteMenu;
    for (const auto& f : files)
        deleteMenu.addItem (f.getFileNameWithoutExtension(), [this, f] { deleteCustomPresetFile (f);    });

    custom.addSubMenu (ovt::tr (ovt::Keys::kMenuDeletePreset), deleteMenu, ! files.isEmpty());

    juce::PopupMenu menu;
    // Preset Gallery opens the browsable grid (factory + custom, with thumbnails).
    menu.addItem (ovt::tr (ovt::Keys::kMenuPresetGallery), [this] { showPresetGallery();    });
    menu.addSubMenu (ovt::tr (ovt::Keys::kMenuFactory), factory);
    menu.addSubMenu (ovt::tr (ovt::Keys::kMenuCustom), custom);
    menu.addSeparator();
    menu.addItem (ovt::tr (ovt::Keys::kMenuCleanCurves), [this] {
        if (curveEditor != nullptr) curveEditor->clearCurve();
        processorRef.resetTransportTime();
    });
    return menu;
}

void OpenVoxTunerAudioProcessorEditor::showPresetsMenu (const juce::MouseEvent* mouseEvent)
{
    if (curveEditor == nullptr)
        return;

    juce::PopupMenu menu = buildPresetsMenu();
    applyMenuLookAndFeel (menu, customLookAndFeel);

    auto opts = juce::PopupMenu::Options();
    if (mouseEvent != nullptr)
    {
        opts = opts.withTargetComponent (mouseEvent->eventComponent)
                   .withTargetScreenArea (juce::Rectangle<int> (mouseEvent->getScreenX(), mouseEvent->getScreenY(), 1, 1));
    }
    else
    {
        opts = opts.withTargetComponent (&optionsButton)
                   .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards);
    }

    menu.showMenuAsync (opts, [] (int) {    });
}

void OpenVoxTunerAudioProcessorEditor::showCurveOptionsMenu()
{
    if (curveEditor == nullptr)
        return;

    const bool isStandalone = processorRef.isStandaloneWrapper();
    juce::PopupMenu menu;

    // Auto-Scroll (ticked): follows the playhead during playback. In Standalone
    // the playhead loops on the Measures window, so the option is disabled.
    const bool autoScroll = curveEditor->getAutoScroll();
    menu.addItem (ovt::tr (ovt::Keys::kMenuAutoScroll), ! isStandalone, autoScroll, [this] {
        if (curveEditor == nullptr)
            return;
        const bool next = ! curveEditor->getAutoScroll();
        curveEditor->setAutoScroll (next);
        if (auto* p = processorRef.getParameters().getRawParameterValue ("auto_scroll"))
            const_cast<std::atomic<float>*>(p)->store (next ? 1.0f : 0.0f);
    });

    // Loop Playhead (Measures): in ARA the host owns the timeline (disabled,
    // follows host); in Standalone the playhead always loops on the Measures
    // window (disabled, ticked); in plugin mode the user chooses (enabled).
    const bool isARA = processorRef.isBoundToARA_custom();
    const bool loopItemEnabled = ! isARA && ! isStandalone;
    const bool loopItemTicked = isARA ? false : (isStandalone ? true : processorRef.getPlayheadLoop());
    menu.addItem (ovt::tr (ovt::Keys::kMenuLoopPlayhead), loopItemEnabled, loopItemTicked, [this] {
        if (curveEditor == nullptr)
            return;
        processorRef.setPlayheadLoop (! processorRef.getPlayheadLoop());
    });

    menu.addSeparator();

    // === Display options (Curve Editor only) ===
    // Both "Show Input Trace" (red line) and "Show Harmonies Trace" (blue
    // lines) are kept together here because they belong to the same logical
    // group: visual overlays of the curve editor. The PitchVisualizer is
    // unaffected and manages its own overlays via the harmony engine.
    {
        const bool showInputTrace = curveEditor->getShowInputTrace();
        menu.addItem (ovt::tr (ovt::Keys::kMenuShowInputTrace), true, showInputTrace, [this] {
            if (curveEditor == nullptr)
                return;
            const bool next = ! curveEditor->getShowInputTrace();
            curveEditor->setShowInputTrace (next);
        });

        // Show Harmonies Trace: toggles the blue harmony voice lines
        // in the Curve Editor only.
        menu.addItem (ovt::tr (ovt::Keys::kMenuShowHarmoniesTrace), true, showHarmoniesTrace, [this] {
            showHarmoniesTrace = ! showHarmoniesTrace;
            if (curveEditor != nullptr)
                curveEditor->setShowHarmoniesTrace (showHarmoniesTrace);
        });
    }

    menu.addSeparator();

    // Reset Playhead is hidden (there is a dedicated button for this now).

    // Standalone-only controls.
    if (isStandalone)
    {
        // Tempo (BPM): the standalone timeline advances at this rate.
        juce::PopupMenu tempo;
        const float currentBpm = processorRef.getBpm();
        static constexpr float tempoChoices[] = { 60.0f, 70.0f, 80.0f, 90.0f, 100.0f,
                                                  110.0f, 120.0f, 130.0f, 140.0f, 150.0f,
                                                  160.0f, 180.0f };
        for (float b : tempoChoices)
            tempo.addItem (juce::String (static_cast<int> (b)) + " BPM",
                           true, std::abs (b - currentBpm) < 0.5f,
                           [this, b] { processorRef.setBpm (b);    });
        menu.addSubMenu (ovt::tr (ovt::Keys::kMenuTempo), tempo, true);
    }

    menu.addSeparator();

    // Curve Presets: opens the preset manager as a submenu.
    menu.addSubMenu (ovt::tr (ovt::Keys::kMenuCurvePresets), buildPresetsMenu(), true);

    menu.addSeparator();

    // Import MIDI: open a file chooser for .mid files and generate a pitch curve.
    menu.addItem (ovt::tr (ovt::Keys::kMenuImportMidi), true, false, [this] {
        midiImportFileChooser = std::make_unique<juce::FileChooser> (
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            juce::File(),
            "*.mid;*.midi");

        juce::Component::SafePointer<OpenVoxTunerAudioProcessorEditor> editorSafe (this);
        midiImportFileChooser->launchAsync (
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [editorSafe] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (editorSafe != nullptr && file.existsAsFile())
                    editorSafe->handleMidiFileImport (file);
            });
    });

    applyMenuLookAndFeel (menu, customLookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&optionsButton)
                            .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards),
                        [] (int) {    });
}

void OpenVoxTunerAudioProcessorEditor::showPresetGallery()
{
    if (curveEditor == nullptr)
        return;

    if (galleryWindow != nullptr)
    {
        galleryWindow->setVisible (true);
        galleryWindow->toFront (true);
        return;
    }

    class ClosableGalleryWindow : public juce::DocumentWindow
    {
    public:
        ClosableGalleryWindow (const juce::String& name, juce::Colour backgroundColour, int requiredButtons)
            : juce::DocumentWindow (name, backgroundColour, requiredButtons) {}

        void closeButtonPressed() override
        {
            setVisible (false);
        }
    };

    auto* w = new ClosableGalleryWindow ("OpenVoxTuner - Preset Gallery",
                                           ebs::bgDark(),
                                           juce::DocumentWindow::closeButton);

    auto* gallery = new PresetGallery (
        [this] (const juce::String& id) { applyFactoryPreset (id); },
        [this] (const juce::File& f)      { loadCustomPresetFromFile (f); },
        [this] (const juce::File& f)      { /* delete handled via gallery refresh */ },
        [this] () -> juce::Array<juce::File> { return getUserPresetsDirectory().findChildFiles (juce::File::findFiles, false, "*.xml");    });

    // Keep a safe pointer so the delete callback can refresh the grid
    // after a custom preset is removed (the deletion is async).
    juce::Component::SafePointer<PresetGallery> gallerySp (gallery);
    gallery->setOnDeleteCallback ([this, gallerySp] (const juce::File& f) {
        // Parent the confirmation dialog to the gallery window so it shows
        // in front of it (the gallery is a separate DocumentWindow above the editor).
        deleteCustomPresetFile (f, [gallerySp] (bool ok) {
            if (ok && gallerySp != nullptr)
                gallerySp->refresh();
            }, gallerySp.getComponent());
    });

    w->setContentOwned (gallery, true);
    w->setSize (760, 540);
    w->setResizeLimits (480, 360, 2200, 1400);
    w->centreWithSize (760, 540);
    w->setAlwaysOnTop (true);
    w->setVisible (true);
    w->toFront (true);

    galleryWindow.reset (w);
}

void OpenVoxTunerAudioProcessorEditor::syncTransportButtons()
{
    const bool playing = processorRef.isTransportPlaying();
    // Single Play/Pause toggle: shows the Stop glyph (and tooltip) while playing,
    // the Play glyph (and tooltip) while stopped.
    playButton.setToggleState (playing, juce::dontSendNotification);
    playButton.setTooltip (playing ? ovt::tr (ovt::Keys::kTooltipStop)
                                   : ovt::tr (ovt::Keys::kTooltipPlay));
}

void OpenVoxTunerAudioProcessorEditor::applyThemeToAllComponents()
{
    // Refresh LookAndFeel colours
    customLookAndFeel.refreshThemeColours();

    // Re-apply colours to ALL sliders (knobs)
    auto applySliderColours = [] (juce::Slider& s) {
        s.setColour (juce::Slider::textBoxTextColourId,       ebs::text());
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::rotarySliderFillColourId,  ebs::accent());
        s.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
        s.setColour (juce::Slider::thumbColourId,             juce::Colours::white);
    };
    applySliderColours (speedSlider);
    applySliderColours (amountSlider);
    applySliderColours (formantSlider);
    applySliderColours (reverbMixSlider);
    applySliderColours (noiseGateThresholdSlider);
    applySliderColours (flexTuneSlider);
    applySliderColours (humanizeSlider);
    applySliderColours (vibratoPreserveSlider);
    applySliderColours (attackReleaseSlider);
    applySliderColours (harmonyGainSlider);
    applySliderColours (harmonyBlendSlider);
    applySliderColours (harmonyFormantSlider);

    // Re-apply colours to ALL labels
    auto applyLabelColours = [] (juce::Label& l) {
        l.setColour (juce::Label::textColourId, ebs::text());
    };
    applyLabelColours (speedLabel);
    applyLabelColours (amountLabel);
    applyLabelColours (flexTuneLabel);
    applyLabelColours (humanizeLabel);
    applyLabelColours (vibratoPreserveLabel);
    applyLabelColours (attackReleaseLabel);
    applyLabelColours (reverbMixLabel);
    applyLabelColours (noiseGateThresholdLabel);
    applyLabelColours (harmonyToneColorLabel);
    applyLabelColours (keyLabel);
    applyLabelColours (scaleLabel);
    applyLabelColours (keySourceLabel);
    applyLabelColours (companionGroupLabel);
    applyLabelColours (latencyModeLabel);
    applyLabelColours (harmonyTypeLabel);

    // Re-apply colours to ALL combo boxes
    auto applyComboColours = [] (juce::ComboBox& c) {
        // Closed combo state: use darkest plugin background
        c.setColour (juce::ComboBox::backgroundColourId, ebs::bgDark());
        c.setColour (juce::ComboBox::textColourId,       ebs::text());
        c.setColour (juce::ComboBox::outlineColourId,    ebs::accentSoft());
        c.setColour (juce::ComboBox::arrowColourId,      ebs::accent());
        // Dropdown popup: same dark background
        c.setColour (juce::PopupMenu::backgroundColourId, ebs::bgDark());
        c.setColour (juce::PopupMenu::textColourId,       ebs::text());
        c.setColour (juce::PopupMenu::highlightedBackgroundColourId, ebs::accentSoft());
    };
    applyComboColours (keyBox);
    applyComboColours (scaleBox);
    applyComboColours (keySourceBox);
    applyComboColours (companionGroupBox);
    applyComboColours (latencyModeBox);
    applyComboColours (harmonyTypeBox);
    applyComboColours (shiftedVoicesBox);
    applyComboColours (harmonyToneBox);

    // Re-apply colours to toggle buttons
    auto applyToggleColours = [] (juce::ToggleButton& b) {
        b.setColour (juce::ToggleButton::textColourId, ebs::text());
    };
    applyToggleColours (harmonyEnableButton);
    applyToggleColours (harmonyFollowLeadButton);
    applyToggleColours (useVoiceButton);
    applyToggleColours (formantEnableButton);
    applyToggleColours (reverbEnableButton);
    applyToggleColours (noiseGateEnableButton);

    // Re-apply to other buttons
    updateButton.setColour (juce::TextButton::buttonColourId,   ebs::bgPanel());
    updateButton.setColour (juce::TextButton::textColourOffId,  ebs::text());
    bypassToggleButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    midiToggleButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    correctionModeButton.setColour (juce::TextButton::buttonColourId,  ebs::bgPanel());
    correctionModeButton.setColour (juce::TextButton::textColourOffId, ebs::text());
    applyToggleColours (attackAwareButton);
    applyToggleColours (keyDetectPowerButton);
    buttonA.setColour (juce::TextButton::buttonColourId,  ebs::bgPanel());
    buttonA.setColour (juce::TextButton::textColourOffId, ebs::text());
    buttonB.setColour (juce::TextButton::buttonColourId,  ebs::bgPanel());
    buttonB.setColour (juce::TextButton::textColourOffId, ebs::text());

    // Re-apply to the Curve Editor "Options" button: keep its distinct
    // accent-tinted background and bright icon (it is a DrawableButton, not a
    // TextButton like the other re-applied buttons above).
    optionsButton.setColour (juce::DrawableButton::backgroundColourId, ebs::accent().withAlpha (0.22f));
    optionsButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.4f));
    optionsButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
    optionsButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);

    // Re-apply DrawableButton colours (background and text)
    auto applyDrawableBtnColours = [] (juce::DrawableButton& btn) {
        btn.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        btn.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        btn.setColour (juce::DrawableButton::textColourId, ebs::text());
    };
    applyDrawableBtnColours (snapButton);
    applyDrawableBtnColours (snapGridButton);
    applyDrawableBtnColours (stepModeButton);
    applyDrawableBtnColours (zoomInButton);
    applyDrawableBtnColours (zoomOutButton);
    applyDrawableBtnColours (scrollUpButton);
    applyDrawableBtnColours (scrollDownButton);
    applyDrawableBtnColours (resetViewButton);

    // Re-apply to tabbed component
    tabbedComponent.setColour (juce::TabbedComponent::backgroundColourId, ebs::bgDark());
    tabbedComponent.setColour (juce::TabbedComponent::outlineColourId,   ebs::accentSoft());

    // Force repaint of tab content components
    tabbedComponent.repaint();
    if (pitchVisualizer != nullptr)
        pitchVisualizer->repaint();
    if (curveEditor != nullptr)
        curveEditor->repaint();

    repaint();
}








