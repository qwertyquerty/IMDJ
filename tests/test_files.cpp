#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

#include "audio/loudness.h"
#include "audio/sample_buffer.h"
#include "audio/sample_player.h"
#include "audio/track_metadata.h"
#include "core/base64.h"
#include "core/paths.h"
#include "library/audio_tags.h"

using namespace imdj;

namespace {

std::string TempDir()
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "imdj_tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    return PathToUtf8(dir);
}

std::string TempFile(const std::string& name) { return TempDir() + "/" + name; }

void WriteWavSine(const std::string& path, double hz, float amplitude, uint32_t frames, bool withInfoChunk)
{
    std::vector<int16_t> samples(frames * 2);
    for (uint32_t i = 0; i < frames; ++i) {
        double value = amplitude * std::sin(2.0 * std::numbers::pi * hz * i / SAMPLE_RATE);
        int16_t quantized = static_cast<int16_t>(std::lround(value * 32767.0));
        samples[i * 2] = quantized;
        samples[i * 2 + 1] = quantized;
    }

    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    std::string info;
    if (withInfoChunk) {
        auto chunk = [](const char* id, const std::string& value) {
            std::string padded = value;
            padded.push_back('\0');
            if (padded.size() & 1) {
                padded.push_back('\0');
            }

            uint32_t size = static_cast<uint32_t>(padded.size());
            return std::string(id, 4) + std::string(reinterpret_cast<const char*>(&size), 4) + padded;
        };
        std::string body = "INFO" + chunk("INAM", "Test Title") + chunk("IART", "Test Artist");
        uint32_t size = static_cast<uint32_t>(body.size());
        info = "LIST" + std::string(reinterpret_cast<const char*>(&size), 4) + body;
    }

    const uint32_t riffSize = 4 + 24 + 8 + dataBytes + static_cast<uint32_t>(info.size());
    std::ofstream file(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
    auto put32 = [&file](uint32_t value) { file.write(reinterpret_cast<const char*>(&value), 4); };
    auto put16 = [&file](uint16_t value) { file.write(reinterpret_cast<const char*>(&value), 2); };

    file.write("RIFF", 4);
    put32(riffSize);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    put32(16);
    put16(1);
    put16(2);
    put32(SAMPLE_RATE);
    put32(SAMPLE_RATE * 2 * 2);
    put16(4);
    put16(16);
    file.write(info.data(), static_cast<std::streamsize>(info.size()));
    file.write("data", 4);
    put32(dataBytes);
    file.write(reinterpret_cast<const char*>(samples.data()), dataBytes);
}

std::string SyncSafe(uint32_t value)
{
    std::string out;
    out.push_back(static_cast<char>((value >> 21) & 0x7F));
    out.push_back(static_cast<char>((value >> 14) & 0x7F));
    out.push_back(static_cast<char>((value >> 7) & 0x7F));
    out.push_back(static_cast<char>(value & 0x7F));

    return out;
}

void WriteId3File(const std::string& path, const std::string& title, const std::string& artist)
{
    auto frame = [](const char* id, const std::string& text) {
        std::string body;
        body.push_back(3);
        body += text;
        uint32_t size = static_cast<uint32_t>(body.size());
        std::string header(id, 4);
        header.push_back(static_cast<char>((size >> 24) & 0xFF));
        header.push_back(static_cast<char>((size >> 16) & 0xFF));
        header.push_back(static_cast<char>((size >> 8) & 0xFF));
        header.push_back(static_cast<char>(size & 0xFF));
        header.push_back(0);
        header.push_back(0);

        return header + body;
    };

    std::string frames = frame("TIT2", title) + frame("TPE1", artist);
    std::ofstream file(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
    file.write("ID3", 3);
    file.put(3);
    file.put(0);
    file.put(0);
    std::string size = SyncSafe(static_cast<uint32_t>(frames.size()));
    file.write(size.data(), 4);
    file.write(frames.data(), static_cast<std::streamsize>(frames.size()));
}

SampleBuffer LoadedSine(const std::string& name, double hz, float amplitude, uint32_t frames)
{
    std::string path = TempFile(name);
    WriteWavSine(path, hz, amplitude, frames, false);

    SampleBuffer buffer;
    REQUIRE(buffer.load(path));

    return buffer;
}

} // namespace

TEST_CASE("Base64 round trips with padding")
{
    const std::vector<uint8_t> man = {'M', 'a', 'n'};
    CHECK(Base64Encode(man) == "TWFu");
    CHECK(Base64Encode(std::span(man).first(2)) == "TWE=");
    CHECK(Base64Encode(std::span(man).first(1)) == "TQ==");
    CHECK(Base64Encode({}).empty());
    CHECK(Base64Decode("TWE=") == std::vector<uint8_t>{'M', 'a'});

    std::vector<uint8_t> bytes(256);
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(i);
    }

    CHECK(Base64Decode(Base64Encode(bytes)) == bytes);
}

TEST_CASE("SampleBuffer decodes a wav file")
{
    SampleBuffer buffer = LoadedSine("decode.wav", 1000.0, 0.5f, SAMPLE_RATE);

    CHECK_FALSE(buffer.empty());
    CHECK(buffer.frameCount() == SAMPLE_RATE);
    CHECK(buffer.durationSeconds() == doctest::Approx(1.0));

    SampleBuffer missing;
    Status loaded = missing.load(TempFile("does_not_exist.wav"));
    CHECK_FALSE(loaded.ok());
    CHECK_FALSE(loaded.error().empty());
}

TEST_CASE("Loudness of silence is treated as silent")
{
    SampleBuffer silence = LoadedSine("silence.wav", 1000.0, 0.0f, SAMPLE_RATE);

    CHECK(MeasureIntegratedLufs(silence) <= SILENCE_LUFS);
    CHECK(NormalizeGainForLufs(SILENCE_LUFS) == doctest::Approx(1.0f));

    SampleBuffer empty;
    CHECK(MeasureIntegratedLufs(empty) <= SILENCE_LUFS);
}

TEST_CASE("Normalize gain moves loudness toward the target")
{
    CHECK(NormalizeGainForLufs(-20.0, -14.0) > 1.0f);
    CHECK(NormalizeGainForLufs(-8.0, -14.0) < 1.0f);
    CHECK(NormalizeGainForLufs(-14.0, -14.0) == doctest::Approx(1.0f));
    CHECK(NormalizeGainForLufs(-60.0, -14.0) <= 4.0f);
    CHECK(NormalizeGainForLufs(0.0, -14.0) >= 0.25f);
}

TEST_CASE("ReadBlock stops at the end of the track")
{
    SampleBuffer buffer = LoadedSine("short.wav", 100.0, 1.0f, 100);

    StereoBuffer output;
    output.resize(256);
    StereoBlock block = output.block(256);

    PlaybackResult result = ReadBlock(buffer, block, {.startFrame = 0.0, .rate = 1.0, .gain = 1.0f});
    CHECK(result.hitBoundary);
    CHECK(result.framesWritten == 100);
    CHECK(block.left[150] == doctest::Approx(0.0f));
}

TEST_CASE("ReadBlock wraps inside a loop")
{
    SampleBuffer buffer = LoadedSine("loop.wav", 100.0, 1.0f, 1000);

    StereoBuffer output;
    output.resize(64);
    StereoBlock block = output.block(64);

    PlaybackResult result = ReadBlock(
        buffer, block, {.startFrame = 90.0, .rate = 1.0, .gain = 1.0f, .loop = true, .loopStart = 10, .loopEnd = 100}
    );
    CHECK(result.framesWritten == 64);
    CHECK(result.endFrame < 100.0);
    CHECK_FALSE(result.hitBoundary);
}

TEST_CASE("ReadBlock runs silent before the start unless asked to stop")
{
    SampleBuffer buffer = LoadedSine("before.wav", 100.0, 1.0f, 1000);

    StereoBuffer output;
    output.resize(32);
    StereoBlock block = output.block(32);

    PlaybackResult playing = ReadBlock(buffer, block, {.startFrame = -20.0, .rate = 1.0, .gain = 1.0f});
    CHECK_FALSE(playing.hitBoundary);
    CHECK(playing.framesWritten == 32);
    CHECK(block.left[0] == doctest::Approx(0.0f));

    PlaybackResult scratching =
        ReadBlock(buffer, block, {.startFrame = -20.0, .rate = 1.0, .gain = 1.0f, .stopBeforeStart = true});
    CHECK(scratching.hitBoundary);
    CHECK(scratching.framesWritten == 0);
}

TEST_CASE("ReadBlock interpolates between frames and applies gain")
{
    SampleBuffer buffer = LoadedSine("interpolate.wav", 100.0, 0.5f, 1000);

    StereoBuffer output;
    output.resize(16);
    StereoBlock block = output.block(16);

    ReadBlock(buffer, block, {.startFrame = 10.5, .rate = 1.0, .gain = 2.0f});
    const float expected = (buffer.frame(10)[0] + buffer.frame(11)[0]) * 0.5f * 2.0f;
    CHECK(block.left[0] == doctest::Approx(expected));
}

TEST_CASE("ReadBlock follows the playback rate in both directions")
{
    SampleBuffer buffer = LoadedSine("rate.wav", 100.0, 0.5f, 1000);

    StereoBuffer output;
    output.resize(16);
    StereoBlock block = output.block(16);

    PlaybackResult doubled = ReadBlock(buffer, block, {.startFrame = 0.0, .rate = 2.0, .gain = 1.0f});
    CHECK(block.left[3] == doctest::Approx(buffer.frame(6)[0]));
    CHECK(doubled.endFrame == doctest::Approx(32.0));

    PlaybackResult reversed = ReadBlock(buffer, block, {.startFrame = 100.0, .rate = -1.0, .gain = 1.0f});
    CHECK(block.left[1] == doctest::Approx(buffer.frame(99)[0]));
    CHECK(reversed.endFrame == doctest::Approx(84.0));
    CHECK_FALSE(reversed.hitBoundary);
}

TEST_CASE("Track metadata round trips through its sidecar")
{
    std::string path = TempFile("metadata.wav");
    WriteWavSine(path, 100.0, 0.5f, 1000, false);

    TrackMetadata saved;
    saved.info.title = "artista";
    saved.info.artist = "artistb";
    saved.info.bpm = 174.0;
    saved.startFrame = 24000;
    saved.info.key = 2;
    saved.hotCueSet = true;
    saved.hotCueFrame = 48000;
    saved.markers = {{"drop", 96000}, {"intro", 0}};
    REQUIRE(saved.save(path));

    TrackMetadata loaded;
    REQUIRE(TrackMetadata::Load(path, loaded));

    CHECK(loaded.info.title == "artista");
    CHECK(loaded.info.artist == "artistb");
    CHECK(loaded.info.bpm == doctest::Approx(174.0));
    CHECK(loaded.startFrame == 24000);
    CHECK(loaded.info.key == 2);
    CHECK(loaded.hotCueSet);
    CHECK(loaded.hotCueFrame == 48000);
    CHECK(loaded.markers.size() == 2);
    CHECK(loaded.markers[0].name == "intro");
    CHECK(loaded.markers[1].name == "drop");
}

TEST_CASE("Track metadata clamps to the track length")
{
    TrackMetadata meta;
    meta.startFrame = 5000;
    meta.hotCueFrame = 9000;
    meta.markers = {{"late", 9999}};

    meta.clampTo(1000);
    CHECK(meta.startFrame == 1000);
    CHECK(meta.hotCueFrame == 1000);
    CHECK(meta.markers[0].frame == 1000);
}

TEST_CASE("Tags come from an ID3v2 header")
{
    std::string path = TempFile("tagged.mp3");
    WriteId3File(path, "Song Name", "Band Name");

    TrackInfo info;
    REQUIRE(ReadAudioTags(path, info));
    CHECK(info.title == "Song Name");
    CHECK(info.artist == "Band Name");
}

TEST_CASE("Tags come from a wav INFO chunk")
{
    std::string path = TempFile("tagged.wav");
    WriteWavSine(path, 100.0, 0.5f, 1000, true);

    TrackInfo info;
    REQUIRE(ReadAudioTags(path, info));
    CHECK(info.title == "Test Title");
    CHECK(info.artist == "Test Artist");
}
