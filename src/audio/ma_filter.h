#pragma once

#include <cstdint>
#include <span>
#include <type_traits>

#include "miniaudio.h"

namespace imdj {

template <typename Filter, typename Config, auto Reinit>
class MaFilter {
public:
    explicit MaFilter(uint32_t channels = 1)
    {
        const ma_biquad_config passthrough = ma_biquad_config_init(ma_format_f32, channels, 1, 0, 0, 1, 0, 0);
        ma_biquad_init(&passthrough, nullptr, &biquad());
    }

    ~MaFilter() { ma_biquad_uninit(&biquad(), nullptr); }

    MaFilter(const MaFilter&) = delete;
    MaFilter& operator=(const MaFilter&) = delete;

    void reinit(const Config& config) { Reinit(&config, &filter_); }
    void reset() { ma_biquad_clear_cache(&biquad()); }

    void process(float* out, const float* in, uint64_t frames)
    {
        ma_biquad_process_pcm_frames(&biquad(), out, in, frames);
    }

    void process(std::span<float> samples)
    {
        process(samples.data(), samples.data(), samples.size() / biquad().channels);
    }

private:
    ma_biquad& biquad()
    {
        if constexpr (std::is_same_v<Filter, ma_biquad>) {
            return filter_;
        }
        else {
            return filter_.bq;
        }
    }

    Filter filter_{};
};

using Biquad = MaFilter<ma_biquad, ma_biquad_config, ma_biquad_reinit>;
using LowPass = MaFilter<ma_lpf2, ma_lpf2_config, ma_lpf2_reinit>;
using HighPass = MaFilter<ma_hpf2, ma_hpf2_config, ma_hpf2_reinit>;
using LowShelf = MaFilter<ma_loshelf2, ma_loshelf2_config, ma_loshelf2_reinit>;
using Peak = MaFilter<ma_peak2, ma_peak2_config, ma_peak2_reinit>;
using HighShelf = MaFilter<ma_hishelf2, ma_hishelf2_config, ma_hishelf2_reinit>;

} // namespace imdj
