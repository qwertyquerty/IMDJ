#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "audio/track_info.h"
#include "core/result.h"

namespace imdj {

struct Marker {
    std::string name;
    uint64_t frame = 0;

    bool operator==(const Marker& other) const { return name == other.name && frame == other.frame; }
};

struct TrackMetadata {
    TrackInfo info;
    uint64_t startFrame = 0;
    bool hotCueSet = false;
    uint64_t hotCueFrame = 0;
    std::vector<Marker> markers;

    static std::string SidecarPath(const std::string& trackPath);

    static bool Load(const std::string& trackPath, TrackMetadata& out);
    Status save(const std::string& trackPath) const;

    void clampTo(uint64_t frameCount);

    bool operator==(const TrackMetadata& other) const;
    bool operator!=(const TrackMetadata& other) const { return !(*this == other); }
};

} // namespace imdj
