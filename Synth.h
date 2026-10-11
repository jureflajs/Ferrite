#pragma once
// ---------------------------------------------------------------------------
//  Ferrite 8 - DSP engine
//  8-voice analog-style polysynth: 2 osc + sub + noise -> 24 dB ladder LPF
//  -> VCA, two ADSRs, LFO, glide, analog drift, ensemble chorus.
// ---------------------------------------------------------------------------
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ferrite
{
constexpr int   kNumVoices = 8;
constexpr float kPi        = 3.14159265358979323846f;
constexpr float kTwoPi     = 6.28318530717958647692f;

inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

//==============================================================================
struct Params
{
    int   osc1Wave = 0, osc2Wave = 0, lfoShape = 0;      // wave: 0 saw, 1 pulse, 2 tri
    float pw = 0.5f, osc2Semi = 0.f, osc2Fine = 0.f;
    float mix1 = 0.8f, mix2 = 0.8f, mixSub = 0.f, mixNoise = 0.f;
    float cutoff = 1200.f, reso = 0.2f, envAmt = 0.4f, keyTrack = 0.5f;
    float fA = 0.005f, fD = 0.4f, fS = 0.3f, fR = 0.3f;
    float aA = 0.01f,  aD = 0.3f, aS = 0.8f, aR = 0.4f;
    float lfoRate = 5.f, lfoPitch = 0.f, lfoFilter = 0.f, lfoPwm = 0.f;
    float glide = 0.f, drift = 0.2f, chorus = 0.4f, master = 0.7f;
};

struct Frame            // per-sample modulation shared by all voices
{
    float lfo = 0.f, cutoff = 1000.f, reso = 0.f, bend = 0.f, mod = 0.f;
};

struct Rng
{
    uint32_t s = 1u;
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float bip() noexcept { return (float) (int32_t) next() * (1.0f / 2147483648.0f); }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
};

//==============================================================================
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (float sr) noexcept { sampleRate = sr; }
    void gateOn() noexcept   { stage = Stage::Attack; }
    void gateOff() noexcept  { if (stage != Stage::Idle) stage = Stage::Release; }
    void kill() noexcept     { stage = Stage::Idle; level = 0.f; }
    bool isIdle() const noexcept      { return stage == Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    float getLevel() const noexcept   { return level; }

    float next (float a, float d, float s, float r) noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                level = 0.f;
                break;
            case Stage::Attack:
                level += (1.3f - level) * coef (a, 1.5f);
                if (level >= 1.f) { level = 1.f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level += (s - level) * coef (d, 3.f);
                if (level - s < 0.0005f)
                {
                    level = s;
                    stage = (s <= 0.0005f) ? Stage::Idle : Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                level = s;
                if (s <= 0.0005f) stage = Stage::Idle;
                break;
            case Stage::Release:
                level += (0.f - level) * coef (r, 3.f);
                if (level < 0.0001f) { level = 0.f; stage = Stage::Idle; }
                break;
        }
        return level;
    }

private:
    float coef (float t, float k) const noexcept
    {
        return 1.f - std::exp (-k / (std::max (t, 0.0005f) * sampleRate));
    }

    Stage stage = Stage::Idle;
    float level = 0.f, sampleRate = 44100.f;
};

//==============================================================================
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)         { t /= dt;           return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

inline float oscillator (int wave, float& phase, float dt, float pw) noexcept
{
    float v;
    switch (wave)
    {
        case 0:  v = 2.f * phase - 1.f;  v -= polyBlep (phase, dt); break;       // saw
        case 1:                                                                    // pulse
        {
            v = phase < pw ? 1.f : -1.f;
            v += polyBlep (phase, dt);
            float t2 = phase - pw; if (t2 < 0.f) t2 += 1.f;
            v -= polyBlep (t2, dt);
            break;
        }
        default: v = 2.f * std::abs (2.f * phase - 1.f) - 1.f; break;           // triangle
    }
    phase += dt;
    if (phase >= 1.f) phase -= 1.f;
    return v;
}

//==============================================================================
class Voice
{
public:
    void init (float sampleRate, uint32_t seed)
    {
        rng.s = seed != 0 ? seed : 1u;
        ampEnv.setSampleRate (sampleRate);
        filtEnv.setSampleRate (sampleRate);
        kill();
    }

    void kill() noexcept { active = false; gate = false; held = false; ampEnv.kill(); filtEnv.kill(); s.fill (0.f); }

    void start (int n, float vel, float fromNote, uint64_t a) noexcept
    {
        const bool wasActive = active;
        note = n; velocity = vel; targetNote = (float) n; currentNote = fromNote;
        age = a; active = true; gate = true; held = false;
        if (! wasActive)
        {
            s.fill (0.f);
            ph1 = rng.uni(); ph2 = rng.uni(); phSub = rng.uni();
            ampEnv.kill(); filtEnv.kill();
        }
        ampEnv.gateOn();
        filtEnv.gateOn();
    }

    void release() noexcept { gate = false; held = false; ampEnv.gateOff(); filtEnv.gateOff(); }

    float tick (const Params& p, const Frame& f, float sr, float glideCoef) noexcept
    {
        if (! active) return 0.f;

        currentNote += (targetNote - currentNote) * glideCoef;

        if (--driftCounter <= 0)
        {
            driftCounter = 20000 + (int) (rng.next() % 40000u);
            driftTarget  = rng.bip();
        }
        driftValue += (driftTarget - driftValue) * 0.00005f;

        const float semis = currentNote + f.bend + driftValue * p.drift * 0.25f
                          + f.lfo * (p.lfoPitch * 2.f + f.mod * 2.f);

        const float hz1 = midiToHz (semis);
        const float hz2 = midiToHz (semis + p.osc2Semi + p.osc2Fine * 0.01f);
        const float dt1 = std::min (hz1 / sr, 0.45f);
        const float dt2 = std::min (hz2 / sr, 0.45f);
        const float pw  = std::clamp (p.pw + f.lfo * p.lfoPwm * 0.4f, 0.05f, 0.95f);

        const float o1  = oscillator (p.osc1Wave, ph1, dt1, pw);
        const float o2  = oscillator (p.osc2Wave, ph2, dt2, pw);
        const float sub = oscillator (1, phSub, dt1 * 0.5f, 0.5f);
        const float nz  = rng.bip();

        const float mix = (o1 * p.mix1 + o2 * p.mix2 + sub * p.mixSub + nz * p.mixNoise) * 0.4f;

        const float fe = filtEnv.next (p.fA, p.fD, p.fS, p.fR);
        const float ae = ampEnv.next (p.aA, p.aD, p.aS, p.aR);
        if (ampEnv.isIdle()) { active = false; return 0.f; }

        const float octaves = p.envAmt * 5.f * fe
                            + f.lfo * p.lfoFilter * 3.f
                            + p.keyTrack * (currentNote - 60.f) / 12.f;
        const float fc = std::clamp (f.cutoff * std::exp2 (octaves), 20.f, std::min (20000.f, sr * 0.45f));

        const float y = ladder (mix, fc, f.reso, sr);
        if (! std::isfinite (y)) { s.fill (0.f); return 0.f; }

        return y * ae * (0.3f + 0.7f * velocity);
    }

    bool  active = false, gate = false, held = false;
    int   note = -1;
    uint64_t age = 0;
    Envelope ampEnv, filtEnv;

private:
    // Huovilainen-style non-linear 4-pole ladder, run 2x oversampled.
    float ladder (float in, float fc, float res, float sr) noexcept
    {
        const float g = 1.f - std::exp (-kPi * fc / sr);       // = 2*pi*fc/(2*sr)
        const float k = res * 4.f;
        for (int i = 0; i < 2; ++i)
        {
            const float x = std::tanh (in * 1.2f - k * s[3]);
            const float t0 = std::tanh (s[0]), t1 = std::tanh (s[1]), t2 = std::tanh (s[2]);
            s[0] += g * (x - t0);
            s[1] += g * (t0 - t1);
            s[2] += g * (t1 - t2);
            s[3] += g * (t2 - std::tanh (s[3]));
        }
        return s[3] * (1.f + res * 0.6f);
    }

    float velocity = 1.f, currentNote = 60.f, targetNote = 60.f;
    float ph1 = 0.f, ph2 = 0.f, phSub = 0.f;
    float driftTarget = 0.f, driftValue = 0.f;
    int   driftCounter = 0;
    std::array<float, 4> s {};
    Rng rng;
};

//==============================================================================
class Chorus
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        int size = 1; while (size < (int) (sr * 0.04f)) size <<= 1;
        buf.assign ((size_t) size, 0.f);
        mask = size - 1; pos = 0; phase = 0.f; phase2 = 0.f;
    }

    void process (float in, float mix, float& outL, float& outR) noexcept
    {
        buf[(size_t) pos] = in;
        phase  += 0.7f / sr;  if (phase  >= 1.f) phase  -= 1.f;
        phase2 += 6.1f / sr;  if (phase2 >= 1.f) phase2 -= 1.f;

        const float m1 = std::sin (kTwoPi * phase);
        const float m2 = std::sin (kTwoPi * phase2);
        const float dL = (0.0070f + 0.0025f * m1 + 0.0003f * m2) * sr;
        const float dR = (0.0070f - 0.0025f * m1 - 0.0003f * m2) * sr;

        const float wl = read (dL), wr = read (dR);
        pos = (pos + 1) & mask;

        const float dry = 1.f - 0.35f * mix;
        outL = in * dry + wl * mix * 0.7f;
        outR = in * dry + wr * mix * 0.7f;
    }

private:
    float read (float delay) const noexcept
    {
        float rp = (float) pos - delay;
        while (rp < 0.f) rp += (float) (mask + 1);
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        return buf[(size_t) (i0 & mask)] * (1.f - fr) + buf[(size_t) ((i0 + 1) & mask)] * fr;
    }

    std::vector<float> buf;
    int pos = 0, mask = 0;
    float phase = 0.f, phase2 = 0.f, sr = 44100.f;
};

//==============================================================================
class Synth
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (size_t i = 0; i < voices.size(); ++i)
            voices[i].init (sr, 0x1234567u + (uint32_t) i * 7919u);

        cutoffSm.reset (sampleRate, 0.015);  cutoffSm.setCurrentAndTargetValue (1000.f);
        resoSm.reset (sampleRate, 0.015);    resoSm.setCurrentAndTargetValue (0.f);
        masterSm.reset (sampleRate, 0.02);   masterSm.setCurrentAndTargetValue (0.7f);
        chorus.prepare (sr);
        lfoRng.s = 0xBADC0FFEu;
        lfoPhase = 0.f; lfoSH = 0.f; bend = bendSm = mod = 0.f;
        sustain = false; lastNote = -1;
    }

    void setParams (const Params& p)
    {
        params = p;
        cutoffSm.setTargetValue (std::max (p.cutoff, 20.f));
        resoSm.setTargetValue (p.reso);
        masterSm.setTargetValue (p.master);
        glideCoef = p.glide < 0.002f ? 1.f : 1.f - std::exp (-1.f / (p.glide * sr));
    }

    void setPitchBend (float semitones) noexcept { bend = semitones; }
    void setModWheel (float v) noexcept          { mod = v; }

    void noteOn (int note, float vel) noexcept
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.note == note) { v = &x; break; }
        if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr)
        {
            float best = 1e9f;
            for (auto& x : voices)
                if (x.ampEnv.isReleasing() && x.ampEnv.getLevel() < best) { best = x.ampEnv.getLevel(); v = &x; }
            if (v == nullptr)
            {
                v = &voices[0];
                for (auto& x : voices) if (x.age < v->age) v = &x;
            }
        }
        const float from = (params.glide >= 0.002f && lastNote >= 0) ? (float) lastNote : (float) note;
        lastNote = note;
        v->start (note, vel, from, ++counter);
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices)
            if (v.active && v.gate && v.note == note)
            {
                if (sustain) v.held = true; else v.release();
            }
    }

    void setSustain (bool on) noexcept
    {
        sustain = on;
        if (! on) for (auto& v : voices) if (v.active && v.held) v.release();
    }

    void allNotesOff (bool immediately = false) noexcept
    {
        for (auto& v : voices)
        {
            if (immediately) v.kill();
            else if (v.active) v.release();
        }
    }

    int getActiveVoiceCount() const noexcept
    {
        int n = 0; for (auto& v : voices) if (v.active) ++n; return n;
    }

    void render (float* L, float* R, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            frame.cutoff = cutoffSm.getNextValue();
            frame.reso   = resoSm.getNextValue();
            bendSm += (bend - bendSm) * 0.02f;
            frame.bend = bendSm;
            frame.mod  = mod;
            frame.lfo  = nextLfo();

            float sum = 0.f;
            for (auto& v : voices) sum += v.tick (params, frame, sr, glideCoef);

            const float m = masterSm.getNextValue() * 0.8f;
            float l, r;
            chorus.process (sum * m, params.chorus, l, r);
            L[i] = std::tanh (l);
            R[i] = std::tanh (r);
        }
    }

private:
    float nextLfo() noexcept
    {
        lfoPhase += params.lfoRate / sr;
        bool wrapped = false;
        if (lfoPhase >= 1.f) { lfoPhase -= 1.f; wrapped = true; }
        switch (params.lfoShape)
        {
            case 0:  return std::sin (kTwoPi * lfoPhase);
            case 1:  return 4.f * std::abs (lfoPhase - 0.5f) - 1.f;
            case 2:  return 2.f * lfoPhase - 1.f;
            case 3:  return lfoPhase < 0.5f ? 1.f : -1.f;
            default: if (wrapped) lfoSH = lfoRng.bip(); return lfoSH;
        }
    }

    std::array<Voice, kNumVoices> voices;
    Params params;
    Frame  frame;
    Chorus chorus;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSm;
    juce::SmoothedValue<float> resoSm, masterSm;
    Rng   lfoRng;
    float sr = 44100.f, glideCoef = 1.f;
    float lfoPhase = 0.f, lfoSH = 0.f, bend = 0.f, bendSm = 0.f, mod = 0.f;
    bool  sustain = false;
    int   lastNote = -1;
    uint64_t counter = 0;
};

} // namespace ferrite
