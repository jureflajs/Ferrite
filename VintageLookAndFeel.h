#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Palette -------------------------------------------------------------------
namespace vintage
{
    inline const juce::Colour cream   { 0xffe9ddc0 };
    inline const juce::Colour ivory   { 0xfff3e8cc };
    inline const juce::Colour amber   { 0xffffb347 };
    inline const juce::Colour orange  { 0xffd9731f };
    inline const juce::Colour red     { 0xffb8321f };
    inline const juce::Colour gold    { 0xffd9a82b };
    inline const juce::Colour olive   { 0xff8a9a3b };
    inline const juce::Colour teal    { 0xff3f8f86 };
    inline const juce::Colour panelHi { 0xff2f2823 };
    inline const juce::Colour panelLo { 0xff211c18 };

    inline juce::Font serif (float h, int style = juce::Font::plain)
    {
        return juce::Font (juce::FontOptions ("Georgia", h, style));
    }
    inline juce::Font mono (float h)
    {
        return juce::Font (juce::FontOptions ("Courier New", h, juce::Font::bold));
    }
}

//==============================================================================
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       vintage::amber);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId,               vintage::cream);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (vintage::mono (11.5f));
        l->setJustificationType (juce::Justification::centred);
        l->setBorderSize ({ 0, 0, 0, 0 });
        return l;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = bg;
        if (down) c = c.darker (0.25f); else if (over) c = c.brighter (0.12f);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.f, 1.5f), 3.f);
        g.setGradientFill (juce::ColourGradient (c.brighter (0.15f), r.getX(), r.getY(),
                                                 c.darker (0.2f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawRoundedRectangle (r, 3.f, 1.f);
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int) override
    {
        return b.getComponentID() == "lcd" ? vintage::mono (12.5f) : vintage::serif (11.f, juce::Font::bold);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float r = std::min (w, h) * 0.5f - 9.f;

        // scale ticks (stepped knobs get one tick per step)
        int steps = 10;
        const auto range = s.getRange();
        if (s.getInterval() >= 1.0 && range.getLength() / s.getInterval() <= 8.0)
            steps = (int) std::round (range.getLength() / s.getInterval());

        g.setColour (vintage::cream.withAlpha (0.8f));
        for (int i = 0; i <= steps; ++i)
        {
            const float a = startA + (float) i / (float) steps * (endA - startA);
            const bool major = (steps == 10) ? (i % 5 == 0) : true;
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (r + 3.f, a),
                                           c.getPointOnCircumference (r + (major ? 8.f : 6.f), a)),
                        major ? 1.4f : 1.f);
        }

        // drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - r + 1.5f, c.y - r + 3.f, r * 2.f, r * 2.f);

        // black knurled skirt
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3731), c.x - r * 0.5f, c.y - r * 0.8f,
                                                 juce::Colour (0xff0d0b09), c.x + r * 0.6f, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);

        g.setColour (juce::Colours::white.withAlpha (0.16f));
        for (int i = 0; i < 30; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / 30.f;
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (r * 0.84f, a),
                                           c.getPointOnCircumference (r * 0.98f, a)), 0.8f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 1.f);

        // ivory cap
        const float cr = r * 0.72f;
        g.setGradientFill (juce::ColourGradient (vintage::ivory, c.x - cr * 0.6f, c.y - cr * 0.8f,
                                                 juce::Colour (0xffb5a47c), c.x + cr * 0.7f, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f, 1.f);

        // pointer
        const float a = startA + pos * (endA - startA);
        juce::Path p;
        p.startNewSubPath (c.getPointOnCircumference (cr * 0.12f, a));
        p.lineTo (c.getPointOnCircumference (cr * 0.93f, a));
        g.setColour (juce::Colour (0xff2a1a10));
        g.strokePath (p, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};
