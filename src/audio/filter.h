#pragma once

#include <algorithm>
#include <cmath>

#include "audio/audio_buffer.h"
#include "audio/ma_filter.h"

namespace imdj {

class OneKnobFilter {
public:
    static constexpr double LOW_PASS_MIN_HZ = 60.0;
    static constexpr double LOW_PASS_MAX_HZ = 20000.0;
    static constexpr double HIGH_PASS_MIN_HZ = 20.0;
    static constexpr double HIGH_PASS_MAX_HZ = 9000.0;
    static constexpr double RESONANCE = 0.9;
    static constexpr float DEAD_ZONE = 0.02f;

    static bool engaged(float position) { return std::fabs(position) > DEAD_ZONE; }

    static double cutoffHz(float position)
    {
        float amount = (std::fabs(position) - DEAD_ZONE) / (1.0f - DEAD_ZONE);
        amount = std::clamp(amount, 0.0f, 1.0f);
        if (position < 0.0f) {
            return LOW_PASS_MAX_HZ * std::pow(LOW_PASS_MIN_HZ / LOW_PASS_MAX_HZ, amount);
        }

        return HIGH_PASS_MIN_HZ * std::pow(HIGH_PASS_MAX_HZ / HIGH_PASS_MIN_HZ, amount);
    }

    void process(const StereoBlock& block, float position, double sampleRate)
    {
        if (!engaged(position)) {
            reset();
            return;
        }

        const ma_uint32 rate = static_cast<ma_uint32>(sampleRate);
        const double cutoff = cutoffHz(position);
        if (position < 0.0f) {
            ResetAll(highPass_);
            Run(lowPass_, ma_lpf2_config_init(ma_format_f32, 1, rate, cutoff, RESONANCE), block);
        }
        else {
            ResetAll(lowPass_);
            Run(highPass_, ma_hpf2_config_init(ma_format_f32, 1, rate, cutoff, RESONANCE), block);
        }
    }

    void reset()
    {
        ResetAll(lowPass_);
        ResetAll(highPass_);
    }

private:
    static constexpr int CASCADE = 2;

    template <typename Stage, typename Config>
    static void Run(Stage (&stages)[2][CASCADE], const Config& config, const StereoBlock& block)
    {
        for (int channel = 0; channel < 2; ++channel) {
            for (Stage& stage : stages[channel]) {
                stage.reinit(config);
                stage.process(channel == 0 ? block.left : block.right);
            }
        }
    }

    template <typename Stage>
    static void ResetAll(Stage (&stages)[2][CASCADE])
    {
        for (auto& channel : stages) {
            for (Stage& stage : channel) {
                stage.reset();
            }
        }
    }

    LowPass lowPass_[2][CASCADE];
    HighPass highPass_[2][CASCADE];
};

} // namespace imdj
