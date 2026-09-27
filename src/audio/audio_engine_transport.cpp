#include "audio/audio_engine.h"

#include <algorithm>

namespace imdj {

void AudioEngine::setCrossfade(float x) { crossfade_.store(x); }

float AudioEngine::crossfadeGain(int deckIndex) const
{
    if (!validDeck(deckIndex)) {
        return 0.0f;
    }

    CrossfadeGains gains = CrossfadeGainsFor(crossfaderCurve(), crossfade_.load());
    return CrossfadeGainForSide(gains, deck(deckIndex).side());
}

void AudioEngine::setCrossfaderCurve(CrossfaderCurve curve)
{
    crossfaderCurve_.store(static_cast<int>(curve));
    settings_.crossfaderCurve = curve;
}

void AudioEngine::setMasterVolume(float v) { masterVolume_.store(v); }

void AudioEngine::startAutoTransition(float target, double durationSeconds)
{
    crossfadeRamp_.start(crossfade_.load(), std::clamp(target, 0.0f, 1.0f), durationSeconds);
}

void AudioEngine::setDeckCount(int count)
{
    count = count > 2 ? MAX_DECK_COUNT : 2;
    const int previous = deckCount();
    for (int i = count; i < previous; ++i) {
        decks_[i].playing.store(false);
        decks_[i].cue.store(false);
        decks_[i].syncEnabled.store(false);
    }

    deckCount_.store(count);
}

void AudioEngine::alignBeatToOtherDeck(int deckIndex) { alignBeatToDeck(deckIndex, tempoReferenceDeck(deckIndex)); }

void AudioEngine::alignBeatToDeck(int deckIndex, int referenceIndex)
{
    if (!validDeck(deckIndex) || !validDeck(referenceIndex) || deckIndex == referenceIndex) {
        return;
    }

    Deck* deck = &this->deck(deckIndex);
    const Deck* other = &this->deck(referenceIndex);
    if (!deck->hasTrack() || !other->hasTrack()) {
        return;
    }

    const BeatGrid grid = deck->grid();
    const BeatGrid otherGrid = other->grid();
    if (!grid.valid() || !otherGrid.valid()) {
        return;
    }

    deck->cancelScratch();

    const double period = SnapResolutionBeats(snapResolution());
    const double rate = deck->playbackRate.load();
    const double otherRate = other->playbackRate.load();
    const double beatLength = period * grid.beatSeconds() / rate;
    const double otherBeatLength = period * otherGrid.beatSeconds() / otherRate;
    const double sinceBeat = grid.phaseAt(deck->position().count(), period) * grid.beatSeconds() / rate;
    const double otherSinceBeat =
        otherGrid.phaseAt(other->position().count(), period) * otherGrid.beatSeconds() / otherRate;

    const double shiftSeconds = NearestBeatShift(sinceBeat, beatLength, otherSinceBeat, otherBeatLength);

    double target = deck->position().count() + shiftSeconds * rate * grid.sampleRate;
    deck->setPosition(Frames{std::min(target, static_cast<double>(deck->frameCount()))});
}

int AudioEngine::tempoReferenceDeck(int deckIndex) const
{
    const int master = syncMasterDeck();
    if (master >= 0 && master != deckIndex) {
        return master;
    }

    for (int i = 0; i < deckCount(); ++i) {
        if (i != deckIndex && decks_[i].hasTrack()) {
            return i;
        }
    }

    return -1;
}

void AudioEngine::syncTempoToReferenceDeck(int deckIndex)
{
    const int reference = tempoReferenceDeck(deckIndex);
    if (validDeck(deckIndex) && reference >= 0) {
        deck(deckIndex).setTargetBpm(decks_[reference].effectiveBpm());
    }
}

void AudioEngine::snapToNearestGrid(int deckIndex)
{
    if (!validDeck(deckIndex)) {
        return;
    }

    Deck& deck = this->deck(deckIndex);
    if (!deck.hasTrack()) {
        return;
    }

    deck.cancelScratch();

    double snapped = deck.grid().quantize(
        static_cast<double>(deck.positionInTrack().index()), SnapResolutionBeats(snapResolution())
    );
    deck.setPosition(Frames{std::min(snapped, static_cast<double>(deck.frameCount()))});
}

bool AudioEngine::syncEnabled(int deckIndex) const
{
    return validDeck(deckIndex) && deck(deckIndex).syncEnabled.load();
}

void AudioEngine::toggleSyncEnabled(int deckIndex) { setSyncEnabled(deckIndex, !syncEnabled(deckIndex)); }

void AudioEngine::setSyncEnabled(int deckIndex, bool enabled)
{
    if (!validDeck(deckIndex)) {
        return;
    }

    Deck* deck = &this->deck(deckIndex);
    deck->syncEnabled.store(enabled);
    if (!enabled) {
        return;
    }

    const int master = syncMasterDeck();
    if (master < 0 || master == deckIndex) {
        return;
    }

    deck->setTargetBpm(decks_[master].effectiveBpm());
    if (deck->playing.load() && decks_[master].playing.load()) {
        alignBeatToDeck(deckIndex, master);
    }
}

int AudioEngine::syncMasterDeck() const
{
    int idle = -1;
    for (int i = 0; i < deckCount(); ++i) {
        const Deck& deck = decks_[i];
        if (!deck.hasTrack() || deck.bpm.load() <= 0.0) {
            continue;
        }

        if (deck.syncEnabled.load()) {
            continue;
        }

        if (deck.playing.load()) {
            return i;
        }

        if (idle < 0) {
            idle = i;
        }
    }

    return idle;
}

void AudioEngine::updateSync()
{
    constexpr double PHASE_GAIN = 0.5;
    constexpr double MAX_CORRECTION = 0.02;

    const int master = syncMasterDeck();
    if (master < 0) {
        return;
    }

    const Deck& reference = decks_[master];
    const double masterBpm = reference.effectiveBpm();
    if (masterBpm <= 0.0) {
        return;
    }

    for (int i = 0; i < deckCount(); ++i) {
        if (i == master) {
            continue;
        }

        Deck& deck = decks_[i];
        if (!deck.syncEnabled.load() || !deck.hasTrack() || deck.bpm.load() <= 0.0) {
            continue;
        }

        double rate = masterBpm / deck.bpm.load();
        if (deck.playing.load() && reference.playing.load() && !deck.scratching.load()) {
            double error =
                reference.grid().phaseAt(reference.position().count()) - deck.grid().phaseAt(deck.position().count());
            if (error > 0.5) {
                error -= 1.0;
            }

            if (error <= -0.5) {
                error += 1.0;
            }

            rate *= 1.0 + std::clamp(error * PHASE_GAIN, -MAX_CORRECTION, MAX_CORRECTION);
        }

        deck.setPlaybackRate(static_cast<float>(rate));
    }
}

void AudioEngine::update(double nowSeconds)
{
    drainFinishedLoads();
    preview_.poll();
    updateSync();

    if (crossfadeRamp_.active()) {
        setCrossfade(static_cast<float>(crossfadeRamp_.update(nowSeconds)));
    }

    for (Deck& deck : decks()) {
        if (deck.tempoRamp.active()) {
            deck.setPlaybackRate(static_cast<float>(deck.tempoRamp.update(nowSeconds)));
        }

        if (deck.spinbackRamp.active()) {
            deck.spinbackRate.store(deck.spinbackRamp.update(nowSeconds));
            if (!deck.spinbackRamp.active()) {
                deck.spinbackActive.store(false);
            }
        }
    }
}

void AudioEngine::setSnapResolution(SnapResolution r)
{
    snapResolution_.store(static_cast<int>(r));
    settings_.snapResolution = r;
}

} // namespace imdj
