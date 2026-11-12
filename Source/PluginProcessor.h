#pragma once
#include <bitset>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "SpectralSnapper.h"
#include "Resonator.h"
#include "Liquid.h"
#include "BandLimiter.h"

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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    SpectralSnapper snapper;
    ResonatorBank resonator;
    Liquid liquid;
    std::atomic<float> inLevel { 0.f };      // nivel de entrada (para que la interfaz brille solo con sonido)
    std::atomic<bool> midiHeld { false };    // hay notas MIDI sostenidas

private:
    struct Ring   // retardo para alinear con la latencia del ajuste espectral
    {
        std::array<float, 8192> b {};
        int pos = 0;
        float tick (float x, int d) { b[(size_t) pos] = x; const float y = b[(size_t) ((pos - d) & 8191)]; pos = (pos + 1) & 8191; return y; }
        void clear() { b.fill (0.f); pos = 0; }
    };

    void applyQuality (int q);
    void handleMidi (const juce::MidiBuffer&);

    std::atomic<float> *pColor, *pKey, *pScale, *pQuality, *pGain, *pBypass;
    std::atomic<float> *pMidi, *pChord, *pOctave, *pDecay, *pSpacing, *pDrift, *pShift, *pResMix;
    std::atomic<float> *pWave, *pLiquid, *pFlow, *pSplitLo, *pSplitHi;

    std::array<Ring, 2> dryRing;
    BandLimiter bandLimiter;   // rango de frecuencias afectado (split)
    juce::AudioBuffer<float> midBuf, dryBuf;
    std::bitset<128> held, latched, pending;   // sostenidas / ultimo acorde / pendientes de soltar (pedal)
    bool pedal = false;
    float bypassMix = 1.f, gate = 1.f;
    double sampleRate = 44100.0;
    int currentQuality = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArchaicProcessor)
};
