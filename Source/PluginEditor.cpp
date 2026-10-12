#include "PluginEditor.h"
#include "Eq.h"

using namespace juce;
namespace v = vintage;

static constexpr int kCell = 62, kSecH = 128, kEnvSecH = 202, kKnobH = 98, kRow2 = 276, kKeysY = 484,
                        kEqX = 946, kEqY = 8, kEqW = 280, kEqH = 470, kEqGraphH = 118, kEqColW = 66;

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
void ScopeDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colours::black);
    g.fillRoundedRectangle (b, 8.f);
    auto screen = b.reduced (3.f);

    g.setGradientFill (ColourGradient (Colour (0xff0d2214), screen.getCentreX(), screen.getCentreY(),
                                       Colour (0xff030805), screen.getX(), screen.getY(), true));
    g.fillRoundedRectangle (screen, 6.f);

    Path clip; clip.addRoundedRectangle (screen, 6.f);
    g.saveState();
    g.reduceClipRegion (clip);

    // graticule
    const Colour phosphor (0xff7dffa8);
    g.setColour (phosphor.withAlpha (0.13f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (screen.getX() + screen.getWidth() * (float) i / 8.f), screen.getY(), screen.getBottom());
    for (int j = 1; j < 4; ++j)
        g.drawHorizontalLine ((int) (screen.getY() + screen.getHeight() * (float) j / 4.f), screen.getX(), screen.getRight());
    g.setColour (phosphor.withAlpha (0.28f));
    g.drawHorizontalLine ((int) screen.getCentreY(), screen.getX(), screen.getRight());

    // grab the latest samples
    constexpr int N = 4096;
    snap.resize (N);
    const int head = proc.scopeHead.load (std::memory_order_acquire);
    for (int k = 0; k < N; ++k)
        snap[(size_t) k] = proc.scope[(size_t) ((head - N + k) & (FerriteProcessor::kScopeSize - 1))].load (std::memory_order_relaxed);

    const int window = 2048 >> zoom;                      // 2048 / 1024 / 512 samples
    int start = N - window;
    for (int i = N - window - 1; i >= 1; --i)             // newest rising zero-crossing
        if (snap[(size_t) i - 1] < 0.f && snap[(size_t) i] >= 0.f) { start = i; break; }

    float peak = 0.f;
    for (int i = 0; i < window; ++i) peak = std::max (peak, std::abs (snap[(size_t) (start + i)]));
    if (peak > 0.002f)
        gain += (jlimit (0.5f, 12.f, 0.85f / peak) - gain) * 0.15f;

    const int px = (int) screen.getWidth();
    const float cy = screen.getCentreY(), amp = screen.getHeight() * 0.45f;
    Path wave;
    for (int x = 0; x <= px; ++x)
    {
        const float idx = (float) start + (float) x / (float) px * (float) (window - 1);
        const int i0 = (int) idx;
        const float fr = idx - (float) i0;
        const float smp = snap[(size_t) i0] * (1.f - fr) + snap[(size_t) std::min (i0 + 1, N - 1)] * fr;
        const float y = cy - (peak > 0.002f ? smp * gain : 0.f) * amp;
        if (x == 0) wave.startNewSubPath (screen.getX(), y); else wave.lineTo (screen.getX() + (float) x, y);
    }
    g.setColour (phosphor.withAlpha (0.12f));
    g.strokePath (wave, PathStrokeType (6.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (phosphor.withAlpha (0.30f));
    g.strokePath (wave, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xffc8ffd8));
    g.strokePath (wave, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));

    // CRT scanlines + glass reflection
    g.setColour (Colours::black.withAlpha (0.18f));
    for (float y = screen.getY(); y < screen.getBottom(); y += 3.f)
        g.fillRect (screen.getX(), y, screen.getWidth(), 1.f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), 0.f, screen.getY(),
                                       Colours::transparentWhite, 0.f, screen.getY() + screen.getHeight() * 0.45f, false));
    g.fillRect (screen);
    g.restoreState();

    static const char* labels[] = { "ZOOM x0.5", "ZOOM x1", "ZOOM x2" };
    g.setColour (phosphor.withAlpha (0.55f));
    g.setFont (vintage::mono (9.f));
    g.drawText (labels[zoom], screen.reduced (6.f, 3.f), Justification::bottomLeft, false);

    g.setColour (Colour (0xff6b5a47));
    g.drawRoundedRectangle (b.reduced (0.5f), 8.f, 1.5f);
}

//==============================================================================
//  Parametric EQ display
//==============================================================================
static constexpr float kEqMinHz = 20.f, kEqMaxHz = 20000.f, kEqRangeDb = 18.f, kEqRefSr = 48000.f;

Colour EqDisplay::bandColour (int band)
{
    static const Colour c[] = { Colour (0xffe8803a), Colour (0xffd9a82b), Colour (0xff6fd0c4), Colour (0xffd9534f) };
    return c[jlimit (0, 3, band)];
}

EqDisplay::EqDisplay (AudioProcessorValueTreeState& apvts)
{
    static const char* ids[N][3] = { { "eqLowF", "eqLowG", "eqLowQ" }, { "eqM1F", "eqM1G", "eqM1Q" },
                                     { "eqM2F", "eqM2G", "eqM2Q" },    { "eqHighF", "eqHighG", "eqHighQ" } };
    for (int b = 0; b < N; ++b)
    {
        pF[b] = apvts.getParameter (ids[b][0]);
        pG[b] = apvts.getParameter (ids[b][1]);
        pQ[b] = apvts.getParameter (ids[b][2]);
    }
    pOn = apvts.getParameter ("eqOn");
    startTimerHz (20);
}

Rectangle<float> EqDisplay::area() const { return getLocalBounds().toFloat().reduced (12.f, 12.f); }

float EqDisplay::freqToX (float f) const
{
    const auto r = area();
    return r.getX() + std::log (jlimit (kEqMinHz, kEqMaxHz, f) / kEqMinHz) / std::log (kEqMaxHz / kEqMinHz) * r.getWidth();
}
float EqDisplay::xToFreq (float x) const
{
    const auto r = area();
    return kEqMinHz * std::pow (kEqMaxHz / kEqMinHz, jlimit (0.f, 1.f, (x - r.getX()) / r.getWidth()));
}
float EqDisplay::gainToY (float db) const
{
    const auto r = area();
    return r.getCentreY() - jlimit (-kEqRangeDb, kEqRangeDb, db) / kEqRangeDb * (r.getHeight() * 0.5f);
}
float EqDisplay::yToGain (float y) const
{
    const auto r = area();
    return jlimit (-kEqRangeDb, kEqRangeDb, (r.getCentreY() - y) / (r.getHeight() * 0.5f) * kEqRangeDb);
}

int EqDisplay::hitTest (Point<float> p) const
{
    int best = -1; float bestD = 14.f;
    for (int b = 0; b < N; ++b)
    {
        const float d = p.getDistanceFrom (nodePos (b));
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EqDisplay::timerCallback()
{
    float cur[N * 3 + 1];
    for (int b = 0; b < N; ++b) { cur[b * 3] = pF[b]->getValue(); cur[b * 3 + 1] = pG[b]->getValue(); cur[b * 3 + 2] = pQ[b]->getValue(); }
    cur[N * 3] = pOn->getValue();
    bool changed = false;
    for (int i = 0; i < N * 3 + 1; ++i) if (cur[i] != last[i]) { last[i] = cur[i]; changed = true; }
    if (changed) repaint();
}

void EqDisplay::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto r = area();
    const bool on = pOn->getValue() > 0.5f;
    const Colour amber = vintage::amber;

    g.setColour (Colour (0xff130d08));
    g.fillRoundedRectangle (b, 5.f);

    // grid: frequency decades + dB lines
    const float freqLines[] = { 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 100.f, 200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f,
                                1000.f, 2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f, 10000.f };
    for (float f : freqLines)
    {
        const bool major = (f == 100.f || f == 1000.f || f == 10000.f);
        g.setColour (amber.withAlpha (major ? 0.20f : 0.06f));
        g.drawVerticalLine ((int) freqToX (f), r.getY(), r.getBottom());
    }
    for (float db : { -12.f, -6.f, 6.f, 12.f })
    {
        g.setColour (amber.withAlpha (0.07f));
        g.drawHorizontalLine ((int) gainToY (db), r.getX(), r.getRight());
    }
    g.setColour (amber.withAlpha (0.30f));
    g.drawHorizontalLine ((int) gainToY (0.f), r.getX(), r.getRight());

    g.setFont (vintage::mono (8.f));
    g.setColour (amber.withAlpha (0.45f));
    for (auto lab : { std::pair<float, const char*> { 100.f, "100" }, { 1000.f, "1k" }, { 10000.f, "10k" } })
        g.drawText (lab.second, Rectangle<float> (freqToX (lab.first) + 2.f, b.getBottom() - 11.f, 24.f, 9.f), Justification::left, false);
    g.drawText ("+12", Rectangle<float> (b.getX() + 2.f, gainToY (12.f) - 5.f, 18.f, 9.f), Justification::left, false);
    g.drawText ("-12", Rectangle<float> (b.getX() + 2.f, gainToY (-12.f) - 5.f, 18.f, 9.f), Justification::left, false);

    // combined response of the four bands
    ferrite::BiquadCoeffs co[N];
    for (int i = 0; i < N; ++i)
        co[i] = ferrite::makeEqBand (ferrite::eqBandType (i), kEqRefSr, freqOf (i), gainOf (i), qOf (i));

    Path curve;
    const int px = (int) r.getWidth();
    for (int x = 0; x <= px; x += 2)
    {
        const float f = xToFreq (r.getX() + (float) x);
        float db = 0.f;
        for (int i = 0; i < N; ++i) db += ferrite::eqMagnitudeDb (co[i], kEqRefSr, f);
        const float y = gainToY (db);
        if (x == 0) curve.startNewSubPath (r.getX(), y); else curve.lineTo (r.getX() + (float) x, y);
    }
    const float alpha = on ? 1.f : 0.3f;
    Path fill (curve);
    fill.lineTo (r.getRight(), gainToY (0.f));
    fill.lineTo (r.getX(), gainToY (0.f));
    fill.closeSubPath();
    g.setColour (amber.withAlpha (0.20f * alpha));
    g.fillPath (fill);
    g.setColour (amber.withAlpha (0.25f * alpha));
    g.strokePath (curve, PathStrokeType (4.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (amber.brighter (0.4f).withAlpha (alpha));
    g.strokePath (curve, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

    // band nodes
    for (int i = 0; i < N; ++i)
    {
        const auto p = nodePos (i);
        const bool active = (i == hover || i == dragging);
        const float rad = active ? 7.f : 5.5f;
        const auto col = bandColour (i).withAlpha (alpha);
        if (active) { g.setColour (col.withAlpha (0.3f)); g.fillEllipse (p.x - rad - 3.f, p.y - rad - 3.f, (rad + 3.f) * 2.f, (rad + 3.f) * 2.f); }
        g.setColour (col);
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2.f, rad * 2.f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawEllipse (p.x - rad, p.y - rad, rad * 2.f, rad * 2.f, 1.3f);
        g.setColour (Colours::black.withAlpha (0.85f));
        g.setFont (vintage::mono (8.f));
        g.drawText (String (i + 1), Rectangle<float> (p.x - rad, p.y - rad, rad * 2.f, rad * 2.f), Justification::centred, false);
    }

    // read-out
    const int h = dragging >= 0 ? dragging : hover;
    String text;
    if (h >= 0)
    {
        const float f = freqOf (h), db = gainOf (h);
        static const char* names[] = { "LOW", "MID1", "MID2", "HIGH" };
        text = String (names[h]) + "  " + (f >= 1000.f ? String (f / 1000.f, 2) + "kHz" : String (roundToInt (f)) + "Hz")
             + "  " + String (db > 0.049f ? "+" : "") + String (db, 1) + "dB  Q" + String (qOf (h), 2);
    }
    else if (! on)
        text = "BYPASSED";
    if (text.isNotEmpty())
    {
        g.setFont (vintage::mono (10.f));
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawText (text, b.reduced (6.f, 2.f).translated (1.f, 1.f).withHeight (12.f), Justification::centredRight, false);
        g.setColour (amber);
        g.drawText (text, b.reduced (6.f, 2.f).withHeight (12.f), Justification::centredRight, false);
    }

    g.setColour (Colour (0xff5a4a38));
    g.drawRoundedRectangle (b.reduced (0.5f), 5.f, 1.f);
}

void EqDisplay::mouseDown (const MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging < 0) return;
    pF[dragging]->beginChangeGesture();
    pG[dragging]->beginChangeGesture();
    mouseDrag (e);
}

void EqDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const int b = dragging;
    pF[b]->setValueNotifyingHost (jlimit (0.f, 1.f, pF[b]->convertTo0to1 (xToFreq (e.position.x))));
    pG[b]->setValueNotifyingHost (jlimit (0.f, 1.f, pG[b]->convertTo0to1 (yToGain (e.position.y))));
    repaint();
}

void EqDisplay::mouseUp (const MouseEvent&)
{
    if (dragging >= 0) { pF[dragging]->endChangeGesture(); pG[dragging]->endChangeGesture(); }
    dragging = -1;
    repaint();
}

void EqDisplay::mouseMove (const MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void EqDisplay::mouseExit (const MouseEvent&) { hover = -1; repaint(); }

void EqDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int b = hitTest (e.position);
    if (b < 0) return;
    for (auto* prm : { pF[b], pG[b], pQ[b] })
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getDefaultValue());
        prm->endChangeGesture();
    }
}

void EqDisplay::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int b = hitTest (e.position);
    if (b < 0) return;
    pQ[b]->beginChangeGesture();
    pQ[b]->setValueNotifyingHost (jlimit (0.f, 1.f, pQ[b]->getValue() + w.deltaY * 0.4f));
    pQ[b]->endChangeGesture();
    repaint();
}

//==============================================================================
//  Pitch / mod wheels
//==============================================================================
WheelControl::WheelControl (std::atomic<float>& v, bool bi, const String& l, Colour c)
    : value (v), bipolar (bi), label (l), accent (c)
{
    shown = value.load();
    startTimerHz (60);
}

WheelControl::~WheelControl()
{
    if (bipolar && (dragging || returning)) value.store (0.f);      // never leave the pitch bent behind
}

Rectangle<float> WheelControl::face() const { return Rectangle<float> (2.f, 2.f, (float) getWidth() - 4.f, (float) getHeight() - 14.f); }

void WheelControl::setFromY (float y)
{
    const auto f = face();
    float v;
    if (bipolar)
    {
        v = (f.getCentreY() - y) / (f.getHeight() * 0.42f);
        v = jlimit (-1.f, 1.f, v);
        if (std::abs (v) < 0.05f) v = 0.f;                          // centre detent
    }
    else
    {
        v = jlimit (0.f, 1.f, (f.getBottom() - 4.f - y) / (f.getHeight() - 8.f));
    }
    value.store (v);
    shown = v;
    repaint();
}

void WheelControl::timerCallback()
{
    if (dragging) return;

    if (returning)                                                  // spring back to centre
    {
        float cur = value.load() * 0.72f;
        if (std::abs (cur) < 0.01f) { cur = 0.f; returning = false; }
        value.store (cur);
    }

    const float now = value.load();
    if (now != shown) { shown = now; repaint(); }
}

void WheelControl::mouseDown (const MouseEvent& e)      { dragging = true; returning = false; setFromY (e.position.y); }
void WheelControl::mouseDrag (const MouseEvent& e)      { if (dragging) setFromY (e.position.y); }
void WheelControl::mouseUp (const MouseEvent&)          { dragging = false; if (bipolar) returning = true; }
void WheelControl::mouseDoubleClick (const MouseEvent&) { value.store (0.f); shown = 0.f; repaint(); }

void WheelControl::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (bipolar) return;
    const float v = jlimit (0.f, 1.f, value.load() + w.deltaY * 0.15f);
    value.store (v); shown = v; repaint();
}

void WheelControl::paint (Graphics& g)
{
    const auto f = face();

    g.setColour (Colour (0xff050403));                              // recessed slot
    g.fillRoundedRectangle (f.expanded (2.f), 4.f);

    g.saveState();
    Path clip; clip.addRoundedRectangle (f, 3.f);
    g.reduceClipRegion (clip);

    ColourGradient body (Colour (0xff17120e), 0.f, f.getY(), Colour (0xff17120e), 0.f, f.getBottom(), false);
    body.addColour (0.5, Colour (0xff75664f));                      // lit middle of the cylinder
    g.setGradientFill (body);
    g.fillRect (f);

    const float spacing = 5.f;                                      // ribs scroll with the value
    float offset = std::fmod (shown * f.getHeight() * 0.9f, spacing);
    if (offset < 0.f) offset += spacing;
    for (float y = f.getY() - spacing + offset; y < f.getBottom(); y += spacing)
    {
        const float rel = (y - f.getCentreY()) / (f.getHeight() * 0.5f);
        const float shade = jmax (0.f, 1.f - rel * rel);
        g.setColour (Colours::black.withAlpha (0.55f * (0.4f + 0.6f * shade)));
        g.drawHorizontalLine ((int) y, f.getX(), f.getRight());
        g.setColour (Colours::white.withAlpha (0.11f * shade));
        g.drawHorizontalLine ((int) y + 1, f.getX(), f.getRight());
    }

    ColourGradient sides (Colours::black.withAlpha (0.55f), f.getX(), 0.f, Colours::black.withAlpha (0.55f), f.getRight(), 0.f, false);
    sides.addColour (0.3, Colours::transparentBlack);
    sides.addColour (0.7, Colours::transparentBlack);
    g.setGradientFill (sides);
    g.fillRect (f);

    const float ny = bipolar ? f.getCentreY() - shown * f.getHeight() * 0.42f
                             : f.getBottom() - 4.f - shown * (f.getHeight() - 8.f);
    g.setColour (accent.withAlpha (0.35f));
    g.fillRect (f.getX() + 2.f, ny - 2.f, f.getWidth() - 4.f, 4.f);
    g.setColour (accent.brighter (0.4f));
    g.fillRect (f.getX() + 3.f, ny - 1.f, f.getWidth() - 6.f, 2.f);
    g.restoreState();

    if (bipolar)                                                    // centre marks on the bezel
    {
        g.setColour (vintage::cream.withAlpha (0.8f));
        g.fillRect (0.f, f.getCentreY() - 0.5f, 2.f, 1.f);
        g.fillRect ((float) getWidth() - 2.f, f.getCentreY() - 0.5f, 2.f, 1.f);
    }

    g.setColour (vintage::cream.withAlpha (0.85f));
    g.setFont (vintage::serif (8.5f, Font::bold).withExtraKerningFactor (0.1f));
    g.drawText (label, Rectangle<float> (0.f, (float) getHeight() - 12.f, (float) getWidth(), 11.f), Justification::centred, false);
}

//==============================================================================
//  Keyboard
//==============================================================================
void VintageKeyboard::drawWhiteNote (int note, Graphics& g, Rectangle<float> a, bool down, bool over, Colour, Colour)
{
    const Colour top = down ? Colour (0xffffc86a) : Colour (0xfff7edd3);
    const Colour bot = down ? Colour (0xffe08a2c) : Colour (0xffd2c298);
    g.setGradientFill (ColourGradient (top, a.getX(), a.getY(), bot, a.getX(), a.getBottom(), false));
    g.fillRect (a);
    if (over && ! down) { g.setColour (Colours::white.withAlpha (0.14f)); g.fillRect (a); }

    g.setColour (Colours::black.withAlpha (down ? 0.10f : 0.16f));              // worn front lip
    g.fillRect (a.withTop (a.getBottom() - 5.f));
    g.setColour (Colour (0xff3a2f22));
    g.drawRect (a, 1.f);

    if (note % 12 == 0)
    {
        g.setColour (Colour (0xff2a1a10).withAlpha (0.6f));
        g.setFont (v::mono (9.f));
        g.drawText ("C" + String (note / 12 - 2), a.withTrimmedBottom (7.f), Justification::centredBottom, false);
    }
}

void VintageKeyboard::drawBlackNote (int, Graphics& g, Rectangle<float> a, bool down, bool, Colour)
{
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (a.translated (1.5f, 2.f), 2.f);

    const Colour top = down ? Colour (0xffc9741f) : Colour (0xff3d352d);
    const Colour bot = down ? Colour (0xff7a3f10) : Colour (0xff0b0907);
    g.setGradientFill (ColourGradient (top, a.getX(), a.getY(), bot, a.getX(), a.getBottom(), false));
    g.fillRoundedRectangle (a, 2.f);

    g.setColour (Colours::white.withAlpha (down ? 0.05f : 0.10f));              // glossy top face
    g.fillRect (a.reduced (2.5f, 0.f).withTrimmedBottom (a.getHeight() * 0.25f).withTrimmedTop (1.f));
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (a, 2.f, 1.f);
}

//==============================================================================
//  Envelope graph
//==============================================================================
EnvelopeDisplay::EnvelopeDisplay (AudioProcessorValueTreeState& apvts, const String& a, const String& d,
                                  const String& s, const String& r, Colour c)
    : pA (apvts.getParameter (a)), pD (apvts.getParameter (d)),
      pS (apvts.getParameter (s)), pR (apvts.getParameter (r)), accent (c)
{
    startTimerHz (20);
}

Rectangle<float> EnvelopeDisplay::area() const { return getLocalBounds().toFloat().reduced (11.f, 10.f).withTrimmedBottom (7.f); }

Point<float> EnvelopeDisplay::handlePos (int h) const
{
    const auto r = area();
    const float slot = r.getWidth() * 0.25f;
    switch (h)
    {
        case attack: return { r.getX() + pA->getValue() * slot, r.getY() };
        case decay:  return { r.getX() + slot + pD->getValue() * slot, r.getY() + (1.f - pS->getValue()) * r.getHeight() };
        default:     return { r.getX() + 3.f * slot + pR->getValue() * slot, r.getBottom() };
    }
}

int EnvelopeDisplay::hitTest (Point<float> p) const
{
    int best = none; float bestD = 13.f;
    for (int h = attack; h <= release; ++h)
    {
        const float d = p.getDistanceFrom (handlePos (h));
        if (d < bestD) { bestD = d; best = h; }
    }
    return best;
}

void EnvelopeDisplay::beginGesture (int h)
{
    if (h == attack)  pA->beginChangeGesture();
    if (h == decay)   { pD->beginChangeGesture(); pS->beginChangeGesture(); }
    if (h == release) pR->beginChangeGesture();
}

void EnvelopeDisplay::endGesture (int h)
{
    if (h == attack)  pA->endChangeGesture();
    if (h == decay)   { pD->endChangeGesture(); pS->endChangeGesture(); }
    if (h == release) pR->endChangeGesture();
}

void EnvelopeDisplay::timerCallback()
{
    const float cur[4] = { pA->getValue(), pD->getValue(), pS->getValue(), pR->getValue() };
    bool changed = false;
    for (int i = 0; i < 4; ++i) if (cur[i] != last[i]) { last[i] = cur[i]; changed = true; }
    if (changed) repaint();
}

void EnvelopeDisplay::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff130d08));
    g.fillRoundedRectangle (b, 5.f);

    const auto r = area();
    const float slot = r.getWidth() * 0.25f;

    // grid + stage letters
    g.setColour (accent.withAlpha (0.12f));
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine ((int) (r.getX() + (float) i * slot), r.getY() - 4.f, r.getBottom() + 4.f);
    g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * 0.5f), r.getX(), r.getRight());
    g.setColour (accent.withAlpha (0.4f));
    g.setFont (v::mono (9.f));
    const char* letters[] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
        g.drawText (letters[i], Rectangle<float> (r.getX() + (float) i * slot, b.getBottom() - 11.f, slot, 10.f),
                    Justification::centred, false);

    // curve - same shapes the engine uses (fast-then-flattening attack, exponential decay / release)
    const Point<float> p0 { r.getX(), r.getBottom() };
    const auto p1 = handlePos (attack), p2 = handlePos (decay), p3 = handlePos (release);
    const Point<float> ps { r.getX() + 3.f * slot, p2.y };
    const int N = 24;
    auto expo = [] (float t, float k) { return (std::exp (-k * t) - std::exp (-k)) / (1.f - std::exp (-k)); };

    Path env;
    env.startNewSubPath (p0);
    for (int i = 1; i <= N; ++i)                                    // attack
    {
        const float t = (float) i / (float) N;
        env.lineTo (p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * (1.f - expo (t, 1.5f)));
    }
    for (int i = 1; i <= N; ++i)                                    // decay
    {
        const float t = (float) i / (float) N;
        env.lineTo (p1.x + (p2.x - p1.x) * t, p2.y + (p1.y - p2.y) * expo (t, 3.f));
    }
    env.lineTo (ps);                                                // sustain
    for (int i = 1; i <= N; ++i)                                    // release
    {
        const float t = (float) i / (float) N;
        env.lineTo (ps.x + (p3.x - ps.x) * t, ps.y + (p3.y - ps.y) * (1.f - expo (t, 3.f)));
    }

    Path fill (env);
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (accent.withAlpha (0.35f), 0.f, r.getY(), accent.withAlpha (0.04f), 0.f, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (env, PathStrokeType (4.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (accent.brighter (0.5f));
    g.strokePath (env, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

    // handles
    for (int h = attack; h <= release; ++h)
    {
        const auto p = handlePos (h);
        const bool active = (h == hover || h == dragging);
        const float rad = active ? 6.5f : 4.8f;
        if (active)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillEllipse (p.x - rad - 3.f, p.y - rad - 3.f, (rad + 3.f) * 2.f, (rad + 3.f) * 2.f);
        }
        g.setGradientFill (ColourGradient (v::ivory, p.x - rad * 0.4f, p.y - rad * 0.5f, Colour (0xffb5a47c), p.x + rad, p.y + rad, true));
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2.f, rad * 2.f);
        g.setColour (Colours::black.withAlpha (0.75f));
        g.drawEllipse (p.x - rad, p.y - rad, rad * 2.f, rad * 2.f, 1.2f);
    }

    // value read-out for the handle being touched
    const int h = dragging != none ? dragging : hover;
    if (h != none)
    {
        String text;
        if (h == attack)  text = "ATK " + pA->getText (pA->getValue(), 0);
        if (h == decay)   text = "DEC " + pD->getText (pD->getValue(), 0) + "  SUS " + pS->getText (pS->getValue(), 0);
        if (h == release) text = "REL " + pR->getText (pR->getValue(), 0);
        g.setFont (v::mono (10.f));
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawText (text, b.reduced (6.f, 2.f).translated (1.f, 1.f).withHeight (12.f), Justification::centredRight, false);
        g.setColour (v::amber);
        g.drawText (text, b.reduced (6.f, 2.f).withHeight (12.f), Justification::centredRight, false);
    }

    g.setColour (Colour (0xff5a4a38));
    g.drawRoundedRectangle (b.reduced (0.5f), 5.f, 1.f);
}

void EnvelopeDisplay::mouseDown (const MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging == none) return;
    beginGesture (dragging);
    mouseDrag (e);
}

void EnvelopeDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragging == none) return;
    const auto r = area();
    const float slot = r.getWidth() * 0.25f;
    auto setNorm = [] (RangedAudioParameter* p, float n) { p->setValueNotifyingHost (jlimit (0.f, 1.f, n)); };

    if (dragging == attack)  setNorm (pA, (e.position.x - r.getX()) / slot);
    if (dragging == decay)   { setNorm (pD, (e.position.x - r.getX() - slot) / slot);
                               setNorm (pS, 1.f - (e.position.y - r.getY()) / r.getHeight()); }
    if (dragging == release) setNorm (pR, (e.position.x - r.getX() - 3.f * slot) / slot);
    repaint();
}

void EnvelopeDisplay::mouseUp (const MouseEvent&)
{
    if (dragging != none) endGesture (dragging);
    dragging = none;
    repaint();
}

void EnvelopeDisplay::mouseMove (const MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void EnvelopeDisplay::mouseExit (const MouseEvent&) { hover = none; repaint(); }

void EnvelopeDisplay::mouseDoubleClick (const MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h == none) return;
    auto reset = [] (RangedAudioParameter* p) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); };
    if (h == attack)  reset (pA);
    if (h == decay)   { reset (pD); reset (pS); }
    if (h == release) reset (pR);
}

//==============================================================================
//  LFO shape preview
//==============================================================================
LfoPreview::LfoPreview (AudioProcessorValueTreeState& apvts, Colour c)
    : shape (apvts.getRawParameterValue ("lfoShape")), accent (c)
{
    startTimerHz (10);
}

void LfoPreview::timerCallback()
{
    const int s = (int) shape->load();
    if (s != lastShape) { lastShape = s; repaint(); }
}

void LfoPreview::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff130d08));
    g.fillRoundedRectangle (b, 5.f);
    const auto r = b.reduced (12.f, 12.f);

    g.setColour (accent.withAlpha (0.12f));
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 4.f), r.getY() - 4.f, r.getBottom() + 4.f);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    const int shapeIdx = (int) shape->load();
    Random rng (5);
    float held = rng.nextFloat() * 2.f - 1.f;
    int lastStep = -1;

    Path w;
    const int px = (int) r.getWidth();
    for (int x = 0; x <= px; ++x)
    {
        const float t = (float) x / (float) px * 2.f;               // two cycles
        const float ph = t - std::floor (t);
        float y;
        switch (shapeIdx)
        {
            case 0:  y = std::sin (MathConstants<float>::twoPi * ph); break;
            case 1:  y = 4.f * std::abs (ph - 0.5f) - 1.f; break;
            case 2:  y = 2.f * ph - 1.f; break;
            case 3:  y = ph < 0.5f ? 1.f : -1.f; break;
            default:
            {
                const int step = (int) (t * 4.f);                   // 8 random steps
                if (step != lastStep) { held = rng.nextFloat() * 2.f - 1.f; lastStep = step; }
                y = held; break;
            }
        }
        const float py = r.getCentreY() - y * r.getHeight() * 0.5f;
        if (x == 0) w.startNewSubPath (r.getX(), py); else w.lineTo (r.getX() + (float) x, py);
    }
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (w, PathStrokeType (4.f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (accent.brighter (0.5f));
    g.strokePath (w, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

    static const char* names[] = { "SINE", "TRIANGLE", "SAW", "SQUARE", "SAMPLE & HOLD" };
    g.setColour (v::amber);
    g.setFont (v::mono (10.f));
    g.drawText (names[jlimit (0, 4, shapeIdx)], b.reduced (7.f, 3.f).withHeight (12.f), Justification::centredRight, false);

    g.setColour (Colour (0xff5a4a38));
    g.drawRoundedRectangle (b.reduced (0.5f), 5.f, 1.f);
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
    slider.setTextBoxStyle (Slider::TextBoxBelow, true, kCell - 10, 16);
    // LCD-style readout (set explicitly so it never depends on when the look-and-feel is attached)
    slider.setColour (Slider::textBoxTextColourId,       v::amber);
    slider.setColour (Slider::textBoxBackgroundColourId, Colour (0xff130d08));
    slider.setColour (Slider::textBoxOutlineColourId,    Colour (0xff5a4a38));
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
}

void KnobControl::parentHierarchyChanged()
{
    slider.sendLookAndFeelChange();      // rebuild the readout with the editor's look-and-feel
}

void KnobControl::resized()
{
    name.setBounds (0, 0, getWidth(), 16);
    slider.setBounds (0, 16, getWidth(), getHeight() - 16);
}

//==============================================================================
FerriteEditor::FerriteEditor (FerriteProcessor& p)
    : AudioProcessorEditor (&p), proc (p), led (p.activeVoices), scope (p), keyboard (p.keyboardState),
      pitchWheel (p.pitchBendNorm, true, "PITCH", Colour (0xffe8803a)),
      modWheel (p.modWheelNorm, false, "MOD", Colour (0xff5fd0c4))
{
    setLookAndFeel (&laf);
    setResizable (false, false);

    // ---- layout of sections (x, y fixed; each knob cell is 62 px wide)
    const int r1 = 142, r2 = kRow2;
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

    auto& fenv = addSection ("FILTER ENVELOPE", v::red, { 48, r2, 264, kEnvSecH });
    fenv.knobTop = 100;
    addKnob (fenv, "fA", "ATTACK");  addKnob (fenv, "fD", "DECAY");
    addKnob (fenv, "fS", "SUSTAIN"); addKnob (fenv, "fR", "RELEASE");

    auto& aenv = addSection ("AMP ENVELOPE", v::olive, { 327, r2, 264, kEnvSecH });
    aenv.knobTop = 100;
    addKnob (aenv, "aA", "ATTACK");  addKnob (aenv, "aD", "DECAY");
    addKnob (aenv, "aS", "SUSTAIN"); addKnob (aenv, "aR", "RELEASE");

    auto& lfo = addSection ("MODULATION LFO", v::teal, { 606, r2, 326, kEnvSecH });
    lfo.knobTop = 100;
    addKnob (lfo, "lfoRate", "RATE");   addKnob (lfo, "lfoShape", "SHAPE");
    addKnob (lfo, "lfoPitch", "PITCH"); addKnob (lfo, "lfoFilter", "FILTER");
    addKnob (lfo, "lfoPwm", "PWM");

    auto& out = addSection ("OUTPUT", v::gold, { 668, 8, 264, kSecH });
    addKnob (out, "glide", "GLIDE");     addKnob (out, "drift", "DRIFT");
    addKnob (out, "chorus", "ENSEMBLE"); addKnob (out, "master", "VOLUME");

    addSection ("OSCILLOSCOPE", v::teal, { 322, 8, 332, kSecH });

    auto& eqs = addSection ("PARAMETRIC EQ", v::gold, { kEqX, kEqY, kEqW, kEqH });
    eqs.cols = 4;  eqs.cellW = kEqColW;
    eqs.knobTop = kEqGraphH + 46;                       // below the graph and the band headers
    for (auto b : { "eqLowF", "eqM1F", "eqM2F", "eqHighF" }) addKnob (eqs, b, "FREQ");
    for (auto b : { "eqLowG", "eqM1G", "eqM2G", "eqHighG" }) addKnob (eqs, b, "GAIN");
    for (auto b : { "eqLowQ", "eqM1Q", "eqM2Q", "eqHighQ" }) addKnob (eqs, b, "Q");
    addAndMakeVisible (led);
    addAndMakeVisible (scope);

    fenvGraph = std::make_unique<EnvelopeDisplay> (proc.apvts, "fA", "fD", "fS", "fR", Colour (0xffe8803a));
    aenvGraph = std::make_unique<EnvelopeDisplay> (proc.apvts, "aA", "aD", "aS", "aR", Colour (0xffb5c94a));
    lfoGraph  = std::make_unique<LfoPreview> (proc.apvts, Colour (0xff5fd0c4));
    addAndMakeVisible (*fenvGraph);
    addAndMakeVisible (*aenvGraph);
    addAndMakeVisible (*lfoGraph);

    eqGraph = std::make_unique<EqDisplay> (proc.apvts);
    addAndMakeVisible (*eqGraph);
    eqOnBtn.setClickingTogglesState (true);
    eqOnBtn.setColour (TextButton::buttonColourId,   Colour (0xff4a3f33));
    eqOnBtn.setColour (TextButton::buttonOnColourId, v::amber);
    eqOnBtn.setColour (TextButton::textColourOffId,  v::cream);
    eqOnBtn.setColour (TextButton::textColourOnId,   Colour (0xff2a1a10));
    addAndMakeVisible (eqOnBtn);
    eqOnAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "eqOn", eqOnBtn);

    keyboard.setAvailableRange (36, 108);                // C1 .. C7 in Ableton naming (middle C = C3), 6 octaves
    keyboard.setKeyWidth (25.f);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setVelocity (0.85f, true);                  // harder click lower on the key = louder
    keyboard.setWantsKeyboardFocus (false);              // don't fight the host over computer-keyboard shortcuts
    addAndMakeVisible (keyboard);
    addAndMakeVisible (pitchWheel);
    addAndMakeVisible (modWheel);

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
    setSize (kWidth, kHeight);          // last: triggers resized() once every child exists
}

FerriteEditor::~FerriteEditor()
{
    stopTimer();
    proc.keyboardState.allNotesOff (0);               // never leave a note hanging when the window closes
    setLookAndFeel (nullptr);
}

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
        {
            const int col = s.cols > 0 ? i % s.cols : i, row = s.cols > 0 ? i / s.cols : 0;
            k->setBounds (s.bounds.getX() + 8 + col * s.cellW, s.bounds.getY() + s.knobTop + row * s.rowH, s.cellW, kKnobH);
            ++i;
        }
    }
    eqGraph->setBounds (kEqX + 8, kEqY + 28, kEqW - 16, kEqGraphH);
    eqOnBtn.setBounds (kEqX + kEqW - 8 - 72, kEqY + 5, 72, 16);
    scope.setBounds (330, 36, 316, 92);
    pitchWheel.setBounds (58, kKeysY + 4, 34, 62);
    modWheel.setBounds   (96, kKeysY + 4, 34, 62);
    keyboard.setBounds   (138, kKeysY + 6, 1078, 58);
    fenvGraph->setBounds (56,  kRow2 + 28, 248, 68);
    aenvGraph->setBounds (335, kRow2 + 28, 248, 68);
    lfoGraph->setBounds  (614, kRow2 + 28, 310, 68);
    led.setBounds (628, 6, 24, 24);
    prevBtn.setBounds (48, 102, 22, 24);
    lcdBtn.setBounds  (72, 102, 112, 24);
    nextBtn.setBounds (186, 102, 22, 24);
    saveBtn.setBounds (214, 102, 46, 24);
    initBtn.setBounds (262, 102, 46, 24);
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
    g.setFont (v::serif (44.f, Font::bold | Font::italic).withExtraKerningFactor (0.04f));
    g.drawText ("Ferrite", Rectangle<float> (50.f, 11.f, 190.f, 50.f), Justification::centredLeft, false);
    g.setColour (v::ivory);
    g.drawText ("Ferrite", Rectangle<float> (48.f, 9.f, 190.f, 50.f), Justification::centredLeft, false);

    Rectangle<float> badge (240.f, 16.f, 38.f, 38.f);
    g.setGradientFill (ColourGradient (v::orange.brighter (0.2f), badge.getX(), badge.getY(), v::red, badge.getRight(), badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 5.f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (badge, 5.f, 1.2f);
    g.setColour (v::ivory);
    g.setFont (v::serif (30.f, Font::bold));
    g.drawText ("8", badge, Justification::centred, false);

    g.setColour (v::cream.withAlpha (0.85f));
    g.setFont (v::serif (9.f, Font::bold).withExtraKerningFactor (0.2f));
    g.drawText ("POLYPHONIC  ANALOG  SYNTHESIZER", Rectangle<float> (50.f, 62.f, 262.f, 14.f), Justification::centredLeft, false);

    // racing stripes
    const Colour stripes[] = { v::gold, v::orange, v::red, Colour (0xff6e2a1a) };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (stripes[i]);
        g.fillRect (48.f, 80.f + (float) i * 4.f, 260.f, 3.f);
    }

    // scope LED legend
    g.setColour (v::cream.withAlpha (0.8f));
    g.setFont (v::serif (9.f, Font::bold).withExtraKerningFactor (0.2f));
    g.drawText ("VOICE", Rectangle<float> (560.f, 8.f, 62.f, 22.f), Justification::centredRight, false);

    // EQ band headers
    {
        static const char* names[] = { "LOW SHELF", "MID 1", "MID 2", "HIGH SHELF" };
        g.setFont (v::serif (8.5f, Font::bold).withExtraKerningFactor (0.08f));
        for (int i = 0; i < 4; ++i)
        {
            const float x = (float) (kEqX + 8 + i * kEqColW), y = (float) (kEqY + 28 + kEqGraphH + 4);
            g.setColour (EqDisplay::bandColour (i));
            g.fillRect (x + 6.f, y + 12.f, (float) kEqColW - 12.f, 2.f);
            g.drawText (names[i], Rectangle<float> (x, y, (float) kEqColW, 12.f), Justification::centred, false);
        }
    }

    // keyboard recess
    {
        Rectangle<float> slot (48.f, (float) kKeysY, (float) (kEqX + kEqW - 48), 70.f);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (slot.translated (0.f, 2.f), 6.f);
        g.setGradientFill (ColourGradient (Colour (0xff0a0705), 0.f, slot.getY(), Colour (0xff1c1612), 0.f, slot.getBottom(), false));
        g.fillRoundedRectangle (slot, 6.f);
        g.setColour (Colour (0xff6b5a47).withAlpha (0.8f));
        g.drawRoundedRectangle (slot.reduced (0.5f), 6.f, 1.f);
    }

    // footer
    g.setColour (v::cream.withAlpha (0.45f));
    g.setFont (v::serif (10.f, Font::bold).withExtraKerningFactor (0.25f));
    g.drawText ("MODEL F-8   *   8 VOICE ANALOG MODELLED   *   SERIAL  No. 0001",
                Rectangle<float> (0.f, H - 26.f, W, 14.f), Justification::centred, false);
    drawScrew (g, { 62.f, H - 19.f }, 0.5f);
    drawScrew (g, { W - 62.f, H - 19.f }, 2.2f);
}
