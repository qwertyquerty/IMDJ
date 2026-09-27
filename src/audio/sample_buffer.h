#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/audio_constants.h"
#include "core/result.h"

namespace imdj {

class SampleBuffer {
public:
    Status load(const std::string& utf8Path, std::atomic<float>* progress = nullptr);
    void clear();

    bool empty() const { return frameCount_ == 0; }
    uint64_t frameCount() const { return frameCount_; }
    double durationSeconds() const { return static_cast<double>(frameCount_) / SAMPLE_RATE; }

    const float* data() const { return samples_.data(); }
    const float* frame(uint64_t index) const { return samples_.data() + index * CHANNELS; }

private:
    std::vector<float> samples_;
    uint64_t frameCount_ = 0;
};

} // namespace imdj
