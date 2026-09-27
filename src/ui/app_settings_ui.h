#pragma once

#include <string>
#include <vector>

#include "app/app_settings.h"
#include "audio/audio_engine.h"
#include "ui/app_ui_settings.h"

namespace imdj {

struct UiContext;

struct AppSettingsUiState {
    std::vector<AudioDeviceInfo> devices;
    AudioSettings draft;
};

void DrawAudioSettingsPopup(const UiContext& ui);

struct HotkeysUiState {
    bool listening = false;
};

void DrawHotkeysPopup(const UiContext& ui);

} // namespace imdj
