#include "midi/midi_bindings_json.h"

namespace imdj {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MidiBinding, bound, statusHighNibble, channel, data1)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    FxKnobAssignment, assigned, targetKind, chain, pluginIndex, paramId, paramLabel
)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PadAction, assigned, kind, chain, pluginIndex, markerIndex, label)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    MidiController::BindingSnapshot, bindings, fxKnobAssignments, padAssignments
)

void WriteMidiBindings(Json& root, const MidiController::BindingSnapshot& snapshot) { root = snapshot; }

MidiController::BindingSnapshot ReadMidiBindings(const Json& root)
{
    return JsonAs(root, MidiController::BindingSnapshot{});
}

} // namespace imdj
