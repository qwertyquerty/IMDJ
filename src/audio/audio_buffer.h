#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace imdj {

struct GainStep {
    float from = 1.0f;
    float to = 1.0f;

    constexpr GainStep() = default;
    constexpr GainStep(float gain) : from(gain), to(gain) {}
    constexpr GainStep(float fromGain, float toGain) : from(fromGain), to(toGain) {}

    bool unity() const { return from == 1.0f && to == 1.0f; }
    float delta(uint32_t frames) const { return frames > 0 ? (to - from) / static_cast<float>(frames) : 0.0f; }
};

class SmoothedGain {
public:
    explicit SmoothedGain(float initial = 1.0f) : current_(initial) {}

    GainStep next(float target)
    {
        const GainStep step{current_, target};
        current_ = target;

        return step;
    }

private:
    float current_;
};

struct StereoBlock {
    std::span<float> left;
    std::span<float> right;

    bool valid() const { return !left.empty() && left.size() == right.size(); }
    uint32_t frames() const { return static_cast<uint32_t>(left.size()); }

    void clear() const
    {
        std::ranges::fill(left, 0.0f);
        std::ranges::fill(right, 0.0f);
    }

    void scale(GainStep leftGain, GainStep rightGain) const
    {
        const float leftDelta = leftGain.delta(frames());
        const float rightDelta = rightGain.delta(frames());
        float l = leftGain.from;
        float r = rightGain.from;
        for (uint32_t i = 0; i < frames(); ++i) {
            l += leftDelta;
            r += rightDelta;
            left[i] *= l;
            right[i] *= r;
        }
    }

    void addFrom(const StereoBlock& src, GainStep gain = {}) const
    {
        const uint32_t n = std::min(frames(), src.frames());
        const float delta = gain.delta(n);
        float g = gain.from;
        for (uint32_t i = 0; i < n; ++i) {
            g += delta;
            left[i] += src.left[i] * g;
            right[i] += src.right[i] * g;
        }
    }

    StereoBlock firstFrames(uint32_t n) const
    {
        uint32_t count = std::min(n, frames());
        return StereoBlock{left.first(count), right.first(count)};
    }
};

class StereoBuffer {
public:
    void resize(uint32_t frameCapacity)
    {
        left_.assign(frameCapacity, 0.0f);
        right_.assign(frameCapacity, 0.0f);
    }

    uint32_t capacity() const { return static_cast<uint32_t>(left_.size()); }

    StereoBlock block(uint32_t frames)
    {
        uint32_t n = std::min(frames, capacity());
        return StereoBlock{std::span(left_).first(n), std::span(right_).first(n)};
    }

private:
    std::vector<float> left_;
    std::vector<float> right_;
};

} // namespace imdj
