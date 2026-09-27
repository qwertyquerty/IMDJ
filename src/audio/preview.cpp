#include "audio/preview.h"

#include <algorithm>

#include "audio/audio_constants.h"
#include "audio/sample_player.h"

namespace imdj {

void Preview::prepare(uint32_t maxBlockFrames) { scratch_.resize(maxBlockFrames); }

void Preview::start(const std::string& utf8Path)
{
    playing_.store(false);
    lastError_.clear();
    pendingPath_ = utf8Path;
    loader_.request(SLOT, utf8Path);
}

void Preview::stop()
{
    playing_.store(false);
    pendingPath_.clear();
}

void Preview::poll()
{
    for (TrackLoader::Loaded& loaded : loader_.takeCompleted()) {
        if (!loaded.ok()) {
            lastError_ = loaded.error;
            pendingPath_.clear();
            continue;
        }

        if (loaded.path != pendingPath_) {
            continue;
        }

        playing_.store(false);
        retiredBuffer_ = std::move(buffer_);
        buffer_ = std::move(loaded.buffer);
        path_ = loaded.path;
        pendingPath_.clear();
        frame_.store(0.0);
        playing_.store(true);
    }
}

bool Preview::isPlaying(const std::string& utf8Path) const { return playing_.load() && path_ == utf8Path; }

bool Preview::isLoading(const std::string& utf8Path) const { return pendingPath_ == utf8Path; }

double Preview::positionSeconds() const { return frame_.load() / SAMPLE_RATE; }

double Preview::durationSeconds() const { return buffer_.durationSeconds(); }

void Preview::setVolume(float volume) { volume_.store(std::clamp(volume, 0.0f, 1.5f)); }

void Preview::mixInto(const StereoBlock& cue)
{
    if (!playing_.load() || !cue.valid()) {
        return;
    }

    StereoBlock block = scratch_.block(cue.frames());
    PlaybackResult result = ReadBlock(
        buffer_,
        block,
        {
            .startFrame = frame_.load(),
            .gain = volume_.load(),
        }
    );

    cue.addFrom(block);
    frame_.store(result.endFrame);
    if (result.hitBoundary) {
        playing_.store(false);
    }
}

} // namespace imdj
