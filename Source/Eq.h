#pragma once
// ---------------------------------------------------------------------------
//  Ferrite 8 - 4-band parametric equaliser
//  band 0 = low shelf, bands 1 & 2 = peaking bells, band 3 = high shelf.
//  Coefficients follow the RBJ "Audio EQ Cookbook". The same maths drives
//  the audio filters and the response curve drawn in the editor, so what
//  you see is what you hear.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cmath>

namespace ferrite
{
constexpr int kEqBands = 4;

struct EqParams
{
    bool  on = true;
    float freq[kEqBands] = { 120.f, 500.f, 3000.f, 8000.f };
    float gain[kEqBands] = { 0.f, 0.f, 0.f, 0.f };           // dB
    float q[kEqBands]    = { 0.71f, 1.f, 1.f, 0.71f };
};

enum class EqBandType { lowShelf, peak, highShelf };

inline EqBandType eqBandType (int band) noexcept
{
    return band == 0 ? EqBandType::lowShelf : (band == kEqBands - 1 ? EqBandType::highShelf : EqBandType::peak);
}

struct BiquadCoeffs { float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f; };

inline BiquadCoeffs makeEqBand (EqBandType type, float sampleRate, float freq, float gainDb, float q) noexcept
{
    constexpr float twoPi = 6.28318530717958647692f;
    freq = std::clamp (freq, 10.f, sampleRate * 0.45f);
    q    = std::max (q, 0.1f);

    const float A     = std::pow (10.f, gainDb / 40.f);
    const float w0    = twoPi * freq / sampleRate;
    const float cosw  = std::cos (w0), sinw = std::sin (w0);
    const float alpha = sinw / (2.f * q);

    float b0, b1, b2, a0, a1, a2;
    switch (type)
    {
        case EqBandType::peak:
            b0 = 1.f + alpha * A;  b1 = -2.f * cosw;  b2 = 1.f - alpha * A;
            a0 = 1.f + alpha / A;  a1 = -2.f * cosw;  a2 = 1.f - alpha / A;
            break;
        case EqBandType::lowShelf:
        {
            const float s = 2.f * std::sqrt (A) * alpha;
            b0 = A * ((A + 1.f) - (A - 1.f) * cosw + s);
            b1 = 2.f * A * ((A - 1.f) - (A + 1.f) * cosw);
            b2 = A * ((A + 1.f) - (A - 1.f) * cosw - s);
            a0 = (A + 1.f) + (A - 1.f) * cosw + s;
            a1 = -2.f * ((A - 1.f) + (A + 1.f) * cosw);
            a2 = (A + 1.f) + (A - 1.f) * cosw - s;
            break;
        }
        default:
        {
            const float s = 2.f * std::sqrt (A) * alpha;
            b0 = A * ((A + 1.f) + (A - 1.f) * cosw + s);
            b1 = -2.f * A * ((A - 1.f) + (A + 1.f) * cosw);
            b2 = A * ((A + 1.f) + (A - 1.f) * cosw - s);
            a0 = (A + 1.f) - (A - 1.f) * cosw + s;
            a1 = 2.f * ((A - 1.f) - (A + 1.f) * cosw);
            a2 = (A + 1.f) - (A - 1.f) * cosw - s;
            break;
        }
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

// magnitude response of one band in dB at a given frequency
inline float eqMagnitudeDb (const BiquadCoeffs& c, float sampleRate, float freq) noexcept
{
    constexpr float twoPi = 6.28318530717958647692f;
    const float w = twoPi * freq / sampleRate;
    const float c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.f * w), s2 = std::sin (2.f * w);
    const float nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = c.b1 * s1 + c.b2 * s2;
    const float dr = 1.f + c.a1 * c1 + c.a2 * c2,  di = c.a1 * s1 + c.a2 * s2;
    return 10.f * std::log10 (std::max ((nr * nr + ni * ni) / std::max (dr * dr + di * di, 1e-20f), 1e-20f));
}

//==============================================================================
class ParametricEq
{
public:
    void prepare (float sr) noexcept
    {
        sampleRate = sr;
        reset();
        initialised = false;
    }

    void reset() noexcept
    {
        for (auto& ch : z) for (auto& s : ch) s = { 0.f, 0.f };
    }

    // called once per audio block: smooths the controls, then recalculates the filters
    void setParams (const EqParams& p) noexcept
    {
        if (! initialised)
        {
            cur = p; initialised = true;
            mix = p.on ? 1.f : 0.f;
        }
        else
        {
            for (int b = 0; b < kEqBands; ++b)
            {
                const float lf = std::log (cur.freq[b]);
                cur.freq[b] = std::exp (lf + (std::log (p.freq[b]) - lf) * 0.4f);   // glide in log-frequency
                cur.gain[b] += (p.gain[b] - cur.gain[b]) * 0.4f;
                cur.q[b]    += (p.q[b]    - cur.q[b])    * 0.4f;
            }
        }
        mixTarget = p.on ? 1.f : 0.f;
        for (int b = 0; b < kEqBands; ++b)
            coeffs[(size_t) b] = makeEqBand (eqBandType (b), sampleRate, cur.freq[b], cur.gain[b], cur.q[b]);
    }

    void process (float& l, float& r) noexcept
    {
        if (mixTarget == 0.f && mix < 1.0e-4f)          // fully bypassed: no work, no stale state
        {
            if (! wasBypassed) { reset(); wasBypassed = true; }
            return;
        }
        wasBypassed = false;

        float wl = l, wr = r;
        for (size_t b = 0; b < (size_t) kEqBands; ++b)
        {
            wl = tick (z[0][b], coeffs[b], wl);
            wr = tick (z[1][b], coeffs[b], wr);
        }
        mix += (mixTarget - mix) * 0.003f;              // short fade when switching on / off
        l += (wl - l) * mix;
        r += (wr - r) * mix;
    }

private:
    struct State { float z1, z2; };

    static float tick (State& s, const BiquadCoeffs& c, float x) noexcept      // transposed direct form II
    {
        const float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    float sampleRate = 44100.f, mix = 1.f, mixTarget = 1.f;
    bool initialised = false, wasBypassed = false;
    EqParams cur;
    std::array<BiquadCoeffs, kEqBands> coeffs;
    std::array<std::array<State, kEqBands>, 2> z {};
};

} // namespace ferrite
