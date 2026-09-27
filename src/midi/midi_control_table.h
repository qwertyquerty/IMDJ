#pragma once

#include <string>

#include "midi/midi_types.h"

namespace imdj {

struct ControlInfo {
    int deckIndex = -1;
    ActionKind action = ActionKind::Play;
    std::string key;
    std::string labelKey;
    int slot = -1;
};

const ControlInfo& ControlInfoFor(MidiControl control);

} // namespace imdj
