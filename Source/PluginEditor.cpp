#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    constexpr int baseW = 1200, baseH = 700;

    juce::String hzText (double v)
    {
        if (v < 1000.0) return juce::String (juce::roundToInt (v)) + " Hz";
        return (v < 10000.0 ? juce::String (v / 1000.0, 1) : juce::String (juce::roundToInt (v / 1000.0))) + " kHz";
    }
}

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
    auto r = getLocalBounds().toFloat();
    ui::well (g, r, r.getHeight() * 0.06f, t);

    auto inner = r.reduced (r.getWidth() * 0.025f, r.getHeight() * 0.12f);
    const float colW = inner.getWidth() / (float) dry.size();
    for (size_t i = 0; i < dry.size(); ++i)
    {
        const float x = inner.getX() + (float) i * colW;
        auto bar = [&] (float v, Colour c)
        {
            if (v < 0.02f) return;
            const float h = inner.getHeight() * v;
            g.setColour (c.withAlpha (0.4f + 0.6f * v));
            g.fillRoundedRectangle (Rectangle<float> (x + colW * 0.2f, inner.getBottom() - h, colW * 0.6f, h), colW * 0.3f);
        };
        bar (dry[i], t.violet);
        bar (tuned[i], t.amber.interpolatedWith (t.pink, 0.35f));
    }
    g.setColour (Colours::white.withAlpha (0.4f));
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
      logoLight (juce::ImageFileFormat::loadFrom (BinaryData::LogoLight_png, (size_t) BinaryData::LogoLight_pngSize)),
      logoDark (juce::ImageFileFormat::loadFrom (BinaryData::LogoDark_png, (size_t) BinaryData::LogoDark_pngSize)),
      viz (p.snapper, lnf),
      keyAttach (*p.apvts.getParameter (ID::key), [this] (float v) { updateKeys (v); }),
      themeAttach (*p.apvts.getParameter (ID::dark), [this] (float v) { applyTheme (v > 0.5f); })
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (viz);

    struct K { juce::Slider* s; const char* id; const char* suffix; int dec; bool hz; };
    for (auto k : { K { &color, ID::color, " %", 0, false }, K { &decay, ID::decay, " %", 0, false }, K { &spacing, ID::spacing, "", 0, false },
                    K { &drift, ID::drift, " %", 0, false }, K { &shift, ID::shift, " Hz", 0, false }, K { &resMix, ID::resMix, " %", 0, false },
                    K { &liquid, ID::liquid, " %", 0, false }, K { &flow, ID::flow, " %", 0, false },
                    K { &splitLo, ID::splitLow, "", 0, true }, K { &splitHi, ID::splitHigh, "", 0, true }, K { &gain, ID::gain, " dB", 1, false } })
    {
        k.s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*k.s);
        sliderAtts.push_back (std::make_unique<APVTS::SliderAttachment> (proc.apvts, k.id, *k.s));
        const juce::String suf = k.suffix; const int dec = k.dec; const bool hz = k.hz;   // el attachment pone su formato: lo sobrescribimos
        k.s->textFromValueFunction = [suf, dec, hz] (double v)
        {
            if (hz) return hzText (v);
            return (dec == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, dec)) + suf;
        };
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
    wave.addItemList ({ "Sine", "Triangle", "Square", "Saw", "Pulse" }, 1);
    for (auto* c : { &scale, &chord, &octave, &wave }) addAndMakeVisible (*c);
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ID::scale, scale));
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ID::chord, chord));
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ID::octave, octave));
    comboAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ID::wave, wave));

    themeBtn.setComponentID ("theme");
    bypassBtn.setComponentID ("bypass");
    qualityBtn.setComponentID ("quality");
    midiBtn.setComponentID ("midi");
    for (auto* b : { &themeBtn, &bypassBtn, &qualityBtn, &midiBtn }) addAndMakeVisible (*b);
    themeBtn.onClick = [this] { themeAttach.setValueAsCompleteGesture (themeBtn.getToggleState() ? 1.f : 0.f); };
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, ID::bypass, bypassBtn));
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, ID::quality, qualityBtn));
    buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, ID::midi, midiBtn));

    midiStatus.setJustificationType (juce::Justification::centredRight);
    midiStatus.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (midiStatus);

    keyAttach.sendInitialUpdate();
    themeAttach.sendInitialUpdate();

    // Tamaño: el del proyecto si existe; si no, el ultimo que usaste (guardado en tu usuario)
    int w = baseW;
    if (proc.apvts.state.hasProperty ("uiW")) w = proc.apvts.state.getProperty ("uiW");
    else if (sizeFile().existsAsFile()) w = sizeFile().loadFileAsString().getIntValue();
    w = juce::jlimit (800, 2200, w > 0 ? w : baseW);
    setResizable (true, true);
    setResizeLimits (800, 800 * baseH / baseW, 2200, 2200 * baseH / baseW);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (w, w * baseH / baseW);
    startTimerHz (20);
}

ArchaicEditor::~ArchaicEditor()
{
    stopTimer();
    sizeFile().getParentDirectory().createDirectory();
    sizeFile().replaceWithText (juce::String (getWidth()));
    setLookAndFeel (nullptr);
}

// El brillo solo aparece cuando entra sonido; el estado de MIDI se muestra con texto claro.
void ArchaicEditor::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (proc.inLevel.load(), -80.f);
    const float target = juce::jlimit (0.f, 1.f, (db + 50.f) / 35.f);
    glowSmooth += (target - glowSmooth) * (target > glowSmooth ? 0.5f : 0.15f);
    const float q = glowSmooth < 0.03f ? 0.f : std::round (glowSmooth * 8.f) / 8.f;
    const bool held = proc.midiHeld.load(), midiOn = midiBtn.getToggleState();
    const bool changedMidi = (held != lastHeld) || (midiOn != lastMidiOn);
    if (q != lnf.glow || changedMidi)
    {
        lnf.glow = q;
        midiBtn.getProperties().set ("held", held);
        lastHeld = held; lastMidiOn = midiOn;
        midiStatus.setText (midiOn ? (held ? "MIDI ACTIVE" : "WAITING FOR NOTES...") : "", juce::dontSendNotification);
        for (auto* c : getChildren()) if (c != &viz) c->repaint();
    }
}

void ArchaicEditor::applyTheme (bool dark)
{
    lnf.setDark (dark);
    themeBtn.setToggleState (dark, juce::dontSendNotification);
    midiStatus.setColour (juce::Label::textColourId, lnf.theme().sub);
    rebuildBackground();
    sendLookAndFeelChange();
    repaint();
}

void ArchaicEditor::updateKeys (float v)
{
    for (int i = 0; i < 12; ++i) keys[i].setToggleState (i == (int) std::lround (v), juce::dontSendNotification);
}

// Fondo estatico (paneles, sombras, rotulos, logo) dibujado una sola vez; asi la interfaz es ligera.
void ArchaicEditor::rebuildBackground()
{
    using namespace juce;
    const int W = getWidth(), H = getHeight();
    if (W < 10 || H < 10) return;
    const float sc = 2.f, sx = (float) W / baseW, sy = (float) H / baseH;
    auto S = [&] (float x, float y, float w, float h) { return Rectangle<float> (x * sx, y * sy, w * sx, h * sy); };
    const auto& t = lnf.theme();

    bgCache = Image (Image::ARGB, (int) (W * sc), (int) (H * sc), true);
    Graphics g (bgCache);
    g.addTransform (AffineTransform::scale (sc));
    g.fillAll (t.bg);

    const float corner = 18.f * sy;
    for (auto r : { S (24, 96, 1152, 230), S (24, 342, 200, 334), S (236, 342, 128, 334), S (376, 342, 400, 334),
                    S (788, 342, 230, 160), S (788, 514, 230, 162), S (1030, 342, 146, 334) })
        ui::panel (g, r, corner, t);

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.setColour (Colours::black);   // drawImage usa la opacidad del color activo: debe ser opaco
    const auto& logo = t.dark ? logoDark : logoLight;
    if (logo.isValid())
        g.drawImage (logo, S (24, 8, 330, 80), RectanglePlacement::xLeft | RectanglePlacement::yMid);

    g.setColour (t.sub);
    g.setFont (uiFont (13.f * sy, true));
    auto cap = [&] (const String& s, float x, float y, float w) { g.drawText (s, S (x, y, w, 18), Justification::centred); };
    cap ("HARMONY", 24, 354, 200);  cap ("COLOR", 236, 354, 128);  cap ("RESONATOR", 376, 354, 400);
    cap ("LIQUID", 788, 354, 230);  cap ("SPLIT", 788, 526, 230);  cap ("OUTPUT", 1030, 354, 146);
    g.setFont (uiFont (11.f * sy));
    auto small = [&] (const String& s, float cx, float y, float w = 80) { g.drawText (s, S (cx - w * 0.5f, y, w, 14), Justification::centred); };
    small ("SCALE", 124, 506);  small ("CHORD", 80, 566);  small ("OCTAVE", 168, 566);
    small ("COLOR", 300, 548);  small ("WAVE", 576, 386);
    const char* rk[5] = { "DECAY", "SPACING", "DRIFT", "SHIFT", "MIX" };
    for (int i = 0; i < 5; ++i) small (rk[i], 423.f + 76.f * (float) i, 556, 76);
    small ("AMOUNT", 846, 474);  small ("FLOW", 944, 474);
    small ("FROM", 846, 640);  small ("TO", 944, 640);
    small ("GAIN", 1103, 508);

    g.setColour (t.sub.withAlpha (0.45f));
    g.setFont (uiFont (11.f * sy));
    g.drawText (String ("v") + JucePlugin_VersionString, S (1030, 680, 146, 16), Justification::centredRight);
}

void ArchaicEditor::paint (juce::Graphics& g)
{
    if (bgCache.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (bgCache, getLocalBounds().toFloat());
    }
}

void ArchaicEditor::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight(), sx = W / baseW, sy = H / baseH;
    auto S = [&] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * sx, y * sy, w * sx, h * sy).toNearestInt(); };
    auto K = [&] (float x, float y, float s) { return S (x, y, s, s); };

    bypassBtn.setBounds (S (1120, 16, 56, 56));
    themeBtn.setBounds (S (1004, 24, 100, 40));
    midiBtn.setBounds (S (908, 24, 84, 40));
    midiStatus.setBounds (S (640, 24, 260, 40));
    midiStatus.setFont (uiFont (12.f * sy, true));
    viz.setBounds (S (38, 110, 1124, 202));

    // HARMONY
    for (int i = 0; i < 12; ++i)
        keys[i].setBounds (S (34.f + 45.f * (float) (i % 4), 376.f + 40.f * (float) (i / 4), 45.f, 40.f));
    scale.setBounds (S (36, 520, 176, 38));
    chord.setBounds (S (36, 580, 86, 38));
    octave.setBounds (S (126, 580, 86, 38));

    // COLOR
    color.setBounds (K (240, 426, 120));

    // RESONATOR
    wave.setBounds (S (476, 402, 200, 38));
    const float rx[5] = { 388, 464, 540, 616, 692 };
    juce::Slider* rs[5] = { &decay, &spacing, &drift, &shift, &resMix };
    for (int i = 0; i < 5; ++i) rs[i]->setBounds (K (rx[i], 482, 70));

    // LIQUID / SPLIT
    liquid.setBounds (K (808, 394, 76));
    flow.setBounds (K (906, 394, 76));
    splitLo.setBounds (K (808, 560, 76));
    splitHi.setBounds (K (906, 560, 76));

    // OUTPUT
    gain.setBounds (K (1050, 398, 106));
    qualityBtn.setBounds (S (1042, 580, 122, 40));

    proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);
    rebuildBackground();
}
