#include "audio/sample_player.h"

#include <algorithm>

namespace imdj {

PlaybackResult ReadBlock(const SampleBuffer& buffer, const StereoBlock& out, const PlaybackRequest& request)
{
    out.clear();

    PlaybackResult result;
    result.endFrame = request.startFrame;
    if (buffer.empty() || !out.valid()) {
        return result;
    }

    const double lastFrame = static_cast<double>(buffer.frameCount() - 1);
    const bool loop = request.loop && request.loopEnd > request.loopStart;
    double pos = request.startFrame;

    for (uint32_t i = 0; i < out.frames(); ++i) {
        if (loop && pos >= static_cast<double>(request.loopEnd)) {
            pos = static_cast<double>(request.loopStart);
        }

        if (pos < 0.0) {
            if (request.stopBeforeStart) {
                result.hitBoundary = true;
                break;
            }

            pos += request.rate;
            ++result.framesWritten;
            continue;
        }

        if (pos >= static_cast<double>(buffer.frameCount())) {
            result.hitBoundary = true;
            break;
        }

        uint64_t index = static_cast<uint64_t>(pos);
        double frac = pos - static_cast<double>(index);
        const float* a = buffer.frame(index);
        const float* b = buffer.frame(std::min<uint64_t>(index + 1, static_cast<uint64_t>(lastFrame)));
        out.left[i] = static_cast<float>(a[0] + (b[0] - a[0]) * frac) * request.gain;
        out.right[i] = static_cast<float>(a[1] + (b[1] - a[1]) * frac) * request.gain;

        pos += request.rate;
        ++result.framesWritten;
    }

    result.endFrame = pos;
    return result;
}

} // namespace imdj
