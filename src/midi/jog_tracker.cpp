#include "midi/jog_tracker.h"

#include <algorithm>
#include <cmath>

namespace imdj {

bool JogTracker::idle(double now) const { return now - lastMessageTime_.load() > IDLE_TIMEOUT_SEC; }

std::optional<Seconds> JogTracker::rotate(
    double now, int ticks, Seconds deckPosition, Seconds duration, bool scratching
)
{
    if (idle(now)) {
        lastRotateTime_.store(0.0);
        smoothedDelta_ = 0.0;
        pendingTicks_ = 0.0;
    }

    lastMessageTime_.store(now);
    pendingTicks_ += ticks;

    if (now - lastProcessTime_ < MIN_PROCESS_INTERVAL_SEC) {
        return std::nullopt;
    }

    lastProcessTime_ = now;

    const double batch = pendingTicks_;
    pendingTicks_ = 0.0;
    smoothedDelta_ = smoothedDelta_ * (1.0 - FILTER_SMOOTHING) + batch * FILTER_SMOOTHING;

    const double lastRotate = lastRotateTime_.load();
    const double minimumTicks = lastRotate > 0.0 ? DEAD_ZONE_TICKS_PER_SEC * (now - lastRotate) : DEAD_ZONE_FLOOR_TICKS;
    if (std::abs(smoothedDelta_) < minimumTicks) {
        return std::nullopt;
    }

    lastRotateTime_.store(now);

    double target = (scratching ? virtualSeconds_.load() : deckPosition.count()) + batch * SECONDS_PER_TICK;
    if (duration.count() > 0.0) {
        target = std::clamp(target, 0.0, duration.count());
    }

    virtualSeconds_.store(target);
    return Seconds{target};
}

} // namespace imdj
