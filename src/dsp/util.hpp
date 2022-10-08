#pragma once
#ifndef INFS_DSPUTILS_H
#define INFS_DSPUTILS_H

#include <cmath>

namespace infrasonic {

// Coefficient for one pole smoothing filter based on Tau time constant for `time_s`
inline float onepole_coef(float time_s, float sample_rate) {
    if (time_s <= 0.0f || sample_rate <= 0.0f) { return 1.0f; }
    return fmin(1.0f / (time_s * sample_rate), 1.0f);
}

inline float onepole_coef_t60(float time_s, float sample_rate)
{
	return onepole_coef(time_s * 0.1447597f, sample_rate);
}

}

#endif