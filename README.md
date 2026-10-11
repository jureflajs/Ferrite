# Ferrite 8  -  vintage analog polysynth (VST3)

8-voice analog-modelled polyphonic synthesizer with a wood-and-cream vintage front panel.

## Signal path
`OSC 1 + OSC 2 (saw / pulse / tri, PolyBLEP) + SUB + NOISE -> 24 dB ladder low-pass (non-linear, self-oscillating) -> VCA -> ensemble chorus -> soft saturation`

| Section | Controls |
|---|---|
| Oscillators | Wave 1, Wave 2, Pulse Width, Semitone, Fine detune (osc 2) |
| Mixer | Osc 1, Osc 2, Sub (square, -1 oct), Noise |
| Low-pass filter | Cutoff, Resonance, Env Amount (bipolar), Key Track |
| Filter / Amp envelope | Attack, Decay, Sustain, Release each |
| Modulation LFO | Rate, Shape (sine/tri/saw/square/S&H), -> Pitch, -> Filter, -> PWM |
| Output | Glide, Drift (analog pitch wander), Ensemble (chorus), Volume |

MIDI: notes + velocity, pitch bend (+/-2 st), mod wheel (CC1 -> vibrato), sustain pedal (CC64), all-notes-off.
Double-click any knob to reset it. Everything is automatable from Ableton.

## Presets
- **9 factory presets** (Init, Warm Pad, PWM Strings, Brass Stab, Sub Bass, Acid Squelch, Pluck Lead, Soaring Lead, Wind FX).
- **< / >** step through presets, click the amber display for the full menu, **SAVE** stores your current sound, **INIT** resets everything.
- Your presets are plain files in `Documents\Ferrite Audio\Ferrite 8\Presets` (`.f8preset`). Copy them to share or back up. The menu has *Show preset folder* and *Delete current user preset*.
- Your patch is also saved inside every Ableton Live set automatically, and Live's own "Save as default" / device-preset buttons work too.

## Getting the plugin on Windows
**Option A - no compiler needed (recommended)**
1. Create a free GitHub repo and push this folder to it (include the `.github` folder).
2. Open the repo's **Actions** tab -> *Build Ferrite 8 (VST3)* -> wait for the green tick (about 10 min).
3. Download **Ferrite8-Windows-VST3** from the run page and unzip it. You get a `Ferrite 8.vst3` folder.

**Option B - build locally**
Install Visual Studio 2022 (workload "Desktop development with C++") and CMake, then in a terminal in this folder:

    cmake -B build
    cmake --build build --config Release --parallel

Output: `build\Ferrite8_artefacts\Release\VST3\Ferrite 8.vst3`

## Installing in Ableton Live 12
1. Copy the whole `Ferrite 8.vst3` folder to `C:\Program Files\Common Files\VST3\` (needs admin).
2. In Live: *Settings -> Plug-ins* -> turn on **Use VST3 Plug-in System Folders** -> press **Rescan**.
3. Find it under *Plug-ins -> Ferrite 8* (or search "Ferrite") and drag it onto a MIDI track.

(macOS notes: copy to `~/Library/Audio/Plug-Ins/VST3/`, then `xattr -cr` and `codesign --force --deep -s -` on the bundle.)

## Source layout
- `Source/Synth.h` - all DSP (voices, filter, envelopes, LFO, chorus)
- `Source/PluginProcessor.*` - parameters, MIDI, state, preset save/load
- `Source/FactoryPresets.h` - the factory patches (easy to edit or add to)
- `Source/PluginEditor.*`, `Source/VintageLookAndFeel.h` - the vintage UI
