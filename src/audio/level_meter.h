#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

#include "audio/audio_buffer.h"

namespace imdj {

inline void SmoothMeter(
    std::atomic<float>& value, float target, double blockSeconds, double attackTau, double releaseTau
)
{
    float prev = value.load();
    double tau = target > prev ? attackTau : releaseTau;
    float alpha = static_cast<float>(1.0 - std::exp(-blockSeconds / tau));
    value.store(prev + (target - prev) * alpha);
}

class LevelMeter {
public:
    void update(float peak, double blockSeconds)
    {
        SmoothMeter(level_, peak, blockSeconds, LEVEL_ATTACK, LEVEL_RELEASE);
        SmoothMeter(peakHold_, peak, blockSeconds, HOLD_ATTACK, HOLD_RELEASE);
    }

    float level() const { return level_.load(); }
    float peakHold() const { return peakHold_.load(); }

private:
    static constexpr double LEVEL_ATTACK = 0.01;
    static constexpr double LEVEL_RELEASE = 0.3;
    static constexpr double HOLD_ATTACK = 0.001;
    static constexpr double HOLD_RELEASE = 1.5;

    std::atomic<float> level_{0.0f};
    std::atomic<float> peakHold_{0.0f};
};

class StereoMeter {
public:
    void update(const StereoBlock& block, double blockSeconds)
    {
        float peak[2] = {0.0f, 0.0f};
        for (uint32_t i = 0; i < block.frames(); ++i) {
            peak[0] = std::max(peak[0], std::fabs(block.left[i]));
            peak[1] = std::max(peak[1], std::fabs(block.right[i]));
        }

        channel_[0].update(peak[0], blockSeconds);
        channel_[1].update(peak[1], blockSeconds);
    }

    void decay(double blockSeconds)
    {
        channel_[0].update(0.0f, blockSeconds);
        channel_[1].update(0.0f, blockSeconds);
    }

    const LevelMeter& channel(int index) const { return channel_[index & 1]; }
    float level(int channel) const { return channel_[channel & 1].level(); }
    float peakHold(int channel) const { return channel_[channel & 1].peakHold(); }

private:
    LevelMeter channel_[2];
};

class MasterMeter {
public:
    void update(const StereoBlock& block, double blockSeconds)
    {
        stereo_.update(block, blockSeconds);

        float midPeak = 0.0f;
        float sidePeak = 0.0f;
        double sumSq = 0.0;
        for (uint32_t i = 0; i < block.frames(); ++i) {
            float l = block.left[i];
            float r = block.right[i];
            midPeak = std::max(midPeak, std::fabs((l + r) * 0.5f));
            sidePeak = std::max(sidePeak, std::fabs((l - r) * 0.5f));
            sumSq += static_cast<double>(l) * l + static_cast<double>(r) * r;
        }

        mid_.update(midPeak, blockSeconds);
        side_.update(sidePeak, blockSeconds);
        const float rms = block.frames() > 0 ? static_cast<float>(std::sqrt(sumSq / (block.frames() * 2))) : 0.0f;
        SmoothMeter(loudness_, rms, blockSeconds, 0.05, 0.4);
    }

    const StereoMeter& stereo() const { return stereo_; }
    const LevelMeter& mid() const { return mid_; }
    const LevelMeter& side() const { return side_; }
    float loudness() const { return loudness_.load(); }

private:
    StereoMeter stereo_;
    LevelMeter mid_;
    LevelMeter side_;
    std::atomic<float> loudness_{0.0f};
};

} // namespace imdj
