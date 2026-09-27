#pragma once

#include <string>
#include <vector>

namespace imdj {

struct UiContext;

struct SessionPresetUiState {
    std::vector<std::string> presetNames;
    char nameBuffer[128] = "";
    std::string pendingOverwriteName;
    bool restoreTracks = true;
};

void DrawSessionPresetsPopup(const UiContext& ui);

} // namespace imdj
