#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include "audio/audio_buffer.h"
#include "audio/sample_buffer.h"
#include "audio/track_loader.h"

namespace imdj {

class Preview {
public:
    void prepare(uint32_t maxBlockFrames);

    void start(const std::string& utf8Path);
    void stop();
    void poll();

    bool isPlaying(const std::string& utf8Path) const;
    bool isLoading(const std::string& utf8Path) const;
    const std::string& lastError() const { return lastError_; }

    double positionSeconds() const;
    double durationSeconds() const;

    void setVolume(float volume);
    float volume() const { return volume_.load(); }

    void mixInto(const StereoBlock& cue);

private:
    static constexpr int SLOT = 0;

    SampleBuffer buffer_;
    std::mutex bufferMutex_;
    StereoBuffer scratch_;
    TrackLoader loader_;
    std::string path_;
    std::string pendingPath_;
    std::string lastError_;
    std::atomic<double> frame_{0.0};
    std::atomic<bool> playing_{false};
    std::atomic<float> volume_{0.5f};
};

} // namespace imdj
