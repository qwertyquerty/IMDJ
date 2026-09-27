#include "audio/audio_engine.h"
#include "core/strings.h"

namespace imdj {

namespace {

std::string FileNameOf(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

void AudioEngine::loadTrackAsync(int deckIndex, const std::string& path, double seekSeconds)
{
    if (!validDeck(deckIndex)) {
        return;
    }

    loadErrors_[deckIndex].clear();
    pendingSeek_[deckIndex] = seekSeconds;
    loader_.request(deckIndex, path);
}

const std::string& AudioEngine::lastLoadError(int deckIndex) const
{
    static const std::string EMPTY_STRING;
    return validDeck(deckIndex) ? loadErrors_[deckIndex] : EMPTY_STRING;
}

void AudioEngine::drainFinishedLoads()
{
    for (TrackLoader::Loaded& loaded : loader_.takeCompleted()) {
        if (!validDeck(loaded.slot)) {
            continue;
        }

        if (!loaded.ok()) {
            loadErrors_[loaded.slot] = loaded.error;
            continue;
        }

        installLoadedTrack(loaded);
    }
}

void AudioEngine::installLoadedTrack(TrackLoader::Loaded& loaded)
{
    if (!validDeck(loaded.slot)) {
        return;
    }

    Deck* deck = &this->deck(loaded.slot);

    deck->cancelScratch();
    deck->playing.store(false);
    deck->clearLoop();

    deck->retiredBuffer = std::move(deck->buffer);
    deck->buffer = std::move(loaded.buffer);
    deck->peaks = std::move(loaded.peaks);
    deck->filePath = loaded.path;
    deck->fileName = FileNameOf(loaded.path);

    deck->setPosition(Frames{0.0});
    deck->playbackRate.store(1.0f);
    deck->bpm.store(120.0);
    deck->normalizeGain.store(loaded.normalizeGain);
    deck->normalizeEnabled.store(true);
    deck->dsp.chain.reset();

    deck->applyMetadata(loaded.hasMetadata ? loaded.metadata : TrackMetadata{});
    deck->saved = deck->metadata();

    if (pendingSeek_[loaded.slot] >= 0.0) {
        deck->seek(Seconds{pendingSeek_[loaded.slot]});
        pendingSeek_[loaded.slot] = -1.0;
    }

    loadErrors_[loaded.slot].clear();
}

Status AudioEngine::saveTrackMetadata(int deckIndex)
{
    Deck& deck = this->deck(deckIndex);
    if (!deck.hasTrack() || deck.filePath.empty()) {
        return Status::Fail(TrFormat("error.no_track"));
    }

    TrackMetadata meta = deck.metadata();
    if (Status saved = meta.save(deck.filePath); !saved) {
        return saved;
    }

    deck.saved = std::move(meta);
    return Status::Ok();
}

bool AudioEngine::hasUnsavedChanges(int deckIndex) const
{
    const Deck& deck = this->deck(deckIndex);
    return validDeck(deckIndex) && deck.hasTrack() && deck.metadata() != deck.saved;
}

} // namespace imdj
