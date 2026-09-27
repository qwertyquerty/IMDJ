#include "ui/dancer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "backends/imgui_impl_opengl3.h"
#include "core/crash_guard.h"
#include "core/paths.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr int DEFAULT_BEATS_PER_LOOP = 4;

int BeatsFromName(std::string_view stem)
{
    const size_t underscore = stem.find_last_of('_');
    if (underscore == std::string_view::npos || stem.size() < underscore + 3 ||
        std::tolower(static_cast<unsigned char>(stem.back())) != 'b') {
        return DEFAULT_BEATS_PER_LOOP;
    }

    const char* first = stem.data() + underscore + 1;
    const char* last = stem.data() + stem.size() - 1;
    int beats = 0;
    const auto [end, error] = std::from_chars(first, last, beats);

    return error == std::errc{} && end == last && beats > 0 ? beats : DEFAULT_BEATS_PER_LOOP;
}

} // namespace

std::vector<DancerInfo> ScanDancers(const std::string& dir)
{
    namespace fs = std::filesystem;
    std::vector<DancerInfo> dancers;
    std::error_code ec;
    for (fs::directory_iterator it(PathFromUtf8(dir), fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end;
         it.increment(ec)) {
        if (it->is_regular_file(ec) && HasExtension(it->path(), {".gif"})) {
            const std::string stem = PathToUtf8(it->path().stem());
            dancers.push_back({PathToUtf8(it->path()), stem, BeatsFromName(stem)});
        }
    }

    std::ranges::sort(dancers, {}, &DancerInfo::path);
    return dancers;
}

std::string DancerLabel(const DancerInfo& info)
{
    return TrFormat(info.beatsPerLoop == 1 ? "dancer.label_one" : "dancer.label", info.name, info.beatsPerLoop);
}

DancerSprite::~DancerSprite() { unload(); }

void DancerSprite::unload()
{
    if (texture_.TexID != ImTextureID_Invalid) {
        texture_.SetStatus(ImTextureStatus_WantDestroy);
        texture_.UnusedFrames = 1;
        ImGui_ImplOpenGL3_UpdateTexture(&texture_);
    }

    texture_.DestroyPixels();
    texture_.SetStatus(ImTextureStatus_Destroyed);
    stbi_image_free(pixels_);
    pixels_ = nullptr;
    width_ = 0;
    height_ = 0;
    frameCount_ = 0;
    uploadedFrame_ = -1;
}

Status DancerSprite::load(const DancerInfo& info)
{
    unload();

    std::ifstream file(PathFromUtf8(info.path), std::ios::binary);
    if (!file) {
        return Status::Fail(TrFormat("error.file_read", info.path));
    }

    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (buffer.empty()) {
        return Status::Fail(TrFormat("error.file_empty", info.path));
    }

    int* delays = nullptr;
    int w = 0, h = 0, frames = 0, comp = 0;
    unsigned char* data = nullptr;
    const bool completed = RunGuarded([&] {
        data = stbi_load_gif_from_memory(
            buffer.data(), static_cast<int>(buffer.size()), &delays, &w, &h, &frames, &comp, 4
        );
    });
    if (!completed) {
        return Status::Fail(TrFormat("error.gif_crash", info.path));
    }

    stbi_image_free(delays);
    if (!data || frames <= 0) {
        stbi_image_free(data);
        const char* reason = stbi_failure_reason();

        return Status::Fail(
            reason ? TrFormat("error.gif_decode_reason", info.path, std::string_view(reason))
                   : TrFormat("error.gif_decode", info.path)
        );
    }

    pixels_ = data;
    width_ = w;
    height_ = h;
    frameCount_ = frames;
    beatsPerLoop_ = std::max(1, info.beatsPerLoop);

    return Status::Ok();
}

ImTextureID DancerSprite::textureForBeat(double currentBeat)
{
    if (!isLoaded()) {
        return ImTextureID_Invalid;
    }

    const double phase = std::fmod(std::fmod(currentBeat, beatsPerLoop_) + beatsPerLoop_, beatsPerLoop_);
    const int frame = std::clamp(static_cast<int>(phase / beatsPerLoop_ * frameCount_), 0, frameCount_ - 1);
    if (frame == uploadedFrame_) {
        return texture_.GetTexID();
    }

    if (texture_.Status == ImTextureStatus_Destroyed) {
        texture_.Create(ImTextureFormat_RGBA32, width_, height_);
    }

    std::memcpy(
        texture_.GetPixels(),
        pixels_ + static_cast<size_t>(frame) * texture_.GetSizeInBytes(),
        texture_.GetSizeInBytes()
    );
    if (texture_.Status == ImTextureStatus_OK) {
        texture_.Updates.resize(0);
        texture_.Updates.push_back({0, 0, static_cast<unsigned short>(width_), static_cast<unsigned short>(height_)});
        texture_.SetStatus(ImTextureStatus_WantUpdates);
    }

    ImGui_ImplOpenGL3_UpdateTexture(&texture_);
    uploadedFrame_ = frame;

    return texture_.GetTexID();
}

} // namespace imdj
