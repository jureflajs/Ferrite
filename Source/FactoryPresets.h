#pragma once
#include <utility>
#include <vector>

// Each preset starts from the Init patch (all parameter defaults) and then
// overrides only the values listed here.
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets =
    {
        { "Init", {} },

        { "Warm Pad", {
            { "osc2Fine", 9.f }, { "cutoff", 700.f }, { "reso", 0.15f }, { "envAmt", 0.35f },
            { "fA", 0.8f }, { "fD", 1.5f }, { "fS", 0.6f }, { "fR", 1.5f },
            { "aA", 0.7f }, { "aD", 1.0f }, { "aS", 0.85f }, { "aR", 1.8f },
            { "lfoRate", 0.4f }, { "lfoFilter", 0.15f }, { "drift", 0.4f }, { "chorus", 0.75f } } },

        { "PWM Strings", {
            { "osc1Wave", 1.f }, { "osc2Wave", 1.f }, { "pw", 0.5f }, { "osc2Fine", 6.f },
            { "cutoff", 1800.f }, { "reso", 0.1f }, { "envAmt", 0.1f },
            { "aA", 0.5f }, { "aR", 1.2f }, { "lfoRate", 0.6f }, { "lfoPwm", 0.6f },
            { "drift", 0.35f }, { "chorus", 0.85f } } },

        { "Brass Stab", {
            { "osc2Fine", 6.f }, { "cutoff", 500.f }, { "reso", 0.1f }, { "envAmt", 0.6f },
            { "fA", 0.08f }, { "fD", 0.5f }, { "fS", 0.5f }, { "fR", 0.25f },
            { "aA", 0.06f }, { "aD", 0.4f }, { "aS", 0.8f }, { "aR", 0.2f }, { "chorus", 0.5f } } },

        { "Sub Bass", {
            { "osc2Wave", 1.f }, { "mix1", 0.7f }, { "mix2", 0.f }, { "mixSub", 0.9f },
            { "cutoff", 350.f }, { "reso", 0.3f }, { "envAmt", 0.5f }, { "keyTrack", 0.3f },
            { "fA", 0.001f }, { "fD", 0.25f }, { "fS", 0.2f }, { "fR", 0.15f },
            { "aA", 0.002f }, { "aD", 0.2f }, { "aS", 0.9f }, { "aR", 0.12f },
            { "glide", 0.05f }, { "drift", 0.1f }, { "chorus", 0.f } } },

        { "Acid Squelch", {
            { "mix1", 1.f }, { "mix2", 0.f }, { "cutoff", 300.f }, { "reso", 0.8f }, { "envAmt", 0.75f },
            { "keyTrack", 0.3f }, { "fA", 0.001f }, { "fD", 0.22f }, { "fS", 0.f }, { "fR", 0.1f },
            { "aA", 0.001f }, { "aD", 0.3f }, { "aS", 0.8f }, { "aR", 0.08f },
            { "glide", 0.06f }, { "chorus", 0.f }, { "drift", 0.1f } } },

        { "Pluck Lead", {
            { "osc1Wave", 1.f }, { "pw", 0.35f }, { "osc2Semi", 12.f }, { "osc2Fine", 5.f }, { "mix2", 0.5f },
            { "cutoff", 900.f }, { "reso", 0.35f }, { "envAmt", 0.7f },
            { "fA", 0.001f }, { "fD", 0.35f }, { "fS", 0.f }, { "fR", 0.2f },
            { "aA", 0.001f }, { "aD", 0.6f }, { "aS", 0.3f }, { "aR", 0.25f }, { "chorus", 0.4f } } },

        { "Soaring Lead", {
            { "osc2Fine", -8.f }, { "cutoff", 2500.f }, { "reso", 0.3f }, { "envAmt", 0.2f },
            { "aA", 0.01f }, { "aS", 0.9f }, { "aR", 0.3f }, { "glide", 0.08f },
            { "lfoRate", 5.5f }, { "lfoPitch", 0.08f }, { "chorus", 0.35f } } },

        { "Wind FX", {
            { "mix1", 0.f }, { "mix2", 0.f }, { "mixNoise", 0.8f }, { "cutoff", 800.f }, { "reso", 0.5f },
            { "envAmt", 0.f }, { "aA", 1.5f }, { "aR", 2.5f },
            { "lfoRate", 0.3f }, { "lfoFilter", 0.5f }, { "chorus", 0.6f } } },
    };
    return presets;
}
