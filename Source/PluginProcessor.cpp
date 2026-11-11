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
    applyQuality ((int) pQuality->load());
}

bool ArchaicProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void ArchaicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int q = (int) pQuality->load();
    if (q != currentQuality) applyQuality (q);

    SpectralSnapper::Params p;
    p.color = pColor->load();
    p.key = (int) pKey->load();
    p.scale = (int) pScale->load();
    const float mix = juce::jmin (p.color, 100.f) / 100.f;
    const float gain = juce::Decibels::decibelsToGain (pGain->load());

    for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c)
        snapper.process (c, buffer.getWritePointer (c), buffer.getNumSamples(), p, mix, gain);
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
