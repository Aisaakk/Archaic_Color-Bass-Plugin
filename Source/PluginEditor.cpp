#include "PluginEditor.h"
#include <BinaryData.h>

namespace { constexpr int baseW = 1100, baseH = 700; }

// ------------------------------------------------------------ Visualizer
void Visualizer::timerCallback()
{
    for (size_t i = 0; i < dry.size(); ++i)
    {
        dry[i] = juce::jmax (snap.vizDry[i].load(), dry[i] * 0.85f);
        tuned[i] = juce::jmax (snap.vizTuned[i].load(), tuned[i] * 0.85f);
    }
    repaint();
}

void Visualizer::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& t = lnf.theme();
    auto r = getLocalBounds().toFloat().reduced (3.f);
    glass::panel (g, r, r.getHeight() * 0.09f, t);

    auto inner = r.reduced (r.getWidth() * 0.025f, r.getHeight() * 0.12f);
    const float colW = inner.getWidth() / (float) dry.size();
    for (size_t i = 0; i < dry.size(); ++i)
    {
        const float x = inner.getX() + (float) i * colW;
        auto bar = [&] (float v, Colour c, float glow)
        {
            if (v < 0.02f) return;
            const float h = inner.getHeight() * v;
            auto b = Rectangle<float> (x + colW * 0.2f, inner.getBottom() - h, colW * 0.6f, h);
            g.setColour (c.withAlpha (glow * v));
            g.fillRoundedRectangle (b.expanded (colW * 0.5f, 0.f), colW * 0.6f);
            g.setColour (c.withAlpha (0.35f + 0.65f * v));
            g.fillRoundedRectangle (b, colW * 0.3f);
        };
        bar (dry[i], t.violet, 0.25f);
        bar (tuned[i], t.amber.interpolatedWith (t.pink, 0.35f), 0.35f);
    }
    g.setColour (t.sub.withAlpha (0.8f));
    g.setFont (uiFont (r.getHeight() * 0.065f));
    const auto lab = r.withTop (inner.getBottom() + r.getHeight() * 0.02f);
    g.drawText ("100 Hz", lab.reduced (r.getWidth() * 0.025f, 0.f), Justification::centredLeft);
    g.drawText ("1 kHz", lab, Justification::centred);
    g.drawText ("6 kHz", lab.reduced (r.getWidth() * 0.025f, 0.f), Justification::centredRight);
}

// ---------------------------------------------------------------- Editor
juce::File ArchaicEditor::sizeFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Archaic").getChildFile ("ui-size.txt");
}

ArchaicEditor::ArchaicEditor (ArchaicProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      wordFont (juce::Typeface::createSystemTypefaceFor (BinaryData::WordmarkFont_ttf, (size_t) BinaryData::WordmarkFont_ttfSize)),
      viz (p.snapper, lnf),
      keyAttach (*p.apvts.getParameter (ID::key), [this] (float v) { updateKeys (v); }),
      themeAttach (*p.apvts.getParameter (ID::dark), [this] (float v) { applyTheme (v > 0.5f); })
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (viz);

    struct K { juce::Slider* s; const char* id; const char* suffix; int dec; };
    for (auto k : { K { &color, ID::color, " %", 0 }, K { &decay, ID::decay, " %", 0 }, K { &spacing, ID::spacing, "", 0 },
                    K { &drift, ID::drift, " %", 0 }, K { &shift, ID::shift, " Hz", 0 }, K { &resMix, ID::resMix, " %", 0 },
                    K { &gain, ID::gain, " dB", 1 } })
    {
        k.s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.s->setTextValueSuffix (k.suffix);
        k.s->setNumDecimalPlacesToDisplay (k.dec);
        addAndMakeVisible (*k.s);
        sliderAtts.push_back (std::make_unique<APVTS::SliderAttachment> (proc.apvts, k.id, *k.s));
        const juce::String suf = k.suffix; const int dec = k.dec;   // el attachment pone su propio formato: lo sobrescribimos
        k.s->textFromValueFunction = [suf, dec] (double v) { return (dec == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, dec)) + suf; };
    }
    color.setComponentID ("color");

    for (int i = 0; i < 12; ++i)
    {
        keys[i].setButtonText (getNoteNames()[i]);
        keys[i].setClickingTogglesState (false);
        keys[i].onClick = [this, i] { keyAttach.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (keys[i]);
    }

    juce::StringArray names;
    for (auto& s : getScales()) names.add (s.name);
    scale.addItemList (names, 1);
    chord.addItemList ({ "Triad", "Seventh", "Ninth", "Sus2", "Sus4", "Power", "Scale" }, 1);
    octave.addItemList ({ "Oct 1", "Oct 2", "Oct 3", "Oct 4", "Oct 5" }, 1);
    for (auto* c : { &scale, &chord, &octave }) addAndMakeVisible (*c);
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ID::scale, scale));
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ID::chord, chord));
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ID::octave, octave));

    themeBtn.setComponentID ("theme");
    bypassBtn.setComponentID ("bypass");
    qualityBtn.setComponentID ("quality");
    midiBtn.setComponentID ("midi");
    for (auto* b : { &themeBtn, &bypassBtn, &qualityBtn, &midiBtn }) addAndMakeVisible (*b);
    themeBtn.onClick = [this] { themeAttach.setValueAsCompleteGesture (themeBtn.getToggleState() ? 1.f : 0.f); };
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, ID::bypass, bypassBtn));
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, ID::quality, qualityBtn));
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, ID::midi, midiBtn));
    keyAttach.sendInitialUpdate();
    themeAttach.sendInitialUpdate();

    // Tamaño: el del proyecto si existe; si no, el ultimo que usaste (guardado en tu usuario)
    int w = baseW;
    if (proc.apvts.state.hasProperty ("uiW")) w = proc.apvts.state.getProperty ("uiW");
    else if (sizeFile().existsAsFile()) w = sizeFile().loadFileAsString().getIntValue();
    w = juce::jlimit (700, 2000, w > 0 ? w : baseW);
    setResizable (true, true);
    setResizeLimits (700, 700 * baseH / baseW, 2000, 2000 * baseH / baseW);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (w, w * baseH / baseW);
}

ArchaicEditor::~ArchaicEditor()
{
    sizeFile().getParentDirectory().createDirectory();
    sizeFile().replaceWithText (juce::String (getWidth()));
    setLookAndFeel (nullptr);
}

void ArchaicEditor::applyTheme (bool dark)
{
    lnf.setDark (dark);
    themeBtn.setToggleState (dark, juce::dontSendNotification);
    sendLookAndFeelChange();
    repaint();
}

void ArchaicEditor::updateKeys (float v)
{
    for (int i = 0; i < 12; ++i) keys[i].setToggleState (i == (int) std::lround (v), juce::dontSendNotification);
}

void ArchaicEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& t = lnf.theme();
    const float W = (float) getWidth(), H = (float) getHeight();
    auto R = [&] (float x, float y, float w, float h) { return Rectangle<float> (x * W, y * H, w * W, h * H); };

    g.setGradientFill (ColourGradient (t.bgTop, 0.f, 0.f, t.bgBottom, 0.f, H, false));
    g.fillAll();
    auto glow = [&] (Point<float> c, float rad, Colour col, float a)
    {
        g.setGradientFill (ColourGradient (col.withAlpha (a), c.x, c.y, col.withAlpha (0.f), c.x + rad, c.y, true));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f);
    };
    glow ({ W * 0.12f, H * 0.08f }, W * 0.55f, t.pink, t.dark ? 0.26f : 0.20f);
    glow ({ W * 0.55f, H * 1.0f }, W * 0.5f, t.amber, t.dark ? 0.18f : 0.18f);
    glow ({ W * 0.98f, H * 0.3f }, W * 0.4f, t.violet, t.dark ? 0.20f : 0.12f);

    drawWordmark (g, R (0.035f, 0.025f, 0.26f, 0.105f), wordFont, t.dark);

    glass::panel (g, R (0.03f, 0.44f, 0.24f, 0.52f), H * 0.035f, t);
    glass::panel (g, R (0.28f, 0.44f, 0.16f, 0.52f), H * 0.035f, t);
    glass::panel (g, R (0.45f, 0.44f, 0.38f, 0.52f), H * 0.035f, t);
    glass::panel (g, R (0.84f, 0.44f, 0.13f, 0.52f), H * 0.035f, t);

    g.setColour (t.sub);
    g.setFont (uiFont (H * 0.024f, true));
    auto cap = [&] (const char* s, float x, float y, float w) { g.drawText (s, R (x, y, w, 0.04f), Justification::centred); };
    cap ("KEY / SCALE", 0.03f, 0.455f, 0.24f);
    cap ("COLOR", 0.28f, 0.905f, 0.16f);
    cap ("RESONATOR", 0.45f, 0.455f, 0.38f);
    cap ("OUTPUT", 0.84f, 0.905f, 0.13f);
    for (auto [s, x] : { std::pair<const char*, float> { "DECAY", 0.458f }, { "SPACING", 0.5316f }, { "DRIFT", 0.6052f }, { "SHIFT", 0.6788f }, { "MIX", 0.7524f } })
        cap (s, x, 0.905f, 0.07f);

    g.setColour (t.sub.withAlpha (0.35f));   // version: muy sutil
    g.setFont (uiFont (H * 0.02f));
    g.drawText (String ("v") + JucePlugin_VersionString, R (0.84f, 0.965f, 0.13f, 0.03f), Justification::centredRight);
}

void ArchaicEditor::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();
    auto R = [&] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * W, y * H, w * W, h * H).toNearestInt(); };

    const int s = (int) (H * 0.085f);
    bypassBtn.setBounds ((int) (W * 0.97f) - s, (int) (H * 0.03f), s, s);
    themeBtn.setBounds (R (0.69f, 0.04f, 0.14f, 0.065f));
    midiBtn.setBounds (R (0.585f, 0.04f, 0.09f, 0.065f));
    viz.setBounds (R (0.03f, 0.145f, 0.94f, 0.27f));

    const auto kb = R (0.04f, 0.52f, 0.22f, 0.17f);
    for (int i = 0; i < 12; ++i)
        keys[i].setBounds (kb.getX() + (i % 6) * kb.getWidth() / 6, kb.getY() + (i / 6) * kb.getHeight() / 2, kb.getWidth() / 6, kb.getHeight() / 2);
    scale.setBounds (R (0.05f, 0.74f, 0.20f, 0.08f));

    color.setBounds (R (0.285f, 0.52f, 0.15f, 0.36f));

    chord.setBounds (R (0.46f, 0.52f, 0.17f, 0.075f));
    octave.setBounds (R (0.645f, 0.52f, 0.17f, 0.075f));
    const float kx[5] = { 0.458f, 0.5316f, 0.6052f, 0.6788f, 0.7524f };
    juce::Slider* ks[5] = { &decay, &spacing, &drift, &shift, &resMix };
    for (int i = 0; i < 5; ++i) ks[i]->setBounds (R (kx[i], 0.63f, 0.07f, 0.265f));

    gain.setBounds (R (0.85f, 0.52f, 0.11f, 0.2f));
    qualityBtn.setBounds (R (0.85f, 0.78f, 0.11f, 0.065f));

    proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);
}
