#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "audio/audio_constants.h"

namespace imdj {

constexpr int PADS_PER_DECK = 8;
constexpr int FX_KNOBS_PER_DECK = 4;

enum class ActionKind {
    Play,
    Stop,
    Cue,
    Sync,
    SyncLock,
    AlignPlay,
    SetHotCue,
    JumpHotCue,
    LoopIn,
    LoopOut,
    LoopToggle,
    Volume,
    Gain,
    Tempo,
    JogRotate,
    Crossfader,
    FxKnob,
    Pad,
    MonitorToggle,
    MonitorToggleMaster,
};

enum class MidiControl {
    DeckA_Play,
    DeckA_Stop,
    DeckA_Cue,
    DeckA_Sync,
    DeckA_SyncLock,
    DeckA_AlignPlay,
    DeckA_SetHotCue,
    DeckA_JumpHotCue,
    DeckA_LoopIn,
    DeckA_LoopOut,
    DeckA_LoopToggle,
    DeckA_Volume,
    DeckA_Gain,
    DeckA_Tempo,
    DeckA_JogRotate,
    DeckA_Pad1,
    DeckA_FxKnob1 = DeckA_Pad1 + PADS_PER_DECK,
    DeckControlCount = DeckA_FxKnob1 + FX_KNOBS_PER_DECK,

    Crossfader = DeckControlCount * MAX_DECK_COUNT,
    MonitorToggleA,
    MonitorToggleB,
    MonitorToggleC,
    MonitorToggleD,
    MonitorToggleMaster,
    Count,
    None = -1,
};

constexpr int MIDI_CONTROLS_PER_DECK = static_cast<int>(MidiControl::DeckControlCount);

constexpr MidiControl DeckMidiControl(int deck, MidiControl deckAControl, int slot = 0)
{
    return static_cast<MidiControl>(static_cast<int>(deckAControl) + deck * MIDI_CONTROLS_PER_DECK + slot);
}

std::string MidiControlLabel(MidiControl c);
const char* MidiControlKey(MidiControl c);

enum class FxChainTarget { DeckA, DeckB, Master, DeckC, DeckD };

constexpr FxChainTarget DeckFxChainTarget(int deck)
{
    switch (deck) {
        case 1: return FxChainTarget::DeckB;
        case 2: return FxChainTarget::DeckC;
        case 3: return FxChainTarget::DeckD;
        default: return FxChainTarget::DeckA;
    }
}

constexpr int FxChainTargetDeck(FxChainTarget target)
{
    switch (target) {
        case FxChainTarget::DeckB: return 1;
        case FxChainTarget::DeckC: return 2;
        case FxChainTarget::DeckD: return 3;
        case FxChainTarget::DeckA: return 0;
        case FxChainTarget::Master: break;
    }

    return -1;
}

enum class KnobTargetKind {
    VstParam,
    DeckGain,
    DeckPan,
    EqLow,
    EqMid,
    EqHigh,
    Filter,
};

struct FxKnobAssignment {
    bool assigned = false;
    KnobTargetKind targetKind = KnobTargetKind::VstParam;
    FxChainTarget chain = FxChainTarget::DeckA;
    size_t pluginIndex = 0;
    int32_t paramId = 0;
    std::string paramLabel;
};

enum class PadActionKind {
    None,
    TogglePluginBypass,
    JumpToMarker,
};

struct PadAction {
    bool assigned = false;
    PadActionKind kind = PadActionKind::None;
    FxChainTarget chain = FxChainTarget::DeckA;
    size_t pluginIndex = 0;
    size_t markerIndex = 0;
    std::string label;
};

struct MidiBinding {
    bool bound = false;
    uint8_t statusHighNibble = 0;
    uint8_t channel = 0;
    uint8_t data1 = 0;
};

} // namespace imdj
