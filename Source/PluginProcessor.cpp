#include "PluginProcessor.h"
#include "PluginEditor.h"

ArchaicProcessor::ArchaicProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pColor = get (ID::color); pKey = get (ID::key); pScale = get (ID::scale); pQuality = get (ID::quality);
    pGain = get (ID::gain); pBypass = get (ID::bypass); pMidi = get (ID::midi); pChord = get (ID::chord);
    pOctave = get (ID::octave); pDecay = get (ID::decay); pSpacing = get (ID::spacing); pDrift = get (ID::drift);
    pShift = get (ID::shift); pResMix = get (ID::resMix); pWave = get (ID::wave); pLiquid = get (ID::liquid);
    pFlow = get (ID::flow); pSplitLo = get (ID::splitLow); pSplitHi = get (ID::splitHigh);
}

void ArchaicProcessor::applyQuality (int q)
{
    currentQuality = q;
    snapper.setSize (q == 0 ? 1024 : 4096);
    setLatencySamples (snapper.getLatency());
}

void ArchaicProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    snapper.prepare (sr);
    resonator.prepare (sr);
    liquid.prepare (sr);
    applyQuality ((int) pQuality->load());

    bandLimiter.prepare (sr);
    for (auto* b : { &midBuf, &dryBuf }) b->setSize (2, juce::jmax (1, samplesPerBlock));
    for (auto& r : dryRing) r.clear();
    held.reset(); latched.reset(); pending.reset(); pedal = false;
    bypassMix = pBypass->load() > 0.5f ? 0.f : 1.f;
    gate = 1.f;
}

bool ArchaicProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

// MIDI "limpio": notas sostenidas + pedal de sustain; el ultimo acorde queda para que las colas suenen afinadas
void ArchaicProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const auto n = (size_t) m.getNoteNumber();
            if (held.none()) latched.reset();
            held.set (n); latched.set (n); pending.reset (n);
        }
        else if (m.isNoteOff())
        {
            const auto n = (size_t) m.getNoteNumber();
            if (pedal) pending.set (n); else held.reset (n);
        }
        else if (m.isController() && m.getControllerNumber() == 64)
        {
            const bool down = m.getControllerValue() >= 64;
            if (pedal && ! down) { held &= ~pending; pending.reset(); }
            pedal = down;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { held.reset(); pending.reset(); }
    }
    midiHeld.store (held.any());
}

void ArchaicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int q = (int) pQuality->load();
    if (q != currentQuality) applyQuality (q);
    for (auto* b : { &midBuf, &dryBuf })
        if (b->getNumSamples() < n) b->setSize (2, n, false, false, true);   // solo si el host usa bloques mayores

    handleMidi (midi);
    const bool midiMode = pMidi->load() > 0.5f;
    const bool useMidi = midiMode && latched.any();
    const int lat = snapper.getLatency();

    // nivel de entrada (para el brillo de la interfaz)
    float pk = 0.f;
    for (int c = 0; c < numCh; ++c) pk = juce::jmax (pk, buffer.getMagnitude (c, 0, n));
    const float lv = inLevel.load();
    inLevel.store (pk > lv ? pk : lv * 0.9f);

    // ---- parametros
    SpectralSnapper::Params p;
    p.color = pColor->load();
    ResonatorBank::Params rp;
    rp.decay = pDecay->load() / 100.f;
    rp.spacing = pSpacing->load() / 100.f;
    rp.drift = pDrift->load() / 100.f;
    rp.shiftHz = pShift->load();
    rp.wave = juce::jlimit (0, 4, (int) pWave->load());
    auto toHz = [] (int midiNote) { return 440.f * std::exp2 (((float) midiNote - 69.f) / 12.f); };
    const int key = (int) pKey->load();
    const auto& sc = getScales()[(size_t) juce::jlimit (0, (int) getScales().size() - 1, (int) pScale->load())];
    if (useMidi)
    {
        for (int i = 0; i < 128; ++i)
            if (latched[(size_t) i])
            {
                p.mask[(size_t) (i % 12)] = true;
                if (rp.nVoices < ResonatorBank::maxVoices) rp.freq[(size_t) rp.nVoices++] = toHz (i);
            }
    }
    else
    {
        for (int i : sc.iv) p.mask[(size_t) ((key + i) % 12)] = true;
        const int rootMidi = 12 * ((int) pOctave->load() + 2) + key;
        const int m = (int) sc.iv.size();
        for (int d : chordDegrees ((int) pChord->load(), m))
            if (rp.nVoices < ResonatorBank::maxVoices)
                rp.freq[(size_t) rp.nVoices++] = toHz (rootMidi + sc.iv[(size_t) (d % m)] + 12 * (d / m));
    }
    const float mix = juce::jmin (p.color, 100.f) / 100.f;
    float lo = pSplitLo->load(), hi = pSplitHi->load();
    hi = juce::jmax (hi, lo * 1.05f);

    // ---- entrada: dry alineado con la latencia
    for (int c = 0; c < numCh; ++c)
    {
        const float* in = buffer.getReadPointer (c);
        float* dry = dryBuf.getWritePointer (c);
        float* mid = midBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i) { dry[i] = dryRing[(size_t) c].tick (in[i], lat); mid[i] = in[i]; }
    }

    // ---- efecto: tonalidad -> resonador -> liquid
    for (int c = 0; c < numCh; ++c) snapper.process (c, midBuf.getWritePointer (c), n, p, mix, 1.f);
    resonator.process (midBuf.getArrayOfWritePointers(), numCh, n, rp, pResMix->load() / 100.f);
    liquid.process (midBuf.getArrayOfWritePointers(), numCh, n, pLiquid->load() / 100.f, pFlow->load() / 100.f);

    // ---- rango (split): fuera del rango queda la señal original (con el rango completo no hay filtros)
    bandLimiter.setRange (lo, hi);
    if (bandLimiter.isActive())
        for (int c = 0; c < numCh; ++c) bandLimiter.apply (c, midBuf.getWritePointer (c), dryBuf.getReadPointer (c), n);

    // ---- salida: ganancia, compuerta MIDI (sin notas = silencio) y bypass real (vuelve al dry alineado)
    const float gainLin = juce::Decibels::decibelsToGain (pGain->load());
    const float gateT = (midiMode && ! held.any()) ? 0.f : 1.f;
    const float bypassT = pBypass->load() > 0.5f ? 0.f : 1.f;
    const float aUp = 1.f - std::exp (-1.f / (0.01f * (float) sampleRate));
    const float aDown = 1.f - std::exp (-1.f / (0.12f * (float) sampleRate));
    const float aByp = 1.f - std::exp (-1.f / (0.02f * (float) sampleRate));
    for (int i = 0; i < n; ++i)
    {
        gate += (gateT - gate) * (gateT > gate ? aUp : aDown);
        bypassMix += (bypassT - bypassMix) * aByp;
        for (int c = 0; c < numCh; ++c)
        {
            const float wet = midBuf.getSample (c, i) * gainLin * gate;
            buffer.setSample (c, i, wet * bypassMix + dryBuf.getSample (c, i) * (1.f - bypassMix));
        }
    }
    if (bypassT == 0.f && bypassMix < 0.0005f) bypassMix = 0.f;
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
