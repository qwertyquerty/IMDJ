#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include "core/strings.h"

namespace imdj {

enum class CrossfaderSide { A, None, B };
constexpr int CROSSFADER_SIDE_COUNT = 3;

inline const char* CrossfaderSideLabel(CrossfaderSide side)
{
    switch (side) {
        case CrossfaderSide::A: return "A";
        case CrossfaderSide::B: return "B";
        case CrossfaderSide::None: break;
    }

    return Tr("crossfader.thru");
}

enum class CrossfaderCurve { EqualPower, Linear, Sharp };
constexpr int CROSSFADER_CURVE_COUNT = 3;

inline const char* CrossfaderCurveLabel(CrossfaderCurve curve)
{
    switch (curve) {
        case CrossfaderCurve::Linear: return Tr("crossfader.linear");
        case CrossfaderCurve::Sharp: return Tr("crossfader.sharp");
        case CrossfaderCurve::EqualPower: break;
    }

    return Tr("crossfader.equal_power");
}

struct CrossfadeGains {
    float a = 1.0f;
    float b = 1.0f;
};

inline float CrossfadeGainForSide(const CrossfadeGains& gains, CrossfaderSide side)
{
    switch (side) {
        case CrossfaderSide::A: return gains.a;
        case CrossfaderSide::B: return gains.b;
        case CrossfaderSide::None: break;
    }

    return 1.0f;
}

inline CrossfadeGains CrossfadeGainsFor(CrossfaderCurve curve, float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    switch (curve) {
        case CrossfaderCurve::Linear: return {1.0f - x, x};
        case CrossfaderCurve::Sharp: return {std::min(1.0f, 2.0f * (1.0f - x)), std::min(1.0f, 2.0f * x)};
        case CrossfaderCurve::EqualPower: break;
    }

    constexpr float HALF_PI = std::numbers::pi_v<float> / 2.0f;
    return {std::cos(x * HALF_PI), std::sin(x * HALF_PI)};
}

} // namespace imdj
