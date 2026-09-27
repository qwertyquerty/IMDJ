#include "ui/app_ui_settings.h"

#include <algorithm>

#include "app/app_paths.h"
#include "core/json_io.h"

namespace imdj {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    AppUiSettings, theme, focusSearchHotkey, uiScale, language, songFolders, collapsedSections
)

namespace {

constexpr int UI_SETTINGS_FILE_VERSION = 1;

std::string UiSettingsFilePath() { return AppDataDir() + "/ui_settings.json"; }

} // namespace

AppUiSettings LoadAppUiSettings()
{
    AppUiSettings settings = LoadJsonAs<AppUiSettings>(UiSettingsFilePath());
    settings.uiScale = std::clamp(settings.uiScale, MIN_UI_SCALE, MAX_UI_SCALE);

    return settings;
}

Status SaveAppUiSettings(const AppUiSettings& settings)
{
    return SaveVersionedJson(UiSettingsFilePath(), settings, UI_SETTINGS_FILE_VERSION);
}

} // namespace imdj
