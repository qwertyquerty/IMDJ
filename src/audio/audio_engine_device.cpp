#include "audio/audio_engine.h"
#include "core/strings.h"

#include <algorithm>

namespace imdj {

namespace {

ma_device_config PlaybackConfig(
    int channelCount, ma_uint32 bufferSizeFrames, ma_device_data_proc callback, void* userData
)
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = static_cast<ma_uint32>(std::clamp(channelCount, 1, MAX_ROUTING_CHANNELS));
    config.sampleRate = SAMPLE_RATE;
    config.periodSizeInFrames = bufferSizeFrames;
    config.dataCallback = callback;
    config.pUserData = userData;

    return config;
}

} // namespace

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() { shutdown(); }

Status AudioEngine::init(const AudioSettings& settings)
{
    if (initialized_) {
        return Status::Ok();
    }

    masterBus_.resize(MAX_BLOCK_FRAMES);
    cueBus_.resize(MAX_BLOCK_FRAMES);
    headphonesScratch_.assign(static_cast<size_t>(MAX_BLOCK_FRAMES) * MAX_ROUTING_CHANNELS, 0.0f);
    for (Deck& deck : decks_) {
        deck.dsp.buffer.resize(MAX_BLOCK_FRAMES);
        deck.dsp.stretcher.prepare(MAX_BLOCK_FRAMES);
        if (deck.dsp.chain.size() == 0) {
            BuildDefaultDeckChain(deck.dsp.chain);
        }
    }

    decks_[0].setCrossfaderSide(CrossfaderSide::A);
    decks_[1].setCrossfaderSide(CrossfaderSide::B);
    for (double& seek : pendingSeek_) {
        seek = -1.0;
    }

    preview_.prepare(MAX_BLOCK_FRAMES);

    if (ma_context_init(nullptr, 0, nullptr, &context_) != MA_SUCCESS) {
        return Status::Fail(TrFormat("error.audio_backend"));
    }

    contextInitialized_ = true;

    if (settings.monitoringMode == MonitoringMode::SeparateOutputDevices) {
        createHeadphonesDevice(settings);
    }

    if (Status created = createDevice(settings); !created) {
        destroyHeadphonesDevice();
        ma_context_uninit(&context_);
        contextInitialized_ = false;

        return created;
    }

    applySettingsValues(settings);
    initialized_ = true;

    return Status::Ok();
}

Status AudioEngine::applySettings(const AudioSettings& settings)
{
    if (!initialized_) {
        return init(settings);
    }

    if (deviceInitialized_) {
        ma_device_uninit(&device_);
        deviceInitialized_ = false;
    }

    destroyHeadphonesDevice();
    if (settings.monitoringMode == MonitoringMode::SeparateOutputDevices) {
        createHeadphonesDevice(settings);
    }

    if (Status created = createDevice(settings); !created) {
        initialized_ = false;
        return created;
    }

    applySettingsValues(settings);
    return Status::Ok();
}

void AudioEngine::applySettingsValues(const AudioSettings& settings)
{
    settings_ = settings;
    metronomeVolume_.store(std::clamp(settings.metronomeVolume, 0.0f, 1.0f));
    jogSpinbackDurationSec_.store(std::clamp(settings.jogSpinbackDurationSec, 0.05f, 3.0f));
    snapResolution_.store(static_cast<int>(settings.snapResolution));
    crossfaderCurve_.store(static_cast<int>(settings.crossfaderCurve));
}

Status AudioEngine::createDevice(const AudioSettings& settings)
{
    ma_device_config config = PlaybackConfig(
        settings.primaryChannelCount, settings.bufferSizeFrames, &AudioEngine::primaryDataCallback, this
    );
    ma_device_id resolvedId{};
    if (resolveDeviceId(settings.outputDeviceName, resolvedId)) {
        config.playback.pDeviceID = &resolvedId;
    }

    if (ma_device_init(&context_, &config, &device_) != MA_SUCCESS) {
        return Status::Fail(TrFormat("error.audio_device_open"));
    }

    deviceInitialized_ = true;

    primaryRouting_ = settings.primaryRoutingMatrix;
    if (settings.monitoringMode == MonitoringMode::Disabled) {
        primaryRouting_.muteSource(ROUTE_CUE_L);
        primaryRouting_.muteSource(ROUTE_CUE_R);
    }

    if (ma_device_start(&device_) != MA_SUCCESS) {
        ma_device_uninit(&device_);
        deviceInitialized_ = false;

        return Status::Fail(TrFormat("error.audio_device_start"));
    }

    return Status::Ok();
}

Status AudioEngine::createHeadphonesDevice(const AudioSettings& settings)
{
    ma_device_config config = PlaybackConfig(
        settings.secondaryChannelCount, settings.bufferSizeFrames, &AudioEngine::headphonesDataCallback, this
    );
    ma_device_id resolvedId{};
    if (resolveDeviceId(settings.monitorDeviceName, resolvedId)) {
        config.playback.pDeviceID = &resolvedId;
    }

    if (ma_device_init(&context_, &config, &headphonesDevice_) != MA_SUCCESS) {
        return Status::Fail(TrFormat("error.headphones_open"));
    }

    secondaryRouting_ = settings.secondaryRoutingMatrix;

    if (cueRingBufferInitialized_) {
        ma_pcm_rb_uninit(&cueRingBuffer_);
        cueRingBufferInitialized_ = false;
    }

    if (ma_pcm_rb_init(ma_format_f32, config.playback.channels, SAMPLE_RATE / 2, nullptr, nullptr, &cueRingBuffer_) !=
        MA_SUCCESS) {
        ma_device_uninit(&headphonesDevice_);
        return Status::Fail(TrFormat("error.headphones_buffer"));
    }
    cueRingBufferInitialized_ = true;

    if (ma_device_start(&headphonesDevice_) != MA_SUCCESS) {
        ma_device_uninit(&headphonesDevice_);
        ma_pcm_rb_uninit(&cueRingBuffer_);
        cueRingBufferInitialized_ = false;

        return Status::Fail(TrFormat("error.headphones_start"));
    }

    headphonesReady_.store(true);
    return Status::Ok();
}

void AudioEngine::destroyHeadphonesDevice()
{
    if (headphonesReady_.load()) {
        headphonesReady_.store(false);
        ma_device_uninit(&headphonesDevice_);
    }

    if (cueRingBufferInitialized_) {
        ma_pcm_rb_uninit(&cueRingBuffer_);
        cueRingBufferInitialized_ = false;
    }
}

bool AudioEngine::resolveDeviceId(const std::string& name, ma_device_id& idOut) const
{
    if (name.empty()) {
        return false;
    }

    for (const AudioDeviceInfo& info : listPlaybackDevices()) {
        if (info.name == name) {
            idOut = info.id;
            return true;
        }
    }

    return false;
}

bool AudioEngine::headphonesConnected() const
{
    switch (settings_.monitoringMode) {
        case MonitoringMode::DualOutputDevice: return deviceInitialized_;
        case MonitoringMode::SeparateOutputDevices: return deviceInitialized_ || headphonesReady_.load();
        default: return false;
    }
}

std::vector<AudioDeviceInfo> AudioEngine::listPlaybackDevices() const
{
    std::vector<AudioDeviceInfo> result;
    if (!contextInitialized_) {
        return result;
    }

    ma_device_info* infos = nullptr;
    ma_uint32 count = 0;
    ma_context* context = const_cast<ma_context*>(&context_);
    if (ma_context_get_devices(context, &infos, &count, nullptr, nullptr) != MA_SUCCESS) {
        return result;
    }

    result.reserve(count);
    for (ma_uint32 i = 0; i < count; ++i) {
        AudioDeviceInfo info;
        info.name = infos[i].name;
        info.id = infos[i].id;
        info.isDefault = infos[i].isDefault != 0;

        ma_device_info detail{};
        if (ma_context_get_device_info(context, ma_device_type_playback, &infos[i].id, &detail) == MA_SUCCESS) {
            ma_uint32 maxChannels = 0;
            for (ma_uint32 f = 0; f < detail.nativeDataFormatCount; ++f) {
                maxChannels = std::max(maxChannels, detail.nativeDataFormats[f].channels);
            }

            info.channelCount = maxChannels;
        }

        result.push_back(std::move(info));
    }

    return result;
}

void AudioEngine::shutdown()
{
    if (deviceInitialized_) {
        ma_device_uninit(&device_);
        deviceInitialized_ = false;
    }

    destroyHeadphonesDevice();
    if (contextInitialized_) {
        ma_context_uninit(&context_);
        contextInitialized_ = false;
    }

    initialized_ = false;
}

} // namespace imdj
