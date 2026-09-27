#pragma once

#include <algorithm>
#include <atomic>
#include <span>
#include <string>
#include <vector>

#include "audio/audio_buffer.h"
#include "audio/audio_types.h"
#include "audio/crossfader.h"
#include "audio/deck.h"
#include "audio/level_meter.h"
#include "audio/preview.h"
#include "audio/ramp.h"
#include "audio/track_loader.h"
#include "core/clamped_atomic.h"
#include "miniaudio.h"
#include "vst/vst_host.h"

namespace imdj {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    Status init(const AudioSettings& settings = AudioSettings{});
    void shutdown();
    bool isInitialized() const { return initialized_; }

    std::vector<AudioDeviceInfo> listPlaybackDevices() const;
    Status applySettings(const AudioSettings& settings);
    const AudioSettings& currentSettings() const { return settings_; }
    bool headphonesConnected() const;

    int deckCount() const { return deckCount_.load(); }
    void setDeckCount(int count);

    std::span<Deck> decks() { return {decks_, static_cast<size_t>(deckCount())}; }
    std::span<const Deck> decks() const { return {decks_, static_cast<size_t>(deckCount())}; }

    Deck& deck(int index) { return decks_[clampIndex(index)]; }
    const Deck& deck(int index) const { return decks_[clampIndex(index)]; }
    bool validDeck(int index) const { return index >= 0 && index < deckCount(); }

    void update(double nowSeconds);

    void loadTrackAsync(int deckIndex, const std::string& path, double seekSeconds = -1.0);
    bool isDeckLoading(int deckIndex) const { return loader_.busy(deckIndex); }
    float deckLoadProgress(int deckIndex) const { return loader_.progress(deckIndex); }
    const std::string& lastLoadError(int deckIndex) const;
    Status saveTrackMetadata(int deckIndex);
    bool hasUnsavedChanges(int deckIndex) const;

    Preview& preview() { return preview_; }
    const Preview& preview() const { return preview_; }

    void setCrossfade(float x);
    float crossfade() const { return crossfade_.load(); }
    float crossfadeGain(int deckIndex) const;
    void setCrossfaderCurve(CrossfaderCurve curve);
    CrossfaderCurve crossfaderCurve() const { return static_cast<CrossfaderCurve>(crossfaderCurve_.load()); }

    void setMasterVolume(float v);
    float masterVolume() const { return masterVolume_.load(); }
    VstChain& masterVstChain() { return masterVstChain_; }

    void startAutoTransition(float target, double durationSeconds);
    bool isTransitioning() const { return crossfadeRamp_.active(); }

    void setCueMaster(bool enabled) { cueMaster_.store(enabled); }
    bool cueMaster() const { return cueMaster_.load(); }

    void alignBeatToOtherDeck(int deckIndex);
    void alignBeatToDeck(int deckIndex, int referenceIndex);
    int tempoReferenceDeck(int deckIndex) const;
    void syncTempoToReferenceDeck(int deckIndex);
    void snapToNearestGrid(int deckIndex);
    void jumpToHotCue(int deckIndex);

    void setSyncEnabled(int deckIndex, bool enabled);
    void toggleSyncEnabled(int deckIndex);
    bool syncEnabled(int deckIndex) const;
    int syncMasterDeck() const;

    void setSnapResolution(SnapResolution r);
    SnapResolution snapResolution() const { return static_cast<SnapResolution>(snapResolution_.load()); }

    void setMetronomeVolume(float volume);
    float metronomeVolume() const { return metronomeVolume_.load(); }
    void setJogSpinbackDurationSec(float seconds);
    float jogSpinbackDurationSec() const { return jogSpinbackDurationSec_.load(); }

    const MasterMeter& masterMeter() const { return masterMeter_; }
    const StereoMeter& cueMeter() const { return cueMeter_; }
    const StereoMeter& deckMeter(int deckIndex) const { return decks_[clampIndex(deckIndex)].dsp.meter; }
    float dspLoad() const { return dspLoad_.load(); }

private:
    int clampIndex(int index) const { return std::clamp(index, 0, deckCount() - 1); }

    static void primaryDataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);
    static void headphonesDataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);

    uint32_t renderBlock(uint32_t frameCount);
    void renderDeck(Deck& deck, const StereoBlock& master, float crossfadeGain);
    void finishDeckBlock(
        Deck& deck, const StereoBlock& master, const StereoBlock& block, double startFrame, double rate,
        float crossfadeGain, uint32_t framesWritten
    );
    void mixCueBus(const StereoBlock& master);
    void pushToHeadphones(uint32_t frameCount);
    void renderHeadphones(float* output, uint32_t frameCount, uint32_t channelCount);

    Status createDevice(const AudioSettings& settings);
    Status createHeadphonesDevice(const AudioSettings& settings);
    void destroyHeadphonesDevice();
    bool resolveDeviceId(const std::string& name, ma_device_id& idOut) const;
    void applySettingsValues(const AudioSettings& settings);
    void updateSync();
    void drainFinishedLoads();
    void installLoadedTrack(TrackLoader::Loaded& loaded);

    ma_context context_{};
    bool contextInitialized_ = false;
    ma_device device_{};
    bool deviceInitialized_ = false;
    bool initialized_ = false;
    AudioSettings settings_;

    RoutingMatrix primaryRouting_;
    RoutingMatrix secondaryRouting_;

    ma_device headphonesDevice_{};
    std::atomic<bool> headphonesReady_{false};
    ma_pcm_rb cueRingBuffer_{};
    bool cueRingBufferInitialized_ = false;
    std::vector<float> headphonesScratch_;

    Deck decks_[MAX_DECK_COUNT];
    Preview preview_;
    TrackLoader loader_;
    std::string loadErrors_[MAX_DECK_COUNT];
    double pendingSeek_[MAX_DECK_COUNT] = {};

    StereoBuffer masterBus_;
    StereoBuffer cueBus_;

    std::atomic<bool> cueMaster_{false};
    std::atomic<int> deckCount_{DEFAULT_DECK_COUNT};
    ClampedAtomic<float> crossfade_{0.5f, 0.0f, 1.0f};
    std::atomic<int> crossfaderCurve_{static_cast<int>(CrossfaderCurve::EqualPower)};
    ClampedAtomic<float> masterVolume_{1.0f, 0.0f, 1.5f};
    SmoothedGain masterGain_;
    ClampedAtomic<float> metronomeVolume_{0.5f, 0.0f, 1.0f};
    ClampedAtomic<float> jogSpinbackDurationSec_{0.35f, 0.05f, 3.0f};
    std::atomic<int> snapResolution_{static_cast<int>(SnapResolution::Quarter)};
    std::atomic<float> dspLoad_{0.0f};

    VstChain masterVstChain_;
    MasterMeter masterMeter_;
    StereoMeter cueMeter_;

    Ramp crossfadeRamp_;
};

} // namespace imdj
