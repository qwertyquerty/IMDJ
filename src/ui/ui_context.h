#pragma once

#include <array>
#include <vector>

#include "audio/audio_engine.h"
#include "library/song_library.h"
#include "midi/midi_controller.h"
#include "ui/app_settings_ui.h"
#include "ui/app_ui_settings.h"
#include "ui/bindings_ui.h"
#include "ui/dancer.h"
#include "ui/deck_ui_state.h"
#include "ui/deck_view.h"
#include "ui/midi_controller_ui.h"
#include "ui/notifications.h"
#include "ui/session_preset_ui.h"
#include "ui/vst_library_ui.h"

namespace imdj {

struct UiContext {
    AudioEngine& engine;
    MidiController& midi;
    SongLibrary& library;
    VstLibraryUiState& vstLibrary;
    std::array<DeckUiState, MAX_DECK_COUNT>& deckUi;
    const std::vector<DancerInfo>& dancers;

    AppUiSettings& uiSettings;
    AppSettingsUiState& settingsUi;
    MidiControllerUiState& midiUi;
    BindingsUiState& bindingsUi;
    SessionPresetUiState& presetsUi;
    HotkeysUiState& hotkeysUi;
    Notifications& notifications;

    Deck& deck(int index) const { return engine.deck(index); }
    int deckCount() const { return engine.deckCount(); }

    DeckView deckView(int index) const
    {
        return DeckView{engine, engine.deck(index), midi, deckUi[index], index, uiSettings, notifications};
    }
};

} // namespace imdj
