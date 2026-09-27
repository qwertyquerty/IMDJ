#include "app/app_settings.h"

#include <algorithm>
#include <array>

#include "app/app_paths.h"
#include "core/json_io.h"

namespace imdj {

template <int SOURCES, int OUTPUTS>
void to_json(Json& json, const ChannelMatrix<SOURCES, OUTPUTS>& matrix)
{
    json = matrix.cell;
}

template <int SOURCES, int OUTPUTS>
void from_json(const Json& json, ChannelMatrix<SOURCES, OUTPUTS>& matrix)
{
    const auto cells = json.get<std::array<std::array<bool, OUTPUTS>, SOURCES>>();
    for (int s = 0; s < SOURCES; ++s) {
        std::ranges::copy(cells[s], matrix.cell[s]);
    }
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    AudioSettings, outputDeviceName, bufferSizeFrames, metronomeVolume, jogSpinbackDurationSec, snapResolution,
    crossfaderCurve, warnBeforeLoadingAudibleDeck, monitoringMode, monitorDeviceName, primaryChannelCount,
    primaryRoutingMatrix, secondaryChannelCount, secondaryRoutingMatrix
)

namespace {

constexpr int SETTINGS_FILE_VERSION = 1;

std::string SettingsFilePath() { return AppDataDir() + "/settings.json"; }

} // namespace

AudioSettings LoadAppSettings()
{
    AudioSettings settings = LoadJsonAs<AudioSettings>(SettingsFilePath());
    settings.metronomeVolume = std::clamp(settings.metronomeVolume, 0.0f, 1.0f);
    settings.jogSpinbackDurationSec = std::clamp(settings.jogSpinbackDurationSec, 0.05f, 3.0f);
    settings.primaryChannelCount = std::clamp(settings.primaryChannelCount, 1, MAX_ROUTING_CHANNELS);
    settings.secondaryChannelCount = std::clamp(settings.secondaryChannelCount, 1, MAX_ROUTING_CHANNELS);

    return settings;
}

Status SaveAppSettings(const AudioSettings& settings)
{
    return SaveVersionedJson(SettingsFilePath(), settings, SETTINGS_FILE_VERSION);
}

} // namespace imdj
