#pragma once
#include "PluginProcessor.h"
#include "Look.h"

// Visualizador provisional (espectro 100 Hz - 6 kHz) dentro de una pantalla oscura.
// violeta = señal original, naranja/rosa = frecuencias movidas a la tonalidad.
class Visualizer : public juce::Component, private juce::Timer
{
public:
    Visualizer (SpectralSnapper& s, ArchaicLNF& l) : snap (s), lnf (l)
    {
        dry.fill (0.f); tuned.fill (0.f);
        startTimerHz (30);
    }
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    SpectralSnapper& snap;
    ArchaicLNF& lnf;
    std::array<float, SpectralSnapper::numViz> dry, tuned;
};

class ArchaicEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ArchaicEditor (ArchaicProcessor&);
    ~ArchaicEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyTheme (bool dark);
    void updateKeys (float v);
    void rebuildBackground();
    static juce::File sizeFile();

    ArchaicProcessor& proc;
    ArchaicLNF lnf;
    juce::Image logoLight, logoDark, bgCache;
    Visualizer viz;
    juce::Slider color, decay, spacing, drift, shift, resMix, liquid, flow, splitLo, splitHi, gain;
    juce::ComboBox scale, chord, octave, wave;
    juce::TextButton keys[12];
    juce::ToggleButton themeBtn, bypassBtn, qualityBtn, midiBtn;
    juce::Label midiStatus;
    juce::ParameterAttachment keyAttach, themeAttach;
    using APVTS = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sliderAtts;
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAtts;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
    float glowSmooth = 0.f;
    bool lastHeld = false, lastMidiOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArchaicEditor)
};
