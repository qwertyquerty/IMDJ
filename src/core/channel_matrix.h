#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace imdj {

template <int SOURCES, int OUTPUTS>
struct ChannelMatrix {
    static constexpr int SOURCE_COUNT = SOURCES;
    static constexpr int OUTPUT_COUNT = OUTPUTS;

    bool cell[SOURCES][OUTPUTS] = {};

    bool* operator[](int source) { return cell[source]; }
    const bool* operator[](int source) const { return cell[source]; }

    static constexpr ChannelMatrix Identity()
    {
        ChannelMatrix matrix;
        for (int i = 0; i < std::min(SOURCES, OUTPUTS); ++i) {
            matrix.cell[i][i] = true;
        }

        return matrix;
    }

    void muteSource(int source) { std::ranges::fill(cell[source], false); }
    bool usesSource(int source) const { return std::ranges::any_of(cell[source], std::identity{}); }

    void mixInterleaved(
        float* dst, uint32_t outputCount, uint32_t frames, const std::array<const float*, SOURCES>& sources
    ) const
    {
        for (uint32_t i = 0; i < frames; ++i) {
            float* frame = dst + static_cast<size_t>(i) * outputCount;
            for (uint32_t c = 0; c < outputCount; ++c) {
                float sum = 0.0f;
                for (int s = 0; s < SOURCES; ++s) {
                    if (c < OUTPUTS && cell[s][c]) {
                        sum += sources[s][i];
                    }
                }

                frame[c] = sum;
            }
        }
    }
};

} // namespace imdj
