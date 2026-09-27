#include "midi/midi_control_table.h"

#include "core/strings.h"

#include <iterator>
#include <vector>

namespace imdj {

namespace {

struct DeckControl {
    ActionKind action;
    const char* key;
    const char* label;
};

constexpr DeckControl DECK_CONTROLS[] = {
    {ActionKind::Play, "Play", "midi.action.play"},
    {ActionKind::Stop, "Stop", "midi.action.stop"},
    {ActionKind::Cue, "Cue", "midi.action.cue"},
    {ActionKind::Sync, "Sync", "midi.action.sync"},
    {ActionKind::SyncLock, "SyncLock", "midi.action.sync_lock"},
    {ActionKind::AlignPlay, "AlignPlay", "midi.action.align_play"},
    {ActionKind::SetHotCue, "SetHotCue", "midi.action.set_hot_cue"},
    {ActionKind::JumpHotCue, "JumpHotCue", "midi.action.jump_hot_cue"},
    {ActionKind::LoopIn, "LoopIn", "midi.action.loop_in"},
    {ActionKind::LoopOut, "LoopOut", "midi.action.loop_out"},
    {ActionKind::LoopToggle, "LoopToggle", "midi.action.loop_toggle"},
    {ActionKind::Volume, "Volume", "midi.action.volume"},
    {ActionKind::Gain, "Gain", "midi.action.gain"},
    {ActionKind::Tempo, "Tempo", "midi.action.tempo"},
    {ActionKind::JogRotate, "JogRotate", "midi.action.jog_rotate"},
};
static_assert(std::size(DECK_CONTROLS) == static_cast<size_t>(MidiControl::DeckA_Pad1));

std::vector<ControlInfo> BuildControlTable()
{
    std::vector<ControlInfo> table;
    table.reserve(static_cast<size_t>(MidiControl::Count));

    const auto slotted = [](const std::string& prefix, const char* stem, int slot) {
        return prefix + stem + std::to_string(slot + 1);
    };

    for (int deck = 0; deck < MAX_DECK_COUNT; ++deck) {
        const std::string keyPrefix = std::string("Deck") + DeckLetter(deck) + "_";

        for (const DeckControl& control : DECK_CONTROLS) {
            table.push_back({deck, control.action, keyPrefix + control.key, control.label});
        }

        for (int pad = 0; pad < PADS_PER_DECK; ++pad) {
            table.push_back({deck, ActionKind::Pad, slotted(keyPrefix, "Pad", pad), "common.pad_n", pad});
        }

        for (int knob = 0; knob < FX_KNOBS_PER_DECK; ++knob) {
            table.push_back(
                {deck, ActionKind::FxKnob, slotted(keyPrefix, "FxKnob", knob), "midi.action.fx_knob", knob}
            );
        }
    }

    table.push_back({-1, ActionKind::Crossfader, "Crossfader", "midi.action.crossfader"});
    for (int deck = 0; deck < MAX_DECK_COUNT; ++deck) {
        table.push_back({
            -1,
            ActionKind::MonitorToggle,
            std::string("MonitorToggle") + DeckLetter(deck),
            "midi.action.monitor_toggle",
            deck,
        });
    }

    table.push_back({-1, ActionKind::MonitorToggleMaster, "MonitorToggleMaster", "midi.action.monitor_master"});
    return table;
}

} // namespace

const ControlInfo& ControlInfoFor(MidiControl control)
{
    static const std::vector<ControlInfo> table = BuildControlTable();
    static const ControlInfo empty;

    size_t index = static_cast<size_t>(control);
    return index < table.size() ? table[index] : empty;
}

std::string MidiControlLabel(MidiControl c)
{
    const ControlInfo& info = ControlInfoFor(c);
    if (info.labelKey.empty()) {
        return {};
    }

    if (info.deckIndex >= 0) {
        const std::string action =
            info.slot >= 0 ? TrFormat(info.labelKey, info.slot + 1) : std::string(Tr(info.labelKey));
        return TrFormat("midi.deck_control", DeckLetter(info.deckIndex), action);
    }

    if (info.action == ActionKind::MonitorToggle) {
        return TrFormat(info.labelKey, DeckLetter(info.slot));
    }

    return Tr(info.labelKey);
}

const char* MidiControlKey(MidiControl c) { return ControlInfoFor(c).key.c_str(); }

} // namespace imdj
