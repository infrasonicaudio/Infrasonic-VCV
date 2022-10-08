#pragma once

#include <cstdint>
#include <simd/functions.hpp>
#include "phasor4.hpp"
#include "smooth.hpp"

namespace infrasonic
{
    class PhaseDistortionOscillator
    {
        public:

            enum Routing
            {
                ROUTING_PM_PRE,
                ROUTING_PM_POST,
                ROUTING_PM_LAST 
            };

            enum PhaseDistType
            {
                PD_TYPE_BEND,
                PD_TYPE_SYNC,
                PD_TYPE_FORMANT,
                PD_TYPE_FOLD,
                PD_TYPE_LAST
            };

            enum WindowType
            {
                WIN_TYPE_NONE,
                WIN_TYPE_SAW,
                WIN_TYPE_TRI,
                WIN_TYPE_LAST
            };

            enum AltOutputType
            {
                OUT_TYPE_90,
                OUT_TYPE_SIN,
                OUT_TYPE_SUB,
                OUT_TYPE_PHASOR,
                OUT_TYPE_LAST
            };

            struct Patch
            {
                float           carrier_freq;
                float           pd_amt[2];
                float           pm_amt;
                float           pm_ratio;
                Routing         routing;
                PhaseDistType   pd_type[2];
                WindowType      win_type;
                AltOutputType   alt_out_type;

                Patch()
                    : carrier_freq(220.0f)
                    , pm_amt(0.0f)
                    , pm_ratio(1.0f)
                    , routing(ROUTING_PM_PRE)
                    , win_type(WIN_TYPE_NONE)
                    , alt_out_type(OUT_TYPE_90)
                {
                    pd_amt[0] = 0.0f;
                    pd_amt[1] = 0.0f;
                    pd_type[0] = PD_TYPE_BEND;
                    pd_type[1] = PD_TYPE_SYNC;
                }
            };

            PhaseDistortionOscillator() = default;
            ~PhaseDistortionOscillator() = default;

            void Init(const float sample_rate);
            void SetSampleRate(const float sample_rate);
            void Reset();

            // Interleaved 2-channel block {osc_out, alt_out} of size
            void ProcessBlock(const Patch &patch, const float *ext_pm_in, float *out, const size_t size);

        private:
            simd::Phasor4 phasor_, sub_phasor_, pm_phasor_;

            SmoothedValue pd_1_amt_, pd_2_amt_, pm_amt_;

            rack::simd::float_4 processPhaseMod(rack::simd::float_4 phase, const rack::simd::float_4 ext_pm_in, const float ratio);
            rack::simd::float_4 processPhaseDist(const PhaseDistType type, const rack::simd::float_4 phase, const rack::simd::float_4 amt) const;
            rack::simd::float_4 processWindow(const WindowType type, const rack::simd::float_4 phase) const;
    };
}