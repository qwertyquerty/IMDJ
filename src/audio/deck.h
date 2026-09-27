#pragma once

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "audio/audio_buffer.h"
#include "audio/audio_constants.h"
#include "audio/beat_grid.h"
#include "audio/crossfader.h"
#include "audio/deck_processor.h"
#include "audio/eq.h"
#include "audio/level_meter.h"
#include "audio/pitch_stretcher.h"
#include "audio/ramp.h"
#include "audio/sample_buffer.h"
#include "audio/scratch.h"
#include "audio/track_metadata.h"
#include "audio/waveform.h"
#include "core/clamped_atomic.h"
#include "core/units.h"
#include "vst/vst_host.h"

namespace imdj {

struct Deck {
    SampleBuffer buffer;
    // The audio thread only try-locks this; the UI holds it while swapping the loaded track.
    std::mutex renderMutex;
    WaveformPeaks peaks;
    std::string filePath;
    std::string fileName;
    TrackInfo info;
    std::vector<Marker> markers;
    TrackMetadata saved;

    std::atomic<double> playFrame{0.0};
    std::atomic<bool> playing{false};

    std::atomic<bool> loopEnabled{false};
    std::atomic<uint64_t> loopStart{0};
    std::atomic<uint64_t> loopEnd{0};
    std::atomic<bool> loopInSet{false};
    std::atomic<bool> loopOutSet{false};
    std::atomic<bool> quantizeLoops{true};

    ClampedAtomic<float> volume{1.0f, 0.0f, 1.0f};
    ClampedAtomic<float> gain{1.0f, 0.0f, 2.0f};
    ClampedAtomic<float> pan{0.0f, -1.0f, 1.0f};
    ClampedAtomic<float> eqLow{0.0f, -ThreeBandEq::MAX_GAIN_DB, ThreeBandEq::MAX_GAIN_DB};
    ClampedAtomic<float> eqMid{0.0f, -ThreeBandEq::MAX_GAIN_DB, ThreeBandEq::MAX_GAIN_DB};
    ClampedAtomic<float> eqHigh{0.0f, -ThreeBandEq::MAX_GAIN_DB, ThreeBandEq::MAX_GAIN_DB};
    ClampedAtomic<float> filter{0.0f, -1.0f, 1.0f};

    ClampedAtomic<float> normalizeGain{1.0f, 0.0f, 8.0f};
    std::atomic<bool> normalizeEnabled{true};

    ClampedAtomic<float> playbackRate{1.0f, MIN_PLAYBACK_RATE, MAX_PLAYBACK_RATE};
    std::atomic<bool> spinbackActive{false};
    std::atomic<double> spinbackRate{0.0};

    ClampedAtomic<double> bpm{120.0, MIN_BPM, MAX_BPM};
    std::atomic<uint64_t> startFrame{0};

    std::atomic<bool> cueHeld{false};
    std::atomic<bool> hotCueSet{false};
    std::atomic<uint64_t> hotCueFrame{0};

    std::atomic<bool> metronomeEnabled{false};
    std::atomic<bool> syncEnabled{false};
    std::atomic<bool> keylock{false};
    std::atomic<bool> cue{false};
    std::atomic<int> crossfaderSide{static_cast<int>(CrossfaderSide::None)};

    std::atomic<bool> scratching{false};
    std::atomic<int> pendingMarkerJump{-1};
    std::atomic<double> scratchTargetFrame{0.0};
    std::atomic<double> scratchVelocity{0.0};

    VstChain vstChain;

    struct Dsp {
        StereoBuffer buffer;
        DeckChain chain;
        PitchStretcher stretcher;
        StereoMeter meter;
        SmoothedGain faderGain{0.0f};
        double lastScratchTarget = 0.0;
    } dsp;

    Ramp tempoRamp;
    Ramp spinbackRamp;
    ScratchTracker scratchTracker;

    const std::string& displayTitle() const { return info.title.empty() ? fileName : info.title; }

    bool hasTrack() const { return !buffer.empty(); }
    uint64_t frameCount() const { return buffer.frameCount(); }
    Seconds duration() const { return Seconds{buffer.durationSeconds()}; }
    double durationSeconds() const { return buffer.durationSeconds(); }

    Frames position() const { return Frames{playFrame.load()}; }
    Seconds positionSeconds() const { return position().toSeconds(); }
    void setPosition(Frames frame) { playFrame.store(frame.count()); }

    Frames positionInTrack() const
    {
        return Frames{std::clamp(playFrame.load(), 0.0, static_cast<double>(frameCount()))};
    }

    BeatGrid grid() const { return BeatGrid{bpm.load(), static_cast<double>(startFrame.load()), SAMPLE_RATE}; }
    CrossfaderSide side() const { return static_cast<CrossfaderSide>(crossfaderSide.load()); }
    EqGainsDb eqGains() const { return EqGainsDb{eqLow.load(), eqMid.load(), eqHigh.load()}; }
    float effectiveGain() const { return gain.load() * (normalizeEnabled.load() ? normalizeGain.load() : 1.0f); }
    double effectiveBpm() const { return bpm.load() * playbackRate.load(); }

    void setVolume(float value) { volume.store(value); }
    void setGain(float value) { gain.store(value); }
    void setPan(float value) { pan.store(value); }
    void setEqLow(float gainDb) { eqLow.store(gainDb); }
    void setEqMid(float gainDb) { eqMid.store(gainDb); }
    void setEqHigh(float gainDb) { eqHigh.store(gainDb); }
    void setEq(const EqGainsDb& gains);
    void setFilter(float value) { filter.store(value); }
    void setNormalizeEnabled(bool enabled) { normalizeEnabled.store(enabled); }
    void setKeylock(bool enabled) { keylock.store(enabled); }
    void setMetronomeEnabled(bool enabled) { metronomeEnabled.store(enabled); }
    void setQuantizeLoops(bool enabled) { quantizeLoops.store(enabled); }
    void setCue(bool enabled) { cue.store(enabled); }
    void toggleCue() { cue.store(!cue.load()); }
    void setCrossfaderSide(CrossfaderSide value) { crossfaderSide.store(static_cast<int>(value)); }
    void setKey(int value) { info.key = std::clamp(value, -1, 11); }
    void setBpm(double value);
    void setPlaybackRate(float rate) { playbackRate.store(rate); }
    void setTargetBpm(double targetBpm);

    void play();
    void pause() { playing.store(false); }
    void togglePlay();
    void stop();
    void cuePress();
    void cueRelease();
    void seek(Seconds seconds);

    void setLoopEnabled(bool enabled) { loopEnabled.store(enabled); }
    void setLoopInHere();
    void setLoopOutHere();
    void setLoopLengthBeats(double beats);
    void setLoopToWholeTrack();
    void clearLoop();
    void halveLoop() { resizeLoop(0.5); }
    void doubleLoop() { resizeLoop(2.0); }
    void shiftLoop(int direction);
    void resizeLoop(double factor);

    void setStartHere() { startFrame.store(positionInTrack().index()); }
    void clearStart() { startFrame.store(0); }
    Seconds startPoint() const { return Frames{static_cast<double>(startFrame.load())}.toSeconds(); }

    void addMarkerHere(const std::string& name = std::string());
    void removeMarker(size_t index);
    void renameMarker(size_t index, const std::string& name);
    void jumpToMarker(size_t index);

    void setHotCue();
    Seconds hotCue() const { return Frames{static_cast<double>(hotCueFrame.load())}.toSeconds(); }

    Beats currentBeat() const { return Beats{grid().beatAt(playFrame.load())}; }
    double beatPhase() const { return currentBeat().fractional(); }

    void setScratchTarget(Seconds seconds);
    double endScratch();
    void cancelScratch();
    void startSpinback(double startRate, double durationSeconds);
    void startTempoRamp(double targetBpm, double durationSeconds);

    TrackMetadata metadata() const;
    void applyMetadata(const TrackMetadata& meta);
};

} // namespace imdj
