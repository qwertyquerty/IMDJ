#include "audio/deck.h"
#include "core/strings.h"

#include <algorithm>

namespace imdj {

void Deck::setEq(const EqGainsDb& gains)
{
    eqLow.store(gains.low);
    eqMid.store(gains.mid);
    eqHigh.store(gains.high);
}

void Deck::setBpm(double value)
{
    if (value < MIN_BPM || value > MAX_BPM) {
        return;
    }

    bpm.store(value);
}

void Deck::setTargetBpm(double targetBpm)
{
    if (targetBpm <= 0.0 || bpm.load() <= 0.0) {
        return;
    }

    setPlaybackRate(static_cast<float>(targetBpm / bpm.load()));
}

void Deck::play()
{
    if (!hasTrack()) {
        return;
    }

    cueHeld.store(false);
    if (playFrame.load() >= static_cast<double>(frameCount())) {
        setPosition(Frames{static_cast<double>(startFrame.load())});
    }

    playing.store(true);
}

void Deck::togglePlay()
{
    if (cueHeld.load()) {
        cueHeld.store(false);
        return;
    }

    if (playing.load()) {
        pause();
    }
    else {
        play();
    }
}

void Deck::stop()
{
    cancelScratch();
    cueHeld.store(false);
    playing.store(false);
    setPosition(Frames{static_cast<double>(startFrame.load())});
}

void Deck::cuePress()
{
    if (!hasTrack()) {
        return;
    }

    cancelScratch();
    if (playing.load()) {
        playing.store(false);
        setPosition(Frames{static_cast<double>(startFrame.load())});

        return;
    }

    if (positionInTrack().index() != startFrame.load()) {
        setPosition(Frames{static_cast<double>(startFrame.load())});
    }

    cueHeld.store(true);
    playing.store(true);
}

void Deck::cueRelease()
{
    if (!cueHeld.load()) {
        return;
    }

    cueHeld.store(false);
    playing.store(false);
    setPosition(Frames{static_cast<double>(startFrame.load())});
}

void Deck::seek(Seconds seconds)
{
    if (!hasTrack()) {
        return;
    }

    cancelScratch();
    setPosition(Seconds{std::clamp(seconds.count(), 0.0, durationSeconds())}.toFrames());
}

void Deck::setLoopInHere()
{
    uint64_t position = positionInTrack().index();
    if (quantizeLoops.load()) {
        position = static_cast<uint64_t>(grid().quantize(static_cast<double>(position)));
    }

    loopStart.store(position);
    loopInSet.store(true);
    if (loopEnd.load() <= position) {
        loopEnd.store(frameCount());
    }
}

void Deck::setLoopOutHere()
{
    uint64_t position = positionInTrack().index();
    if (quantizeLoops.load()) {
        position = static_cast<uint64_t>(grid().quantize(static_cast<double>(position)));
    }

    if (position <= loopStart.load()) {
        return;
    }

    loopEnd.store(position);
    loopOutSet.store(true);
    loopEnabled.store(true);
}

void Deck::setLoopLengthBeats(double beats)
{
    if (!hasTrack()) {
        return;
    }

    const BeatGrid beatGrid = grid();
    if (!beatGrid.valid()) {
        return;
    }

    uint64_t start = positionInTrack().index();
    if (quantizeLoops.load()) {
        start = static_cast<uint64_t>(beatGrid.quantize(static_cast<double>(start)));
    }

    uint64_t length = std::max<uint64_t>(static_cast<uint64_t>(beats * beatGrid.beatFrames()), 1);

    loopStart.store(start);
    loopEnd.store(std::min<uint64_t>(start + length, frameCount()));
    loopInSet.store(true);
    loopOutSet.store(true);
    loopEnabled.store(true);
}

void Deck::setLoopToWholeTrack()
{
    if (!hasTrack()) {
        return;
    }

    uint64_t start = std::min(startFrame.load(), frameCount());
    if (frameCount() <= start) {
        return;
    }

    loopStart.store(start);
    loopEnd.store(frameCount());
    loopInSet.store(true);
    loopOutSet.store(true);
    loopEnabled.store(true);
}

void Deck::resizeLoop(double factor)
{
    if (!loopInSet.load() || !loopOutSet.load()) {
        return;
    }

    const uint64_t start = loopStart.load();
    const uint64_t length =
        std::max<uint64_t>(static_cast<uint64_t>(static_cast<double>(loopEnd.load() - start) * factor), 1);
    loopEnd.store(std::min<uint64_t>(start + length, frameCount()));
}

void Deck::shiftLoop(int direction)
{
    if (!loopInSet.load() || !loopOutSet.load()) {
        return;
    }

    const uint64_t start = loopStart.load();
    const uint64_t end = loopEnd.load();
    const uint64_t length = end - start;
    if (length == 0) {
        return;
    }

    if (direction < 0 && start < length) {
        return;
    }

    if (direction > 0 && end + length > frameCount()) {
        return;
    }

    const uint64_t newStart = direction < 0 ? start - length : start + length;
    const uint64_t position = positionInTrack().index();
    loopStart.store(newStart);
    loopEnd.store(newStart + length);
    if (loopEnabled.load() && position >= start && position < end) {
        setPosition(Frames{static_cast<double>(direction < 0 ? position - length : position + length)});
    }
}

void Deck::clearLoop()
{
    loopEnabled.store(false);
    loopInSet.store(false);
    loopOutSet.store(false);
    loopStart.store(0);
    loopEnd.store(0);
}

void Deck::addMarkerHere(const std::string& name)
{
    if (!hasTrack()) {
        return;
    }

    std::string label = name.empty() ? TrFormat("deck.marker_default", markers.size() + 1) : name;
    markers.push_back({std::move(label), positionInTrack().index()});
    std::ranges::sort(markers, {}, &Marker::frame);
}

void Deck::removeMarker(size_t index)
{
    if (index >= markers.size()) {
        return;
    }

    markers.erase(markers.begin() + static_cast<long>(index));
}

void Deck::renameMarker(size_t index, const std::string& name)
{
    if (name.empty() || index >= markers.size()) {
        return;
    }

    markers[index].name = name;
}

void Deck::jumpToMarker(size_t index)
{
    if (index >= markers.size()) {
        return;
    }

    seek(Frames{static_cast<double>(markers[index].frame)}.toSeconds());
}

void Deck::setHotCue()
{
    if (!hasTrack()) {
        return;
    }

    hotCueFrame.store(static_cast<uint64_t>(grid().quantize(static_cast<double>(positionInTrack().index()))));
    hotCueSet.store(true);
}

void Deck::setScratchTarget(Seconds seconds)
{
    if (!hasTrack()) {
        return;
    }

    double targetFrame = Seconds{std::clamp(seconds.count(), 0.0, durationSeconds())}.toFrames().count();
    scratchVelocity.store(scratchTracker.setTarget(targetFrame));
    scratchTargetFrame.store(targetFrame);
    scratching.store(true);
}

double Deck::endScratch()
{
    scratching.store(false);
    return scratchTracker.end();
}

void Deck::cancelScratch()
{
    scratching.store(false);
    spinbackActive.store(false);
    spinbackRamp.cancel();
    scratchTracker.reset();
}

void Deck::startSpinback(double startRate, double durationSeconds)
{
    spinbackRamp.start(startRate, playbackRate.load(), std::max(0.05, durationSeconds));
    spinbackRate.store(startRate);
    spinbackActive.store(true);
}

void Deck::startTempoRamp(double targetBpm, double durationSeconds)
{
    if (targetBpm <= 0.0 || bpm.load() <= 0.0) {
        return;
    }

    double target = std::clamp(
        targetBpm / bpm.load(), static_cast<double>(MIN_PLAYBACK_RATE), static_cast<double>(MAX_PLAYBACK_RATE)
    );
    syncEnabled.store(false);
    tempoRamp.start(playbackRate.load(), target, durationSeconds);
}

TrackMetadata Deck::metadata() const
{
    TrackMetadata meta;
    meta.info = info;
    meta.info.bpm = bpm.load();
    meta.startFrame = startFrame.load();
    meta.hotCueSet = hotCueSet.load();
    meta.hotCueFrame = hotCueFrame.load();
    meta.markers = markers;

    return meta;
}

void Deck::applyMetadata(const TrackMetadata& meta)
{
    if (meta.info.bpm > 0.0) {
        bpm.store(meta.info.bpm);
    }

    startFrame.store(meta.startFrame);
    setPosition(Frames{static_cast<double>(meta.startFrame)});
    info = meta.info;
    hotCueSet.store(meta.hotCueSet);
    hotCueFrame.store(meta.hotCueFrame);
    markers = meta.markers;
}

} // namespace imdj
