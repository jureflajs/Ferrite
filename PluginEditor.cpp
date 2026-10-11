#include "PluginEditor.h"

using namespace juce;
namespace v = vintage;

static constexpr int kCell = 62, kSecH = 128, kKnobH = 98;

//==============================================================================
void LedLamp::timerCallback()
{
    const float target = voices.load() > 0 ? 1.f : 0.f;
    level += (target - level) * 0.35f;
    if (std::abs (level - drawn) > 0.01f) { drawn = level; repaint(); }
}

void LedLamp::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (6.f);
    const auto c = b.getCentre();
    const float r = b.getWidth() * 0.5f;

    if (level > 0.02f)
    {
        g.setGradientFill (ColourGradient (Colour (0xffff4a2a).withAlpha (0.55f * level), c.x, c.y,
                                           Colours::transparentBlack, c.x + r * 2.2f, c.y, true));
        g.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
    }
    g.setColour (Colours::black);
    g.fillEllipse (b.expanded (2.5f));                                   // bezel
    g.setColour (Colour (0xff8a7f68));
    g.drawEllipse (b.expanded (2.5f), 1.2f);

    const auto off = Colour (0xff3a0d0a), on = Colour (0xffff5a38);
    g.setGradientFill (ColourGradient (off.interpolatedWith (on, level).brighter (0.3f), c.x - r * 0.3f, c.y - r * 0.4f,
                                       off.interpolatedWith (on, level).darker (0.5f), c.x + r, c.y + r, true));
    g.fillEllipse (b);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.6f, r * 0.6f, r * 0.4f);  // glass highlight
}

//==============================================================================
KnobControl::KnobControl (AudioProcessorValueTreeState& apvts, const String& id, const String& caption)
{
    name.setText (caption, dontSendNotification);
    name.setJustificationType (Justification::centred);
    name.setFont (v::serif (10.5f, Font::bold));
    name.setColour (Label::textColourId, v::cream);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (Slider::TextBoxBelow, true, kCell, 16);
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
}

void KnobControl::resized()
{
    name.setBounds (0, 0, getWidth(), 16);
    slider.setBounds (0, 16, getWidth(), getHeight() - 16);
}

//==============================================================================
FerriteEditor::FerriteEditor (FerriteProcessor& p)
    : AudioProcessorEditor (&p), proc (p), led (p.activeVoices)
{
    setLookAndFeel (&laf);
    setSize (kWidth, kHeight);
    setResizable (false, false);

    // ---- layout of sections (x, y fixed; each knob cell is 62 px wide)
    const int r1 = 142, r2 = 276;
    auto& osc = addSection ("OSCILLATORS", v::orange, { 48, r1, 326, kSecH });
    addKnob (osc, "osc1Wave", "WAVE 1");  addKnob (osc, "osc2Wave", "WAVE 2");
    addKnob (osc, "pw", "PULSE W");       addKnob (osc, "osc2Semi", "SEMI");
    addKnob (osc, "osc2Fine", "FINE");

    auto& mix = addSection ("MIXER", v::gold, { 389, r1, 264, kSecH });
    addKnob (mix, "mix1", "OSC 1");  addKnob (mix, "mix2", "OSC 2");
    addKnob (mix, "mixSub", "SUB");  addKnob (mix, "mixNoise", "NOISE");

    auto& flt = addSection ("LOW-PASS FILTER", v::red, { 668, r1, 264, kSecH });
    addKnob (flt, "cutoff", "CUTOFF");  addKnob (flt, "reso", "RESO");
    addKnob (flt, "envAmt", "ENV AMT"); addKnob (flt, "keyTrack", "KEY TRK");

    auto& fenv = addSection ("FILTER ENVELOPE", v::red, { 48, r2, 264, kSecH });
    addKnob (fenv, "fA", "ATTACK");  addKnob (fenv, "fD", "DECAY");
    addKnob (fenv, "fS", "SUSTAIN"); addKnob (fenv, "fR", "RELEASE");

    auto& aenv = addSection ("AMP ENVELOPE", v::olive, { 327, r2, 264, kSecH });
    addKnob (aenv, "aA", "ATTACK");  addKnob (aenv, "aD", "DECAY");
    addKnob (aenv, "aS", "SUSTAIN"); addKnob (aenv, "aR", "RELEASE");

    auto& lfo = addSection ("MODULATION LFO", v::teal, { 606, r2, 326, kSecH });
    addKnob (lfo, "lfoRate", "RATE");   addKnob (lfo, "lfoShape", "SHAPE");
    addKnob (lfo, "lfoPitch", "PITCH"); addKnob (lfo, "lfoFilter", "FILTER");
    addKnob (lfo, "lfoPwm", "PWM");

    auto& out = addSection ("OUTPUT", v::gold, { 668, 8, 264, kSecH });
    addKnob (out, "glide", "GLIDE");     addKnob (out, "drift", "DRIFT");
    addKnob (out, "chorus", "ENSEMBLE"); addKnob (out, "master", "VOLUME");

    addAndMakeVisible (led);

    // ---- preset bar
    const Colour key (0xffcdbf9a), lcdBg (0xff130d08);
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &initBtn })
    {
        b->setColour (TextButton::buttonColourId, key);
        b->setColour (TextButton::textColourOffId, Colour (0xff2a1a10));
        b->setColour (TextButton::textColourOnId,  Colour (0xff2a1a10));
        addAndMakeVisible (*b);
    }
    lcdBtn.setComponentID ("lcd");
    lcdBtn.setColour (TextButton::buttonColourId, lcdBg);
    lcdBtn.setColour (TextButton::textColourOffId, v::amber);
    lcdBtn.setColour (TextButton::textColourOnId,  v::amber);
    addAndMakeVisible (lcdBtn);

    prevBtn.onClick = [this] { proc.stepPreset (-1); refreshPresetName(); };
    nextBtn.onClick = [this] { proc.stepPreset (+1); refreshPresetName(); };
    initBtn.onClick = [this] { proc.loadFactoryPreset (0); refreshPresetName(); };
    saveBtn.onClick = [this] { promptSave(); };
    lcdBtn.onClick  = [this] { showPresetMenu(); };
    refreshPresetName();
    startTimerHz (5);

    buildBackground();
}

FerriteEditor::~FerriteEditor() { stopTimer(); setLookAndFeel (nullptr); }

void FerriteEditor::timerCallback() { refreshPresetName(); }

void FerriteEditor::refreshPresetName()
{
    const auto n = proc.getCurrentPresetName();
    if (n != shownPreset) { shownPreset = n; lcdBtn.setButtonText (n); }
}

void FerriteEditor::showPresetMenu()
{
    PopupMenu menu, factory, user;
    const auto current = proc.getCurrentPresetName();

    for (int i = 0; i < FerriteProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (1000 + i, FerriteProcessor::getFactoryPresetName (i), true,
                         FerriteProcessor::getFactoryPresetName (i) == current);

    auto userNames = proc.getUserPresetNames();
    for (int i = 0; i < userNames.size(); ++i)
        user.addItem (2000 + i, userNames[i], true, userNames[i] == current);
    if (userNames.isEmpty()) user.addItem (-1, "(none saved yet)", false);

    menu.addSubMenu ("Factory presets", factory);
    menu.addSubMenu ("User presets", user);
    menu.addSeparator();
    menu.addItem (1, "Save preset...");
    menu.addItem (2, "Delete current user preset", proc.isUserPreset (current));
    menu.addItem (3, "Show preset folder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&lcdBtn),
        [safe = SafePointer<FerriteEditor> (this), userNames] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            if (r >= 2000)       pr.loadUserPreset (userNames[r - 2000]);
            else if (r >= 1000)  pr.loadFactoryPreset (r - 1000);
            else if (r == 1)     { safe->promptSave(); return; }
            else if (r == 2)     pr.deleteUserPreset (pr.getCurrentPresetName());
            else if (r == 3)     { pr.getPresetDirectory().createDirectory(); pr.getPresetDirectory().revealToUser(); }
            safe->refreshPresetName();
        });
}

void FerriteEditor::promptSave()
{
    auto* w = new AlertWindow ("Save Preset", "Name your patch:", MessageBoxIconType::NoIcon, this);
    const auto cur = proc.getCurrentPresetName();
    w->addTextEditor ("name", proc.isUserPreset (cur) ? cur : String ("My Patch"));
    w->addButton ("Save",   1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    w->enterModalState (true,
        ModalCallbackFunction::create ([safe = SafePointer<FerriteEditor> (this), w] (int result)
        {
            if (result == 1 && safe != nullptr)
            {
                safe->proc.saveUserPreset (w->getTextEditorContents ("name"));
                safe->refreshPresetName();
            }
        }),
        true);
}

FerriteEditor::Section& FerriteEditor::addSection (const String& t, Colour a, Rectangle<int> b)
{
    sections.push_back ({ t, a, b, {} });
    return sections.back();
}

void FerriteEditor::addKnob (Section& s, const String& id, const String& caption)
{
    auto k = std::make_unique<KnobControl> (proc.apvts, id, caption);
    addAndMakeVisible (*k);
    s.knobs.push_back (k.get());
    knobs.push_back (std::move (k));
}

void FerriteEditor::resized()
{
    for (auto& s : sections)
    {
        int i = 0;
        for (auto* k : s.knobs)
            k->setBounds (s.bounds.getX() + 8 + (i++) * kCell, s.bounds.getY() + 26, kCell, kKnobH);
    }
    led.setBounds (572, 16, 34, 34);
    prevBtn.setBounds (316, 20, 24, 26);
    lcdBtn.setBounds  (342, 20, 178, 26);
    nextBtn.setBounds (522, 20, 24, 26);
    saveBtn.setBounds (342, 52, 86, 22);
    initBtn.setBounds (434, 52, 86, 22);
}

void FerriteEditor::paint (Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());
}

//==============================================================================
static void drawWood (Graphics& g, Rectangle<float> r, int seed, bool rightSide)
{
    Random rng (seed);
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());

    g.setGradientFill (ColourGradient (Colour (0xff4a2813), r.getX(), 0.f, Colour (0xff714120), r.getRight(), 0.f, false));
    g.fillRect (r);

    for (int i = 0; i < 110; ++i)
    {
        const float x0   = r.getX() + rng.nextFloat() * r.getWidth();
        const float amp  = 0.4f + rng.nextFloat() * 2.2f;
        const float freq = 0.003f + rng.nextFloat() * 0.012f;
        const float ph   = rng.nextFloat() * 6.28f;
        Path p;
        p.startNewSubPath (x0, r.getY());
        for (float y = r.getY(); y <= r.getBottom() + 8.f; y += 8.f)
            p.lineTo (x0 + amp * std::sin (y * freq + ph) + 0.7f * std::sin (y * 0.045f + ph * 2.f), y);
        g.setColour ((rng.nextBool() ? Colour (0xff25120a) : Colour (0xff9a6a3a)).withAlpha (0.10f + rng.nextFloat() * 0.25f));
        g.strokePath (p, PathStrokeType (0.5f + rng.nextFloat() * 1.6f));
    }

    ColourGradient gloss (Colours::white.withAlpha (rightSide ? 0.02f : 0.14f), r.getX(), 0.f,
                          Colours::black.withAlpha (rightSide ? 0.28f : 0.02f), r.getRight(), 0.f, false);
    if (rightSide) gloss = ColourGradient (Colours::black.withAlpha (0.02f), r.getX(), 0.f,
                                           Colours::white.withAlpha (0.14f), r.getRight(), 0.f, false);
    g.setGradientFill (gloss);
    g.fillRect (r);
    g.restoreState();
}

static void drawScrew (Graphics& g, Point<float> c, float rot)
{
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - 6.f, c.y - 5.f, 12.f, 12.f);
    g.setGradientFill (ColourGradient (Colour (0xffd8d0bd), c.x - 3.f, c.y - 4.f, Colour (0xff6b6454), c.x + 4.f, c.y + 5.f, true));
    g.fillEllipse (c.x - 5.5f, c.y - 5.5f, 11.f, 11.f);
    Path slot; slot.addRectangle (-4.5f, -0.9f, 9.f, 1.8f);
    g.setColour (Colour (0xff2a251c));
    g.fillPath (slot, AffineTransform::rotation (rot).translated (c.x, c.y));
}

void FerriteEditor::buildBackground()
{
    const float scale = 2.f;
    background = Image (Image::ARGB, (int) (kWidth * scale), (int) (kHeight * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));

    const float W = (float) kWidth, H = (float) kHeight, cheek = 36.f;

    // --- panel: warm charcoal with brushed grain + speckle
    g.setGradientFill (ColourGradient (v::panelHi, 0.f, 0.f, v::panelLo, 0.f, H, false));
    g.fillRect (0.f, 0.f, W, H);
    Random rng (1977);
    for (int i = 0; i < 260; ++i)
    {
        const float y = rng.nextFloat() * H, x = rng.nextFloat() * W, len = 40.f + rng.nextFloat() * 260.f;
        g.setColour (Colours::white.withAlpha (0.012f + rng.nextFloat() * 0.025f));
        g.drawLine (x, y, x + len, y, 0.6f);
    }
    for (int i = 0; i < 9000; ++i)
    {
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.02f + rng.nextFloat() * 0.04f));
        g.fillRect (rng.nextFloat() * W, rng.nextFloat() * H, 1.f, 1.f);
    }

    // --- wooden cheeks
    drawWood (g, { 0.f, 0.f, cheek, H }, 11, false);
    drawWood (g, { W - cheek, 0.f, cheek, H }, 29, true);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRect (cheek - 1.5f, 0.f, 2.5f, H);
    g.fillRect (W - cheek - 1.f, 0.f, 2.5f, H);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillRect (cheek + 1.f, 0.f, 1.f, H);

    // --- section boxes
    for (auto& s : sections)
    {
        auto b = s.bounds.toFloat();
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (b.translated (0.f, 2.f), 6.f);
        g.setGradientFill (ColourGradient (Colour (0xff38302a), 0.f, b.getY(), Colour (0xff27211c), 0.f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 6.f);
        g.setColour (Colour (0xff6b5a47).withAlpha (0.8f));
        g.drawRoundedRectangle (b.reduced (0.5f), 6.f, 1.f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (b.reduced (1.5f), 5.f, 1.f);

        g.setColour (s.accent);
        g.fillRect (b.getX() + 8.f, b.getY() + 22.f, b.getWidth() - 16.f, 2.f);
        g.setColour (v::cream);
        g.setFont (v::serif (11.5f, Font::bold).withExtraKerningFactor (0.18f));
        g.drawText (s.title, b.withTrimmedLeft (10.f).withHeight (21.f), Justification::centredLeft, false);
    }

    // --- logo block
    g.setColour (Colours::black.withAlpha (0.55f));
    g.setFont (v::serif (50.f, Font::bold | Font::italic).withExtraKerningFactor (0.06f));
    g.drawText ("Ferrite", Rectangle<float> (50.f, 17.f, 330.f, 58.f), Justification::centredLeft, false);
    g.setColour (v::ivory);
    g.drawText ("Ferrite", Rectangle<float> (48.f, 14.f, 330.f, 58.f), Justification::centredLeft, false);

    Rectangle<float> badge (258.f, 22.f, 42.f, 42.f);
    g.setGradientFill (ColourGradient (v::orange.brighter (0.2f), badge.getX(), badge.getY(), v::red, badge.getRight(), badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 5.f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (badge, 5.f, 1.2f);
    g.setColour (v::ivory);
    g.setFont (v::serif (34.f, Font::bold));
    g.drawText ("8", badge, Justification::centred, false);

    g.setColour (v::cream.withAlpha (0.85f));
    g.setFont (v::serif (11.f, Font::bold).withExtraKerningFactor (0.3f));
    g.drawText ("POLYPHONIC  ANALOG  SYNTHESIZER", Rectangle<float> (50.f, 76.f, 420.f, 16.f), Justification::centredLeft, false);

    // racing stripes
    const Colour stripes[] = { v::gold, v::orange, v::red, Colour (0xff6e2a1a) };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (stripes[i]);
        g.fillRect (48.f, 102.f + (float) i * 5.f, 590.f - (float) i * 0.f, 3.f);
    }

    // preset legend
    g.setColour (v::cream.withAlpha (0.8f));
    g.setFont (v::serif (9.5f, Font::bold).withExtraKerningFactor (0.2f));
    g.drawText ("PRESET", Rectangle<float> (342.f, 6.f, 80.f, 12.f), Justification::centredLeft, false);

    // LED legend + voice count plate
    g.setColour (v::cream.withAlpha (0.8f));
    g.setFont (v::serif (9.5f, Font::bold).withExtraKerningFactor (0.2f));
    g.drawText ("VOICE", Rectangle<float> (562.f, 52.f, 54.f, 12.f), Justification::centred, false);
    g.setColour (v::cream.withAlpha (0.55f));
    g.setFont (v::mono (10.f));
    g.drawText ("8 VOICES  /  LADDER VCF  /  ENSEMBLE", Rectangle<float> (50.f, 122.f, 420.f, 14.f), Justification::centredLeft, false);

    // footer
    g.setColour (v::cream.withAlpha (0.45f));
    g.setFont (v::serif (10.f, Font::bold).withExtraKerningFactor (0.25f));
    g.drawText ("MODEL F-8   *   ANALOG MODELLED   *   SERIAL  No. 0001",
                Rectangle<float> (0.f, H - 26.f, W, 14.f), Justification::centred, false);
    drawScrew (g, { 62.f, H - 19.f }, 0.5f);
    drawScrew (g, { W - 62.f, H - 19.f }, 2.2f);
}
