#pragma once
#include "PluginProcessor.h"
#include "Look.h"

// Espectro 100 Hz - 6 kHz: morado = señal original, celeste = frecuencias movidas
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

class ArchaicEditor : public juce::AudioProcessorEditor
{
public:
    explicit ArchaicEditor (ArchaicProcessor&);
    ~ArchaicEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applyTheme (bool dark);
    void updateKeys (float v);

    ArchaicProcessor& proc;
    ArchaicLNF lnf;
    Visualizer viz;
    juce::Slider color, gain;
    juce::ComboBox scale;
    juce::TextButton keys[12];
    juce::ToggleButton themeBtn, bypassBtn, qualityBtn;
    juce::ParameterAttachment keyAttach, themeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> colorAtt, gainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, qualityAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArchaicEditor)
};
