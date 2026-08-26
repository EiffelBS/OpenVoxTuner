// PresetGallery.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PresetGallery.h"
#include "../dsp/PitchCurve.h"

PresetGallery::PresetGallery (LoadFactoryFn   loadFactory,
                             LoadCustomFn    loadCustom,
                             DeleteCustomFn  deleteCustom,
                             GetCustomFilesFn getCustomFiles)
    : onLoadFactory (std::move (loadFactory)),
      onLoadCustom  (std::move (loadCustom)),
      onDeleteCustom (std::move (deleteCustom)),
      getCustomFiles (std::move (getCustomFiles))
{
    setOpaque (true);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false); // we own 'content'
    viewport.setScrollBarsShown (true, false);

    factoryHeader.setText ("Factory", juce::dontSendNotification);
    factoryHeader.setFont (ovt::fontLabel());
    factoryHeader.setColour (juce::Label::textColourId, ebs::text());
    content.addAndMakeVisible (factoryHeader);

    customHeader.setText ("Custom", juce::dontSendNotification);
    customHeader.setFont (ovt::fontLabel());
    customHeader.setColour (juce::Label::textColourId, ebs::text());
    content.addAndMakeVisible (customHeader);

    emptyLabel.setText ("No custom presets yet.\nSave one from the Curve Editor \"Options\" menu.", juce::dontSendNotification);
    emptyLabel.setFont (ovt::fontLabelSmall());
    emptyLabel.setJustificationType (juce::Justification::centredLeft);
    emptyLabel.setColour (juce::Label::textColourId, ebs::textDim());
    content.addAndMakeVisible (emptyLabel);

    refresh();
}

//---------------------------------------------------------------------------
// Thumbnail rendering
//---------------------------------------------------------------------------

juce::Image PresetGallery::renderCurveThumb (const ovtdsp::PitchCurve& curve, int w, int h)
{
    juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), true);
    juce::Graphics g (img);
    g.fillAll (ebs::vizBg());

    // Faint horizontal reference lines.
    g.setColour (ebs::grid());
    for (int i = 1; i < 4; ++i)
    {
        const float y = (float) (h * i / 4);
        g.drawHorizontalLine (y, 0.0f, (float) w);
    }

    if (curve.getNumPoints() < 1)
        return img;

    // Time range (seconds).
    float tMax = 0.0f;
    for (int i = 0; i < curve.getNumPoints(); ++i)
        tMax = juce::jmax (tMax, (float) curve.getPoint (i).time);
    tMax = juce::jmax (tMax, 1.0f);

    // Pitch range (Hz) with a small margin.
    float pMin = 1.0e9f, pMax = -1.0e9f;
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const float p = curve.getPoint (i).pitch;
        pMin = juce::jmin (pMin, p);
        pMax = juce::jmax (pMax, p);
    }
    if (pMax - pMin < 1.0f)
    {
        pMin -= 20.0f;
        pMax += 20.0f;
    }

    const float pad = 8.0f;
    const float x0 = pad, x1 = (float) w - pad;
    const float y0 = pad, y1 = (float) h - pad;

    auto tx = [&] (float t) { return juce::jmap (t, 0.0f, tMax, x0, x1); };
    auto py = [&] (float p) { return juce::jmap (p, pMin, pMax, y1, y0); };

    juce::Path path;
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const auto& pt = curve.getPoint (i);
        const float X = tx ((float) pt.time);
        const float Y = py (pt.pitch);
        if (i == 0)
            path.startNewSubPath (X, Y);
        else if (curve.isStepMode())
        {
            const auto& prev = curve.getPoint (i - 1);
            path.lineTo (tx ((float) prev.time), py (pt.pitch));
            path.lineTo (X, Y);
        }
        else
            path.lineTo (X, Y);
    }

    g.setColour (ebs::outputColour());
    g.strokePath (path, juce::PathStrokeType (2.0f));
    return img;
}

//---------------------------------------------------------------------------
// Card
//---------------------------------------------------------------------------

PresetGallery::PresetCard::PresetCard (const juce::String& name,
                                         const juce::String& category,
                                         const juce::String& description,
                                         const juce::Image& thumb,
                                         bool isCustom,
                                         std::function<void()> onLoad,
                                         std::function<void()> onDelete)
    : custom (isCustom),
      loadCb (std::move (onLoad)),
      deleteCb (std::move (onDelete))
{
    setOpaque (true);

    // Sub-components must not intercept mouse clicks: the card's own
    // mouseDown() handles load (and forwards delete to the delete button).
    thumbComp.setImage (thumb);
    thumbComp.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (thumbComp);

    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setFont (ovt::fontLabel());
    nameLabel.setColour (juce::Label::textColourId, ebs::text());
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    const juce::String meta = description.isNotEmpty()
        ? (category + " - " + description)
        : category;
    metaLabel.setText (meta, juce::dontSendNotification);
    metaLabel.setFont (ovt::fontLabelSmall());
    metaLabel.setColour (juce::Label::textColourId, ebs::textDim());
    metaLabel.setMinimumHorizontalScale (0.5f);
    metaLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (metaLabel);

    if (custom)
    {
        deleteButton.setButtonText ("x");
        deleteButton.setColour (juce::TextButton::textColourOffId, ebs::textDim());
        deleteButton.setColour (juce::TextButton::textColourOnId, ebs::text());
        deleteButton.setTooltip ("Delete this custom preset");
        deleteButton.onClick = [this] { if (deleteCb) deleteCb(); };
        addAndMakeVisible (deleteButton);
    }
}

void PresetGallery::PresetCard::paint (juce::Graphics& g)
{
    g.fillAll (ebs::bgPanel());
    g.setColour (ebs::accent().withAlpha (0.35f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    if (isMouseOver (true))
    {
        g.setColour (ebs::accent().withAlpha (0.5f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 2.0f);
    }
}

void PresetGallery::PresetCard::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const int thumbH = h - 38;
    thumbComp.setBounds (4, 4, w - 8, thumbH);
    nameLabel.setBounds (8, thumbH + 4, w - (custom ? 30 : 16), 16);
    metaLabel.setBounds (8, thumbH + 22, w - (custom ? 30 : 16), 14);
    if (custom)
        deleteButton.setBounds (w - 26, 6, 20, 18);
}

void PresetGallery::PresetCard::mouseDown (const juce::MouseEvent& e)
{
    // Let the delete button handle its own click first.
    if (custom && deleteButton.getBounds().contains (e.getPosition()))
        return;
    if (loadCb)
        loadCb();
}

//---------------------------------------------------------------------------
// Refresh + layout
//---------------------------------------------------------------------------

void PresetGallery::refresh()
{
    // Clear old cards (OwnedArray destroys them and removes from parent).
    cards.clear();

    // Factory cards (added first, so the first N cards are factory).
    const auto& factory = ovtdsp::getFactoryPresets();
    for (const auto& info : factory)
    {
        ovtdsp::PitchCurve curve;
        curve.loadPreset (info.id);
        juce::Image thumb = renderCurveThumb (curve, 156, 96);
        auto* card = cards.add (new PresetCard (info.displayName, info.category, info.description,
                                                       thumb, false,
                                                       [this, id = info.id] { if (onLoadFactory) onLoadFactory (id); },
                                                       nullptr));
        content.addAndMakeVisible (card);
    }

    // Custom cards (parse each OVT_PRESET file's curve).
    bool anyCustom = false;
    if (getCustomFiles)
    {
        const auto files = getCustomFiles();
        for (const auto& f : files)
        {
            if (! f.existsAsFile()) continue;
            std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument (f).getDocumentElement());
            if (xml == nullptr) continue;

            const juce::XmlElement* curveXml = nullptr;
            juce::String name = f.getFileNameWithoutExtension();
            juce::String category = "Custom";
            juce::String description = "";
            if (xml->hasTagName ("OVT_PRESET"))
            {
                name = xml->getStringAttribute ("name", name);
                category = xml->getStringAttribute ("category", "Custom");
                description = xml->getStringAttribute ("description", "");
                curveXml = xml->getChildByName ("PITCH_CURVE");
            }
            else if (xml->hasTagName ("PITCH_CURVE"))
            {
                curveXml = xml.get();
            }
            if (curveXml == nullptr) continue;

            ovtdsp::PitchCurve curve;
            curve.fromXml (*curveXml);
            juce::Image thumb = renderCurveThumb (curve, 156, 96);
            auto* card = cards.add (new PresetCard (name, category, description,
                                                       thumb, true,
                                                       [this, f] { if (onLoadCustom) onLoadCustom (f); },
                                                       [this, f] { if (onDeleteCustom) onDeleteCustom (f); }));
            content.addAndMakeVisible (card);
            anyCustom = true;
        }
    }

    emptyLabel.setVisible (! anyCustom);
    layoutContent();
}

void PresetGallery::layoutContent()
{
    const int w = viewport.getWidth();
    if (w <= 0) return;

    const int cardW = 170, cardH = 150, gap = 12, headerH = 24;
    const int cols = juce::jmax (1, (w - gap) / (cardW + gap));
    const int leftPad = 8;

    // Factory header.
    int y = 8;
    factoryHeader.setBounds (leftPad, y, w - 2 * leftPad, headerH);
    y += headerH + 4;

    // Factory cards (first N, N = factory preset count).
    const int factoryCount = (int) ovtdsp::getFactoryPresets().size();
    auto placeGrid = [&] (int start, int count, int top)
    {
        int x = leftPad;
        int yy = top;
        int n = 0;
        for (int i = 0; i < count; ++i)
        {
            const int idx = start + i;
            if (idx >= cards.size()) break;
            cards[idx]->setBounds (x, yy, cardW, cardH);
            ++n;
            if (n % cols == 0)
            {
                x = leftPad;
                yy += cardH + gap;
            }
            else
            {
                x += cardW + gap;
            }
        }
        if (n > 0 && n % cols != 0)
            yy += cardH + gap;
        return yy;
    };

    y = placeGrid (0, factoryCount, y);

    // Custom header + custom cards (remaining cards).
    customHeader.setBounds (leftPad, y, w - 2 * leftPad, headerH);
    const int customTop = y + headerH + 4;
    const int customCount = cards.size() - factoryCount;
    if (customCount > 0)
    {
        emptyLabel.setVisible (false);
        y = placeGrid (factoryCount, customCount, customTop);
    }
    else
    {
        emptyLabel.setVisible (true);
        emptyLabel.setBounds (leftPad + 8, customTop + 4, w - 2 * leftPad - 16, 40);
        y = customTop + 50;
    }

    content.setSize (w, juce::jmax (getHeight(), y + 8));
}

void PresetGallery::paint (juce::Graphics& g)
{
    g.fillAll (ebs::bgDark());
}

void PresetGallery::resized()
{
    viewport.setBounds (getLocalBounds().reduced (8));
    layoutContent();
}


