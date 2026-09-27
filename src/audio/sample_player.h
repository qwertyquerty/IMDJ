#pragma once

#include <cstdint>

#include "audio/audio_buffer.h"
#include "audio/sample_buffer.h"

namespace imdj {

struct PlaybackRequest {
    double startFrame = 0.0;
    double rate = 1.0;
    float gain = 1.0f;
    bool loop = false;
    uint64_t loopStart = 0;
    uint64_t loopEnd = 0;
    bool stopBeforeStart = false;
};

struct PlaybackResult {
    double endFrame = 0.0;
    uint32_t framesWritten = 0;
    bool hitBoundary = false;
};

PlaybackResult ReadBlock(const SampleBuffer& buffer, const StereoBlock& out, const PlaybackRequest& request);

} // namespace imdj
