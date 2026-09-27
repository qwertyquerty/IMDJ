#pragma once

#include <string>

#include "audio/audio_constants.h"
#include "audio/crossfader.h"
#include "audio/routing.h"
#include "miniaudio.h"
#include "core/strings.h"

namespace imdj {

struct AudioDeviceInfo {
    std::string name;
    ma_device_id id{};
    bool isDefault = false;
    ma_uint32 channelCount = 0;
};

enum class MonitoringMode {
    Disabled,
    DualOutputDevice,
    SeparateOutputDevices,
};

constexpr int MONITORING_MODE_COUNT = 3;

enum class SnapResolution { Eighth, Quarter, Half, Bar, TwoBar };

constexpr int SNAP_RESOLUTION_COUNT = 5;

constexpr double SnapResolutionBeats(SnapResolution r)
{
    switch (r) {
        case SnapResolution::Eighth: return 0.5;
        case SnapResolution::Quarter: return 1.0;
        case SnapResolution::Half: return 2.0;
        case SnapResolution::Bar: return static_cast<double>(BEATS_PER_BAR);
        case SnapResolution::TwoBar: return static_cast<double>(BEATS_PER_BAR) * 2.0;
    }

    return 1.0;
}

inline const char* SnapResolutionLabel(SnapResolution r)
{
    switch (r) {
        case SnapResolution::Eighth: return "1/8";
        case SnapResolution::Quarter: return "1/4";
        case SnapResolution::Half: return "1/2";
        case SnapResolution::Bar: return Tr("snap.bar");
        case SnapResolution::TwoBar: return Tr("snap.two_bar");
    }

    return "?";
}

struct AudioSettings {
    std::string outputDeviceName;
    ma_uint32 bufferSizeFrames = 0;
    float metronomeVolume = 0.5f;
    float jogSpinbackDurationSec = 0.35f;

    SnapResolution snapResolution = SnapResolution::Quarter;
    CrossfaderCurve crossfaderCurve = CrossfaderCurve::EqualPower;
    bool warnBeforeLoadingAudibleDeck = true;

    MonitoringMode monitoringMode = MonitoringMode::Disabled;
    std::string monitorDeviceName;

    int primaryChannelCount = 2;
    RoutingMatrix primaryRoutingMatrix = RoutingMatrix::Identity();

    int secondaryChannelCount = 2;
    RoutingMatrix secondaryRoutingMatrix = CueOnlyRouting();
};

} // namespace imdj
