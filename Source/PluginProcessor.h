#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Synth.h"

class FerriteProcessor : public juce::AudioProcessor
{
public:
    FerriteProcessor();
    ~FerriteProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ferrite 8"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> activeVoices { 0 };

    // On-screen keyboard <-> audio thread (also lights the keys for MIDI coming from the host)
    juce::MidiKeyboardState keyboardState;

    // Performance wheels. Single source of truth: the on-screen wheels and incoming host MIDI
    // both write here, and the audio thread applies whatever changed.
    std::atomic<float> pitchBendNorm { 0.f };      // -1 .. +1  (+/- 2 semitones)
    std::atomic<float> modWheelNorm  { 0.f };      //  0 .. 1

    // ---- oscilloscope feed (audio thread writes, editor reads) -------------
    static constexpr int kScopeSize = 8192;                  // power of two
    std::array<std::atomic<float>, kScopeSize> scope;
    std::atomic<int> scopeHead { 0 };

    // ---- presets -----------------------------------------------------------
    static int          getNumFactoryPresets();
    static juce::String getFactoryPresetName (int index);
    void                loadFactoryPreset (int index);
    void                resetToInit();

    juce::File          getPresetDirectory() const;
    juce::StringArray   getUserPresetNames() const;
    bool                isUserPreset (const juce::String& name) const;
    bool                saveUserPreset (juce::String name);
    bool                loadUserPreset (const juce::String& name);
    bool                deleteUserPreset (const juce::String& name);

    juce::String        getCurrentPresetName() const;
    void                stepPreset (int delta);      // +1 next, -1 previous (factory, then user)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    ferrite::Params readParams() const;
    void handleMidi (const juce::MidiMessage&);
    void setCurrentPresetName (const juce::String&);
    juce::File presetFile (const juce::String& name) const;

    ferrite::Synth synth;
    float appliedBend = 0.f, appliedMod = 0.f;     // audio thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FerriteProcessor)
};
