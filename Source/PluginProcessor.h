#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class GranularReverbAudioProcessor : public juce::AudioProcessor
{
public:
    GranularReverbAudioProcessor();
    ~GranularReverbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Granular Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Grain
    {
        bool active = false;
        float readPos = 0.f;
        float inc = 1.f;
        int age = 0;
        int length = 1;
        float gL = 1.f;
        float gR = 1.f;
    };

    void spawnGrain (int lengthSamples, float spreadSamples, float pitchRangeSemis);

    std::vector<float> bufL, bufR;
    int writePos = 0;
    std::vector<Grain> grains;
    float lpL = 0.f, lpR = 0.f;
    double currentSampleRate = 44100.0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularReverbAudioProcessor)
};
