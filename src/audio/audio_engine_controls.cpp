#include "audio/audio_engine.h"

namespace imdj {

void AudioEngine::setMetronomeVolume(float volume)
{
    metronomeVolume_.store(volume);
    settings_.metronomeVolume = metronomeVolume_.load();
}

void AudioEngine::setJogSpinbackDurationSec(float seconds)
{
    jogSpinbackDurationSec_.store(seconds);
    settings_.jogSpinbackDurationSec = jogSpinbackDurationSec_.load();
}

void AudioEngine::jumpToHotCue(int deckIndex)
{
    if (!validDeck(deckIndex)) {
        return;
    }

    Deck& deck = this->deck(deckIndex);
    if (!deck.hotCueSet.load()) {
        return;
    }

    bool wasPlaying = deck.playing.load();
    deck.seek(deck.hotCue());
    if (wasPlaying) {
        alignBeatToOtherDeck(deckIndex);
    }
}

} // namespace imdj
