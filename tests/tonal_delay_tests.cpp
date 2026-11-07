#include "TonalDelayEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;
void test(bool condition, const std::string& message) {
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    else std::cout << "PASS: " << message << '\n';
}
void testScaleMapping() {
    test(ktd::midiForDegree(5, ktd::Scale::naturalMinor, 3, 0) == 53, "F minor root is F3 (MIDI 53)");
    const int notes[]{53,55,56,58,60,61,63};
    for (int i = 0; i < 7; ++i)
        test(ktd::midiForDegree(5, ktd::Scale::naturalMinor, 3, i) == notes[i],
             "F natural minor degree " + std::to_string(i + 1));
    test(ktd::scaleLength(ktd::Scale::minorPentatonic) == 5, "Minor pentatonic has five degrees");
    test(ktd::midiForDegree(5, ktd::Scale::minorPentatonic, 3, 4) == 63,
         "F minor pentatonic fifth degree Eb4");
}
void testImpulseAndNoOverflow() {
    ktd::Engine engine;
    engine.prepare(48000.0, 2);
    ktd::Controls c;
    c.mix = 1.0f;
    c.outputDb = 0.0f;
    c.feedback = 0.78f;
    c.brightness = 1.0f;
    engine.update(c, 148.0);
    const double period = engine.targetDelaySamples();
    test(std::abs(period - 48000.0 / ktd::frequencyForMidi(53)) < 0.0001,
         "Tuned comb delay length matches F3 frequency");
    double energyAfterDelay = 0.0;
    double energyNearStart = 0.0;
    bool allFinite = true;
    float maxAmplitude = 0.0f;
    const int center = static_cast<int>(std::lround(period));
    for (int i = 0; i < 96000; ++i) {
        engine.startFrame();
        const float outL = engine.processSample(0, i == 0 ? 1.0f : 0.0f);
        const float outR = engine.processSample(1, 0.0f);
        allFinite &= std::isfinite(outL) && std::isfinite(outR);
        maxAmplitude = std::max(maxAmplitude, std::abs(outL));
        if (i > center - 5 && i < center + 5) energyAfterDelay += outL * outL;
        if (i < 50) energyNearStart += outL * outL;
        if (std::abs(outR) > 0.00001f) allFinite = false;
    }
    test(energyAfterDelay > 0.01, "Impulse excites tuned delay at expected period");
    test(energyNearStart < 0.00001, "Wet-only delayed path has no immediate dry transient");
    test(allFinite && maxAmplitude <= 6.0f, "Two seconds processing finite/bounded and stereo independent");
}
void testMotionAndControls() {
    ktd::Engine engine;
    engine.prepare(48000.0, 1);
    ktd::Controls c;
    c.motion = ktd::Motion::up;
    c.rate = ktd::Rate::thirtySecond;
    engine.update(c, 120.0);
    test(engine.activeMidiNote() == 53, "Ascending begins on F3");
    for (int i = 0; i < 3001; ++i) engine.startFrame();
    test(engine.activeMidiNote() == 55, "Ascending advances to G3 after one 1/32 beat");
    c.motion = ktd::Motion::random;
    engine.update(c, 120.0);
    bool inScale = true;
    for (int i = 0; i < 100000; ++i) {
        engine.startFrame();
        const int midi = engine.activeMidiNote();
        bool found = false;
        for (int degree = 0; degree < 7; ++degree)
            found |= midi == ktd::midiForDegree(c.key, c.scale, c.octave, degree);
        inScale &= found;
    }
    test(inScale, "Random motion never selects notes outside the chosen scale");
    c.mix = 0.0f; c.outputDb = 0.0f; c.motion = ktd::Motion::fixed;
    engine.reset(); engine.update(c, 120.0);
    bool transparent = true;
    for (int i = 0; i < 1000; ++i) {
        engine.startFrame();
        transparent &= std::abs(engine.processSample(0, 0.2f) - 0.2f) < 0.00001f;
    }
    test(transparent, "Mix at zero passes dry audio unchanged at 0 dB output");
}
}
int main() {
    testScaleMapping();
    testImpulseAndNoOverflow();
    testMotionAndControls();
    if (failures) { std::cerr << failures << " tests failed\n"; return EXIT_FAILURE; }
    std::cout << "All DSP tests passed\n";
    return EXIT_SUCCESS;
}
