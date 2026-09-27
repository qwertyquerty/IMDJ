#pragma once

#include <cstdint>

namespace imdj {

constexpr uint32_t SAMPLE_RATE = 48000;
constexpr uint32_t CHANNELS = 2;
constexpr int MAX_DECK_COUNT = 4;
constexpr int DEFAULT_DECK_COUNT = 2;
constexpr int BEATS_PER_BAR = 4;
constexpr uint32_t MAX_BLOCK_FRAMES = 8192;

constexpr float MIN_PLAYBACK_RATE = 0.25f;
constexpr float MAX_PLAYBACK_RATE = 4.0f;
constexpr double MIN_BPM = 20.0;
constexpr double MAX_BPM = 300.0;

constexpr char DeckLetter(int deckIndex) { return static_cast<char>('A' + deckIndex); }

} // namespace imdj
