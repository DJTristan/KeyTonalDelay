#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// Dependency-free tonal comb delay DSP, testable without JUCE.
// Intended for mono/stereo real-time audio at 44.1--192 kHz.
namespace ktd {

constexpr float kPi = 3.14159265358979323846f;

enum class Scale : int { naturalMinor, major, dorian, minorPentatonic, majorPentatonic, harmonicMinor };
enum class Motion : int { fixed, up, bounce, random };
enum class Rate : int { quarter, eighth, sixteenth, thirtySecond };

struct Controls {
    int key = 5;                         // C=0, C#=1 ... F=5
    Scale scale = Scale::naturalMinor;
    int octave = 3;                      // C3 ... C5; scientific pitch notation
    int degree = 0;                      // 0-based, rounded into scale
    Motion motion = Motion::fixed;
    Rate rate = Rate::sixteenth;
    float feedback = 0.84f;              // 0..1
    float brightness = 0.72f;            // 0..1
    float mix = 0.7f;                    // 0..1
    float outputDb = -4.0f;
};

inline int scaleLength(Scale scale) noexcept {
    switch (scale) {
        case Scale::minorPentatonic:
        case Scale::majorPentatonic: return 5;
        default: return 7;
    }
}

inline int semitoneForDegree(Scale scale, int degree) noexcept {
    static constexpr std::array<int, 7> natMinor{0, 2, 3, 5, 7, 8, 10};
    static constexpr std::array<int, 7> major   {0, 2, 4, 5, 7, 9, 11};
    static constexpr std::array<int, 7> dorian  {0, 2, 3, 5, 7, 9, 10};
    static constexpr std::array<int, 7> harmonic{0, 2, 3, 5, 7, 8, 11};
    static constexpr std::array<int, 5> minPent {0, 3, 5, 7, 10};
    static constexpr std::array<int, 5> majPent {0, 2, 4, 7, 9};
    degree = std::clamp(degree, 0, scaleLength(scale) - 1);
    switch (scale) {
        case Scale::naturalMinor: return natMinor[static_cast<size_t>(degree)];
        case Scale::major: return major[static_cast<size_t>(degree)];
        case Scale::dorian: return dorian[static_cast<size_t>(degree)];
        case Scale::minorPentatonic: return minPent[static_cast<size_t>(degree)];
        case Scale::majorPentatonic: return majPent[static_cast<size_t>(degree)];
        case Scale::harmonicMinor: return harmonic[static_cast<size_t>(degree)];
    }
    return 0;
}

inline int midiForDegree(int key, Scale scale, int octave, int degree) noexcept {
    const int midi = 12 * (std::clamp(octave, 2, 5) + 1) + std::clamp(key, 0, 11)
                     + semitoneForDegree(scale, degree);
    return std::clamp(midi, 36, 96);
}

inline double frequencyForMidi(int note) noexcept {
    return 440.0 * std::pow(2.0, (static_cast<double>(note) - 69.0) / 12.0);
}

class Engine {
public:
    void prepare(double sampleRate, int maxChannels = 2) {
        sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
        const size_t size = static_cast<size_t>(std::ceil(sampleRate_ / 50.0)) + 16;
        channels_.resize(static_cast<size_t>(std::clamp(maxChannels, 1, 2)));
        for (auto& channel : channels_) {
            channel.delay.assign(size, 0.0f);
            channel.pos = 0;
            channel.feedbackLP = 0.0f;
            channel.delaySamples = 0.0;
        }
        stepCounter_ = 0.0;
        stepIndex_ = 0;
        rng_ = 0xB477BA5Eu;
        activeDegree_ = 0;
        update(controls_, 120.0);
        reset();
    }

    void reset() noexcept {
        for (auto& channel : channels_) {
            std::fill(channel.delay.begin(), channel.delay.end(), 0.0f);
            channel.pos = 0;
            channel.feedbackLP = 0.0f;
            channel.delaySamples = targetDelaySamples_;
        }
        stepCounter_ = 0.0;
        stepIndex_ = 0;
        activeDegree_ = std::min(controls_.degree, scaleLength(controls_.scale) - 1);
        chooseNote();
    }

    void update(const Controls& controls, double bpm) noexcept {
        const auto old = controls_;
        controls_ = controls;
        controls_.key = std::clamp(controls_.key, 0, 11);
        controls_.octave = std::clamp(controls_.octave, 2, 5);
        controls_.degree = std::clamp(controls_.degree, 0, scaleLength(controls_.scale) - 1);
        controls_.feedback = std::clamp(controls_.feedback, 0.0f, 1.0f);
        controls_.brightness = std::clamp(controls_.brightness, 0.0f, 1.0f);
        controls_.mix = std::clamp(controls_.mix, 0.0f, 1.0f);
        controls_.outputDb = std::clamp(controls_.outputDb, -24.0f, 6.0f);
        outputGain_ = std::pow(10.0f, controls_.outputDb / 20.0f);
        // Logarithmic feedback scaling makes high feedback musically useful.
        feedbackGain_ = 1.0f - std::exp(-(0.70f + 5.75f *
                          std::pow(controls_.feedback, 1.5f)));
        feedbackGain_ = std::min(feedbackGain_, 0.9985f);
        lowpassAlpha_ = 0.12f + 0.86f * controls_.brightness;
        stepSamples_ = sampleRate_ * 60.0 / std::clamp(bpm, 30.0, 300.0);
        switch (controls_.rate) {
            case Rate::quarter: break;
            case Rate::eighth: stepSamples_ /= 2.0; break;
            case Rate::sixteenth: stepSamples_ /= 4.0; break;
            case Rate::thirtySecond: stepSamples_ /= 8.0; break;
        }
        stepSamples_ = std::max(1.0, stepSamples_);
        if (controls_.key != old.key || controls_.scale != old.scale ||
            controls_.octave != old.octave || controls_.degree != old.degree ||
            controls_.motion != old.motion) {
            stepIndex_ = 0;
            activeDegree_ = controls_.degree;
        }
        chooseNote();
    }

    // Call once per audio frame, before processing individual channels.
    void startFrame() noexcept {
        if (controls_.motion == Motion::fixed) return;
        stepCounter_ += 1.0;
        if (stepCounter_ >= stepSamples_) {
            stepCounter_ -= stepSamples_;
            ++stepIndex_;
            chooseNote();
        }
    }

    float processSample(int channel, float sample) noexcept {
        if (channel < 0 || static_cast<size_t>(channel) >= channels_.size()) return sample;
        auto& ch = channels_[static_cast<size_t>(channel)];
        if (ch.delay.empty()) return sample;

        // Glide smoothly between tuned delay lengths instead of abruptly switching.
        const double smoothing = 1.0 - std::exp(-1.0 / (sampleRate_ * 0.012));
        ch.delaySamples += (targetDelaySamples_ - ch.delaySamples) * smoothing;
        const double length = static_cast<double>(ch.delay.size());
        double readPos = static_cast<double>(ch.pos) - ch.delaySamples;
        if (readPos < 0.0) readPos += length;
        const auto idx0 = static_cast<size_t>(readPos);
        const auto idx1 = (idx0 + 1U) % ch.delay.size();
        const float frac = static_cast<float>(readPos - static_cast<double>(idx0));
        const float delayed = ch.delay[idx0] + frac * (ch.delay[idx1] - ch.delay[idx0]);

        // Feedback damping controls harmonic brightness. Internal limiting
        // protects from overflows with very hot input and feedback settings.
        ch.feedbackLP += lowpassAlpha_ * (delayed - ch.feedbackLP);
        const float toWrite = std::clamp(sample + feedbackGain_ * ch.feedbackLP,
                                         -6.0f, 6.0f);
        ch.delay[ch.pos] = toWrite;
        ch.pos = (ch.pos + 1U) % ch.delay.size();

        const float wet = std::tanh(delayed * 0.65f);
        const float mixed = (1.0f - controls_.mix) * sample + controls_.mix * wet;
        return std::clamp(mixed * outputGain_, -6.0f, 6.0f);
    }

    int activeMidiNote() const noexcept { return currentNote_; }
    double targetDelaySamples() const noexcept { return targetDelaySamples_; }
    int activeScaleDegree() const noexcept { return activeDegree_; }

private:
    struct Channel {
        std::vector<float> delay;
        size_t pos = 0;
        float feedbackLP = 0.0f;
        double delaySamples = 0.0;
    };

    uint32_t randomNumber() noexcept {
        // Deterministic, realtime-safe xorshift RNG: no allocation or locks.
        rng_ ^= rng_ << 13U;
        rng_ ^= rng_ >> 17U;
        rng_ ^= rng_ << 5U;
        return rng_;
    }

    void chooseNote() noexcept {
        const int count = scaleLength(controls_.scale);
        switch (controls_.motion) {
            case Motion::fixed:
                activeDegree_ = controls_.degree;
                break;
            case Motion::up:
                activeDegree_ = (controls_.degree + stepIndex_) % count;
                break;
            case Motion::bounce: {
                const int period = 2 * (count - 1);
                const int pos = (controls_.degree + stepIndex_) % period;
                activeDegree_ = pos < count ? pos : period - pos;
                break;
            }
            case Motion::random:
                if (stepIndex_ != lastRandomStep_) {
                    activeDegree_ = static_cast<int>(randomNumber() % static_cast<uint32_t>(count));
                    lastRandomStep_ = stepIndex_;
                }
                break;
        }
        currentNote_ = midiForDegree(controls_.key, controls_.scale,
                                     controls_.octave, activeDegree_);
        targetDelaySamples_ = std::clamp(sampleRate_ / frequencyForMidi(currentNote_),
                                        3.0, sampleRate_ / 50.0);
    }

    double sampleRate_ = 48000.0;
    std::vector<Channel> channels_;
    Controls controls_{};
    float feedbackGain_ = 0.97f;
    float lowpassAlpha_ = 0.75f;
    float outputGain_ = 0.7f;
    double targetDelaySamples_ = 230.0;
    double stepCounter_ = 0.0;
    double stepSamples_ = 6000.0;
    int stepIndex_ = 0;
    int lastRandomStep_ = -1;
    int activeDegree_ = 0;
    int currentNote_ = 53;
    uint32_t rng_ = 0xB477BA5Eu;
};

} // namespace ktd
