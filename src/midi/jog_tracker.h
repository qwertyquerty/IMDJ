#pragma once

#include <atomic>
#include <optional>

#include "core/units.h"

namespace imdj {

class JogTracker {
public:
    static constexpr double IDLE_TIMEOUT_SEC = 0.015;

    bool idle(double now) const;

    std::optional<Seconds> rotate(double now, int ticks, Seconds deckPosition, Seconds duration, bool scratching);

private:
    static constexpr double DEAD_ZONE_TICKS_PER_SEC = 12.0;
    static constexpr double DEAD_ZONE_FLOOR_TICKS = 1.0;
    static constexpr double SECONDS_PER_TICK = 0.005;
    static constexpr double MAX_PROCESS_HZ = 500.0;
    static constexpr double MIN_PROCESS_INTERVAL_SEC = 1.0 / MAX_PROCESS_HZ;
    static constexpr double FILTER_SMOOTHING = 0.3;

    std::atomic<double> virtualSeconds_{0.0};
    std::atomic<double> lastMessageTime_{0.0};
    std::atomic<double> lastRotateTime_{0.0};

    double pendingTicks_ = 0.0;
    double lastProcessTime_ = 0.0;
    double smoothedDelta_ = 0.0;
};

} // namespace imdj
