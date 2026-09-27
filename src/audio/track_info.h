#pragma once

#include <string>

namespace imdj {

struct TrackInfo {
    std::string title;
    std::string artist;
    double bpm = 0.0;
    int key = -1;

    bool operator==(const TrackInfo& other) const = default;

    std::string label(const std::string& fallback) const
    {
        if (title.empty()) {
            return fallback;
        }

        return artist.empty() ? title : artist + " - " + title;
    }
};

} // namespace imdj
