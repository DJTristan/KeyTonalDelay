#include "PluginProcessor.h"

KeyTonalDelayAudioProcessor::KeyTonalDelayAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", makeParameters()) {}

juce::AudioProcessorValueTreeState::ParameterLayout
KeyTonalDelayAudioProcessor::makeParameters() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const StringArray keys{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const StringArray scales{"Natural minor", "Major", "Dorian", "Minor pentatonic", "Major pentatonic", "Harmonic minor"};
    const StringArray motions{"Fixed", "Ascending", "Bounce", "Random"};
    const StringArray rates{"1/4", "1/8", "1/16", "1/32"};
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"key", 1}, "Key", keys, 5));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"scale", 1}, "Scale", scales, 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"octave", 1}, "Octave", 2, 5, 3));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"degree", 1}, "Degree (1-7)", 1, 7, 1));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"motion", 1}, "Motion", motions, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"rate", 1}, "Step rate", rates, 2));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"feedback", 1}, "Feedback", NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 84.0f));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"brightness", 1}, "Brightness", NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 72.0f));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"mix", 1}, "Wet mix", NormalisableRange<float>{0.0f, 100.0f, 0.1f}, 70.0f));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"output", 1}, "Output dB", NormalisableRange<float>{-24.0f, 6.0f, 0.1f}, -4.0f));
    return {p.begin(), p.end()};
}

void KeyTonalDelayAudioProcessor::prepareToPlay(double sampleRate, int) {
    engine.prepare(sampleRate, 2);
}

bool KeyTonalDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto in = layout.getMainInputChannelSet();
    const auto out = layout.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
           && in == out;
}

void KeyTonalDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(2, getTotalNumInputChannels(), buffer.getNumChannels());
    for (int c = channels; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, buffer.getNumSamples());
    if (channels < 1) return;

    const auto load = [&](const char* id) { return parameters.getRawParameterValue(id)->load(); };
    ktd::Controls v;
    v.key = static_cast<int>(load("key"));
    v.scale = static_cast<ktd::Scale>(static_cast<int>(load("scale")));
    v.octave = static_cast<int>(load("octave"));
    v.degree = static_cast<int>(load("degree")) - 1;
    v.motion = static_cast<ktd::Motion>(static_cast<int>(load("motion")));
    v.rate = static_cast<ktd::Rate>(static_cast<int>(load("rate")));
    v.feedback = load("feedback") * 0.01f;
    v.brightness = load("brightness") * 0.01f;
    v.mix = load("mix") * 0.01f;
    v.outputDb = load("output");

    double bpm = 120.0;
    if (auto* playhead = getPlayHead()) {
        if (auto position = playhead->getPosition()) {
            if (auto tempo = position->getBpm()) bpm = *tempo;
        }
    }
    engine.update(v, bpm);
    float* left = buffer.getWritePointer(0);
    float* right = channels == 2 ? buffer.getWritePointer(1) : nullptr;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        engine.startFrame();
        left[i] = engine.processSample(0, left[i]);
        if (right != nullptr) right[i] = engine.processSample(1, right[i]);
    }
}

juce::AudioProcessorEditor* KeyTonalDelayAudioProcessor::createEditor() {
    return new juce::GenericAudioProcessorEditor(*this);
}

void KeyTonalDelayAudioProcessor::getStateInformation(juce::MemoryBlock& out) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, out);
}

void KeyTonalDelayAudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new KeyTonalDelayAudioProcessor();
}
