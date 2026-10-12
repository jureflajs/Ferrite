#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace juce;

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout FerriteProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto fmtPct    = [] (float v, int) { return String (roundToInt (v * 100.f)) + "%"; };
    auto fmtSigned = [] (float v, int) { int i = roundToInt (v * 100.f); return String (i > 0 ? "+" : "") + String (i) + "%"; };
    auto fmtTime   = [] (float v, int) { return v < 1.f ? String (roundToInt (v * 1000.f)) + " ms" : String (v, 2) + " s"; };
    auto fmtHz     = [] (float v, int) { return v >= 1000.f ? String (v / 1000.f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto fmtLfo    = [] (float v, int) { return String (v, 1) + " Hz"; };
    auto fmtSemi   = [] (float v, int) { int i = roundToInt (v); return String (i > 0 ? "+" : "") + String (i) + " st"; };
    auto fmtFreq   = [] (float v, int) { return v >= 1000.f ? String (v / 1000.f, 1) + "kHz" : String (roundToInt (v)) + "Hz"; };
    auto fmtDb     = [] (float v, int) { return String (v > 0.049f ? "+" : "") + String (v, 1) + "dB"; };
    auto fmtQ      = [] (float v, int) { return String (v, 2); };
    auto fmtCents  = [] (float v, int) { int i = roundToInt (v); return String (i > 0 ? "+" : "") + String (i) + " ct"; };

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     std::function<String (float, int)> fmt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                     AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto addC = [&] (const char* id, const char* name, StringArray choices, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };
    const NormalisableRange<float> unit (0.f, 1.f);

    // Oscillators
    addC ("osc1Wave", "Osc 1 Wave", { "Saw", "Pulse", "Tri" }, 0);
    addC ("osc2Wave", "Osc 2 Wave", { "Saw", "Pulse", "Tri" }, 0);
    addF ("pw",       "Pulse Width", NormalisableRange<float> (0.05f, 0.95f), 0.5f, fmtPct);
    addF ("osc2Semi", "Osc 2 Semitone", NormalisableRange<float> (-24.f, 24.f, 1.f), 0.f, fmtSemi);
    addF ("osc2Fine", "Osc 2 Fine", NormalisableRange<float> (-50.f, 50.f, 0.1f), 7.f, fmtCents);

    // Mixer
    addF ("mix1",     "Mix Osc 1", unit, 0.8f, fmtPct);
    addF ("mix2",     "Mix Osc 2", unit, 0.8f, fmtPct);
    addF ("mixSub",   "Mix Sub",   unit, 0.0f, fmtPct);
    addF ("mixNoise", "Mix Noise", unit, 0.0f, fmtPct);

    // Filter
    addF ("cutoff",   "Cutoff",    skewed (20.f, 20000.f, 1000.f), 1200.f, fmtHz);
    addF ("reso",     "Resonance", unit, 0.25f, fmtPct);
    addF ("envAmt",   "Filter Env Amount", NormalisableRange<float> (-1.f, 1.f), 0.4f, fmtSigned);
    addF ("keyTrack", "Key Track", unit, 0.5f, fmtPct);

    // Envelopes
    addF ("fA", "Filter Attack",  skewed (0.001f, 10.f, 0.4f), 0.005f, fmtTime);
    addF ("fD", "Filter Decay",   skewed (0.005f, 10.f, 0.6f), 0.4f,   fmtTime);
    addF ("fS", "Filter Sustain", unit, 0.3f, fmtPct);
    addF ("fR", "Filter Release", skewed (0.005f, 10.f, 0.6f), 0.3f,   fmtTime);
    addF ("aA", "Amp Attack",     skewed (0.001f, 10.f, 0.4f), 0.01f,  fmtTime);
    addF ("aD", "Amp Decay",      skewed (0.005f, 10.f, 0.6f), 0.3f,   fmtTime);
    addF ("aS", "Amp Sustain",    unit, 0.8f, fmtPct);
    addF ("aR", "Amp Release",    skewed (0.005f, 10.f, 0.6f), 0.4f,   fmtTime);

    // LFO
    addF ("lfoRate",   "LFO Rate",   skewed (0.05f, 30.f, 4.f), 5.f, fmtLfo);
    addC ("lfoShape",  "LFO Shape",  { "Sine", "Tri", "Saw", "Square", "S&H" }, 0);
    addF ("lfoPitch",  "LFO > Pitch",  unit, 0.f, fmtPct);
    addF ("lfoFilter", "LFO > Filter", unit, 0.f, fmtPct);
    addF ("lfoPwm",    "LFO > PWM",    unit, 0.f, fmtPct);

    // Output
    addF ("glide",  "Glide",    skewed (0.f, 3.f, 0.4f), 0.f, fmtTime);
    addF ("drift",  "Drift",    unit, 0.25f, fmtPct);
    addF ("chorus", "Ensemble", unit, 0.4f,  fmtPct);
    addF ("master", "Volume",   unit, 0.7f,  fmtPct);

    // Parametric EQ: low shelf, two bells, high shelf
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "eqOn", 1 }, "EQ On", true));
    const NormalisableRange<float> eqGain (-18.f, 18.f, 0.1f);
    const NormalisableRange<float> shelfQ (0.3f, 1.5f);
    addF ("eqLowF",  "EQ Low Freq",   skewed (30.f, 600.f, 130.f),     120.f,  fmtFreq);
    addF ("eqLowG",  "EQ Low Gain",   eqGain, 0.f, fmtDb);
    addF ("eqLowQ",  "EQ Low Q",      shelfQ, 0.71f, fmtQ);
    addF ("eqM1F",   "EQ Mid 1 Freq", skewed (100.f, 3000.f, 520.f),   500.f,  fmtFreq);
    addF ("eqM1G",   "EQ Mid 1 Gain", eqGain, 0.f, fmtDb);
    addF ("eqM1Q",   "EQ Mid 1 Q",    skewed (0.3f, 10.f, 1.5f), 1.f, fmtQ);
    addF ("eqM2F",   "EQ Mid 2 Freq", skewed (800.f, 12000.f, 3200.f), 3000.f, fmtFreq);
    addF ("eqM2G",   "EQ Mid 2 Gain", eqGain, 0.f, fmtDb);
    addF ("eqM2Q",   "EQ Mid 2 Q",    skewed (0.3f, 10.f, 1.5f), 1.f, fmtQ);
    addF ("eqHighF", "EQ High Freq",  skewed (2000.f, 18000.f, 7000.f), 8000.f, fmtFreq);
    addF ("eqHighG", "EQ High Gain",  eqGain, 0.f, fmtDb);
    addF ("eqHighQ", "EQ High Q",     shelfQ, 0.71f, fmtQ);

    return { p.begin(), p.end() };
}

//==============================================================================
FerriteProcessor::FerriteProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FERRITE8", createLayout())
{
    for (auto& x : scope) x.store (0.f, std::memory_order_relaxed);
}

void FerriteProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
}

bool FerriteProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

ferrite::Params FerriteProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    ferrite::Params p;
    p.osc1Wave = (int) v ("osc1Wave");  p.osc2Wave = (int) v ("osc2Wave");  p.lfoShape = (int) v ("lfoShape");
    p.pw = v ("pw");  p.osc2Semi = v ("osc2Semi");  p.osc2Fine = v ("osc2Fine");
    p.mix1 = v ("mix1");  p.mix2 = v ("mix2");  p.mixSub = v ("mixSub");  p.mixNoise = v ("mixNoise");
    p.cutoff = v ("cutoff");  p.reso = v ("reso");  p.envAmt = v ("envAmt");  p.keyTrack = v ("keyTrack");
    p.fA = v ("fA");  p.fD = v ("fD");  p.fS = v ("fS");  p.fR = v ("fR");
    p.aA = v ("aA");  p.aD = v ("aD");  p.aS = v ("aS");  p.aR = v ("aR");
    p.lfoRate = v ("lfoRate");  p.lfoPitch = v ("lfoPitch");  p.lfoFilter = v ("lfoFilter");  p.lfoPwm = v ("lfoPwm");
    p.glide = v ("glide");  p.drift = v ("drift");  p.chorus = v ("chorus");  p.master = v ("master");

    static const char* eqIds[ferrite::kEqBands][3] = { { "eqLowF", "eqLowG", "eqLowQ" }, { "eqM1F", "eqM1G", "eqM1Q" },
                                                      { "eqM2F", "eqM2G", "eqM2Q" },   { "eqHighF", "eqHighG", "eqHighQ" } };
    p.eq.on = v ("eqOn") > 0.5f;
    for (int b = 0; b < ferrite::kEqBands; ++b)
    {
        p.eq.freq[b] = v (eqIds[b][0]);  p.eq.gain[b] = v (eqIds[b][1]);  p.eq.q[b] = v (eqIds[b][2]);
    }
    return p;
}

void FerriteProcessor::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())                synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())          synth.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff (m.isAllSoundOff());
    else if (m.isPitchWheel())
    {
        appliedBend = jlimit (-1.f, 1.f, ((float) m.getPitchWheelValue() - 8192.f) / 8192.f);
        pitchBendNorm.store (appliedBend);                      // so the on-screen wheel follows the host
        synth.setPitchBend (appliedBend * 2.f);
    }
    else if (m.isController())
    {
        if (m.getControllerNumber() == 1)
        {
            appliedMod = (float) m.getControllerValue() / 127.f;
            modWheelNorm.store (appliedMod);
            synth.setModWheel (appliedMod);
        }
        if (m.getControllerNumber() == 64) synth.setSustain (m.getControllerValue() >= 64);
    }
}

void FerriteProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    synth.setParams (readParams());

    // wheels moved on screen since the last block
    {
        const float pb = pitchBendNorm.load(), mw = modWheelNorm.load();
        if (pb != appliedBend) { appliedBend = pb; synth.setPitchBend (pb * 2.f); }
        if (mw != appliedMod)  { appliedMod  = mw; synth.setModWheel (mw); }
    }

    // merge notes played on the on-screen keyboard into the MIDI stream
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0;

    for (const auto meta : midi)
    {
        const int evPos = jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        if (evPos > pos)
        {
            synth.render (L + pos, R + pos, evPos - pos);
            pos = evPos;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < buffer.getNumSamples())
        synth.render (L + pos, R + pos, buffer.getNumSamples() - pos);

    activeVoices.store (synth.getActiveVoiceCount());

    // feed the oscilloscope (mono mix of the output)
    {
        int h = scopeHead.load (std::memory_order_relaxed);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            scope[(size_t) h].store (0.5f * (L[i] + R[i]), std::memory_order_relaxed);
            h = (h + 1) & (kScopeSize - 1);
        }
        scopeHead.store (h, std::memory_order_release);
    }
}

AudioProcessorEditor* FerriteProcessor::createEditor() { return new FerriteEditor (*this); }

void FerriteProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void FerriteProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

//==============================================================================
//  Presets
//==============================================================================
int FerriteProcessor::getNumFactoryPresets() { return (int) getFactoryPresets().size(); }

String FerriteProcessor::getFactoryPresetName (int i)
{
    const auto& all = getFactoryPresets();
    return isPositiveAndBelow (i, (int) all.size()) ? String (all[(size_t) i].name) : String();
}

void FerriteProcessor::setCurrentPresetName (const String& n)
{
    apvts.state.setProperty ("presetName", n, nullptr);
}

String FerriteProcessor::getCurrentPresetName() const
{
    return apvts.state.getProperty ("presetName", "Init").toString();
}

void FerriteProcessor::resetToInit()
{
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (p))
            r->setValueNotifyingHost (r->getDefaultValue());
    setCurrentPresetName ("Init");
}

void FerriteProcessor::loadFactoryPreset (int index)
{
    const auto& all = getFactoryPresets();
    if (! isPositiveAndBelow (index, (int) all.size())) return;

    resetToInit();
    for (const auto& kv : all[(size_t) index].values)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));

    setCurrentPresetName (all[(size_t) index].name);
}

File FerriteProcessor::getPresetDirectory() const
{
    return File::getSpecialLocation (File::userDocumentsDirectory)
              .getChildFile ("Ferrite Audio").getChildFile ("Ferrite 8").getChildFile ("Presets");
}

File FerriteProcessor::presetFile (const String& name) const
{
    return getPresetDirectory().getChildFile (File::createLegalFileName (name) + ".f8preset");
}

StringArray FerriteProcessor::getUserPresetNames() const
{
    StringArray names;
    for (const auto& f : getPresetDirectory().findChildFiles (File::findFiles, false, "*.f8preset"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool FerriteProcessor::isUserPreset (const String& name) const
{
    return presetFile (name).existsAsFile();
}

bool FerriteProcessor::saveUserPreset (String name)
{
    name = name.trim();
    if (name.isEmpty()) return false;
    for (int i = 0; i < getNumFactoryPresets(); ++i)         // never shadow a factory name
        if (name.equalsIgnoreCase (getFactoryPresetName (i))) { name << " (User)"; break; }

    if (! getPresetDirectory().createDirectory().wasOk()) return false;

    setCurrentPresetName (name);
    if (auto xml = apvts.copyState().createXml())
        return presetFile (name).replaceWithText (xml->toString());
    return false;
}

bool FerriteProcessor::loadUserPreset (const String& name)
{
    auto f = presetFile (name);
    if (! f.existsAsFile()) return false;
    if (auto xml = XmlDocument::parse (f))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            setCurrentPresetName (name);
            return true;
        }
    return false;
}

bool FerriteProcessor::deleteUserPreset (const String& name)
{
    return presetFile (name).deleteFile();
}

void FerriteProcessor::stepPreset (int delta)
{
    const int nf = getNumFactoryPresets();
    auto users = getUserPresetNames();
    const int total = nf + users.size();
    if (total <= 0) return;

    const auto cur = getCurrentPresetName();
    int idx = -1;
    for (int i = 0; i < nf; ++i)  if (getFactoryPresetName (i) == cur) idx = i;
    if (idx < 0) { const int u = users.indexOf (cur); if (u >= 0) idx = nf + u; }

    idx = (idx < 0) ? (delta > 0 ? 0 : total - 1) : (idx + delta + total) % total;
    if (idx < nf) loadFactoryPreset (idx); else loadUserPreset (users[idx - nf]);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FerriteProcessor(); }
