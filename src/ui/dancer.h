#pragma once

#include <string>
#include <vector>

#include "core/result.h"
#include "imgui.h"

namespace imdj {

struct DancerInfo {
    std::string path;
    std::string name;
    int beatsPerLoop = 4;
};

std::vector<DancerInfo> ScanDancers(const std::string& dir);
std::string DancerLabel(const DancerInfo& info);

class DancerSprite {
public:
    DancerSprite() = default;
    ~DancerSprite();
    DancerSprite(const DancerSprite&) = delete;
    DancerSprite& operator=(const DancerSprite&) = delete;

    Status load(const DancerInfo& info);
    void unload();
    bool isLoaded() const { return frameCount_ > 0; }

    int width() const { return width_; }
    int height() const { return height_; }

    ImTextureID textureForBeat(double currentBeat);

private:
    unsigned char* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int frameCount_ = 0;
    int beatsPerLoop_ = 4;
    int uploadedFrame_ = -1;
    ImTextureData texture_;
};

} // namespace imdj
