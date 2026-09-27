#pragma once

#include <cstdint>

#include "audio/audio_buffer.h"
#include "core/channel_matrix.h"

namespace imdj {

enum RoutingSource : int { ROUTE_MASTER_L = 0, ROUTE_MASTER_R = 1, ROUTE_CUE_L = 2, ROUTE_CUE_R = 3 };
constexpr int ROUTING_SOURCE_COUNT = 4;
constexpr int MAX_ROUTING_CHANNELS = 8;

using RoutingMatrix = ChannelMatrix<ROUTING_SOURCE_COUNT, MAX_ROUTING_CHANNELS>;

constexpr RoutingMatrix CueOnlyRouting()
{
    RoutingMatrix matrix;
    matrix.cell[ROUTE_CUE_L][0] = true;
    matrix.cell[ROUTE_CUE_R][1] = true;

    return matrix;
}

inline void RouteBuses(
    const RoutingMatrix& matrix, float* dst, uint32_t channelCount, const StereoBlock& master, const StereoBlock& cue
)
{
    matrix.mixInterleaved(
        dst, channelCount, master.frames(), {master.left.data(), master.right.data(), cue.left.data(), cue.right.data()}
    );
}

} // namespace imdj
