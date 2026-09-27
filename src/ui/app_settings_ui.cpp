#include "ui/app_settings_ui.h"

#include "imgui.h"
#include "ui/modal.h"
#include "ui/ui_common.h"
#include "ui/ui_context.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr ma_uint32 BUFFER_SIZE_PRESETS[] = {0, 128, 256, 512, 1024, 2048, 4096};
const char* RoutingSourceLabel(int source)
{
    static const char* const KEYS[ROUTING_SOURCE_COUNT] = {
        "settings.route_master_l", "settings.route_master_r", "settings.route_monitor_l", "settings.route_monitor_r"
    };
    return Tr(KEYS[source]);
}
const char* MonitoringModeLabel(MonitoringMode mode)
{
    static const char* const KEYS[MONITORING_MODE_COUNT] = {
        "settings.monitor_none", "settings.monitor_combined", "settings.monitor_separate"
    };
    return Tr(KEYS[static_cast<int>(mode)]);
}

std::string BufferSizeLabel(ma_uint32 frames)
{
    if (frames == 0) {
        return Tr("settings.buffer_auto");
    }

    return TrFormat("settings.buffer_frames", frames, static_cast<double>(frames) / SAMPLE_RATE * 1000.0);
}

void SettingTitle(const char* title)
{
    ImGui::Spacing();
    AccentText(title);
}

void SaveOrReportError(const AudioSettings& settings, Notifications& notifications)
{
    Status saved = SaveAppSettings(settings);
    if (!saved) {
        notifications.error(TrFormat("notify.settings_not_saved", saved.error().c_str()));
    }
}

void DrawChannelRouting(
    const char* id, int channelCount, RoutingMatrix& matrix, int sourceRowCount = ROUTING_SOURCE_COUNT
)
{
    ImGui::PushID(id);
    ImGui::Text("%s", TrFormat("settings.output_channels", channelCount).c_str());

    const int columns = std::clamp(channelCount, 1, MAX_ROUTING_CHANNELS);
    const int rows = std::clamp(sourceRowCount, 1, ROUTING_SOURCE_COUNT);
    if (ImGui::BeginTable("matrix", columns + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(TrLabel("settings.route_source"));
        for (int c = 0; c < columns; ++c) {
            ImGui::TableSetupColumn(TrFormat("settings.channel_n", c + 1).c_str());
        }

        ImGui::TableHeadersRow();

        for (int s = 0; s < rows; ++s) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(RoutingSourceLabel(s));
            for (int c = 0; c < columns; ++c) {
                ImGui::TableSetColumnIndex(c + 1);
                ImGui::PushID(s * MAX_ROUTING_CHANNELS + c);
                ImGui::Checkbox("##route", &matrix[s][c]);
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::PopID();
}

void DrawDevicePicker(const char* id, const std::vector<AudioDeviceInfo>& devices, std::string& deviceName)
{
    std::string label = deviceName.empty() ? Tr("settings.system_default") : deviceName;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo(id, label.c_str())) {
        if (ImGui::Selectable(TrLabel("settings.system_default"), deviceName.empty())) {
            deviceName.clear();
        }

        for (const AudioDeviceInfo& device : devices) {
            std::string itemLabel = device.isDefault ? device.name + Tr("settings.system_default_suffix") : device.name;
            if (ImGui::Selectable(itemLabel.c_str(), deviceName == device.name)) {
                deviceName = device.name;
            }
        }

        ImGui::EndCombo();
    }

    if (devices.empty()) {
        ImGui::TextDisabled("%s", Tr("settings.no_devices"));
    }
}

int DetectChannelCount(const std::vector<AudioDeviceInfo>& devices, const std::string& deviceName)
{
    for (const AudioDeviceInfo& device : devices) {
        bool isMatch = deviceName.empty() ? device.isDefault : device.name == deviceName;
        if (isMatch && device.channelCount > 0) {
            return std::min(static_cast<int>(device.channelCount), MAX_ROUTING_CHANNELS);
        }
    }

    return 2;
}

void DrawDeviceSection(
    const char* title, const char* id, const std::vector<AudioDeviceInfo>& devices, std::string& deviceName,
    int& channelCount, RoutingMatrix& matrix, int sourceRowCount = ROUTING_SOURCE_COUNT
)
{
    AccentText(title);
    DrawDevicePicker(id, devices, deviceName);
    channelCount = DetectChannelCount(devices, deviceName);
    ImGui::Spacing();
    DrawChannelRouting(id, channelCount, matrix, sourceRowCount);
}

void DrawConnectedStatus(bool connected)
{
    if (connected) {
        ImGui::TextColored(Colors().infoText, "%s", Tr("settings.connected"));
    }
    else {
        ImGui::TextDisabled("%s", Tr("settings.click_apply"));
    }
}

void DrawGeneralTab(AudioEngine& engine, AppSettingsUiState& ui, Notifications& notifications)
{
    AccentText(Tr("settings.buffer_size"));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##bufferSize", BufferSizeLabel(ui.draft.bufferSizeFrames).c_str())) {
        for (ma_uint32 preset : BUFFER_SIZE_PRESETS) {
            if (ImGui::Selectable(BufferSizeLabel(preset).c_str(), preset == ui.draft.bufferSizeFrames)) {
                ui.draft.bufferSizeFrames = preset;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::TextDisabled("%s", Tr("settings.buffer_hint"));

    SettingTitle(Tr("settings.metronome_volume"));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat(
            "##metronomeVolume", &ui.draft.metronomeVolume, 0.0f, 1.0f, Tr("settings.metronome_format")
        )) {
        engine.setMetronomeVolume(ui.draft.metronomeVolume);
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SaveOrReportError(engine.currentSettings(), notifications);
    }

    ImGui::TextDisabled("%s", Tr("settings.metronome_hint"));

    SettingTitle(Tr("settings.spinback"));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat(
            "##jogSpinback", &ui.draft.jogSpinbackDurationSec, 0.05f, 3.0f, Tr("settings.spinback_format")
        )) {
        engine.setJogSpinbackDurationSec(ui.draft.jogSpinbackDurationSec);
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SaveOrReportError(engine.currentSettings(), notifications);
    }

    ImGui::TextDisabled("%s", Tr("settings.spinback_hint"));

    SettingTitle(Tr("settings.loading"));
    ImGui::Checkbox(TrLabel("settings.warn_audible"), &ui.draft.warnBeforeLoadingAudibleDeck);
    ImGui::TextDisabled("%s", Tr("settings.warn_audible_hint"));
}

void DrawRoutingTab(AudioEngine& engine, AppSettingsUiState& ui)
{
    AccentText(Tr("settings.monitoring_mode"));
    ImGui::SetNextItemWidth(-1);
    EnumCombo(
        "##monitorMode", ui.draft.monitoringMode, MONITORING_MODE_COUNT, MonitoringModeLabel, [&](MonitoringMode mode) {
            ui.draft.monitoringMode = mode;
        }
    );

    ImGui::Spacing();
    ImGui::Separator();

    switch (ui.draft.monitoringMode) {
        case MonitoringMode::Disabled:
            DrawDeviceSection(
                Tr("settings.master_device"),
                "##masterDevice",
                ui.devices,
                ui.draft.outputDeviceName,
                ui.draft.primaryChannelCount,
                ui.draft.primaryRoutingMatrix,
                2
            );
            ImGui::TextDisabled("%s", Tr("settings.monitor_none_hint"));
            break;

        case MonitoringMode::DualOutputDevice:
            DrawDeviceSection(
                Tr("settings.output_device"),
                "##outputDevice",
                ui.devices,
                ui.draft.outputDeviceName,
                ui.draft.primaryChannelCount,
                ui.draft.primaryRoutingMatrix
            );
            ImGui::TextDisabled("%s", Tr("settings.monitor_combined_hint"));
            DrawConnectedStatus(engine.headphonesConnected());
            break;

        case MonitoringMode::SeparateOutputDevices:
            DrawDeviceSection(
                Tr("settings.master_device"),
                "##masterDevice",
                ui.devices,
                ui.draft.outputDeviceName,
                ui.draft.primaryChannelCount,
                ui.draft.primaryRoutingMatrix
            );
            ImGui::Spacing();
            ImGui::Separator();
            DrawDeviceSection(
                Tr("settings.monitor_device"),
                "##monitorDevice",
                ui.devices,
                ui.draft.monitorDeviceName,
                ui.draft.secondaryChannelCount,
                ui.draft.secondaryRoutingMatrix
            );
            DrawConnectedStatus(engine.headphonesConnected());
            break;
    }
}

} // namespace

void DrawAudioSettingsPopup(const UiContext& context)
{
    AudioEngine& engine = context.engine;
    AppSettingsUiState& ui = context.settingsUi;

    Modal modal = CenteredModal(TrLabel("popup.audio_settings"), 0.5f, 0.5f);
    if (!modal) {
        return;
    }

    if (ImGui::BeginTabBar("audioSettingsTabs")) {
        if (ImGui::BeginTabItem(TrLabel("settings.tab_general"))) {
            DrawGeneralTab(engine, ui, context.notifications);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(TrLabel("settings.tab_routing"))) {
            DrawRoutingTab(engine, ui);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button(TrLabel("settings.apply"), ImVec2(120, 0))) {
        if (Status applied = engine.applySettings(ui.draft); applied) {
            context.notifications.info(Tr("notify.settings_applied"));
            SaveOrReportError(ui.draft, context.notifications);
        }
        else {
            context.notifications.error(TrFormat("notify.settings_failed", applied.error().c_str()));
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("settings.reset"), ImVec2(150, 0))) {
        ui.draft = AudioSettings{};
    }

    ImGui::SameLine();
    modal.closeButton();
}

void DrawHotkeysPopup(const UiContext& context)
{
    AppUiSettings& uiSettings = context.uiSettings;
    HotkeysUiState& ui = context.hotkeysUi;
    Modal modal(TrLabel("popup.hotkeys"), ImVec2(360, 0));
    if (!modal) {
        return;
    }

    ImGuiKey current =
        uiSettings.focusSearchHotkey > 0 ? static_cast<ImGuiKey>(uiSettings.focusSearchHotkey) : ImGuiKey_Slash;
    AccentText(Tr("hotkeys.focus_search"));
    ImGui::Text("%s", TrFormat("hotkeys.current", ImGui::GetKeyName(current)).c_str());

    if (ui.listening) {
        BusyText(Tr("hotkeys.listening"));
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ui.listening = false;
        }
        else {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                ImGuiKey key = static_cast<ImGuiKey>(k);
                if (key == ImGuiKey_Escape || !ImGui::IsKeyPressed(key, false)) {
                    continue;
                }

                uiSettings.focusSearchHotkey = static_cast<int>(key);
                Status saved = SaveAppUiSettings(uiSettings);
                if (!saved) {
                    context.notifications.error(TrFormat("notify.hotkeys_not_saved", saved.error().c_str()));
                }

                ui.listening = false;
                break;
            }
        }
    }
    else if (ImGui::Button(TrLabel("hotkeys.rebind"))) {
        ui.listening = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button(TrLabel("common.close"), ImVec2(80, 0))) {
        ui.listening = false;
        ImGui::CloseCurrentPopup();
    }
}

} // namespace imdj
