#include "PluginEditor.h"

namespace { constexpr int baseW = 900, baseH = 600; }

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
        bar (dry[i], t.purple, 0.25f);
        bar (tuned[i], t.cyan, 0.35f);
    }
    g.setColour (t.sub.withAlpha (0.8f));
    g.setFont (uiFont (r.getHeight() * 0.065f));
    const auto lab = r.withTop (inner.getBottom() + r.getHeight() * 0.02f);
    g.drawText ("100 Hz", lab.reduced (r.getWidth() * 0.025f, 0.f), Justification::centredLeft);
    g.drawText ("1 kHz", lab, Justification::centred);
    g.drawText ("6 kHz", lab.reduced (r.getWidth() * 0.025f, 0.f), Justification::centredRight);
}

// ---------------------------------------------------------------- Editor
ArchaicEditor::ArchaicEditor (ArchaicProcessor& p)
    : AudioProcessorEditor (&p), proc (p), viz (p.snapper, lnf),
      keyAttach (*p.apvts.getParameter (ID::key), [this] (float v) { updateKeys (v); }),
      themeAttach (*p.apvts.getParameter (ID::dark), [this] (float v) { applyTheme (v > 0.5f); })
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (viz);

    for (auto* s : { &color, &gain })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
    }
    color.setComponentID ("color");
    color.setTextValueSuffix (" %");
    color.setNumDecimalPlacesToDisplay (0);
    gain.setTextValueSuffix (" dB");
    gain.setNumDecimalPlacesToDisplay (1);

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
    addAndMakeVisible (scale);

    themeBtn.setComponentID ("theme");
    bypassBtn.setComponentID ("bypass");
    qualityBtn.setComponentID ("quality");
    for (auto* b : { &themeBtn, &bypassBtn, &qualityBtn }) addAndMakeVisible (*b);
    themeBtn.onClick = [this] { themeAttach.setValueAsCompleteGesture (themeBtn.getToggleState() ? 1.f : 0.f); };

    colorAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ID::color, color);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ID::gain, gain);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ID::scale, scale);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ID::bypass, bypassBtn);
    qualityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ID::quality, qualityBtn);
    keyAttach.sendInitialUpdate();
    themeAttach.sendInitialUpdate();

    // reescalable, proporcion fija; el tamaño se guarda con el proyecto
    const int w = proc.apvts.state.getProperty ("uiW", baseW);
    setResizable (true, true);
    setResizeLimits (600, 400, 1800, 1200);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (w, w * baseH / baseW);
}

ArchaicEditor::~ArchaicEditor() { setLookAndFeel (nullptr); }

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

    g.setGradientFill (ColourGradient (t.bgTop, 0.f, 0.f, t.bgBottom, 0.f, H, false));
    g.fillAll();
    auto glow = [&] (Point<float> c, float rad, Colour col, float a)
    {
        g.setGradientFill (ColourGradient (col.withAlpha (a), c.x, c.y, col.withAlpha (0.f), c.x + rad, c.y, true));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f);
    };
    glow ({ W * 0.15f, H * 0.1f }, W * 0.6f, t.purple, t.dark ? 0.30f : 0.20f);
    glow ({ W * 0.92f, H * 0.95f }, W * 0.5f, t.cyan, t.dark ? 0.20f : 0.16f);

    g.setColour (t.text);
    g.setFont (uiFont (H * 0.062f, true));
    g.drawText (L"A  R  C  H  A  I  C", Rectangle<float> (W * 0.04f, H * 0.03f, W * 0.5f, H * 0.1f), Justification::centredLeft);

    auto R = [&] (float x, float y, float w, float h) { return Rectangle<float> (x * W, y * H, w * W, h * H); };
    glass::panel (g, R (0.04f, 0.57f, 0.44f, 0.36f), H * 0.04f, t);
    glass::panel (g, R (0.50f, 0.57f, 0.24f, 0.36f), H * 0.04f, t);
    glass::panel (g, R (0.76f, 0.57f, 0.20f, 0.36f), H * 0.04f, t);

    g.setColour (t.sub);
    g.setFont (uiFont (H * 0.026f, true));
    g.drawText ("KEY / SCALE", R (0.04f, 0.585f, 0.44f, 0.04f), Justification::centred);
    g.drawText ("COLOR", R (0.50f, 0.885f, 0.24f, 0.04f), Justification::centred);
    g.drawText ("OUTPUT", R (0.76f, 0.885f, 0.20f, 0.04f), Justification::centred);

    g.setColour (t.sub.withAlpha (0.35f));   // version: muy sutil
    g.setFont (uiFont (H * 0.02f));
    g.drawText (String ("v") + JucePlugin_VersionString, R (0.80f, 0.95f, 0.16f, 0.03f), Justification::centredRight);
}

void ArchaicEditor::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();
    auto R = [&] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * W, y * H, w * W, h * H).toNearestInt(); };

    themeBtn.setBounds (R (0.70f, 0.04f, 0.14f, 0.07f));
    const int s = (int) (H * 0.09f);
    bypassBtn.setBounds ((int) (W * 0.96f) - s, (int) (H * 0.03f), s, s);
    viz.setBounds (R (0.04f, 0.15f, 0.92f, 0.38f));

    const auto kb = R (0.055f, 0.63f, 0.41f, 0.17f);
    for (int i = 0; i < 12; ++i)
        keys[i].setBounds (kb.getX() + (i % 6) * kb.getWidth() / 6, kb.getY() + (i / 6) * kb.getHeight() / 2, kb.getWidth() / 6, kb.getHeight() / 2);
    scale.setBounds (R (0.07f, 0.82f, 0.38f, 0.08f));

    color.setBounds (R (0.51f, 0.60f, 0.22f, 0.29f));
    gain.setBounds (R (0.77f, 0.60f, 0.18f, 0.20f));
    qualityBtn.setBounds (R (0.78f, 0.81f, 0.16f, 0.065f));

    proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);
}
