#include "ui/session_preset_ui.h"

#include "imgui.h"
#include "library/session_preset.h"
#include "ui/modal.h"
#include "ui/ui_common.h"
#include "ui/ui_context.h"
#include "core/strings.h"

namespace imdj {

void DrawSessionPresetsPopup(const UiContext& context)
{
    AudioEngine& engine = context.engine;
    MidiController& midi = context.midi;
    SessionPresetUiState& ui = context.presetsUi;
    Modal modal(TrLabel("popup.session_presets"), ImVec2(400, 420));
    if (!modal) {
        return;
    }

    ImGui::TextDisabled("%s", Tr("presets.hint"));
    ImGui::Spacing();

    ImGui::SetNextItemWidth(-90);
    ImGui::InputTextWithHint("##presetName", Tr("presets.name_hint"), ui.nameBuffer, sizeof(ui.nameBuffer));
    ImGui::SameLine();
    ImGui::BeginDisabled(ui.nameBuffer[0] == '\0');
    if (ImGui::Button(TrLabel("presets.save"), ImVec2(80, 0))) {
        if (Status saved = SaveSessionPreset(ui.nameBuffer, engine, midi); saved) {
            context.notifications.info(TrFormat("notify.preset_saved", std::string_view(ui.nameBuffer)));
            ui.presetNames = ListSessionPresets();
        }
        else {
            context.notifications.error(TrFormat("notify.preset_save_failed", saved.error().c_str()));
        }
    }

    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Checkbox(TrLabel("presets.restore_tracks"), &ui.restoreTracks);
    ImGui::TextDisabled("%s", Tr("presets.restore_hint"));

    ImGui::Spacing();
    AccentText(Tr("presets.saved"));
    bool openOverwriteConfirm = false;
    ImGui::BeginChild("presetList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);
    if (ui.presetNames.empty()) {
        ImGui::TextDisabled("%s", Tr("presets.none"));
    }

    for (const std::string& name : ui.presetNames) {
        ImGui::PushID(name.c_str());
        ImGui::TextUnformatted(name.c_str());
        ImGui::SameLine(180);
        if (ImGui::SmallButton(TrLabel("presets.save"))) {
            ui.pendingOverwriteName = name;
            openOverwriteConfirm = true;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(TrLabel("presets.load"))) {
            SessionPresetOptions options;
            options.restoreTracks = ui.restoreTracks;
            Result<std::string> loaded = LoadSessionPreset(name, engine, midi, options);
            if (loaded) {
                context.notifications.info(
                    loaded.value().empty() ? TrFormat("notify.preset_loaded", name.c_str()) : loaded.value()
                );
            }
            else {
                context.notifications.error(TrFormat("notify.preset_load_failed", loaded.error().c_str()));
            }
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(TrLabel("presets.delete"))) {
            if (Status removed = DeleteSessionPreset(name); removed) {
                ui.presetNames = ListSessionPresets();
                context.notifications.info(TrFormat("notify.preset_deleted", name.c_str()));
            }
            else {
                context.notifications.error(TrFormat("notify.preset_delete_failed", removed.error().c_str()));
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    if (openOverwriteConfirm) {
        ImGui::OpenPopup(TrLabel("popup.confirm_overwrite"));
    }

    {
        Modal confirm(TrLabel("popup.confirm_overwrite"), ImVec2(340, 0));
        if (confirm) {
            ImGui::TextWrapped("%s", TrFormat("presets.overwrite_confirm", ui.pendingOverwriteName.c_str()).c_str());
            ImGui::Spacing();
            if (ImGui::Button(TrLabel("presets.overwrite"), ImVec2(100, 0))) {
                if (Status saved = SaveSessionPreset(ui.pendingOverwriteName, engine, midi); saved) {
                    context.notifications.info(TrFormat("notify.preset_saved", ui.pendingOverwriteName.c_str()));
                    ui.presetNames = ListSessionPresets();
                }
                else {
                    context.notifications.error(TrFormat("notify.preset_save_failed", saved.error().c_str()));
                }

                confirm.close();
            }

            ImGui::SameLine();
            if (ImGui::Button(TrLabel("common.cancel"), ImVec2(100, 0))) {
                confirm.close();
            }
        }
    }

    ImGui::Spacing();
    modal.closeButton();
}

} // namespace imdj
