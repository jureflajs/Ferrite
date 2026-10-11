#pragma once
#include "PluginProcessor.h"
#include "VintageLookAndFeel.h"

//==============================================================================
// Glowing "voice active" lamp
class LedLamp : public juce::Component, private juce::Timer
{
public:
    explicit LedLamp (std::atomic<int>& v) : voices (v) { startTimerHz (30); }
    void paint (juce::Graphics& g) override;
private:
    void timerCallback() override;
    std::atomic<int>& voices;
    float level = 0.f, drawn = -1.f;
};

//==============================================================================
// One knob: caption on top, rotary in the middle, readout underneath
class KnobControl : public juce::Component
{
public:
    KnobControl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& caption);
    void resized() override;

    juce::Label  name;
    juce::Slider slider;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class FerriteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FerriteEditor (FerriteProcessor&);
    ~FerriteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 980, kHeight = 432;

private:
    struct Section
    {
        juce::String title;
        juce::Colour accent;
        juce::Rectangle<int> bounds;
        std::vector<KnobControl*> knobs;
    };

    Section& addSection (const juce::String& title, juce::Colour accent, juce::Rectangle<int> bounds);
    void addKnob (Section& s, const juce::String& id, const juce::String& caption);
    void buildBackground();
    void timerCallback() override;
    void refreshPresetName();
    void showPresetMenu();
    void promptSave();

    VintageLookAndFeel laf;
    FerriteProcessor& proc;
    std::vector<Section> sections;
    std::vector<std::unique_ptr<KnobControl>> knobs;
    LedLamp led;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, lcdBtn, saveBtn { "SAVE" }, initBtn { "INIT" };
    juce::String shownPreset;
    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FerriteEditor)
};
