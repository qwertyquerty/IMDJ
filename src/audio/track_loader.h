#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "audio/audio_constants.h"
#include "audio/sample_buffer.h"
#include "audio/track_metadata.h"
#include "audio/waveform.h"
#include "core/worker_queue.h"

namespace imdj {

class TrackLoader {
public:
    struct Loaded {
        int slot = -1;
        std::string path;
        SampleBuffer buffer;
        WaveformPeaks peaks;
        TrackMetadata metadata;
        bool hasMetadata = false;
        float normalizeGain = 1.0f;
        std::string error;

        bool ok() const { return error.empty(); }
    };

    TrackLoader();

    void request(int slot, std::string path);
    bool busy(int slot) const;
    float progress(int slot) const;
    std::vector<Loaded> takeCompleted() { return queue_.takeCompleted(); }

private:
    struct Request {
        int slot = -1;
        std::string path;
    };

    static constexpr int SLOTS = MAX_DECK_COUNT;

    Loaded load(const Request& request);

    std::atomic<bool> busy_[SLOTS];
    std::atomic<float> progress_[SLOTS];
    WorkerQueue<Request, Loaded> queue_;
};

} // namespace imdj
