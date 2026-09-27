#include "audio/waveform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include "audio/audio_constants.h"
#include "core/paths.h"

namespace imdj {

namespace {

constexpr char MAGIC[8] = {'I', 'M', 'D', 'J', 'P', 'K', '0', '1'};
constexpr float LOW_PASS_ALPHA = 0.4f;
constexpr float HIGH_BOOST = 1.8f;

int8_t QuantizePeak(float value) { return static_cast<int8_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * 127.0f)); }

uint8_t QuantizeEnergy(float value)
{
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

} // namespace

std::string WaveformPeaks::CachePathFor(const std::string& trackPath) { return trackPath + ".imdj.peaks"; }

WaveformPeaks WaveformPeaks::Compute(const SampleBuffer& buffer)
{
    WaveformPeaks peaks;
    peaks.frameCount_ = buffer.frameCount();
    if (buffer.empty()) {
        return peaks;
    }

    const uint64_t framesPerBucket = std::max<uint64_t>(1, SAMPLE_RATE / BUCKETS_PER_SECOND);
    const size_t bucketCount = static_cast<size_t>((buffer.frameCount() + framesPerBucket - 1) / framesPerBucket);
    peaks.buckets_.reserve(bucketCount);

    float lowPassState = 0.0f;
    for (uint64_t start = 0; start < buffer.frameCount(); start += framesPerBucket) {
        const uint64_t end = std::min(start + framesPerBucket, buffer.frameCount());

        float minimum = 0.0f;
        float maximum = 0.0f;
        float lowSum = 0.0f;
        float highSum = 0.0f;
        uint64_t counted = 0;

        for (uint64_t frame = start; frame < end; ++frame) {
            const float* samples = buffer.frame(frame);
            float mono = 0.5f * (samples[0] + samples[1]);
            minimum = std::min(minimum, mono);
            maximum = std::max(maximum, mono);

            lowPassState += (mono - lowPassState) * LOW_PASS_ALPHA;
            lowSum += std::fabs(lowPassState);
            highSum += std::fabs(mono - lowPassState) * HIGH_BOOST;
            ++counted;
        }

        float low = counted > 0 ? lowSum / counted : 0.0f;
        float high = counted > 0 ? highSum / counted : 0.0f;
        float mid = std::max(0.0f, (maximum - minimum) * 0.5f - low - high);

        peaks.buckets_.push_back(
            {QuantizePeak(minimum),
             QuantizePeak(maximum),
             QuantizeEnergy(low),
             QuantizeEnergy(mid),
             QuantizeEnergy(high)}
        );
    }

    return peaks;
}

bool WaveformPeaks::load(const std::string& trackPath, uint64_t frameCount)
{
    std::ifstream file(PathFromUtf8(CachePathFor(trackPath)), std::ios::binary);
    if (!file) {
        return false;
    }

    char magic[sizeof(MAGIC)] = {};
    uint32_t bucketsPerSecond = 0;
    uint64_t storedFrames = 0;
    uint32_t count = 0;

    file.read(magic, sizeof(magic));
    file.read(reinterpret_cast<char*>(&bucketsPerSecond), sizeof(bucketsPerSecond));
    file.read(reinterpret_cast<char*>(&storedFrames), sizeof(storedFrames));
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!file) {
        return false;
    }

    if (std::memcmp(magic, MAGIC, sizeof(MAGIC)) != 0) {
        return false;
    }

    if (bucketsPerSecond != BUCKETS_PER_SECOND || storedFrames != frameCount) {
        return false;
    }

    std::vector<WaveformBucket> buckets(count);
    file.read(reinterpret_cast<char*>(buckets.data()), static_cast<std::streamsize>(count * sizeof(WaveformBucket)));
    if (!file) {
        return false;
    }

    buckets_ = std::move(buckets);
    frameCount_ = storedFrames;

    return true;
}

bool WaveformPeaks::save(const std::string& trackPath) const
{
    if (buckets_.empty()) {
        return false;
    }

    std::ofstream file(PathFromUtf8(CachePathFor(trackPath)), std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    const uint32_t bucketsPerSecond = BUCKETS_PER_SECOND;
    const uint32_t count = static_cast<uint32_t>(buckets_.size());
    file.write(MAGIC, sizeof(MAGIC));
    file.write(reinterpret_cast<const char*>(&bucketsPerSecond), sizeof(bucketsPerSecond));
    file.write(reinterpret_cast<const char*>(&frameCount_), sizeof(frameCount_));
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    file.write(
        reinterpret_cast<const char*>(buckets_.data()),
        static_cast<std::streamsize>(buckets_.size() * sizeof(WaveformBucket))
    );

    return static_cast<bool>(file);
}

} // namespace imdj
