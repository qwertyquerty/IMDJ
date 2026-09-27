#pragma once

#include "audio/audio_engine.h"
#include "midi/midi_controller.h"
#include "ui/app_ui_settings.h"
#include "ui/deck_ui_state.h"
#include "ui/notifications.h"
#include "ui/ui_common.h"

namespace imdj {

struct DeckView {
    AudioEngine& engine;
    Deck& deck;
    MidiController& midi;
    DeckUiState& ui;
    int index;
    AppUiSettings& uiSettings;
    Notifications& notifications;

    const char* label() const { return DeckLabel(index); }
};

} // namespace imdj
