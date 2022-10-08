#pragma once
#ifndef INFS_SMOOTHED_VALUE_H
#define INFS_SMOOTHED_VALUE_H

#include <cstdint>
#include "util.hpp"

namespace infrasonic {

class SmoothedValue {

public:

    enum class SmoothType {
        Exponential,
        Linear
    };

    SmoothedValue() = default;
    ~SmoothedValue() = default;

    void Init(
        const float sample_rate, 
        const float time_s = 0.05f,
        const SmoothType smooth_type = SmoothType::Exponential
    )
    {
        sample_rate_    = sample_rate;
        c_              = 0.0f;
        target_         = 0.0f;
        value_          = 0.0f;
        smooth_type_    = smooth_type;

        SetTime(time_s);
    }

    void SetSampleRate(const float sample_rate)
    {
        sample_rate_ = sample_rate;
        SetTime(time_);
    }

    inline float Process()
    {
        switch (smooth_type_) {
            case SmoothType::Exponential:
                // one pole lowpass
                value_ += c_ * (target_ - value_);
                break;
            case SmoothType::Linear:
                if (value_ != target_)
                {
                    value_ += c_;
                    if (hasReachedLinearTarget())
                    {
                        value_ = target_;
                    }
                }
                break;
        }
        return value_;
    }

    // Get last value without applying new smoothing
    inline float Get() const
    {
        return value_;
    }

    inline void Set(const float target, const bool immediate = false)
    {
        target_ = target;
        if (immediate)
        {
            value_ = target;
        }
        if (smooth_type_ == SmoothType::Linear)
        {
            c_ = (time_ == 0.0f) ? 0.0f : (target_ - value_) / (time_ * sample_rate_);
        }
    }

    inline void SetTime(const float time_s)
    {
        time_ = time_s;
        switch (smooth_type_) {
            case SmoothType::Exponential:
                c_ = onepole_coef_t60(time_s, sample_rate_);
                break;
            case SmoothType::Linear:
                c_ = (time_ == 0.0f) ? 0.0f : (target_ - value_) / (time_ * sample_rate_);
                break;
        }
    }

    inline float GetTime() const { return time_; }

private:
    SmoothType smooth_type_;
    float sample_rate_, time_;
    float c_;
    float target_, value_;

    inline bool hasReachedLinearTarget() const
    {
        return 
            (time_ == 0.0f) ||
            (c_ > 0.0f && value_ >= target_) ||
            (c_ < 0.0f && value_ <= target_);
    }
};

}

#endif