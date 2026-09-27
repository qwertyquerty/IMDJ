#include "audio/loudness.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include "audio/audio_constants.h"
#include "audio/ma_filter.h"

namespace imdj {

namespace {

constexpr int STEPS_PER_BLOCK = 4;
constexpr double BLOCK_STEP_SECONDS = 0.100;
constexpr double RELATIVE_GATE_LU = 10.0;
constexpr float MIN_GAIN = 0.25f;
constexpr float MAX_GAIN = 4.0f;

ma_biquad_config ShelvingStage(double sampleRate)
{
    const double f0 = 1681.974450955533;
    const double gainDb = 3.999843853973347;
    const double q = 0.7071752369554196;

    double k = std::tan(std::numbers::pi * f0 / sampleRate);
    double vh = std::pow(10.0, gainDb / 20.0);
    double vb = std::pow(vh, 0.4996667741545416);

    return ma_biquad_config_init(
        ma_format_f32,
        CHANNELS,
        vh + vb * k / q + k * k,
        2.0 * (k * k - vh),
        vh - vb * k / q + k * k,
        1.0 + k / q + k * k,
        2.0 * (k * k - 1.0),
        1.0 - k / q + k * k
    );
}

ma_biquad_config HighPassStage(double sampleRate)
{
    const double f0 = 38.13547087602444;
    const double q = 0.5003270373238773;

    double k = std::tan(std::numbers::pi * f0 / sampleRate);
    double a0 = 1.0 + k / q + k * k;

    return ma_biquad_config_init(
        ma_format_f32, CHANNELS, 1.0, -2.0, 1.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0
    );
}

double LoudnessOfMeanSquare(double meanSquare)
{
    if (meanSquare <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }

    return -0.691 + 10.0 * std::log10(meanSquare);
}

std::vector<double> WeightedStepSums(const SampleBuffer& buffer, uint64_t stepFrames)
{
    Biquad shelf(CHANNELS);
    Biquad highPass(CHANNELS);
    shelf.reinit(ShelvingStage(SAMPLE_RATE));
    highPass.reinit(HighPassStage(SAMPLE_RATE));

    std::vector<float> weighted(static_cast<size_t>(stepFrames * CHANNELS));
    std::vector<double> steps;
    steps.reserve(static_cast<size_t>(buffer.frameCount() / stepFrames));

    for (uint64_t start = 0; start + stepFrames <= buffer.frameCount(); start += stepFrames) {
        shelf.process(weighted.data(), buffer.frame(start), stepFrames);
        highPass.process(weighted);

        double sum = 0.0;
        for (float value : weighted) {
            sum += static_cast<double>(value) * value;
        }

        steps.push_back(sum);
    }

    return steps;
}

} // namespace

double MeasureIntegratedLufs(const SampleBuffer& buffer)
{
    if (buffer.empty()) {
        return SILENCE_LUFS;
    }

    const uint64_t stepFrames = static_cast<uint64_t>(BLOCK_STEP_SECONDS * SAMPLE_RATE);
    const uint64_t blockFrames = stepFrames * STEPS_PER_BLOCK;
    if (buffer.frameCount() < blockFrames) {
        return SILENCE_LUFS;
    }

    const std::vector<double> steps = WeightedStepSums(buffer, stepFrames);
    if (steps.size() < STEPS_PER_BLOCK) {
        return SILENCE_LUFS;
    }

    std::vector<double> blocks;
    blocks.reserve(steps.size());
    for (size_t i = 0; i + STEPS_PER_BLOCK <= steps.size(); ++i) {
        double sum = 0.0;
        for (int step = 0; step < STEPS_PER_BLOCK; ++step) {
            sum += steps[i + step];
        }

        blocks.push_back(sum / static_cast<double>(blockFrames));
    }

    auto gatedMeanSquare = [&blocks](double thresholdLufs) {
        double sum = 0.0;
        size_t counted = 0;
        for (double meanSquare : blocks) {
            if (LoudnessOfMeanSquare(meanSquare) <= thresholdLufs) {
                continue;
            }

            sum += meanSquare;
            ++counted;
        }

        return counted > 0 ? sum / static_cast<double>(counted) : 0.0;
    };

    const double absoluteGated = gatedMeanSquare(SILENCE_LUFS);
    if (absoluteGated <= 0.0) {
        return SILENCE_LUFS;
    }

    const double relativeThreshold = LoudnessOfMeanSquare(absoluteGated) - RELATIVE_GATE_LU;
    const double relativeGated = gatedMeanSquare(std::max(SILENCE_LUFS, relativeThreshold));
    if (relativeGated <= 0.0) {
        return SILENCE_LUFS;
    }

    return LoudnessOfMeanSquare(relativeGated);
}

float NormalizeGainForLufs(double lufs, double targetLufs)
{
    if (!std::isfinite(lufs) || lufs <= SILENCE_LUFS) {
        return 1.0f;
    }

    return std::clamp(static_cast<float>(std::pow(10.0, (targetLufs - lufs) / 20.0)), MIN_GAIN, MAX_GAIN);
}

} // namespace imdj
