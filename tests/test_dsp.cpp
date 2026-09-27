#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <vector>

#include "audio/audio_buffer.h"
#include "audio/audio_constants.h"
#include "audio/beat_grid.h"
#include "audio/crossfader.h"
#include "audio/eq.h"
#include "audio/filter.h"
#include "audio/routing.h"

using namespace imdj;

namespace {

StereoBuffer MakeBuffer(uint32_t frames)
{
    StereoBuffer buffer;
    buffer.resize(frames);

    return buffer;
}

void FillSine(const StereoBlock& block, double hz, float amplitude)
{
    for (uint32_t i = 0; i < block.frames(); ++i) {
        float value = amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / SAMPLE_RATE));
        block.left[i] = value;
        block.right[i] = value;
    }
}

float Rms(const StereoBlock& block)
{
    double sum = 0.0;
    for (uint32_t i = 0; i < block.frames(); ++i) {
        sum += static_cast<double>(block.left[i]) * block.left[i];
    }

    return static_cast<float>(std::sqrt(sum / std::max<uint32_t>(1, block.frames())));
}

} // namespace

TEST_CASE("BeatGrid quantizes to the nearest grid point")
{
    BeatGrid grid{120.0, 1000.0, SAMPLE_RATE};

    CHECK(grid.quantize(1000.0 + 24000.0 * 0.4) == doctest::Approx(1000.0));
    CHECK(grid.quantize(1000.0 + 24000.0 * 0.6) == doctest::Approx(1000.0 + 24000.0));
    CHECK(grid.quantize(1000.0 + 24000.0 * 2.1, 2.0) == doctest::Approx(1000.0 + 48000.0));
    CHECK(grid.quantize(0.0) >= 0.0);
}

TEST_CASE("BeatGrid phase stays inside the period")
{
    BeatGrid grid{128.0, 0.0, SAMPLE_RATE};

    CHECK(grid.phaseAt(grid.frameAt(2.25)) == doctest::Approx(0.25));
    CHECK(grid.phaseAt(grid.frameAt(-0.25)) == doctest::Approx(0.75));
    CHECK(grid.phaseAt(grid.frameAt(5.5), 4.0) == doctest::Approx(1.5));

    BeatGrid invalid;
    CHECK_FALSE(invalid.valid());
    CHECK(invalid.beatAt(1000.0) == doctest::Approx(0.0));
}

TEST_CASE("Crossfader curves behave at the endpoints and center")
{
    CrossfadeGains equalLeft = CrossfadeGainsFor(CrossfaderCurve::EqualPower, 0.0f);
    CHECK(equalLeft.a == doctest::Approx(1.0f));
    CHECK(equalLeft.b == doctest::Approx(0.0f));

    CrossfadeGains equalCenter = CrossfadeGainsFor(CrossfaderCurve::EqualPower, 0.5f);
    CHECK(equalCenter.a == doctest::Approx(0.70710678f).epsilon(0.001));
    CHECK(equalCenter.a * equalCenter.a + equalCenter.b * equalCenter.b == doctest::Approx(1.0f).epsilon(0.001));

    CrossfadeGains linear = CrossfadeGainsFor(CrossfaderCurve::Linear, 0.25f);
    CHECK(linear.a == doctest::Approx(0.75f));
    CHECK(linear.b == doctest::Approx(0.25f));

    CrossfadeGains sharp = CrossfadeGainsFor(CrossfaderCurve::Sharp, 0.5f);
    CHECK(sharp.a == doctest::Approx(1.0f));
    CHECK(sharp.b == doctest::Approx(1.0f));
    CHECK(CrossfadeGainsFor(CrossfaderCurve::Sharp, 1.0f).a == doctest::Approx(0.0f));
}

TEST_CASE("One knob filter attenuates the side it is sweeping away")
{
    StereoBuffer buffer = MakeBuffer(4800);
    OneKnobFilter filter;

    StereoBlock block = buffer.block(4800);
    FillSine(block, 6000.0, 0.5f);
    float before = Rms(block);

    filter.process(block, -0.9f, SAMPLE_RATE);
    CHECK(Rms(block) < before * 0.2f);

    filter.reset();
    FillSine(block, 40.0, 0.5f);
    before = Rms(block);
    filter.process(block, 0.9f, SAMPLE_RATE);
    CHECK(Rms(block) < before * 0.2f);
}

TEST_CASE("One knob filter is transparent in the dead zone")
{
    StereoBuffer buffer = MakeBuffer(1024);
    OneKnobFilter filter;

    StereoBlock block = buffer.block(1024);
    FillSine(block, 1000.0, 0.5f);
    float before = Rms(block);

    filter.process(block, 0.0f, SAMPLE_RATE);
    CHECK(Rms(block) == doctest::Approx(before));
}

TEST_CASE("Three band EQ boosts and cuts its bands")
{
    StereoBuffer buffer = MakeBuffer(4800);
    StereoBlock block = buffer.block(4800);

    ThreeBandEq eq;
    FillSine(block, 60.0, 0.25f);
    float before = Rms(block);
    eq.process(block, {.low = ThreeBandEq::MAX_GAIN_DB}, SAMPLE_RATE);
    CHECK(Rms(block) > before * 1.5f);

    eq.reset();
    FillSine(block, 10000.0, 0.25f);
    before = Rms(block);
    eq.process(block, {.high = -ThreeBandEq::MAX_GAIN_DB}, SAMPLE_RATE);
    CHECK(Rms(block) < before * 0.7f);
}

TEST_CASE("Gain changes ramp across the block")
{
    StereoBuffer sourceBuffer = MakeBuffer(4);
    StereoBuffer targetBuffer = MakeBuffer(4);
    StereoBlock source = sourceBuffer.block(4);
    StereoBlock target = targetBuffer.block(4);
    for (uint32_t i = 0; i < 4; ++i) {
        source.left[i] = 1.0f;
        source.right[i] = 1.0f;
    }

    SmoothedGain gain{0.0f};
    target.addFrom(source, gain.next(1.0f));
    CHECK(target.left[0] == doctest::Approx(0.25f));
    CHECK(target.left[1] == doctest::Approx(0.5f));
    CHECK(target.right[3] == doctest::Approx(1.0f));

    target.clear();
    target.addFrom(source, gain.next(1.0f));
    CHECK(target.left[0] == doctest::Approx(1.0f));
}

TEST_CASE("Routing matrix sums sources onto channels")
{
    StereoBuffer masterBuffer = MakeBuffer(4);
    StereoBuffer cueBuffer = MakeBuffer(4);
    StereoBlock master = masterBuffer.block(4);
    StereoBlock cue = cueBuffer.block(4);

    for (uint32_t i = 0; i < 4; ++i) {
        master.left[i] = 0.5f;
        master.right[i] = 0.25f;
        cue.left[i] = -0.5f;
        cue.right[i] = -0.25f;
    }

    RoutingMatrix direct = RoutingMatrix::Identity();
    std::vector<float> out(4 * 4, 99.0f);
    RouteBuses(direct, out.data(), 4, master, cue);

    CHECK(out[0] == doctest::Approx(0.5f));
    CHECK(out[1] == doctest::Approx(0.25f));
    CHECK(out[2] == doctest::Approx(-0.5f));
    CHECK(out[3] == doctest::Approx(-0.25f));

    RoutingMatrix cueOnly = CueOnlyRouting();
    CHECK(cueOnly.usesSource(ROUTE_CUE_L));
    CHECK_FALSE(cueOnly.usesSource(ROUTE_MASTER_L));

    cueOnly.muteSource(ROUTE_CUE_L);
    CHECK_FALSE(cueOnly.usesSource(ROUTE_CUE_L));
}

TEST_CASE("Routing matrix zeroes channels nothing is assigned to")
{
    StereoBuffer masterBuffer = MakeBuffer(2);
    StereoBuffer cueBuffer = MakeBuffer(2);
    StereoBlock master = masterBuffer.block(2);
    StereoBlock cue = cueBuffer.block(2);
    master.clear();
    cue.clear();

    RoutingMatrix matrix;
    std::vector<float> out(2 * 4, 99.0f);
    RouteBuses(matrix, out.data(), 4, master, cue);
    for (float value : out) {
        CHECK(value == doctest::Approx(0.0f));
    }
}

namespace {
bool BeatsCoincide(double sinceBeat, double beatLength, double otherSinceBeat, double otherBeatLength)
{
    for (int m = -8; m <= 8; ++m) {
        const double beat = -sinceBeat + m * beatLength;
        const double offset = std::fmod(beat + otherSinceBeat, otherBeatLength);
        if (std::fabs(offset) < 1e-9 || std::fabs(std::fabs(offset) - otherBeatLength) < 1e-9) {
            return true;
        }
    }

    return false;
}
} // namespace

TEST_CASE("Nearest beat shift lands on a shared beat when tempos differ")
{
    const double tempos[][2] = {{204.0, 102.0}, {102.0, 204.0}, {128.0, 64.0}, {174.0, 87.0}, {120.0, 120.0}};
    for (const auto& pair : tempos) {
        const double beatLength = 60.0 / pair[0];
        const double otherBeatLength = 60.0 / pair[1];
        for (double a = 0.0; a < 1.0; a += 0.07) {
            for (double b = 0.0; b < 1.0; b += 0.11) {
                const double sinceBeat = a * beatLength;
                const double otherSinceBeat = b * otherBeatLength;
                const double shift = NearestBeatShift(sinceBeat, beatLength, otherSinceBeat, otherBeatLength);
                INFO(pair[0] << " vs " << pair[1] << " at " << a << ", " << b);
                CHECK(std::fabs(shift) <= std::min(beatLength, otherBeatLength) * 0.5 + 1e-12);
                CHECK(BeatsCoincide(sinceBeat + shift, beatLength, otherSinceBeat, otherBeatLength));
            }
        }
    }
}
