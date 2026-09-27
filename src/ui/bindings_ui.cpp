#include "ui/bindings_ui.h"

#include <utility>

#include "imgui.h"
#include "ui/modal.h"
#include "ui/ui_common.h"
#include "ui/ui_context.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr std::pair<const char*, KnobTargetKind> BUILTIN_KNOB_TARGETS[] = {
    {"bindings.gain", KnobTargetKind::DeckGain},
    {"bindings.pan", KnobTargetKind::DeckPan},
    {"bindings.lo", KnobTargetKind::EqLow},
    {"bindings.mid", KnobTargetKind::EqMid},
    {"bindings.hi", KnobTargetKind::EqHigh},
    {"bindings.filter", KnobTargetKind::Filter},
};

const char* PadActionLabel(const PadAction& a)
{
    if (!a.assigned || a.kind == PadActionKind::None) {
        return Tr("common.none");
    }

    return a.label.empty() ? Tr("common.unnamed") : a.label.c_str();
}

const char* FxKnobLabel(const FxKnobAssignment& a)
{
    if (!a.assigned) {
        return Tr("common.none");
    }

    for (const auto& [label, kind] : BUILTIN_KNOB_TARGETS) {
        if (a.targetKind == kind) {
            return Tr(label);
        }
    }

    return a.paramLabel.empty() ? Tr("common.unnamed") : a.paramLabel.c_str();
}

void DrawFxKnobRow(MidiController& midi, int deck, int knob)
{
    ImGui::PushID(knob);
    FxKnobAssignment a = midi.fxKnobAssignment(deck, knob);
    ImGui::TextUnformatted(TrFormat("common.knob_n", knob + 1).c_str());
    ImGui::SameLine(90);
    ImGui::TextDisabled("%s", FxKnobLabel(a));

    ImGui::SameLine(260);
    for (const auto& [label, kind] : BUILTIN_KNOB_TARGETS) {
        if (ImGui::SmallButton(TrLabel(label))) {
            midi.assignFxKnobBuiltin(deck, knob, kind);
        }

        ImGui::SameLine();
    }

    ImGui::BeginDisabled(!a.assigned);
    if (ImGui::SmallButton(TrLabel("bindings.clear"))) {
        midi.clearFxKnobAssignment(deck, knob);
    }

    ImGui::EndDisabled();
    ImGui::PopID();
}

void DrawPadRow(MidiController& midi, int deck, int pad)
{
    ImGui::PushID(pad);
    PadAction a = midi.padAssignment(deck, pad);
    ImGui::TextUnformatted(TrFormat("common.pad_n", pad + 1).c_str());
    ImGui::SameLine(90);
    ImGui::TextDisabled("%s", PadActionLabel(a));

    ImGui::SameLine(260);
    ImGui::BeginDisabled(!a.assigned);
    if (ImGui::SmallButton(TrLabel("bindings.clear"))) {
        midi.clearPadAssignment(deck, pad);
    }

    ImGui::EndDisabled();
    ImGui::PopID();
}

} // namespace

void DrawBindingsManagerPopup(const UiContext& context)
{
    AudioEngine& engine = context.engine;
    MidiController& midi = context.midi;
    Modal modal(TrLabel("popup.bindings"));
    if (!modal) {
        return;
    }

    ImGui::TextDisabled("%s", Tr("bindings.hint"));
    ImGui::Spacing();

    for (int deck = 0; deck < engine.deckCount(); ++deck) {
        ImGui::PushID(deck);
        AccentText(DeckLabel(deck));
        ImGui::Separator();
        ImGui::TextDisabled("%s", Tr("bindings.fx_knobs"));
        ImGui::PushID("knobs");
        for (int knob = 0; knob < FX_KNOBS_PER_DECK; ++knob) {
            DrawFxKnobRow(midi, deck, knob);
        }

        ImGui::PopID();

        ImGui::Spacing();
        ImGui::TextDisabled("%s", Tr("bindings.pads"));
        ImGui::PushID("pads");
        for (int pad = 0; pad < PADS_PER_DECK; ++pad) {
            DrawPadRow(midi, deck, pad);
        }

        ImGui::PopID();

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    modal.closeButton();
}

} // namespace imdj
