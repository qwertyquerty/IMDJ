#pragma once

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>

#include "core/text.h"

namespace imdj {

inline std::filesystem::path PathFromUtf8(std::string_view utf8)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

inline std::string PathToUtf8(const std::filesystem::path& path)
{
    std::u8string text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

inline bool HasExtension(const std::filesystem::path& path, std::initializer_list<std::string_view> extensions)
{
    return std::ranges::find(extensions, ToLower(PathToUtf8(path.extension()))) != extensions.end();
}

} // namespace imdj
