#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "audio/sample_buffer.h"

namespace imdj {

struct WaveformBucket {
    int8_t peakLow = 0;
    int8_t peakHigh = 0;
    uint8_t energyLow = 0;
    uint8_t energyMid = 0;
    uint8_t energyHigh = 0;
};

class WaveformPeaks {
public:
    static constexpr uint32_t BUCKETS_PER_SECOND = 60;

    static WaveformPeaks Compute(const SampleBuffer& buffer);
    static std::string CachePathFor(const std::string& trackPath);

    bool load(const std::string& trackPath, uint64_t frameCount);
    bool save(const std::string& trackPath) const;

    bool empty() const { return buckets_.empty(); }
    uint64_t frameCount() const { return frameCount_; }
    size_t size() const { return buckets_.size(); }
    const WaveformBucket& at(size_t index) const { return buckets_[index]; }

    size_t bucketForSeconds(double seconds) const
    {
        double index = seconds * BUCKETS_PER_SECOND;
        if (index <= 0.0) {
            return 0;
        }

        size_t clamped = static_cast<size_t>(index);
        return clamped < buckets_.size() ? clamped : buckets_.size();
    }

private:
    std::vector<WaveformBucket> buckets_;
    uint64_t frameCount_ = 0;
};

} // namespace imdj
