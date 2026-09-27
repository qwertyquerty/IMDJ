#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>
#include <vector>

#include "app/app_paths.h"
#include "app/app_settings.h"
#include "app/app_shell.h"
#include "audio/audio_engine.h"
#include "library/session_preset.h"
#include "library/song_library.h"
#include "midi/midi_controller.h"
#include "ui/app_settings_ui.h"
#include "ui/app_ui_settings.h"
#include "ui/bindings_ui.h"
#include "ui/dancer.h"
#include "ui/deck_panel.h"
#include "ui/library_panel.h"
#include "ui/midi_controller_ui.h"
#include "ui/mixer_panel.h"
#include "ui/session_preset_ui.h"
#include "ui/signal_chain_ui.h"
#include "ui/theme.h"
#include "ui/ui_context.h"
#include "ui/transport_panel.h"
#include "ui/vst_library_ui.h"

#include "imgui.h"
#include "core/strings.h"

using namespace imdj;

namespace {

struct App {
    AppShell shell;
    AudioEngine engine;
    MidiController midi;
    SongLibrary library;
    VstLibraryUiState vstLibrary;
    std::vector<DancerInfo> dancers;
    std::array<DeckUiState, MAX_DECK_COUNT> deckUi{};

    AppUiSettings uiSettings;
    AppSettingsUiState settingsUi;
    MidiControllerUiState midiUi;
    BindingsUiState bindingsUi;
    SessionPresetUiState presetsUi;
    HotkeysUiState hotkeysUi;
    Notifications notifications;
    std::string pendingLanguage;

    float transitionSeconds = 8.0f;
    int barsVisible = 2;

    UiContext context()
    {
        return UiContext{
            engine,
            midi,
            library,
            vstLibrary,
            deckUi,
            dancers,
            uiSettings,
            settingsUi,
            midiUi,
            bindingsUi,
            presetsUi,
            hotkeysUi,
            notifications
        };
    }
};

void LoadLastSessionPreset(App& app)
{
    std::string name = GetLastUsedPresetName();
    if (name.empty()) {
        return;
    }

    SessionPresetOptions options;
    options.restoreTracks = false;
    Result<std::string> loaded = LoadSessionPreset(name, app.engine, app.midi, options);
    if (!loaded) {
        app.notifications.error(TrFormat("notify.preset_autoload_failed", name.c_str(), loaded.error().c_str()));
    }
    else if (loaded.value().empty()) {
        app.notifications.info(TrFormat("notify.preset_autoloaded", name.c_str()));
    }
    else {
        app.notifications.error(loaded.value());
    }
}

void LoadDefaultDancers(App& app)
{
    app.dancers = ScanDancers(ResourcePath("dancers"));
    if (app.dancers.empty()) {
        return;
    }

    for (int i = 0; i < MAX_DECK_COUNT; ++i) {
        int index = i < static_cast<int>(app.dancers.size()) ? i : 0;
        app.deckUi[i].selectedDancerIndex = index;
        app.deckUi[i].dancer.load(app.dancers[index]);
    }
}

void ApplyUiAppearance(App& app)
{
    app.uiSettings.theme = ResolveTheme(app.uiSettings.theme);
    if (Status applied = ApplyAppearance(app.uiSettings.theme, app.uiSettings.uiScale); !applied) {
        app.notifications.error(
            TrFormat("notify.theme_load_failed", app.uiSettings.theme.c_str(), applied.error().c_str())
        );
    }
}

void ApplyLanguage(App& app, const std::string& code)
{
    if (Status loaded = LoadLanguage(ResourcePath("lang"), code); !loaded) {
        app.notifications.error(TrFormat("notify.language_load_failed", code.c_str(), loaded.error().c_str()));
        return;
    }

    app.uiSettings.language = code;
    SaveAppUiSettings(app.uiSettings);
}

void CheckFontCoverage(App& app)
{
    if (app.uiSettings.language == "ja" && !FontHasJapanese()) {
        app.notifications.error(Tr("notify.no_cjk_font"));
    }
}

bool StartUp(App& app)
{
    app.uiSettings = LoadAppUiSettings();
    ApplyLanguage(app, app.uiSettings.language);
    if (Status started = app.shell.init("IMDJ", 1600, 900); !started) {
        std::fprintf(stderr, "%s\n", started.error().c_str());
        return false;
    }

    ApplyUiAppearance(app);
    CheckFontCoverage(app);

    if (Status started = app.engine.init(LoadAppSettings()); !started) {
        app.notifications.error(TrFormat("notify.audio_device_error", started.error().c_str()));
        std::fprintf(stderr, "Audio init failed: %s\n", started.error().c_str());
    }

    app.midi.autoConnectLastDevice(&app.engine);
    LoadLastSessionPreset(app);
    app.library.directories = app.uiSettings.songFolders;
    app.library.rescan();
    LoadDefaultDancers(app);
    app.vstLibrary.rescan();

    return true;
}

void ShutDown(App& app)
{
    app.engine.shutdown();
    for (DeckUiState& ui : app.deckUi) {
        ui.dancer.unload();
    }

    for (int i = 0; i < MAX_DECK_COUNT; ++i) {
        app.engine.deck(i).vstChain.clear();
    }

    app.engine.masterVstChain().clear();
    app.shell.shutdown();
}

void DrawDspLoad(const AudioEngine& engine)
{
    float load = engine.dspLoad();
    const std::string text = TrFormat("menu.dsp_load", load * 100.0f);

    float width = ImGui::CalcTextSize(text.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - width - ImGui::GetStyle().ItemSpacing.x * 2.0f);
    const ThemeColors& colors = Colors();
    const ImVec4 color = load >= 1.0f ? colors.loadOver : load >= 0.8f ? colors.loadHigh : colors.loadOk;
    ImGui::TextColored(color, "%s", text.c_str());
}

void DrawMenuBarAndPopups(App& app)
{
    const UiContext ui = app.context();
    const char* openPopup = nullptr;
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(TrLabel("menu.settings"))) {
            if (ImGui::MenuItem(TrLabel("menu.audio_settings"))) {
                openPopup = TrLabel("popup.audio_settings");
            }

            if (ImGui::MenuItem(TrLabel("menu.midi_controller"))) {
                openPopup = TrLabel("popup.midi_controller");
            }

            if (ImGui::MenuItem(TrLabel("menu.bindings"))) {
                openPopup = TrLabel("popup.bindings");
            }

            if (ImGui::MenuItem(TrLabel("menu.hotkeys"))) {
                openPopup = TrLabel("popup.hotkeys");
            }

            if (ImGui::MenuItem(TrLabel("menu.signal_chain"))) {
                openPopup = TrLabel("popup.signal_chain");
            }

            if (ImGui::BeginMenu(TrLabel("menu.ui_scale"))) {
                for (float scale : {0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f}) {
                    const bool selected = std::fabs(app.uiSettings.uiScale - scale) < 0.01f;
                    if (ImGui::MenuItem(std::format("{:.0f}%", scale * 100.0f).c_str(), nullptr, selected)) {
                        app.uiSettings.uiScale = scale;
                        ApplyUiAppearance(app);
                        SaveAppUiSettings(app.uiSettings);
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(TrLabel("menu.language"))) {
                for (const std::string& code : AvailableLanguages(ResourcePath("lang"))) {
                    const std::string key = "lang." + code;
                    const char* label = Tr(key);
                    if (key == label) {
                        label = code.c_str();
                    }

                    if (ImGui::MenuItem(
                            std::format("{}##{}", label, key.c_str()).c_str(), nullptr, code == CurrentLanguage()
                        )) {
                        app.pendingLanguage = code;
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem(TrLabel("menu.open_app_data"))) {
                OpenFolderInFileManager(AppDataDir());
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(TrLabel("menu.presets"))) {
            if (ImGui::MenuItem(TrLabel("menu.session_presets"))) {
                openPopup = TrLabel("popup.session_presets");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(TrLabel("menu.theme"))) {
            for (const ThemeInfo& theme : ListThemes()) {
                const std::string label = std::format("{}##theme_{}", ThemeDisplayName(theme), theme.id);
                if (ImGui::MenuItem(label.c_str(), nullptr, app.uiSettings.theme == theme.id)) {
                    app.uiSettings.theme = theme.id;
                    ApplyUiAppearance(app);
                    CheckFontCoverage(app);
                    SaveAppUiSettings(app.uiSettings);
                }
            }

            ImGui::EndMenu();
        }

        DrawDspLoad(app.engine);
        ImGui::EndMenuBar();
    }

    if (openPopup) {
        std::string name = openPopup;
        if (name == TrLabel("popup.audio_settings")) {
            app.settingsUi.draft = app.engine.currentSettings();
            app.settingsUi.devices = app.engine.listPlaybackDevices();
        }
        else if (name == TrLabel("popup.midi_controller")) {
            app.midiUi.ports = app.midi.listPorts();
        }
        else if (name == TrLabel("popup.session_presets")) {
            app.presetsUi.presetNames = ListSessionPresets();
        }

        ImGui::OpenPopup(openPopup);
    }

    DrawAudioSettingsPopup(ui);
    DrawMidiControllerPopup(ui);
    DrawBindingsManagerPopup(ui);
    DrawSessionPresetsPopup(ui);
    DrawHotkeysPopup(ui);
    DrawSignalChainPopup(ui);
}

void DrawEndingSoonOutline(bool endingSoon)
{
    if (!endingSoon) {
        return;
    }

    float phase = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);
    const ImVec4& outline = Colors().endingSoon;
    ImU32 color = ToU32(WithAlpha(outline, outline.w * (0.5f + 0.5f * phase)));

    constexpr float THICKNESS = 3.0f;
    const float inset = THICKNESS * 0.5f;
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(min, max, false);
    drawList->AddRect(
        ImVec2(min.x + inset, min.y + inset),
        ImVec2(max.x - inset, max.y - inset),
        color,
        ImGui::GetStyle().ChildRounding,
        0,
        THICKNESS
    );
    drawList->PopClipRect();
}

void DrawDashboard(App& app, bool focusSearch)
{
    const UiContext ui = app.context();
    const float scale = app.uiSettings.uiScale;
    const float TRANSPORT_HEIGHT = 340.0f * scale;
    const float DECK_WIDTH = 400.0f * scale;
    const float MIXER_WIDTH = 260.0f * scale;
    const float PEAK_MONITOR_WIDTH = 220.0f * scale;

    const ImVec2 spacingXY = ImGui::GetStyle().ItemSpacing;
    const float spacing = spacingXY.y;
    const float topHeight = std::max(220.0f * scale, ImGui::GetContentRegionAvail().y - TRANSPORT_HEIGHT - spacing);

    const int deckCount = app.engine.deckCount();
    const int deckColumns = deckCount > 2 ? 2 : deckCount;
    const int deckRows = deckCount / deckColumns;
    const float deckRegionWidth = deckColumns * DECK_WIDTH + (deckColumns - 1) * spacingXY.x;
    const float deckCellHeight = (topHeight - (deckRows - 1) * spacing) / static_cast<float>(deckRows);

    ImGui::BeginChild(
        "DeckRow",
        ImVec2(deckRegionWidth, topHeight),
        false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
    for (int i = 0; i < deckCount; ++i) {
        if (i % deckColumns != 0) {
            ImGui::SameLine();
        }

        ImGui::BeginChild(std::format("Deck{}", DeckLetter(i)).c_str(), ImVec2(DECK_WIDTH, deckCellHeight), true);
        DrawDeckPanel(ui, i);
        DrawEndingSoonOutline(DeckNearingEnd(app.engine, i));
        ImGui::EndChild();
    }

    ImGui::EndChild();
    ImGui::SameLine();

    const float masterHeight = std::min(240.0f * scale, topHeight * 0.45f);
    const float mixerHeight = std::max(120.0f * scale, topHeight - masterHeight - spacing);

    ImGui::BeginGroup();
    ImGui::BeginChild("MixerPanel", ImVec2(MIXER_WIDTH, mixerHeight), true);
    DrawMixerPanel(ui, app.transitionSeconds);
    ImGui::EndChild();
    ImGui::BeginChild("MasterPanel", ImVec2(MIXER_WIDTH, masterHeight), true);
    DrawMasterPanel(ui);
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::SameLine();

    ImGui::BeginChild("LibraryPanel", ImVec2(ImGui::GetContentRegionAvail().x, topHeight), true);
    DrawLibraryPanel(ui, focusSearch);
    ImGui::EndChild();

    float transportWidth = ImGui::GetContentRegionAvail().x - PEAK_MONITOR_WIDTH - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginChild("TransportPanel", ImVec2(std::max(200.0f, transportWidth), TRANSPORT_HEIGHT), true);
    DrawTransportPanel(ui, app.barsVisible);
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("PeakMonitorPanel", ImVec2(PEAK_MONITOR_WIDTH, TRANSPORT_HEIGHT), true);
    DrawPeakMonitorPanel(ui);
    ImGui::EndChild();
}

void DrawFrame(App& app)
{
    constexpr ImGuiWindowFlags ROOT_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                                            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("imdj_root", nullptr, ROOT_FLAGS);

    DrawMenuBarAndPopups(app);

    ImGuiKey searchHotkey =
        app.uiSettings.focusSearchHotkey > 0 ? static_cast<ImGuiKey>(app.uiSettings.focusSearchHotkey) : ImGuiKey_Slash;
    bool focusSearch = !io.WantTextInput && ImGui::IsKeyPressed(searchHotkey, false);

    DrawDashboard(app, focusSearch);
    app.notifications.draw();

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace

int main()
{
    App app;
    if (!StartUp(app)) {
        return 1;
    }

    while (!app.shell.shouldClose()) {
        if (!app.pendingLanguage.empty()) {
            ApplyLanguage(app, app.pendingLanguage);
            CheckFontCoverage(app);
            app.pendingLanguage.clear();
        }

        if (!app.shell.beginFrame()) {
            continue;
        }

        app.engine.update(app.shell.time());
        app.midi.update();
        for (int i = 0; i < app.engine.deckCount(); ++i) {
            app.engine.deck(i).vstChain.pumpEditors();
        }

        app.engine.masterVstChain().pumpEditors();

        DrawFrame(app);
        app.shell.endFrame();
    }

    ShutDown(app);
    return 0;
}
