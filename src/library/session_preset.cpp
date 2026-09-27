#include "library/session_preset.h"

#include <algorithm>
#include <filesystem>

#include "app/app_paths.h"
#include "app/session_state.h"
#include "core/json_io.h"
#include "core/paths.h"
#include "core/text.h"
#include "midi/midi_bindings_json.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr const char* LAST_PRESET_KEY = "preset";

std::string PresetFilePath(const std::string& name)
{
    return SessionPresetsDir() + "/" + SanitizeFileName(name) + ".json";
}

Json WriteChain(VstChain& chain)
{
    Json plugins = Json::array();
    for (size_t i = 0; i < chain.size(); ++i) {
        VstPluginInstance& plugin = chain.at(i);
        plugins.push_back(
            {{"path", plugin.descriptor().modulePath},
             {"bypassed", plugin.bypassed()},
             {"state", plugin.getStateBase64()}}
        );
    }

    return plugins;
}

void AddPlugin(
    VstChain& chain, const std::string& path, bool bypassed, const std::string& state, std::vector<std::string>& errors
)
{
    if (path.empty()) {
        return;
    }

    VstPluginDescriptor descriptor;
    descriptor.modulePath = path;
    auto instance = std::make_unique<VstPluginInstance>(descriptor);

    Status started = instance->initialize(static_cast<double>(SAMPLE_RATE), MAX_BLOCK_FRAMES);
    if (!started) {
        errors.push_back(path + ": " + (started.error().empty() ? TrFormat("vst.load_failed") : started.error()));
        return;
    }

    instance->setBypassed(bypassed);

    if (!state.empty()) {
        if (Status restored = instance->setStateFromBase64(state); !restored) {
            errors.push_back(instance->descriptor().label + ": " + restored.error());
        }
    }

    chain.add(std::move(instance));
}

void ReadChain(const Json& plugins, VstChain& chain, std::vector<std::string>& errors)
{
    chain.clear();
    if (!plugins.is_array()) {
        return;
    }

    for (const Json& plugin : plugins) {
        AddPlugin(
            chain,
            JsonValue<std::string>(plugin, "path", ""),
            JsonValue(plugin, "bypassed", false),
            JsonValue<std::string>(plugin, "state", ""),
            errors
        );
    }
}

constexpr std::pair<const char*, ClampedAtomic<float> Deck::*> FLOAT_FIELDS[] = {
    {"volume", &Deck::volume},
    {"gain", &Deck::gain},
    {"pan", &Deck::pan},
    {"eq_low", &Deck::eqLow},
    {"eq_mid", &Deck::eqMid},
    {"eq_high", &Deck::eqHigh},
    {"filter", &Deck::filter},
    {"playback_rate", &Deck::playbackRate},
};

constexpr std::pair<const char*, std::atomic<bool> Deck::*> BOOL_FIELDS[] = {
    {"cue", &Deck::cue},
    {"keylock", &Deck::keylock},
    {"metronome", &Deck::metronomeEnabled},
    {"normalize", &Deck::normalizeEnabled},
    {"quantize_loops", &Deck::quantizeLoops},
};

Json WriteSignalChain(DeckChain& chain)
{
    Json stages = Json::array();
    for (size_t position = 0; position < chain.size(); ++position) {
        DeckProcessor& stage = chain.at(position);
        stages.push_back({{"id", stage.id()}, {"enabled", stage.enabled()}});
    }

    return stages;
}

void ReadSignalChain(const Json& stages, DeckChain& chain)
{
    if (!stages.is_array()) {
        return;
    }

    std::vector<size_t> slots;
    for (const Json& saved : stages) {
        const std::string id = JsonValue(saved, "id", std::string());
        for (size_t slot = 0; slot < chain.size(); ++slot) {
            if (chain.inSlot(slot).id() == id && std::ranges::find(slots, slot) == slots.end()) {
                chain.inSlot(slot).setEnabled(JsonValue(saved, "enabled", true));
                slots.push_back(slot);
            }
        }
    }

    for (size_t slot = 0; slot < chain.size(); ++slot) {
        if (std::ranges::find(slots, slot) == slots.end()) {
            slots.push_back(slot);
        }
    }

    chain.setOrder(slots);
}

Json WriteDeck(AudioEngine& engine, int index)
{
    Deck& deck = engine.deck(index);
    Json json;
    for (const auto& [key, field] : FLOAT_FIELDS) {
        json[key] = (deck.*field).load();
    }

    for (const auto& [key, field] : BOOL_FIELDS) {
        json[key] = (deck.*field).load();
    }

    json["crossfader_side"] = static_cast<int>(deck.side());
    json["sync"] = deck.syncEnabled.load();
    json["track"] = deck.filePath;
    json["position_seconds"] = deck.positionSeconds().count();
    json["vst"] = WriteChain(deck.vstChain);
    json["signal_chain"] = WriteSignalChain(deck.dsp.chain);

    return json;
}

void ReadDeck(
    const Json& json, AudioEngine& engine, int index, const SessionPresetOptions& options,
    std::vector<std::string>& errors
)
{
    Deck& deck = engine.deck(index);
    for (const auto& [key, field] : FLOAT_FIELDS) {
        (deck.*field).store(JsonValue(json, key, (deck.*field).load()));
    }

    for (const auto& [key, field] : BOOL_FIELDS) {
        (deck.*field).store(JsonValue(json, key, (deck.*field).load()));
    }

    deck.setCrossfaderSide(JsonEnum(json, "crossfader_side", CrossfaderSide::None, CROSSFADER_SIDE_COUNT));
    engine.setSyncEnabled(index, JsonValue(json, "sync", false));
    ReadChain(JsonArray(json, "vst"), deck.vstChain, errors);
    if (json.contains("signal_chain")) {
        ReadSignalChain(json["signal_chain"], deck.dsp.chain);
    }

    const std::string track = JsonValue<std::string>(json, "track", "");
    if (options.restoreTracks && !track.empty()) {
        engine.loadTrackAsync(index, track, JsonValue(json, "position_seconds", 0.0));
    }
}

} // namespace

std::vector<std::string> ListSessionPresets()
{
    std::vector<std::string> names;
    std::error_code ec;
    auto dir = PathFromUtf8(SessionPresetsDir());
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        names.push_back(PathToUtf8(entry.path().stem()));
    }

    std::ranges::sort(names);
    return names;
}

Status SaveSessionPreset(const std::string& name, AudioEngine& engine, const MidiController& midi)
{
    if (name.empty()) {
        return Status::Fail(TrFormat("error.preset_name"));
    }

    Json root;
    root["deck_count"] = engine.deckCount();
    root["snap_resolution"] = static_cast<int>(engine.snapResolution());
    root["master_volume"] = engine.masterVolume();
    root["crossfader"] = {{"position", engine.crossfade()}, {"curve", static_cast<int>(engine.crossfaderCurve())}};

    Json decks = Json::array();
    for (int i = 0; i < engine.deckCount(); ++i) {
        decks.push_back(WriteDeck(engine, i));
    }

    root["decks"] = std::move(decks);
    root["master_vst"] = WriteChain(engine.masterVstChain());

    Json bindings;
    WriteMidiBindings(bindings, midi.exportBindings());
    root["midi"] = std::move(bindings);

    if (Status saved = SaveVersionedJson(PresetFilePath(name), root, SESSION_PRESET_VERSION); !saved) {
        return saved;
    }

    SetSessionValue(LAST_PRESET_KEY, name);
    return Status::Ok();
}

Result<std::string> LoadSessionPreset(
    const std::string& name, AudioEngine& engine, MidiController& midi, const SessionPresetOptions& options
)
{
    Json root;
    if (!LoadJsonFile(PresetFilePath(name), root)) {
        return Error(TrFormat("error.file_read", PresetFilePath(name)));
    }

    std::vector<std::string> errors;
    engine.setDeckCount(JsonValue(root, "deck_count", DEFAULT_DECK_COUNT));
    engine.setMasterVolume(JsonValue(root, "master_volume", engine.masterVolume()));

    const Json crossfader = root.contains("crossfader") ? root["crossfader"] : Json::object();
    engine.setCrossfade(JsonValue(crossfader, "position", engine.crossfade()));
    engine.setCrossfaderCurve(JsonEnum(crossfader, "curve", engine.crossfaderCurve(), CROSSFADER_CURVE_COUNT));

    const Json& decks = JsonArray(root, "decks");
    for (int i = 0; i < engine.deckCount() && i < static_cast<int>(decks.size()); ++i) {
        ReadDeck(decks[i], engine, i, options, errors);
    }

    ReadChain(JsonArray(root, "master_vst"), engine.masterVstChain(), errors);
    if (options.restoreMidiBindings && root.contains("midi")) {
        midi.importBindings(ReadMidiBindings(root["midi"]));
    }

    engine.setSnapResolution(JsonEnum(root, "snap_resolution", engine.snapResolution(), SNAP_RESOLUTION_COUNT));
    SetSessionValue(LAST_PRESET_KEY, name);

    if (errors.empty()) {
        return std::string();
    }

    std::string warning = TrFormat("error.preset_plugins") + "\n";
    for (const std::string& error : errors) {
        warning += "- " + error + "\n";
    }

    return warning;
}

Status DeleteSessionPreset(const std::string& name)
{
    std::error_code ec;
    if (!std::filesystem::remove(PathFromUtf8(PresetFilePath(name)), ec) || ec) {
        return Status::Fail(TrFormat("error.preset_delete"));
    }

    return Status::Ok();
}

std::string GetLastUsedPresetName() { return SessionValue(LAST_PRESET_KEY); }

} // namespace imdj
