#include "core/paths.h"
#include "audio/sample_buffer.h"

#include <algorithm>
#include <atomic>
#include <filesystem>

#include "miniaudio.h"
#include "core/strings.h"

namespace imdj {

namespace {

ma_result DecodeWholeFile(
    const std::string& utf8Path, uint64_t* frameCountOut, void** dataOut, std::atomic<float>* progress
)
{
    *frameCountOut = 0;
    *dataOut = nullptr;

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, CHANNELS, SAMPLE_RATE);
    ma_decoder decoder;
    ma_result result;
#if defined(_WIN32)
    result = ma_decoder_init_file_w(PathFromUtf8(utf8Path).wstring().c_str(), &config, &decoder);
#else
    result = ma_decoder_init_file(utf8Path.c_str(), &config, &decoder);
#endif
    if (result != MA_SUCCESS) {
        return result;
    }

    const ma_uint64 bytesPerFrame = ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);

    ma_uint64 estimated = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &estimated);

    ma_uint64 total = 0;
    ma_uint64 capacity = estimated;
    void* frames = capacity > 0 ? ma_malloc(static_cast<size_t>(capacity * bytesPerFrame), nullptr) : nullptr;
    if (capacity > 0 && !frames) {
        capacity = 0;
    }

    for (;;) {
        if (total == capacity) {
            ma_uint64 grown = capacity == 0 ? 4096 : capacity * 2;
            void* resized = ma_realloc(frames, static_cast<size_t>(grown * bytesPerFrame), nullptr);
            if (!resized) {
                ma_free(frames, nullptr);
                ma_decoder_uninit(&decoder);

                return MA_OUT_OF_MEMORY;
            }

            frames = resized;
            capacity = grown;
        }

        ma_uint64 toRead = std::min<ma_uint64>(capacity - total, SAMPLE_RATE);
        ma_uint64 justRead = 0;
        ma_result readResult = ma_decoder_read_pcm_frames(
            &decoder, static_cast<ma_uint8*>(frames) + total * bytesPerFrame, toRead, &justRead
        );
        total += justRead;
        if (progress && estimated > 0) {
            progress->store(std::min(1.0f, static_cast<float>(total) / static_cast<float>(estimated)));
        }

        if (readResult != MA_SUCCESS || justRead < toRead) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);
    *frameCountOut = total;
    *dataOut = frames;

    return MA_SUCCESS;
}

} // namespace

Status SampleBuffer::load(const std::string& utf8Path, std::atomic<float>* progress)
{
    uint64_t frameCount = 0;
    void* data = nullptr;
    ma_result result = DecodeWholeFile(utf8Path, &frameCount, &data, progress);
    if (result != MA_SUCCESS) {
        return Status::Fail(TrFormat("error.track_load", utf8Path, static_cast<int>(result)));
    }

    const float* decoded = static_cast<const float*>(data);
    samples_.assign(decoded, decoded + frameCount * CHANNELS);
    ma_free(data, nullptr);

    frameCount_ = frameCount;
    return Status::Ok();
}

void SampleBuffer::clear()
{
    samples_.clear();
    samples_.shrink_to_fit();
    frameCount_ = 0;
}

} // namespace imdj
