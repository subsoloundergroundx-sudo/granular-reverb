#pragma once
#include "PluginProcessor.h"
#include <array>
#include <memory>

class GranularReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GranularReverbAudioProcessorEditor (GranularReverbAudioProcessor&);
    ~GranularReverbAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob& k, const juce::String& paramId, const juce::String& name);

    GranularReverbAudioProcessor& proc;
    std::array<Knob, 7> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularReverbAudioProcessorEditor)
};
