#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using P = juce::AudioParameterFloat;
        using R = juce::NormalisableRange<float>;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<P> (juce::ParameterID { "size", 1 },     "Grain Size", R (20.f, 500.f, 1.f, 0.5f),    120.f));
        layout.add (std::make_unique<P> (juce::ParameterID { "density", 1 },  "Density",    R (1.f, 60.f, 0.1f, 0.6f),     20.f));
        layout.add (std::make_unique<P> (juce::ParameterID { "spread", 1 },   "Spread",     R (50.f, 2000.f, 1.f, 0.5f),   600.f));
        layout.add (std::make_unique<P> (juce::ParameterID { "pitch", 1 },    "Pitch Rand", R (0.f, 12.f, 1.f),            0.f));
        layout.add (std::make_unique<P> (juce::ParameterID { "feedback", 1 }, "Feedback",   R (0.f, 0.9f, 0.01f),          0.5f));
        layout.add (std::make_unique<P> (juce::ParameterID { "damp", 1 },     "Damping",    R (500.f, 16000.f, 1.f, 0.4f), 6000.f));
        layout.add (std::make_unique<P> (juce::ParameterID { "mix", 1 },      "Mix",        R (0.f, 1.f, 0.01f),           0.4f));
        return layout;
    }

    inline float readInterp (const std::vector<float>& b, float pos)
    {
        const int n = (int) b.size();
        int i0 = (int) pos;
        const float f = pos - (float) i0;
        i0 %= n;
        const int i1 = (i0 + 1) % n;
        return b[(size_t) i0] * (1.f - f) + b[(size_t) i1] * f;
    }
}

GranularReverbAudioProcessor::GranularReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      grains (128)
{
}

bool GranularReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GranularReverbAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    const auto size = (size_t) (sampleRate * 5.0);
    bufL.assign (size, 0.f);
    bufR.assign (size, 0.f);
    writePos = 0;
    lpL = lpR = 0.f;
    for (auto& g : grains)
        g.active = false;
}

void GranularReverbAudioProcessor::spawnGrain (int len, float spreadSamples, float pitchRange)
{
    if (bufL.empty())
        return;

    for (auto& g : grains)
    {
        if (g.active)
            continue;

        const float bufN = (float) bufL.size();

        float semi = 0.f;
        if (pitchRange > 0.01f)
            semi = std::round ((rng.nextFloat() * 2.f - 1.f) * pitchRange);

        const float inc = std::pow (2.f, semi / 12.f);
        const float minDelay = std::max (64.f, (inc - 1.f) * (float) len + 64.f);
        const float maxDelay = std::max (minDelay + 1.f, spreadSamples);

        float delay = minDelay + rng.nextFloat() * (maxDelay - minDelay);
        delay = std::min (delay, bufN - 2.f * (float) len - 64.f);

        float pos = (float) writePos - delay;
        if (pos < 0.f)
            pos += bufN;

        const float pan = rng.nextFloat();
        constexpr float halfPi = juce::MathConstants<float>::halfPi;

        g.active = true;
        g.readPos = pos;
        g.inc = inc;
        g.age = 0;
        g.length = len;
        g.gL = std::cos (pan * halfPi) * 1.4142f;
        g.gR = std::sin (pan * halfPi) * 1.4142f;
        break;
    }
}

void GranularReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (numCh < 2 || bufL.empty())
        return;

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float sizeMs   = get ("size");
    const float density  = get ("density");
    const float spreadMs = get ("spread");
    const float pitch    = get ("pitch");
    const float feedback = get ("feedback");
    const float damp     = get ("damp");
    const float mix      = get ("mix");

    const float sr = (float) currentSampleRate;
    const int grainLen = std::max (32, (int) (sizeMs * 0.001f * sr));
    const float spawnProb = density / sr;
    const float spreadSamples = spreadMs * 0.001f * sr;
    const float overlap = density * sizeMs * 0.001f;
    const float norm = 1.5f / std::sqrt (std::max (1.f, overlap));
    const float a = std::exp (-juce::MathConstants<float>::twoPi * damp / sr);
    const float bufN = (float) bufL.size();
    const int bufSize = (int) bufL.size();

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = R[i];

        if (rng.nextFloat() < spawnProb)
            spawnGrain (grainLen, spreadSamples, pitch);

        float wetL = 0.f, wetR = 0.f;

        for (auto& g : grains)
        {
            if (! g.active)
                continue;

            const float w = 0.5f * (1.f - std::cos (juce::MathConstants<float>::twoPi * (float) g.age / (float) g.length));
            const float m = 0.5f * (readInterp (bufL, g.readPos) + readInterp (bufR, g.readPos));

            wetL += m * w * g.gL;
            wetR += m * w * g.gR;

            g.readPos += g.inc;
            if (g.readPos >= bufN)
                g.readPos -= bufN;

            if (++g.age >= g.length)
                g.active = false;
        }

        wetL *= norm;
        wetR *= norm;

        lpL = wetL * (1.f - a) + a * lpL;
        lpR = wetR * (1.f - a) + a * lpR;

        bufL[(size_t) writePos] = std::tanh (inL + feedback * lpL);
        bufR[(size_t) writePos] = std::tanh (inR + feedback * lpR);
        writePos = (writePos + 1) % bufSize;

        L[i] = inL * (1.f - mix) + lpL * mix;
        R[i] = inR * (1.f - mix) + lpR * mix;
    }
}

juce::AudioProcessorEditor* GranularReverbAudioProcessor::createEditor()
{
    return new GranularReverbAudioProcessorEditor (*this);
}

void GranularReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GranularReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GranularReverbAudioProcessor();
}
