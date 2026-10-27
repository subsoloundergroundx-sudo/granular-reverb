#include "PluginEditor.h"

GranularReverbAudioProcessorEditor::GranularReverbAudioProcessorEditor (GranularReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setupKnob (knobs[0], "size",     "Grain Size");
    setupKnob (knobs[1], "density",  "Density");
    setupKnob (knobs[2], "spread",   "Spread");
    setupKnob (knobs[3], "pitch",    "Pitch Rand");
    setupKnob (knobs[4], "feedback", "Feedback");
    setupKnob (knobs[5], "damp",     "Damping");
    setupKnob (knobs[6], "mix",      "Mix");

    setSize (700, 280);
}

void GranularReverbAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void GranularReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101014));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f));
    g.drawText ("Granular Reverb", getLocalBounds().removeFromTop (50), juce::Justification::centred);
}

void GranularReverbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (40);

    const int w = area.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (w);
        k.label.setBounds (col.removeFromTop (24));
        k.slider.setBounds (col);
    }
}
