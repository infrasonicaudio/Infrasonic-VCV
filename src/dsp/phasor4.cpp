#include <math.h>
#include "phasor4.hpp"

using namespace infrasonic::simd;
using namespace rack::simd;

void Phasor4::SetFreq(float freq)
{
    freq_ = freq;
    inc_ = (freq_ * 4.0f) / sample_rate_;
    for (int i=1; i<4; i++)
    {
        phs_[i] = phs_[0] + (inc_ * (i / 4.0f));
    }
}

float_4 Phasor4::Process()
{
    float_4 out;

    phs_ -= (phs_ > 1.0f) & 1.0f;
    phs_ = fmax(0.0f, phs_);

    out = phs_;
    phs_ += inc_;

    return out;
}
