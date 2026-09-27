#pragma once

#include <cmath>
#include <compare>
#include <cstdint>

#include "audio/audio_constants.h"

namespace imdj {

template <typename Derived>
class Unit {
public:
    constexpr Unit() = default;
    constexpr explicit Unit(double value) : value_(value) {}

    constexpr double count() const { return value_; }

    constexpr Derived operator+(Derived other) const { return Derived{value_ + other.count()}; }
    constexpr Derived operator-(Derived other) const { return Derived{value_ - other.count()}; }
    constexpr Derived operator*(double scale) const { return Derived{value_ * scale}; }
    constexpr auto operator<=>(const Unit&) const = default;

private:
    double value_ = 0.0;
};

class Seconds;

class Frames : public Unit<Frames> {
public:
    using Unit::Unit;

    constexpr uint64_t index() const { return count() > 0.0 ? static_cast<uint64_t>(count()) : 0; }
    constexpr Seconds toSeconds() const;
};

class Seconds : public Unit<Seconds> {
public:
    using Unit::Unit;

    constexpr Frames toFrames() const { return Frames{count() * SAMPLE_RATE}; }
};

class Beats : public Unit<Beats> {
public:
    using Unit::Unit;

    double fractional() const { return count() - std::floor(count()); }
};

constexpr Seconds Frames::toSeconds() const { return Seconds{count() / SAMPLE_RATE}; }

constexpr Frames FramesOf(uint64_t frames) { return Frames{static_cast<double>(frames)}; }

} // namespace imdj
