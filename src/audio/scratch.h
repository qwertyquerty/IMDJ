#pragma once

#include <chrono>

namespace imdj {

class ScratchTracker {
public:
    double setTarget(double targetFrame)
    {
        const auto now = Clock::now();
        if (hasPrev_) {
            double dt = std::chrono::duration<double>(now - prevTime_).count();
            if (dt > MIN_DELTA_SEC) {
                double instant = (targetFrame - prevTarget_) / dt;
                smoothed_ = initialized_ ? smoothed_ * (1.0 - SMOOTHING) + instant * SMOOTHING : instant;
                initialized_ = true;
            }
        }
        else {
            smoothed_ = 0.0;
            initialized_ = false;
        }

        prevTarget_ = targetFrame;
        prevTime_ = now;
        hasPrev_ = true;

        return smoothed_;
    }

    double end()
    {
        double rate = smoothed_;
        if (hasPrev_) {
            double idle = std::chrono::duration<double>(Clock::now() - prevTime_).count();
            if (idle > STALE_AFTER_SEC) {
                rate = 0.0;
            }
        }

        hasPrev_ = false;
        return rate;
    }

    void reset()
    {
        hasPrev_ = false;
        initialized_ = false;
        smoothed_ = 0.0;
    }

    double velocity() const { return smoothed_; }

private:
    using Clock = std::chrono::steady_clock;

    static constexpr double SMOOTHING = 0.15;
    static constexpr double MIN_DELTA_SEC = 0.001;
    static constexpr double STALE_AFTER_SEC = 0.03;

    double prevTarget_ = 0.0;
    double smoothed_ = 0.0;
    Clock::time_point prevTime_{};
    bool hasPrev_ = false;
    bool initialized_ = false;
};

} // namespace imdj
