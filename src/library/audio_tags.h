#pragma once

#include <string>

#include "audio/track_info.h"

namespace imdj {

bool ReadAudioTags(const std::string& utf8Path, TrackInfo& info);

} // namespace imdj
