#pragma once

#include "audio/audio_types.h"
#include "core/result.h"

namespace imdj {

AudioSettings LoadAppSettings();
Status SaveAppSettings(const AudioSettings& settings);

} // namespace imdj
