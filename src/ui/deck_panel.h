#pragma once

#include "ui/ui_context.h"

namespace imdj {

void DrawDeckPanel(const UiContext& ui, int deckIndex);
bool DeckNearingEnd(const AudioEngine& engine, int deckIndex);

} // namespace imdj
