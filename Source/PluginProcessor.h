#pragma once
#include "Parameters.h"
#include "SpectralSnapper.h"
#include "Resonator.h"
#include <bitset>

class ArchaicProcessor : public juce::AudioProcessor
{
public:
    ArchaicProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Archaic"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ID::bypass); }

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    SpectralSnapper snapper;
    ResonatorBank resonator;

private:
    void applyQuality (int q);
    std::atomic<float> *pColor, *pKey, *pScale, *pQuality, *pGain;
    std::atomic<float> *pMidi, *pChord, *pOctave, *pDecay, *pSpacing, *pDrift, *pShift, *pResMix;
    std::bitset<128> held, latched;   // notas MIDI sostenidas / ultimo acorde
    int currentQuality = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArchaicProcessor)
};
