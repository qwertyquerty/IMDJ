#pragma once

#include "core/json_io.h"
#include "midi/midi_controller.h"

namespace imdj {

void WriteMidiBindings(Json& root, const MidiController::BindingSnapshot& snapshot);
MidiController::BindingSnapshot ReadMidiBindings(const Json& root);

} // namespace imdj
