#include "audio/pitch_stretcher.h"

#include <algorithm>
#include <cmath>

#include "audio/audio_constants.h"
#include "bungee/Bungee.h"

namespace imdj {

namespace {
constexpr double MIN_SPEED = 0.05;
constexpr double MAX_SPEED = 8.0;
} // namespace

struct PitchStretcher::Impl {
    Bungee::Stretcher<Bungee::Basic> stretcher{
        {static_cast<int>(SAMPLE_RATE), static_cast<int>(SAMPLE_RATE)}, static_cast<int>(CHANNELS)
    };
    Bungee::Request request{0.0, 1.0, 1.0, true, resampleMode_autoInOut};

    std::vector<float> inputScratch;
    std::vector<float> pendingLeft;
    std::vector<float> pendingRight;
    size_t pendingRead = 0;
    bool started = false;
    double nextPosition = 0.0;

    size_t pending() const { return pendingLeft.size() - pendingRead; }

    void dropConsumed()
    {
        if (pendingRead == 0) {
            return;
        }

        pendingLeft.erase(pendingLeft.begin(), pendingLeft.begin() + static_cast<long>(pendingRead));
        pendingRight.erase(pendingRight.begin(), pendingRight.begin() + static_cast<long>(pendingRead));
        pendingRead = 0;
    }

    void deinterleave(const SampleBuffer& buffer, int begin, int end, int& muteHead, int& muteTail)
    {
        const int frames = end - begin;
        const int stride = frames;
        inputScratch.assign(static_cast<size_t>(frames) * CHANNELS, 0.0f);

        muteHead = begin < 0 ? std::min(-begin, frames) : 0;
        const int64_t total = static_cast<int64_t>(buffer.frameCount());
        muteTail = end > total ? static_cast<int>(std::min<int64_t>(end - total, frames)) : 0;

        for (int i = 0; i < frames; ++i) {
            const int64_t source = static_cast<int64_t>(begin) + i;
            if (source < 0 || source >= total) {
                continue;
            }

            const float* samples = buffer.frame(static_cast<uint64_t>(source));
            inputScratch[static_cast<size_t>(i)] = samples[0];
            inputScratch[static_cast<size_t>(stride + i)] = samples[1];
        }
    }

    void grain(const SampleBuffer& buffer)
    {
        Bungee::InputChunk chunk = stretcher.specifyGrain(request);

        int muteHead = 0;
        int muteTail = 0;
        deinterleave(buffer, chunk.begin, chunk.end, muteHead, muteTail);
        stretcher.analyseGrain(inputScratch.data(), chunk.end - chunk.begin, muteHead, muteTail);

        Bungee::OutputChunk output{};
        stretcher.synthesiseGrain(output);

        for (int i = 0; i < output.frameCount; ++i) {
            pendingLeft.push_back(output.data[i]);
            pendingRight.push_back(output.data[output.channelStride + i]);
        }

        stretcher.next(request);
    }
};

PitchStretcher::PitchStretcher() : impl_(std::make_unique<Impl>()) {}

PitchStretcher::~PitchStretcher() = default;

void PitchStretcher::prepare(uint32_t maxBlockFrames)
{
    impl_->pendingLeft.reserve(maxBlockFrames * 4);
    impl_->pendingRight.reserve(maxBlockFrames * 4);
}

void PitchStretcher::reset()
{
    impl_->pendingLeft.clear();
    impl_->pendingRight.clear();
    impl_->pendingRead = 0;
    impl_->started = false;
}

double PitchStretcher::fill(
    const SampleBuffer& buffer, const StereoBlock& out, double startFrame, double speed, float gain
)
{
    out.clear();
    if (buffer.empty() || !out.valid()) {
        return startFrame;
    }

    speed = std::clamp(speed, MIN_SPEED, MAX_SPEED);

    constexpr double SEEK_TOLERANCE = 4.0;
    const bool discontinuity = !impl_->started || std::fabs(startFrame - impl_->nextPosition) > SEEK_TOLERANCE;

    if (discontinuity) {
        impl_->pendingLeft.clear();
        impl_->pendingRight.clear();
        impl_->pendingRead = 0;
        impl_->request.position = startFrame;
        impl_->request.speed = speed;
        impl_->request.pitch = 1.0;
        impl_->request.reset = true;
        impl_->stretcher.preroll(impl_->request);
        impl_->started = true;
    }
    else {
        impl_->request.speed = speed;
        impl_->request.reset = false;
    }

    const uint32_t frames = out.frames();
    while (impl_->pending() < frames) {
        impl_->grain(buffer);
    }

    for (uint32_t i = 0; i < frames; ++i) {
        out.left[i] = impl_->pendingLeft[impl_->pendingRead + i] * gain;
        out.right[i] = impl_->pendingRight[impl_->pendingRead + i] * gain;
    }

    impl_->pendingRead += frames;
    impl_->dropConsumed();

    impl_->nextPosition = startFrame + static_cast<double>(frames) * speed;
    return impl_->nextPosition;
}

} // namespace imdj
