#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace imdj {

inline constexpr std::string_view BASE64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline std::string Base64Encode(std::span<const uint8_t> data)
{
    std::string out;
    uint32_t buffer = 0;
    int bits = 0;
    for (uint8_t byte : data) {
        buffer = (buffer << 8) | byte;
        for (bits += 8; bits >= 6; bits -= 6) {
            out += BASE64_ALPHABET[(buffer >> (bits - 6)) & 0x3F];
        }
    }

    if (bits > 0) {
        out += BASE64_ALPHABET[(buffer << (6 - bits)) & 0x3F];
    }

    out.resize((out.size() + 3) / 4 * 4, '=');
    return out;
}

inline std::vector<uint8_t> Base64Decode(std::string_view text)
{
    std::vector<uint8_t> out;
    uint32_t buffer = 0;
    int bits = 0;
    for (char c : text.substr(0, text.find('='))) {
        const size_t value = BASE64_ALPHABET.find(c);
        if (value == std::string_view::npos) {
            continue;
        }

        buffer = (buffer << 6) | static_cast<uint32_t>(value);
        if ((bits += 6) >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>(buffer >> bits));
        }
    }

    return out;
}

} // namespace imdj
