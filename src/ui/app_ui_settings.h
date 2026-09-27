#pragma once

#include <string>
#include <vector>

#include "core/result.h"

namespace imdj {

struct AppUiSettings {
    std::string theme;
    int focusSearchHotkey = -1;
    float uiScale = 1.0f;
    std::string language = "en";
    std::vector<std::string> songFolders;
    std::vector<std::string> collapsedSections{
        "deck.section.metadata", "deck.section.beat_align", "deck.section.effects"
    };
};

constexpr float MIN_UI_SCALE = 0.75f;
constexpr float MAX_UI_SCALE = 2.0f;

AppUiSettings LoadAppUiSettings();
Status SaveAppUiSettings(const AppUiSettings& settings);

} // namespace imdj
