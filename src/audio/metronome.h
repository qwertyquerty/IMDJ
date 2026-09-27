#pragma once

#include "audio/audio_buffer.h"
#include "audio/beat_grid.h"

namespace imdj {

void AddMetronomeClicks(
    const StereoBlock& block, const BeatGrid& grid, double startFrame, double rate, int beatsPerBar, float volume
);

} // namespace imdj
