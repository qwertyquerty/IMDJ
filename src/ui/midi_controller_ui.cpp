#include "ui/midi_controller_ui.h"

#include "imgui.h"
#include "ui/modal.h"
#include "ui/ui_common.h"
#include "ui/ui_context.h"
#include "core/strings.h"

namespace imdj {

namespace {

void DrawBindingRow(MidiController& midi, MidiControl control)
{
    ImGui::TableSetColumnIndex(0);
    ImGui::Selectable(
        "##row",
        false,
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap |
            ImGuiSelectableFlags_NoAutoClosePopups,
        ImVec2(0, 0)
    );
    if (ImGui::IsItemHovered()) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
    }

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextUnformatted(MidiControlLabel(control).c_str());

    ImGui::TableSetColumnIndex(1);
    MidiBinding b = midi.binding(control);
    if (b.bound) {
        const char* kind = b.statusHighNibble == 0xB0 ? "CC" : Tr("midi.note");
        ImGui::TextDisabled(
            "%s",
            TrFormat("midi.binding_format", kind, static_cast<int>(b.data1), static_cast<int>(b.channel) + 1).c_str()
        );
    }
    else {
        ImGui::TextDisabled("%s", Tr("common.none"));
    }

    ImGui::TableSetColumnIndex(2);
    bool learningThis = midi.isLearning() && midi.learningControl() == control;
    if (learningThis) {
        if (ImGui::SmallButton(TrLabel("common.cancel"))) {
            midi.cancelLearn();
        }
    }
    else {
        ImGui::BeginDisabled(midi.isLearning());
        if (ImGui::SmallButton(TrLabel("midi.learn"))) {
            midi.armLearn(control);
        }

        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!b.bound);
    if (ImGui::SmallButton("X")) {
        midi.clearBinding(control);
    }

    ImGui::EndDisabled();
}

void DrawBindingTable(MidiController& midi, int firstControl, int lastControl)
{
    if (!ImGui::BeginTable("midiLearn", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        return;
    }

    ImGui::TableSetupColumn(TrLabel("midi.col_control"), ImGuiTableColumnFlags_WidthFixed, 230.0f);
    ImGui::TableSetupColumn(TrLabel("midi.col_bound"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 120.0f);

    for (int i = firstControl; i < lastControl; ++i) {
        ImGui::PushID(i);
        ImGui::TableNextRow();
        DrawBindingRow(midi, static_cast<MidiControl>(i));
        ImGui::PopID();
    }

    ImGui::EndTable();
}

} // namespace

void DrawMidiControllerPopup(const UiContext& context)
{
    AudioEngine& engine = context.engine;
    MidiController& midi = context.midi;
    MidiControllerUiState& ui = context.midiUi;
    Modal modal(TrLabel("popup.midi_controller"), ImVec2(530, 0));
    if (!modal) {
        return;
    }

    AccentText(Tr("midi.device"));
    std::string currentLabel = midi.isConnected() ? midi.connectedPortName() : Tr("midi.not_connected");
    ImGui::SetNextItemWidth(420.0f);
    if (ImGui::BeginCombo("##midiPort", currentLabel.c_str())) {
        for (size_t i = 0; i < ui.ports.size(); ++i) {
            bool selected = ui.selectedPort == static_cast<int>(i);
            if (ImGui::Selectable(ui.ports[i].c_str(), selected)) {
                ui.selectedPort = static_cast<int>(i);
                if (Status connected = midi.connect(ui.selectedPort, &engine); connected) {
                    context.notifications.info(TrFormat("notify.midi_connected", midi.connectedPortName().c_str()));
                }
                else {
                    context.notifications.error(TrFormat("notify.midi_connect_failed", connected.error().c_str()));
                }
            }
        }

        ImGui::EndCombo();
    }

    if (ui.ports.empty()) {
        ImGui::TextDisabled("%s", Tr("midi.no_devices"));
    }

    if (midi.isConnected()) {
        ImGui::SameLine();
        if (ImGui::SmallButton(TrLabel("midi.disconnect"))) {
            midi.disconnect();
            ui.selectedPort = -1;
        }
    }

    ImGui::BeginDisabled(!midi.isConnected());
    if (ImGui::BeginTabBar("bindingTabs")) {
        for (int deck = 0; deck < engine.deckCount(); ++deck) {
            if (!ImGui::BeginTabItem(DeckLabel(deck))) {
                continue;
            }

            DrawBindingTable(midi, deck * MIDI_CONTROLS_PER_DECK, (deck + 1) * MIDI_CONTROLS_PER_DECK);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(TrLabel("midi.tab_global"))) {
            DrawBindingTable(midi, static_cast<int>(MidiControl::Crossfader), static_cast<int>(MidiControl::Count));
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::BeginDisabled(!midi.isConnected());
    if (ImGui::Button(TrLabel("midi.save_mapping"), ImVec2(140, 0))) {
        if (Status saved = midi.saveBindings(); saved) {
            context.notifications.info(Tr("notify.midi_saved"));
        }
        else {
            context.notifications.error(TrFormat("notify.midi_not_saved", saved.error().c_str()));
        }
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    modal.closeButton();
}

} // namespace imdj
