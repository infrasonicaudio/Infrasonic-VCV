#include "PDO.hpp"

using namespace infrasonic;
using namespace rack::simd;

namespace infrasonic
{
    // amt must be 0-1
    template<typename T>
    inline T bend(T in, const T amt);

    template<>
    inline float bend(float in, const float amt)
    {
        if (amt == 0.0f)
            return in;
        
        const float scale = -10.0f * amt;
        return expm1f(in * scale) / expm1f(scale);
    }

    template<>
    inline float_4 bend(float_4 in, const float_4 amt)
    {
        const float_4 scale = -10.0f * amt;
        float_4 out = (exp(in * scale) - 1.0f) / (exp(scale) - 1.0f);
        // The vectorized version is sensitive to near-zero denominators
        float_4 tooSmall = (amt <= 1e-5f);
        return (out & ~tooSmall) | (in & tooSmall);
    }

    template<typename T>
    inline T formant(T in, const T amt);

    template<>
    inline float formant(float in, const float amt)
    {
        float out;
        in = in + (in - 0.5f) * amt;
        out = rack::math::clamp(in, 0.0f, 1.0f);
        return out - floorf(out);
    }

    template<>
    inline float_4 formant(float_4 in, const float_4 amt)
    {
        float_4 out;
        in = in + (in - 0.5f) * amt;
        out = clamp(in, 0.0f, 1.0f);
        return out - floor(out);
    }

    template<typename T>
    inline T sync(T in, const T amt);

    template<>
    inline float sync(float in, const float amt)
    {
        in *= 1.f + amt;
        return in - floorf(in);
    }

    template<>
    inline float_4 sync(float_4 in, const float_4 amt)
    {
        in *= 1.f + amt;
        return in - floor(in);
    }

    template<typename T>
    inline T fold(T in, const T amt);

    template<>
    inline float fold(float in, const float amt)
    {
        float ft, sgn, out;
        in *= amt;
        ft  = floorf((in + 1.0f) * 0.5f);
        sgn = static_cast<int>(ft) % 2 == 0 ? 1.0f : -1.0f;
        out = sgn * (in - 2.0f * ft);
        return out - floorf(out);
    }

    template<>
    inline float_4 fold(float_4 in, const float_4 amt)
    {
        float_4 ft, sgn, out;
        in *= amt;
        ft  = floor((in + 1.0f) * 0.5f);
        // TODO: can this be vectorized?
        for (int i=0; i<4; i++)
        {
            sgn[i] = static_cast<int>(ft[i]) % 2 == 0 ? 1.0f : -1.0f;
        }
        out = sgn * (in - 2.0f * ft);
        return out - floor(out);
    }
}

void PhaseDistortionOscillator::Init(const float sample_rate)
{
    phasor_.Init(sample_rate);
    pm_phasor_.Init(sample_rate);
    sub_phasor_.Init(sample_rate);
    pd_1_amt_.Init(sample_rate, 0.02f);
    pd_2_amt_.Init(sample_rate, 0.02f);
    pm_amt_.Init(sample_rate, 0.02f);
    Reset();
}

void PhaseDistortionOscillator::SetSampleRate(const float sample_rate)
{
    phasor_.SetSampleRate(sample_rate);
    pm_phasor_.SetSampleRate(sample_rate);
    sub_phasor_.SetSampleRate(sample_rate);
    pd_1_amt_.SetSampleRate(sample_rate);
    pd_2_amt_.SetSampleRate(sample_rate);
    pm_amt_.SetSampleRate(sample_rate);
}

void PhaseDistortionOscillator::Reset()
{
    pd_1_amt_.Set(0.0f, true);
    pd_2_amt_ .Set(0.0f, true);
    pm_amt_.Set(0.0f, true);

    phasor_.SetFreq(220.0f);
    pm_phasor_.SetFreq(220.f);
    sub_phasor_.SetFreq(110.0f);
}

void PhaseDistortionOscillator::ProcessBlock(const Patch &patch, const float *ext_pm_in, float *out, const size_t size)
{
    size_t offset = 0;
    float_4 pd1_amt4, pd2_amt4, ext_pm_in4;
    float_4 pd4, pds4, win4;
    float_4 out4, out_alt4;

    phasor_.SetFreq(patch.carrier_freq);
    pm_phasor_.SetFreq(patch.carrier_freq * patch.pm_ratio);
    sub_phasor_.SetFreq(patch.carrier_freq * 0.5f);

    pd_1_amt_.Set(patch.pd_amt[0]);
    pd_2_amt_.Set(patch.pd_amt[1]);
    pm_amt_.Set(patch.pm_amt);

    while (offset < size)
    {
        ext_pm_in4 = float_4::load(ext_pm_in + offset);
        pd4 = phasor_.Process();
        pds4 = sub_phasor_.Process();
        win4 = processWindow(patch.win_type, pd4);

        if (patch.alt_out_type == OUT_TYPE_SIN) {
            out_alt4 = sin(pd4 * M_PI * 2.0f);
        }

        pd1_amt4 = {pd_1_amt_.Process(), pd_1_amt_.Process(), pd_1_amt_.Process(), pd_1_amt_.Process()};
        pd2_amt4 = {pd_2_amt_.Process(), pd_2_amt_.Process(), pd_2_amt_.Process(), pd_2_amt_.Process()};
        if (patch.routing == Routing::ROUTING_PM_PRE)
        {
            pd4 = processPhaseMod(pd4, ext_pm_in4, patch.pm_ratio);
        }

        pd4 = processPhaseDist(patch.pd_type[0], pd4, pd1_amt4);
        pd4 = processPhaseDist(patch.pd_type[1], pd4, pd2_amt4);

        if (patch.routing == Routing::ROUTING_PM_POST)
        {
            pd4 = processPhaseMod(pd4, ext_pm_in4, patch.pm_ratio);
        }

        out4 = sin(pd4 * M_PI * 2.0f) * win4;

        switch (patch.alt_out_type)
        {
            case OUT_TYPE_90:
                out_alt4 = cos(pd4 * M_PI * 2.0f) * win4;
                break;
            case OUT_TYPE_SIN:
                // Already processed above before PD/PM
                break;
            case OUT_TYPE_SUB:
                out_alt4 = sin(pds4 * M_PI * 2.0f); 
                break;
            case OUT_TYPE_PHASOR:
                out_alt4 = pd4;
                break;
            default:
                break;
        }

        // TODO: faster way to do this?
        for (int i = 0; i < 4; i++)
        {
            out[offset * 2]      = out4[i];
            out[offset * 2 + 1]  = out_alt4[i];
            offset++;
        }
    }
}

// returns phase
float_4 PhaseDistortionOscillator::processPhaseMod(float_4 phase, const float_4 ext_pm_in, const float ratio)
{
        float_4 amt(pm_amt_.Process(), pm_amt_.Process(), pm_amt_.Process(), pm_amt_.Process());
        float_4 mod = sin(pm_phasor_.Process() * M_PI * 2.0f);
        phase += mod * (amt * 10.0f / ratio) + ext_pm_in;
        return phase - floor(phase);
}

float_4 PhaseDistortionOscillator::processPhaseDist(const PhaseDistType type, const float_4 phase, const float_4 amt) const
{   
    switch(type)
    {
        case PD_TYPE_BEND:
            return bend(phase, amt);

        case PD_TYPE_SYNC:
            return sync(phase, pow(2.0f, amt * 5.0f) - 1.0f);

        case PD_TYPE_FORMANT:
            return formant(phase, pow(2.0f, amt * 5.0f) - 1.0f);

        case PD_TYPE_FOLD:
            return fold(phase, pow(2.0f, amt * 5.0f));

        default:
            return phase;
    }
}

float_4 PhaseDistortionOscillator::processWindow(const WindowType type, const float_4 phase) const
{   
    switch (type)
    {
        case WIN_TYPE_SAW:
            return 1.0f - phase;
        case WIN_TYPE_TRI: {
            float_4 cmp = phase < 0.5f;
            return (cmp & (phase * 2.0f)) | (~cmp & (1.0f - (phase - 0.5f) * 2.0f));
        }
        default:
            return 1.0f;
    }
}