#include "audio/audio_engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "audio/crossfader.h"
#include "audio/denormals.h"
#include "audio/sample_player.h"

namespace imdj {

namespace {

constexpr double SCRATCH_CORRECTION_PER_BLOCK = 0.2;

double BlockRate(Deck& deck, uint32_t frames, bool scratching)
{
    if (scratching) {
        double target = deck.scratchTargetFrame.load();
        bool moved = target != deck.dsp.lastScratchTarget;
        deck.dsp.lastScratchTarget = target;

        double base = moved ? deck.scratchVelocity.load() / SAMPLE_RATE : 0.0;
        if (frames == 0) {
            return base;
        }

        double error = target - (deck.position().count() + base * frames);
        return base + (error * SCRATCH_CORRECTION_PER_BLOCK) / frames;
    }

    if (deck.spinbackActive.load()) {
        return deck.spinbackRate.load();
    }

    return std::clamp(static_cast<double>(deck.playbackRate.load()), 0.05, 8.0);
}

} // namespace

void AudioEngine::primaryDataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
{
    auto* engine = static_cast<AudioEngine*>(device->pUserData);
    const uint32_t channels = device->playback.channels;
    float* out = static_cast<float*>(output);
    for (uint32_t done = 0; done < frameCount;) {
        const uint32_t rendered = engine->renderBlock(frameCount - done);
        RouteBuses(
            engine->primaryRouting_,
            out + static_cast<size_t>(done) * channels,
            channels,
            engine->masterBus_.block(rendered),
            engine->cueBus_.block(rendered)
        );
        done += rendered;
    }
}

uint32_t AudioEngine::renderBlock(uint32_t frameCount)
{
    struct DspLoadGuard {
        AudioEngine* engine;
        std::chrono::steady_clock::time_point start;
        uint32_t frames;
        ~DspLoadGuard()
        {
            if (frames == 0) {
                return;
            }

            double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            float load = static_cast<float>(elapsed / (static_cast<double>(frames) / SAMPLE_RATE));
            float prev = engine->dspLoad_.load();
            engine->dspLoad_.store(load > prev ? load : prev * 0.95f + load * 0.05f);
        }
    };

    frameCount = std::min(frameCount, MAX_BLOCK_FRAMES);
    DspLoadGuard guard{this, std::chrono::steady_clock::now(), frameCount};
    ScopedNoDenormals noDenormals;

    StereoBlock master = masterBus_.block(frameCount);
    master.clear();

    const CrossfadeGains crossfade = CrossfadeGainsFor(crossfaderCurve(), crossfade_.load());
    for (Deck& deck : decks()) {
        renderDeck(deck, master, CrossfadeGainForSide(crossfade, deck.side()));
    }

    masterVstChain_.process(
        master.left.data(), master.right.data(), static_cast<int32_t>(frameCount), VstTransportInfo{}
    );

    const GainStep masterVolume = masterGain_.next(masterVolume_.load());
    const float delta = masterVolume.delta(frameCount);
    float gain = masterVolume.from;
    for (uint32_t i = 0; i < frameCount; ++i) {
        gain += delta;
        master.left[i] = std::tanh(master.left[i] * gain);
        master.right[i] = std::tanh(master.right[i] * gain);
    }

    mixCueBus(master);
    StereoBlock cue = cueBus_.block(frameCount);
    preview_.mixInto(cue);
    if (headphonesReady_.load()) {
        pushToHeadphones(frameCount);
    }

    const double blockSeconds = static_cast<double>(frameCount) / SAMPLE_RATE;
    masterMeter_.update(master, blockSeconds);
    cueMeter_.update(cue, blockSeconds);

    return frameCount;
}

void AudioEngine::renderDeck(Deck& deck, const StereoBlock& master, float crossfadeGain)
{
    StereoBlock block = deck.dsp.buffer.block(master.frames());
    const double blockSeconds = static_cast<double>(master.frames()) / SAMPLE_RATE;

    const std::unique_lock lock(deck.renderMutex, std::try_to_lock);
    const bool scratching = deck.scratching.load();
    const bool spinning = deck.spinbackActive.load();
    if (!lock || (!deck.playing.load() && !scratching) || !deck.hasTrack()) {
        block.clear();
        deck.dsp.meter.decay(blockSeconds);

        return;
    }

    const double startFrame = deck.position().count();
    const double rate = BlockRate(deck, block.frames(), scratching);

    const bool keylocked = deck.keylock.load() && !scratching && !spinning && std::fabs(rate - 1.0) > 1e-4;
    if (keylocked) {
        double end = deck.dsp.stretcher.fill(deck.buffer, block, startFrame, rate, deck.effectiveGain());

        const uint64_t loopStart = deck.loopStart.load();
        const uint64_t loopEnd = deck.loopEnd.load();
        const bool looping = deck.loopEnabled.load() && loopEnd > loopStart;
        if (looping && end >= static_cast<double>(loopEnd)) {
            end = static_cast<double>(loopStart);
        }
        else if (end >= static_cast<double>(deck.frameCount())) {
            deck.playing.store(false);
            end = static_cast<double>(deck.frameCount());
        }

        deck.setPosition(Frames{end});
        finishDeckBlock(deck, master, block, startFrame, rate, crossfadeGain, block.frames());

        return;
    }

    deck.dsp.stretcher.reset();

    const PlaybackResult result = ReadBlock(
        deck.buffer,
        block,
        {
            .startFrame = startFrame,
            .rate = rate,
            .gain = deck.effectiveGain(),
            .loop = !scratching && deck.loopEnabled.load(),
            .loopStart = deck.loopStart.load(),
            .loopEnd = deck.loopEnd.load(),
            .stopBeforeStart = scratching || spinning,
        }
    );

    double position = result.endFrame;
    if (result.hitBoundary) {
        if (position < 0.0) {
            position = 0.0;
        }
        else {
            if (!scratching) {
                deck.playing.store(false);
            }

            position = static_cast<double>(deck.frameCount());
        }
    }

    deck.setPosition(Frames{position});

    finishDeckBlock(deck, master, block, startFrame, rate, crossfadeGain, result.framesWritten);
}

void AudioEngine::finishDeckBlock(
    Deck& deck, const StereoBlock& master, const StereoBlock& block, double startFrame, double rate,
    float crossfadeGain, uint32_t framesWritten
)
{
    const StereoBlock written = block.firstFrames(framesWritten);
    const DeckContext context{deck, deck.grid(), startFrame, rate, SAMPLE_RATE, metronomeVolume_.load()};
    deck.dsp.chain.process(written, context);

    deck.dsp.meter.update(written, static_cast<double>(master.frames()) / SAMPLE_RATE);
    master.addFrom(written, deck.dsp.faderGain.next(deck.volume.load() * crossfadeGain));
}

void AudioEngine::mixCueBus(const StereoBlock& master)
{
    StereoBlock cue = cueBus_.block(master.frames());
    cue.clear();

    for (Deck& deck : decks()) {
        if (deck.cue.load()) {
            cue.addFrom(deck.dsp.buffer.block(master.frames()));
        }
    }

    if (cueMaster_.load()) {
        cue.addFrom(master);
    }
}

void AudioEngine::pushToHeadphones(uint32_t frameCount)
{
    const uint32_t channels = headphonesDevice_.playback.channels;
    float* scratch = headphonesScratch_.data();
    RouteBuses(secondaryRouting_, scratch, channels, masterBus_.block(frameCount), cueBus_.block(frameCount));

    uint32_t written = 0;
    while (written < frameCount) {
        ma_uint32 chunk = frameCount - written;
        void* writeBuffer = nullptr;
        if (ma_pcm_rb_acquire_write(&cueRingBuffer_, &chunk, &writeBuffer) != MA_SUCCESS || chunk == 0) {
            break;
        }

        const float* src = scratch + static_cast<size_t>(written) * channels;
        std::copy(src, src + static_cast<size_t>(chunk) * channels, static_cast<float*>(writeBuffer));
        ma_pcm_rb_commit_write(&cueRingBuffer_, chunk);
        written += chunk;
    }
}

void AudioEngine::headphonesDataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
{
    auto* engine = static_cast<AudioEngine*>(device->pUserData);
    engine->renderHeadphones(static_cast<float*>(output), frameCount, device->playback.channels);
}

void AudioEngine::renderHeadphones(float* output, uint32_t frameCount, uint32_t channelCount)
{
    uint32_t remaining = frameCount;
    float* dst = output;
    while (remaining > 0) {
        ma_uint32 chunk = remaining;
        void* readBuffer = nullptr;
        if (ma_pcm_rb_acquire_read(&cueRingBuffer_, &chunk, &readBuffer) != MA_SUCCESS || chunk == 0) {
            break;
        }

        const float* src = static_cast<const float*>(readBuffer);
        std::copy(src, src + static_cast<size_t>(chunk) * channelCount, dst);
        ma_pcm_rb_commit_read(&cueRingBuffer_, chunk);
        dst += static_cast<size_t>(chunk) * channelCount;
        remaining -= chunk;
    }

    if (remaining > 0) {
        std::fill(dst, dst + static_cast<size_t>(remaining) * channelCount, 0.0f);
    }
}

} // namespace imdj
