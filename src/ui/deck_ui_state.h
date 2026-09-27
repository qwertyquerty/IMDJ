#pragma once

#include <vector>

#include <string>

#include "ui/dancer.h"

namespace imdj {

struct DeckUiState {
    std::vector<double> tapTimes;
    std::string reportedLoadError;
    std::string metadataPath;
    char titleBuf[256] = {};
    char artistBuf[256] = {};
    DancerSprite dancer;
    int selectedDancerIndex = -1;
    char markerNameBuf[64] = {};
    float rampTargetBpm = 128.0f;
    float rampDurationSeconds = 8.0f;
};

} // namespace imdj
