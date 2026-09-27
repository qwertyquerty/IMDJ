#pragma once

#include <cctype>
#include <string>

namespace imdj {

inline std::string ToLower(std::string text)
{
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return text;
}

inline std::string SanitizeFileName(std::string name)
{
    for (char& c : name) {
        c = std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    }

    return name;
}

} // namespace imdj
