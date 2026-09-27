#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.h"

namespace imdj {

Status LoadLanguage(const std::string& dir, const std::string& code);
std::vector<std::string> AvailableLanguages(const std::string& dir);

Status LoadLanguageFromText(std::string_view englishJson, std::string_view languageJson, const std::string& code);
const std::string& CurrentLanguage();

const char* Tr(std::string_view key);

// "text##key"
const char* TrLabel(std::string_view key);

std::string FormatTranslation(std::string_view pattern, std::format_args args);
std::string TrFormatArgs(std::string_view key, std::format_args args);

template <typename... Args>
std::string TrFormat(std::string_view key, const Args&... args)
{
    return TrFormatArgs(key, std::make_format_args(args...));
}

} // namespace imdj
