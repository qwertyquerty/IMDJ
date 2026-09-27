#pragma once

#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/result.h"
#include "midi/midi_controller.h"

namespace imdj {

constexpr int SESSION_PRESET_VERSION = 2;

struct SessionPresetOptions {
    bool restoreTracks = true;
    bool restoreMidiBindings = true;
};

std::vector<std::string> ListSessionPresets();

Status SaveSessionPreset(const std::string& name, AudioEngine& engine, const MidiController& midi);

Result<std::string> LoadSessionPreset(
    const std::string& name, AudioEngine& engine, MidiController& midi, const SessionPresetOptions& options = {}
);

Status DeleteSessionPreset(const std::string& name);

std::string GetLastUsedPresetName();

} // namespace imdj
