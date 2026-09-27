#pragma once

#include <memory>
#include <vector>

#include "audio/audio_buffer.h"
#include "audio/sample_buffer.h"

namespace imdj {

class PitchStretcher {
public:
    PitchStretcher();
    ~PitchStretcher();

    PitchStretcher(const PitchStretcher&) = delete;
    PitchStretcher& operator=(const PitchStretcher&) = delete;

    void prepare(uint32_t maxBlockFrames);
    void reset();

    double fill(const SampleBuffer& buffer, const StereoBlock& out, double startFrame, double speed, float gain);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace imdj
