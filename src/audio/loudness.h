#pragma once

#include "audio/sample_buffer.h"

namespace imdj {

constexpr double TARGET_LUFS = -14.0;
constexpr double SILENCE_LUFS = -70.0;

double MeasureIntegratedLufs(const SampleBuffer& buffer);

float NormalizeGainForLufs(double lufs, double targetLufs = TARGET_LUFS);

} // namespace imdj
