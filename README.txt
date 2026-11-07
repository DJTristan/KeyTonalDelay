KEY TONAL DELAY v0.1 -- Ableton Live (Apple Silicon / Intel Mac)
================================================================

STATUS: SOURCE PROJECT; NOT YET VERIFIED ON macOS OR IN ABLETON.

This is a small tonal resonant comb-delay effect, INSPIRED BY the idea of
Effectrix Tonal Delay but independently written. It is NOT a clone or pitch
corrector. It makes incoming audio ring in notes belonging to a selected key.

No custom GUI: parameters appear in Ableton's stock plugin panel and JUCE's
generic parameter editor. Presets and parameter automation are supported.

DEFAULT SOUND: F natural minor (F3), high feedback, with a 70% wet mix.
Try it on a drum loop or percussion; it is not intended to retune full songs.

CONTROLS:
  Key       - root note, default F
  Scale     - natural minor, major, dorian, minor pentatonic,
              major pentatonic, harmonic minor
  Octave    - base octave C2-C5
  Degree    - selected scale degree, 1 to 7 (pentatonic clamps at 5)
  Motion    - fixed note, ascending, bounce, or random scale notes
  Step rate - quarter / eighth / sixteenth / thirty-second at DAW tempo
  Feedback  - amount of resonant ringing
  Brightness - feedback loop low-pass damping
  Wet mix   - wet/dry blend
  Output dB - output trim

IMPORTANT: A tuned feedback delay can emphasize harmonics, especially on
pitched source material. Start at modest speaker volume. This prototype
limits its internal signal but does not replace track-level metering.

BUILD VIA GITHUB ACTIONS
------------------------
1) Make a NEW PUBLIC repository at https://github.com/new (do not overwrite
   the existing working LiveCut Enhanced install or the old wierdcut project).
2) Unzip the supplied KeyTonalDelay_GitHub_Source.zip file on your Mac.
3) Upload the CONTENTS of the KeyTonalDelay folder to the ROOT of your new
   repository (CMakeLists.txt, src, tests, README.txt and hidden .github).
   Finder: Command+Shift+. shows the hidden .github folder.
   GitHub's web upload sometimes skips hidden directories. If so, create
   .github/workflows/build-macos.yml directly through Add file > Create new file,
   and paste the contents from the unzipped file.
4) Commit and open the Actions tab. The build runs automatically on a hosted
   Mac and tests the DSP and Audio Unit loading before it uploads the plugin.
5) ONLY if the workflow is green, download KeyTonalDelay-Universal-Mac.
   You get a GitHub artifact ZIP which contains another ZIP; extract both.
6) Copy "Key Tonal Delay.vst3" into ~/Library/Audio/Plug-Ins/VST3
   OR "Key Tonal Delay.component" into ~/Library/Audio/Plug-Ins/Components.
7) Quit and relaunch Ableton. Check Plugin settings and rescan if necessary.
   Try it with an EMPTY TEST PROJECT. Preserve your working LiveCut install.

If Actions fails, click the red step and inspect logs; DO NOT install a plugin
from a failed build. A green workflow verifies engine tests + auval on the
GitHub Mac, but a separate test inside your Ableton installation is still
necessary. Compiled binary is NOT included in this ZIP.

LOCAL DSP-ONLY TEST (no JUCE download necessary)
-----------------------------------------------
cmake -S . -B build-tests -DBUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure

DETAILS
-------
JUCE 8.0.15 is fetched on the GitHub Mac by CMake. Its license applies to
use and distribution of JUCE-based code. Review licensing before distributing
commercially; this project is a development prototype.

SOURCE:
  src/TonalDelayEngine.h - standalone DSP engine
  src/PluginProcessor.cpp/.h - JUCE DAW plugin wrapper / parameters
  tests/tonal_delay_tests.cpp - automated audio engine tests
  .github/workflows/build-macos.yml - automated build, signing, validation

v0.1 is deliberately tiny, without graphics or extra features.
