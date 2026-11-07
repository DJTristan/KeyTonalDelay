#pragma once

#include <JuceHeader.h>
#include "TonalDelayEngine.h"

class KeyTonalDelayAudioProcessor final : public juce::AudioProcessor {
public:
    KeyTonalDelayAudioProcessor();
    ~KeyTonalDelayAudioProcessor() override = default;

    const juce::String getName() const override { return "Key Tonal Delay"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 4.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    juce::AudioProcessorValueTreeState parameters;
    ktd::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyTonalDelayAudioProcessor)
};
