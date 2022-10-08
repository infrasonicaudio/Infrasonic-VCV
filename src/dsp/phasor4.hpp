#pragma once
#ifndef INFS_PHASOR_SIMD_H
#define INFS_PHASOR_SIMD_H

#include <cstdint>
#include <simd/functions.hpp>

namespace infrasonic
{
namespace simd
{

/// Phasor which operates on a SIMD vector of 4 floats
class Phasor4
{
  public:
    Phasor4() = default;
    ~Phasor4() = default; 

    inline void Init(float sample_rate)
    {
        sample_rate_ = sample_rate;
        phs_ = 0.0f;
        SetFreq(1.0f);
    }

    inline void SetSampleRate(float sample_rate)
    {
      sample_rate_ = sample_rate;
      SetFreq(freq_);
    } 

    rack::simd::float_4 Process();

    void SetFreq(float freq);

  private:
    float freq_, inc_;
    float sample_rate_;
    rack::simd::float_4 phs_;
};
}
}
#endif
