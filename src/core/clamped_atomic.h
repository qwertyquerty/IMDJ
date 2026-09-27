#pragma once

#include <algorithm>
#include <atomic>

namespace imdj {

template <typename T>
class ClampedAtomic {
public:
    constexpr ClampedAtomic(T initial, T minimum, T maximum) : value_(initial), minimum_(minimum), maximum_(maximum) {}

    ClampedAtomic(const ClampedAtomic&) = delete;
    ClampedAtomic& operator=(const ClampedAtomic&) = delete;

    T load() const { return value_.load(); }
    void store(T value) { value_.store(std::clamp(value, minimum_, maximum_)); }

    constexpr T minimum() const { return minimum_; }
    constexpr T maximum() const { return maximum_; }

private:
    std::atomic<T> value_;
    T minimum_;
    T maximum_;
};

} // namespace imdj
