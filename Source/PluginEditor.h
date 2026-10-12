#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
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
// Phosphor-green oscilloscope showing the plugin's output. Click to change zoom.
class ScopeDisplay : public juce::Component, private juce::Timer
{
public:
    explicit ScopeDisplay (FerriteProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { zoom = (zoom + 1) % 3; }
private:
    void timerCallback() override { repaint(); }
    FerriteProcessor& proc;
    std::vector<float> snap;
    float gain = 1.f;
    int zoom = 1;                                   // 0: wide, 1: normal, 2: close-up
};

//==============================================================================
// Ivory-and-ebony on-screen keyboard
class VintageKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit VintageKeyboard (juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard) {}

    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour line, juce::Colour text) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour fill) override;
};

//==============================================================================
// Parametric-EQ response display. Drag a node: left/right = frequency, up/down = gain.
// Mouse wheel over a node = Q (bandwidth). Double-click a node to reset that band.
class EqDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EqDisplay (juce::AudioProcessorValueTreeState& apvts);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static juce::Colour bandColour (int band);

private:
    static constexpr int N = ferrite::kEqBands;
    juce::Rectangle<float> area() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float gainToY (float db) const;
    float yToGain (float y) const;
    float freqOf (int b) const { return pF[b]->convertFrom0to1 (pF[b]->getValue()); }
    float gainOf (int b) const { return pG[b]->convertFrom0to1 (pG[b]->getValue()); }
    float qOf (int b) const    { return pQ[b]->convertFrom0to1 (pQ[b]->getValue()); }
    juce::Point<float> nodePos (int b) const { return { freqToX (freqOf (b)), gainToY (gainOf (b)) }; }
    int  hitTest (juce::Point<float>) const;
    void timerCallback() override;

    juce::RangedAudioParameter *pF[N], *pG[N], *pQ[N], *pOn;
    int hover = -1, dragging = -1;
    float last[N * 3 + 1] {};
};

//==============================================================================
// Pitch / modulation wheel. Drag vertically. The pitch wheel springs back to centre on release;
// the mod wheel stays where you leave it (double-click to zero). Follows host MIDI too.
class WheelControl : public juce::Component, private juce::Timer
{
public:
    WheelControl (std::atomic<float>& value, bool bipolarSpringReturn, const juce::String& label, juce::Colour accent);
    ~WheelControl() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> face() const;
    void setFromY (float y);
    void timerCallback() override;

    std::atomic<float>& value;
    bool bipolar;
    juce::String label;
    juce::Colour accent;
    float shown = 0.f;
    bool dragging = false, returning = false;
};

//==============================================================================
// Interactive ADSR graph. Drag the three handles:
//   attack handle  -> attack time        (left / right)
//   decay handle   -> decay time + sustain level (left / right + up / down)
//   release handle -> release time       (left / right)
// Double-click a handle to reset it. Stays in sync with the knobs.
class EnvelopeDisplay : public juce::Component, private juce::Timer
{
public:
    EnvelopeDisplay (juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& idA, const juce::String& idD,
                     const juce::String& idS, const juce::String& idR, juce::Colour accent);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum Handle { none = -1, attack = 0, decay = 1, release = 2 };
    juce::Rectangle<float> area() const;
    juce::Point<float> handlePos (int h) const;
    int  hitTest (juce::Point<float>) const;
    void beginGesture (int h);
    void endGesture (int h);
    void timerCallback() override;

    juce::RangedAudioParameter *pA, *pD, *pS, *pR;
    juce::Colour accent;
    int hover = none, dragging = none;
    float last[4] { -1.f, -1.f, -1.f, -1.f };
};

// Read-only picture of the selected LFO waveform.
class LfoPreview : public juce::Component, private juce::Timer
{
public:
    LfoPreview (juce::AudioProcessorValueTreeState& apvts, juce::Colour accent);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    std::atomic<float>* shape;
    juce::Colour accent;
    int lastShape = -1;
};

//==============================================================================
// One knob: caption on top, rotary in the middle, readout underneath
class KnobControl : public juce::Component
{
public:
    KnobControl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& caption);
    void resized() override;
    void parentHierarchyChanged() override;

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

    static constexpr int kWidth = 1274, kHeight = 584;

private:
    struct Section
    {
        juce::String title;
        juce::Colour accent;
        juce::Rectangle<int> bounds;
        std::vector<KnobControl*> knobs;
        int knobTop = 26;                       // y offset of the knob row inside the section
        int cellW = 62, cols = 0, rowH = 98;    // cols > 0: knobs wrap into a grid
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
    ScopeDisplay scope;
    VintageKeyboard keyboard;
    WheelControl pitchWheel, modWheel;
    std::unique_ptr<EnvelopeDisplay> fenvGraph, aenvGraph;
    std::unique_ptr<EqDisplay> eqGraph;
    juce::TextButton eqOnBtn { "EQ ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqOnAttach;
    std::unique_ptr<LfoPreview> lfoGraph;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, lcdBtn, saveBtn { "SAVE" }, initBtn { "INIT" };
    juce::String shownPreset;
    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FerriteEditor)
};
