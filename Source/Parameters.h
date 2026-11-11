#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ID
{
    inline constexpr const char* color = "color";
    inline constexpr const char* key = "key";
    inline constexpr const char* scale = "scale";
    inline constexpr const char* quality = "quality";
    inline constexpr const char* gain = "gain";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* dark = "dark";
}

struct ScaleDef { const char* name; std::vector<int> iv; };

inline const std::vector<ScaleDef>& getScales()
{
    static const std::vector<ScaleDef> s {
        { "Major", { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor", { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
        { "Pentatonic Major", { 0, 2, 4, 7, 9 } },
        { "Pentatonic Minor", { 0, 3, 5, 7, 10 } },
        { "Blues", { 0, 3, 5, 6, 7, 10 } },
        { "Dorian", { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian", { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
        { "Whole Tone", { 0, 2, 4, 6, 8, 10 } } };
    return s;
}

inline const juce::StringArray& getNoteNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::color, 1 }, "Color",
           NormalisableRange<float> (0.f, 200.f, 0.1f), 100.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::key, 1 }, "Key", getNoteNames(), 0));
    StringArray names;
    for (auto& s : getScales()) names.add (s.name);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::scale, 1 }, "Scale", names, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::quality, 1 }, "Quality",
           StringArray { "Fast", "Quality" }, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::gain, 1 }, "Gain",
           NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::bypass, 1 }, "Bypass", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::dark, 1 }, "Dark Mode", true,
           AudioParameterBoolAttributes().withAutomatable (false)));
    return l;
}
