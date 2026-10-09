# Resonance

Resonance is a JUCE audio plug-in for real-time monophonic pitch tracking and
performance feedback. It builds as AU, VST3, and a standalone macOS app.

## Features

- YIN-based pitch detection with note, frequency, tuning, confidence, and pitch history.
- Optional MIDI note output generated from the incoming audio.
- MIDI-reference comparison with a running pitch-match score (within one semitone).
- Onset-based tempo estimate and a custom live analysis dashboard.

## Build

Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.6 is fetched during CMake
configuration.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The AU, VST3, and standalone artifacts are written under
`build/Resonance_artefacts/Release/`.

## Use

Insert Resonance on an audio track and enable the host's MIDI-output routing to
record the generated pitch track. To score a performance, route a reference
MIDI part into the plug-in while the performer plays into its audio input. The
pitch-sensitivity control adjusts the detector threshold; MIDI output can be
enabled or disabled independently.
