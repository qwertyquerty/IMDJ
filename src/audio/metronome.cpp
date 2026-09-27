#include "audio/metronome.h"

#include <cmath>
#include <numbers>

namespace imdj {

namespace {

constexpr double CLICK_DURATION_SEC = 0.03;
constexpr double CLICK_DECAY_TAU_SEC = 0.009;
constexpr double DOWNBEAT_HZ = 1500.0;
constexpr double BEAT_HZ = 1000.0;

} // namespace

void AddMetronomeClicks(
    const StereoBlock& block, const BeatGrid& grid, double startFrame, double rate, int beatsPerBar, float volume
)
{
    if (!grid.valid() || volume <= 0.0f || beatsPerBar <= 0 || std::fabs(rate) < 1e-6) {
        return;
    }

    for (uint32_t i = 0; i < block.frames(); ++i) {
        double beats = grid.beatAt(startFrame + static_cast<double>(i) * rate);
        double beatFloor = std::floor(beats);
        double sinceBeatSec = (beats - beatFloor) * grid.beatSeconds() / rate;
        double distSec = std::fabs(sinceBeatSec);
        if (distSec >= CLICK_DURATION_SEC) {
            continue;
        }

        long beatIndex = static_cast<long>(beatFloor);
        bool downbeat = ((beatIndex % beatsPerBar) + beatsPerBar) % beatsPerBar == 0;
        double toneHz = downbeat ? DOWNBEAT_HZ : BEAT_HZ;
        double envelope = std::exp(-distSec / CLICK_DECAY_TAU_SEC);
        float click = static_cast<float>(std::sin(2.0 * std::numbers::pi * toneHz * sinceBeatSec) * envelope) * volume;
        block.left[i] += click;
        block.right[i] += click;
    }
}

} // namespace imdj
