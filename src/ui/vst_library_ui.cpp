#include "ui/vst_library_ui.h"

#include <format>
#include <memory>
#include <set>

#include "audio/audio_constants.h"
#include "imgui.h"
#include "ui/modal.h"
#include "ui/ui_common.h"
#include "core/strings.h"

namespace imdj {

namespace {

struct AssignPopupState {
    VstChain* chain = nullptr;
    int pluginIndex = -1;
    FxChainTarget chainTarget = FxChainTarget::DeckA;
    int deckCount = 2;
};

void DrawBypassPadRows(MidiController& midi, const AssignPopupState& popup, const std::string& pluginLabel)
{
    ImGui::TextDisabled("%s", Tr("vst.bypass"));
    const size_t pluginIndex = static_cast<size_t>(popup.pluginIndex);

    for (int deck = 0; deck < popup.deckCount; ++deck) {
        ImGui::Text("%c", DeckLetter(deck));
        for (int pad = 0; pad < PADS_PER_DECK; ++pad) {
            ImGui::SameLine();
            ImGui::PushID(deck * PADS_PER_DECK + pad);

            PadAction current = midi.padAssignment(deck, pad);
            bool assignedHere = current.assigned && current.kind == PadActionKind::TogglePluginBypass &&
                                current.chain == popup.chainTarget && current.pluginIndex == pluginIndex;

            if (ToggleSmallButton(std::format("{}", pad + 1).c_str(), assignedHere)) {
                if (assignedHere) {
                    midi.clearPadAssignment(deck, pad);
                }
                else {
                    PadAction action;
                    action.kind = PadActionKind::TogglePluginBypass;
                    action.chain = popup.chainTarget;
                    action.pluginIndex = pluginIndex;
                    action.label = TrFormat("vst.bypass_action", pluginLabel.c_str());
                    midi.assignPad(deck, pad, action);
                }
            }

            ImGui::PopID();
        }
    }
}

void DrawParamKnobRows(MidiController& midi, const AssignPopupState& popup, VstPluginInstance& plugin)
{
    const size_t pluginIndex = static_cast<size_t>(popup.pluginIndex);
    std::vector<VstParamInfo> params = plugin.listParameters();

    ImGui::BeginChild("assignParamList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
    if (params.empty()) {
        ImGui::TextDisabled("%s", Tr("vst.no_params"));
    }

    for (const VstParamInfo& param : params) {
        ImGui::PushID(param.id);
        ImGui::TextUnformatted(param.title.c_str());
        ImGui::SameLine(220);

        for (int deck = 0; deck < popup.deckCount; ++deck) {
            for (int knob = 0; knob < FX_KNOBS_PER_DECK; ++knob) {
                if (deck != 0 || knob != 0) {
                    ImGui::SameLine();
                }

                ImGui::PushID(deck * FX_KNOBS_PER_DECK + knob);

                FxKnobAssignment current = midi.fxKnobAssignment(deck, knob);
                bool assignedHere = current.assigned && current.targetKind == KnobTargetKind::VstParam &&
                                    current.chain == popup.chainTarget && current.pluginIndex == pluginIndex &&
                                    current.paramId == param.id;

                if (ToggleSmallButton(std::format("{}{}", DeckLetter(deck), knob + 1).c_str(), assignedHere)) {
                    if (assignedHere) {
                        midi.clearFxKnobAssignment(deck, knob);
                    }
                    else {
                        midi.assignFxKnobVstParam(
                            deck,
                            knob,
                            popup.chainTarget,
                            pluginIndex,
                            param.id,
                            plugin.descriptor().label + ": " + param.title
                        );
                    }
                }

                ImGui::PopID();
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

void DrawAssignControlsPopup(MidiController& midi, const AssignPopupState& popup)
{
    Modal modal(TrLabel("popup.assign_controls"), ImVec2(580, 460));
    if (!modal) {
        return;
    }

    bool valid = popup.chain && popup.pluginIndex >= 0 && static_cast<size_t>(popup.pluginIndex) < popup.chain->size();
    if (!valid) {
        ImGui::TextDisabled("%s", Tr("vst.plugin_gone"));
        modal.closeButton();

        return;
    }

    VstPluginInstance& plugin = popup.chain->at(static_cast<size_t>(popup.pluginIndex));
    ImGui::TextColored(Colors().accentBright, "%s", plugin.descriptor().label.c_str());

    DrawBypassPadRows(midi, popup, plugin.descriptor().label);

    ImGui::Spacing();
    ImGui::TextDisabled("%s", Tr("vst.fx_knobs"));
    ImGui::Separator();
    DrawParamKnobRows(midi, popup, plugin);

    modal.closeButton();
}

void DrawChainRows(
    VstChain& chain, MidiController& midi, FxChainTarget chainTarget, int deckCount, AssignPopupState& assignPopup,
    bool& openAssignPopup
)
{
    for (size_t i = 0; i < chain.size(); ++i) {
        VstPluginInstance& plugin = chain.at(i);
        ImGui::PushID(static_cast<int>(i));

        BoundCheckbox("##enabled", !plugin.bypassed(), [&](bool on) { plugin.setBypassed(!on); });
        ImGui::SameLine();
        ImGui::TextUnformatted(plugin.descriptor().label.c_str());

        if (ImGui::SmallButton(plugin.isEditorOpen() ? TrLabel("vst.hide_gui") : TrLabel("vst.gui"))) {
            plugin.toggleEditor();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(TrLabel("common.up"))) {
            if (i > 0) {
                midi.swapPluginAssignments(chainTarget, i, i - 1);
            }

            chain.moveUp(i);
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(TrLabel("common.down"))) {
            if (i + 1 < chain.size()) {
                midi.swapPluginAssignments(chainTarget, i, i + 1);
            }

            chain.moveDown(i);
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("A")) {
            assignPopup.chain = &chain;
            assignPopup.pluginIndex = static_cast<int>(i);
            assignPopup.chainTarget = chainTarget;
            assignPopup.deckCount = deckCount;
            openAssignPopup = true;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            midi.removePluginAssignments(chainTarget, i);
            chain.removeAt(i);
            ImGui::PopID();
            break;
        }

        if (!plugin.lastEditorError().empty()) {
            BusyText(plugin.lastEditorError());
        }

        ImGui::PopID();
    }
}

void DrawPluginPicker(VstChain& chain, const VstLibraryUiState& library, Notifications& notifications)
{
    static char search[128] = "";
    static std::set<std::string> selected;

    if (ImGui::Button(TrLabel("vst.add_plugin"), ImVec2(-1, 0))) {
        search[0] = '\0';
        selected.clear();
        ImGui::OpenPopup(TrLabel("popup.select_plugins"));
    }

    Modal modal(TrLabel("popup.select_plugins"), ImVec2(420, 460));
    if (!modal) {
        return;
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::InputTextWithHint("##pickerSearch", Tr("vst.search_hint"), search, sizeof(search));

    const std::string filter = ToLower(search);
    ImGui::BeginChild("pickerList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
    if (library.plugins.empty()) {
        ImGui::TextDisabled("%s", Tr("vst.none_found"));
    }

    for (size_t i = 0; i < library.plugins.size(); ++i) {
        const VstPluginDescriptor& descriptor = library.plugins[i];
        if (!filter.empty() && ToLower(descriptor.label).find(filter) == std::string::npos) {
            continue;
        }

        ImGui::PushID(static_cast<int>(i));
        bool checked = selected.contains(descriptor.modulePath);
        bool toggled = ImGui::Checkbox("##pick", &checked);
        ImGui::SameLine();
        toggled |= ImGui::Selectable(descriptor.label.c_str());
        if (toggled) {
            if (selected.contains(descriptor.modulePath)) {
                selected.erase(descriptor.modulePath);
            }
            else {
                selected.insert(descriptor.modulePath);
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    auto addSelected = [&](bool bypassed) {
        for (const VstPluginDescriptor& descriptor : library.plugins) {
            if (!selected.contains(descriptor.modulePath)) {
                continue;
            }

            std::string error = AddPluginToChain(chain, descriptor, bypassed);
            if (!error.empty()) {
                notifications.error(error);
            }
        }

        selected.clear();
        ImGui::CloseCurrentPopup();
    };

    ImGui::TextDisabled("%s", TrFormat("vst.selected", static_cast<int>(selected.size())).c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(selected.empty());
    if (ImGui::Button(TrLabel("vst.add_selected"))) {
        addSelected(false);
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("vst.add_bypassed"))) {
        addSelected(true);
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    modal.closeButton();
}

} // namespace

std::string AddPluginToChain(VstChain& chain, const VstPluginDescriptor& descriptor, bool startBypassed)
{
    auto instance = std::make_unique<VstPluginInstance>(descriptor);
    Status started = instance->initialize(static_cast<double>(SAMPLE_RATE), MAX_BLOCK_FRAMES);
    if (!started) {
        return started.error().empty() ? Tr("vst.load_failed") : started.error();
    }

    if (startBypassed) {
        instance->setBypassed(true);
    }

    chain.add(std::move(instance));
    return {};
}

void DrawVstChainEditor(
    VstChain& chain, const VstLibraryUiState& library, Notifications& notifications, const char* chainLabel,
    MidiController& midi, FxChainTarget chainTarget, int deckCount
)
{
    ImGui::PushID(chainLabel);

    static AssignPopupState assignPopup;
    bool openAssignPopup = false;

    DrawChainRows(chain, midi, chainTarget, deckCount, assignPopup, openAssignPopup);
    if (openAssignPopup) {
        ImGui::OpenPopup(TrLabel("popup.assign_controls"));
    }

    DrawAssignControlsPopup(midi, assignPopup);

    DrawPluginPicker(chain, library, notifications);
    ImGui::PopID();
}

} // namespace imdj
