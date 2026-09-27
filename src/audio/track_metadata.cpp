#include "audio/track_metadata.h"

#include <algorithm>

#include "audio/audio_constants.h"
#include "core/json_io.h"

namespace imdj {

namespace {

constexpr int SIDECAR_FILE_VERSION = 1;

uint64_t SecondsToFrames(double seconds) { return static_cast<uint64_t>(std::max(0.0, seconds) * SAMPLE_RATE); }
double FramesToSeconds(uint64_t frames) { return static_cast<double>(frames) / SAMPLE_RATE; }

} // namespace

std::string TrackMetadata::SidecarPath(const std::string& trackPath) { return trackPath + ".imdj.json"; }

bool TrackMetadata::Load(const std::string& trackPath, TrackMetadata& out)
{
    Json root;
    if (!LoadJsonFile(SidecarPath(trackPath), root) || !root.is_object()) {
        return false;
    }

    TrackMetadata loaded;
    loaded.info.title = JsonValue<std::string>(root, "title", "");
    loaded.info.artist = JsonValue<std::string>(root, "artist", "");
    loaded.info.bpm = std::max(0.0, JsonValue(root, "bpm", 0.0));
    loaded.startFrame = SecondsToFrames(JsonValue(root, "start_seconds", 0.0));
    loaded.info.key = std::clamp(JsonValue(root, "key", -1), -1, 11);

    double hotCueSeconds = JsonValue(root, "hot_cue_seconds", -1.0);
    loaded.hotCueSet = hotCueSeconds >= 0.0;
    loaded.hotCueFrame = loaded.hotCueSet ? SecondsToFrames(hotCueSeconds) : 0;

    for (const Json& entry : JsonArray(root, "markers")) {
        std::string name = JsonValue<std::string>(entry, "name", "");
        if (name.empty()) {
            continue;
        }

        loaded.markers.push_back({std::move(name), SecondsToFrames(JsonValue(entry, "seconds", 0.0))});
    }

    std::ranges::sort(loaded.markers, {}, &Marker::frame);

    out = std::move(loaded);
    return true;
}

Status TrackMetadata::save(const std::string& trackPath) const
{
    Json root;
    if (!info.title.empty()) {
        root["title"] = info.title;
    }

    if (!info.artist.empty()) {
        root["artist"] = info.artist;
    }

    root["bpm"] = info.bpm;
    root["start_seconds"] = FramesToSeconds(startFrame);
    root["key"] = info.key;
    if (hotCueSet) {
        root["hot_cue_seconds"] = FramesToSeconds(hotCueFrame);
    }

    Json entries = Json::array();
    for (const Marker& marker : markers) {
        entries.push_back({{"name", marker.name}, {"seconds", FramesToSeconds(marker.frame)}});
    }

    root["markers"] = std::move(entries);

    return SaveVersionedJson(SidecarPath(trackPath), root, SIDECAR_FILE_VERSION);
}

void TrackMetadata::clampTo(uint64_t frameCount)
{
    startFrame = std::min(startFrame, frameCount);
    hotCueFrame = std::min(hotCueFrame, frameCount);
    for (Marker& marker : markers) {
        marker.frame = std::min(marker.frame, frameCount);
    }
}

bool TrackMetadata::operator==(const TrackMetadata& other) const
{
    return info == other.info && startFrame == other.startFrame && hotCueSet == other.hotCueSet &&
           (!hotCueSet || hotCueFrame == other.hotCueFrame) && markers == other.markers;
}

} // namespace imdj
