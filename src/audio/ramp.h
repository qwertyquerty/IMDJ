#pragma once

#include <algorithm>

namespace imdj {

class Ramp {
public:
    void start(double from, double to, double durationSeconds)
    {
        from_ = from;
        to_ = to;
        value_ = from;
        duration_ = std::max(0.001, durationSeconds);
        startTime_ = -1.0;
        active_ = true;
    }

    void cancel() { active_ = false; }
    bool active() const { return active_; }
    double value() const { return value_; }

    double update(double nowSeconds)
    {
        if (!active_) {
            return value_;
        }

        if (startTime_ < 0.0) {
            startTime_ = nowSeconds;
        }

        double t = std::clamp((nowSeconds - startTime_) / duration_, 0.0, 1.0);
        if (t >= 1.0) {
            active_ = false;
        }

        double smooth = t * t * (3.0 - 2.0 * t);
        value_ = from_ + (to_ - from_) * smooth;

        return value_;
    }

private:
    bool active_ = false;
    double from_ = 0.0;
    double to_ = 0.0;
    double value_ = 0.0;
    double duration_ = 1.0;
    double startTime_ = -1.0;
};

} // namespace imdj
