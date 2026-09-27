#pragma once

#include <algorithm>

#include "audio/audio_buffer.h"
#include "audio/ma_filter.h"

namespace imdj {

struct EqGainsDb {
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;

    bool flat() const { return low == 0.0f && mid == 0.0f && high == 0.0f; }
};

class ThreeBandEq {
public:
    static constexpr double LOW_HZ = 250.0;
    static constexpr double MID_HZ = 1000.0;
    static constexpr double MID_Q = 0.9;
    static constexpr double HIGH_HZ = 3200.0;
    static constexpr double SHELF_SLOPE = 1.0;
    static constexpr float MAX_GAIN_DB = 6.0f;

    void process(const StereoBlock& block, const EqGainsDb& gains, double sampleRate)
    {
        const ma_uint32 rate = static_cast<ma_uint32>(sampleRate);
        const ma_loshelf2_config low =
            ma_loshelf2_config_init(ma_format_f32, 1, rate, clampDb(gains.low), SHELF_SLOPE, LOW_HZ);
        const ma_peak2_config mid = ma_peak2_config_init(ma_format_f32, 1, rate, clampDb(gains.mid), MID_Q, MID_HZ);
        const ma_hishelf2_config high =
            ma_hishelf2_config_init(ma_format_f32, 1, rate, clampDb(gains.high), SHELF_SLOPE, HIGH_HZ);

        for (int channel = 0; channel < 2; ++channel) {
            const std::span<float> samples = channel == 0 ? block.left : block.right;
            low_[channel].reinit(low);
            mid_[channel].reinit(mid);
            high_[channel].reinit(high);
            low_[channel].process(samples);
            mid_[channel].process(samples);
            high_[channel].process(samples);
        }
    }

    void reset()
    {
        for (int channel = 0; channel < 2; ++channel) {
            low_[channel].reset();
            mid_[channel].reset();
            high_[channel].reset();
        }
    }

private:
    static double clampDb(float gainDb) { return std::clamp(gainDb, -MAX_GAIN_DB, MAX_GAIN_DB); }

    LowShelf low_[2];
    Peak mid_[2];
    HighShelf high_[2];
};

} // namespace imdj
