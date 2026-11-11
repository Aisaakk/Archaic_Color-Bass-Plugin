#include "PluginProcessor.h"
#include "PluginEditor.h"

ArchaicProcessor::ArchaicProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pColor = apvts.getRawParameterValue (ID::color);
    pKey = apvts.getRawParameterValue (ID::key);
    pScale = apvts.getRawParameterValue (ID::scale);
    pQuality = apvts.getRawParameterValue (ID::quality);
    pGain = apvts.getRawParameterValue (ID::gain);
    pMidi = apvts.getRawParameterValue (ID::midi);
    pChord = apvts.getRawParameterValue (ID::chord);
    pOctave = apvts.getRawParameterValue (ID::octave);
    pDecay = apvts.getRawParameterValue (ID::decay);
    pSpacing = apvts.getRawParameterValue (ID::spacing);
    pDrift = apvts.getRawParameterValue (ID::drift);
    pShift = apvts.getRawParameterValue (ID::shift);
    pResMix = apvts.getRawParameterValue (ID::resMix);
}

void ArchaicProcessor::applyQuality (int q)
{
    currentQuality = q;
    snapper.setSize (q == 0 ? 1024 : 4096);
    setLatencySamples (snapper.getLatency());
}

void ArchaicProcessor::prepareToPlay (double sr, int)
{
    snapper.prepare (sr);
    resonator.prepare (sr);
    held.reset(); latched.reset();
    applyQuality ((int) pQuality->load());
}

bool ArchaicProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void ArchaicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int q = (int) pQuality->load();
    if (q != currentQuality) applyQuality (q);

    // MIDI: las notas sostenidas definen la tonalidad (y el acorde del resonador)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { if (held.none()) latched.reset(); held.set ((size_t) m.getNoteNumber()); latched.set ((size_t) m.getNoteNumber()); }
        else if (m.isNoteOff()) held.reset ((size_t) m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) held.reset();
    }
    const bool useMidi = pMidi->load() > 0.5f && latched.any();
    const int key = (int) pKey->load();
    const auto& sc = getScales()[(size_t) juce::jlimit (0, (int) getScales().size() - 1, (int) pScale->load())];

    SpectralSnapper::Params p;
    p.color = pColor->load();
    ResonatorBank::Params rp;
    rp.decay = pDecay->load() / 100.f;
    rp.spacing = pSpacing->load() / 100.f;
    rp.drift = pDrift->load() / 100.f;
    rp.shiftHz = pShift->load();
    auto toHz = [] (int midiNote) { return 440.f * std::exp2 (((float) midiNote - 69.f) / 12.f); };

    if (useMidi)
    {
        for (int n = 0; n < 128; ++n)
            if (latched[(size_t) n])
            {
                p.mask[(size_t) (n % 12)] = true;
                if (rp.nVoices < ResonatorBank::maxVoices) rp.freq[(size_t) rp.nVoices++] = toHz (n);
            }
    }
    else
    {
        for (int n : sc.iv) p.mask[(size_t) ((key + n) % 12)] = true;
        const int rootMidi = 12 * ((int) pOctave->load() + 2) + key;   // Oct 1 = C1 (24)
        const int m = (int) sc.iv.size();
        for (int d : chordDegrees ((int) pChord->load(), m))
            if (rp.nVoices < ResonatorBank::maxVoices)
                rp.freq[(size_t) rp.nVoices++] = toHz (rootMidi + sc.iv[(size_t) (d % m)] + 12 * (d / m));
    }

    const float mix = juce::jmin (p.color, 100.f) / 100.f;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    for (int c = 0; c < numCh; ++c)
        snapper.process (c, buffer.getWritePointer (c), buffer.getNumSamples(), p, mix, 1.f);

    resonator.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples(), rp, pResMix->load() / 100.f);

    buffer.applyGain (juce::Decibels::decibelsToGain (pGain->load()));
}

juce::AudioProcessorEditor* ArchaicProcessor::createEditor() { return new ArchaicEditor (*this); }

void ArchaicProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void ArchaicProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArchaicProcessor(); }
