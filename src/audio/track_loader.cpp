#include "audio/track_loader.h"

#include "audio/loudness.h"
#include "library/audio_tags.h"

namespace imdj {

TrackLoader::TrackLoader() : queue_([this](const Request& request) { return load(request); }) {}

namespace {

bool ValidSlot(int slot) { return slot >= 0 && slot < MAX_DECK_COUNT; }

} // namespace

void TrackLoader::request(int slot, std::string path)
{
    if (!ValidSlot(slot)) {
        return;
    }

    busy_[slot].store(true);
    progress_[slot].store(0.0f);
    queue_.submit({slot, std::move(path)});
}

bool TrackLoader::busy(int slot) const { return ValidSlot(slot) && busy_[slot].load(); }

float TrackLoader::progress(int slot) const { return ValidSlot(slot) ? progress_[slot].load() : 0.0f; }

TrackLoader::Loaded TrackLoader::load(const Request& request)
{
    Loaded loaded;
    loaded.slot = request.slot;
    loaded.path = request.path;

    Status decoded = loaded.buffer.load(request.path, &progress_[request.slot]);
    loaded.error = decoded.error();
    if (decoded) {
        if (!loaded.peaks.load(request.path, loaded.buffer.frameCount())) {
            loaded.peaks = WaveformPeaks::Compute(loaded.buffer);
            loaded.peaks.save(request.path);
        }

        loaded.normalizeGain = NormalizeGainForLufs(MeasureIntegratedLufs(loaded.buffer));
        loaded.hasMetadata = TrackMetadata::Load(request.path, loaded.metadata);
        if (loaded.hasMetadata) {
            loaded.metadata.clampTo(loaded.buffer.frameCount());
        }

        if (ReadAudioTags(request.path, loaded.metadata.info)) {
            loaded.hasMetadata = true;
        }
    }

    progress_[request.slot].store(1.0f);
    busy_[request.slot].store(false);

    return loaded;
}

} // namespace imdj
