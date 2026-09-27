#pragma once

#include <string>
#include <vector>

namespace imdj {

struct UiContext;

struct MidiControllerUiState {
    std::vector<std::string> ports;
    int selectedPort = -1;
    int fxDraftChain[2] = {0, 0};
    int fxDraftPlugin[2] = {0, 0};
};

void DrawMidiControllerPopup(const UiContext& ui);

} // namespace imdj
