#include "ui/library_panel.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <optional>

#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "ui/modal.h"
#include "ui/ui_common.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr const char* SONG_FOLDER_DIALOG = "songFolderDialog";
constexpr const char* VST_FOLDER_DIALOG = "vstFolderDialog";

void OpenFolderDialog(const char* key, const char* startPath)
{
    IGFD::FileDialogConfig config;
    config.path = startPath[0] != '\0' ? startPath : ".";
    config.flags = ImGuiFileDialogFlags_Modal;
    ImGuiFileDialog::Instance()->OpenDialog(key, Tr("library.choose_folder"), nullptr, config);
}

std::optional<std::string> ChosenFolder(const char* key)
{
    ImGuiFileDialog* dialog = ImGuiFileDialog::Instance();
    const float em = ImGui::GetFontSize();
    if (!dialog->Display(key, ImGuiWindowFlags_NoCollapse, ImVec2(em * 36.0f, em * 22.0f))) {
        return std::nullopt;
    }

    std::optional<std::string> folder;
    if (dialog->IsOk()) {
        folder = dialog->GetCurrentPath();
    }

    dialog->Close();
    return folder;
}

struct PendingLoad {
    int deckIndex = -1;
    std::string path;
    std::string label;
    bool audible = false;
    bool unsaved = false;
};

void DrawConfirmLoadPopup(AudioEngine& engine, const PendingLoad& pending)
{
    Modal modal(TrLabel("popup.confirm_load"), ImVec2(360, 0));
    if (!modal) {
        return;
    }

    const char* reason = (pending.audible && pending.unsaved) ? Tr("library.reason_audible_unsaved")
                         : pending.audible                    ? Tr("library.reason_audible")
                                                              : Tr("library.reason_unsaved");
    ImGui::TextWrapped(
        "%s", TrFormat("library.confirm_load", DeckLabel(pending.deckIndex), reason, pending.label.c_str()).c_str()
    );
    ImGui::Spacing();
    if (ImGui::Button(TrLabel("library.load_anyway"), ImVec2(150, 0))) {
        engine.loadTrackAsync(pending.deckIndex, pending.path);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("common.cancel"), ImVec2(100, 0))) {
        ImGui::CloseCurrentPopup();
    }

    if (pending.audible && !pending.unsaved) {
        ImGui::TextDisabled("%s", Tr("library.confirm_hint"));
    }
}

void DrawFolderNode(SongLibrary& lib, const FolderNode& node)
{
    ImGui::PushID(node.fullPath.c_str());
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (lib.selectedFolderPath == node.fullPath) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        lib.selectedFolderPath = lib.selectedFolderPath == node.fullPath ? std::string() : node.fullPath;
    }

    if (open && !node.children.empty()) {
        for (const FolderNode& child : node.children) {
            DrawFolderNode(lib, child);
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void RememberSongFolders(const SongLibrary& lib, AppUiSettings& uiSettings, Notifications& notifications)
{
    uiSettings.songFolders = lib.directories;
    if (Status saved = SaveAppUiSettings(uiSettings); !saved) {
        notifications.error(saved.error());
    }
}

void DrawFolderSettings(
    SongLibrary& lib, VstLibraryUiState& vstLibrary, AppUiSettings& uiSettings, Notifications& notifications
)
{
    if (const std::optional<std::string> folder = ChosenFolder(VST_FOLDER_DIALOG)) {
        std::snprintf(vstLibrary.extraDirBuffer, sizeof(vstLibrary.extraDirBuffer), "%s", folder->c_str());
        vstLibrary.rescan();
    }

    if (const std::optional<std::string> folder = ChosenFolder(SONG_FOLDER_DIALOG)) {
        lib.addDirectory(*folder);
        RememberSongFolders(lib, uiSettings, notifications);
    }

    const float browseWidth = ImGui::CalcTextSize(Tr("common.browse")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    if (ImGui::CollapsingHeader(TrLabel("library.vst_folders"))) {
        ImGui::TextWrapped("%s", Tr("library.vst_folders_hint"));
        ImGui::SetNextItemWidth(-(browseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputTextWithHint(
            "##vstdir", Tr("library.vst_extra_hint"), vstLibrary.extraDirBuffer, sizeof(vstLibrary.extraDirBuffer)
        );
        ImGui::SameLine();
        if (ImGui::Button(TrLabel("common.browse"))) {
            OpenFolderDialog(VST_FOLDER_DIALOG, vstLibrary.extraDirBuffer);
        }

        if (ImGui::Button(TrLabel("library.vst_rescan"), ImVec2(-1, 0))) {
            vstLibrary.rescan();
        }

        ImGui::TextDisabled("%s", TrFormat("library.vst_found", static_cast<int>(vstLibrary.plugins.size())).c_str());
    }

    if (!ImGui::CollapsingHeader(TrLabel("library.song_folders"))) {
        return;
    }

    for (size_t i = 0; i < lib.directories.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextWrapped("%s", lib.directories[i].c_str());
        if (ImGui::SmallButton(TrLabel("library.remove_folder"))) {
            lib.removeDirectory(i);
            RememberSongFolders(lib, uiSettings, notifications);
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    const float addWidth = ImGui::CalcTextSize(Tr("library.add_folder")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(-(browseWidth + addWidth + ImGui::GetStyle().ItemSpacing.x * 2.0f));
    ImGui::InputTextWithHint("##newdir", Tr("library.add_folder_hint"), lib.newDirBuffer, sizeof(lib.newDirBuffer));
    ImGui::SameLine();
    if (ImGui::Button(TrLabel("common.browse"))) {
        OpenFolderDialog(SONG_FOLDER_DIALOG, lib.newDirBuffer);
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("library.add_folder"))) {
        lib.addDirectory(lib.newDirBuffer);
        lib.newDirBuffer[0] = '\0';
        RememberSongFolders(lib, uiSettings, notifications);
    }

    if (ImGui::Button(TrLabel("library.rescan"), ImVec2(-1, 0))) {
        lib.rescan();
    }

    static std::string reportedScanError;
    notifications.errorOnChange(reportedScanError, lib.lastScanError);

    ImGui::Spacing();
    if (ImGui::Checkbox(TrLabel("library.watch"), &lib.autoRescan) && lib.autoRescan) {
        lib.rescan();
        lib.lastAutoRescanTime = ImGui::GetTime();
    }

    if (lib.autoRescan) {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat(
            "##autoRescanInterval", &lib.autoRescanIntervalSeconds, 1.0f, 60.0f, Tr("library.watch_interval")
        );
    }
}

void DrawPreviewProgress(Preview& preview)
{
    double duration = preview.durationSeconds();
    float fraction =
        duration > 0.0 ? static_cast<float>(std::clamp(preview.positionSeconds() / duration, 0.0, 1.0)) : 0.0f;

    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight());
    ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_FrameBg));
    if (fraction > 0.0f) {
        ImVec2 fillEnd(topLeft.x + fraction * size.x, bottomRight.y);
        drawList->AddRectFilled(topLeft, fillEnd, ImGui::GetColorU32(ImGuiCol_PlotHistogram));
    }

    drawList->AddRect(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Border));

    ImGui::InvisibleButton("##previewBar", size);
    if (ImGui::IsItemClicked()) {
        preview.stop();
    }
}

struct ColumnWidths {
    float bpm;
    float key;
    float preview;
    float load;
};

ColumnWidths MeasureColumns(int deckCount)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    auto text = [](const char* label) { return ImGui::CalcTextSize(label, nullptr, true).x; };
    auto header = [&](const char* key) { return text(TrLabel(key)) + style.ItemInnerSpacing.x; };
    auto sortableHeader = [&](const char* key) { return header(key) + ImGui::GetFontSize(); };
    auto smallButton = [&](const char* label) { return text(label) + style.FramePadding.x * 2.0f; };

    float key = text(Tr("deck.key_unknown"));
    for (const char* label : KEY_LABELS) {
        key = std::max(key, text(label));
    }

    float load = style.ItemSpacing.x * static_cast<float>(deckCount - 1);
    for (int deck = 0; deck < deckCount; ++deck) {
        load += smallButton(std::format("{}", DeckLetter(deck)).c_str());
    }

    return ColumnWidths{
        std::max(sortableHeader("library.col_bpm"), text("000.0")),
        std::max(sortableHeader("library.col_key"), key),
        std::max(header("library.col_preview"), smallButton(">") * 3.0f),
        std::max(header("library.col_load"), load),
    };
}

bool DeckWouldBeInterrupted(AudioEngine& engine, int deckIndex, bool& audibleOut, bool& unsavedOut)
{
    const Deck& deck = engine.deck(deckIndex);
    bool audible =
        deck.hasTrack() && deck.playing.load() && deck.volume.load() > 0.0f && engine.crossfadeGain(deckIndex) > 0.0f;
    audibleOut = audible && engine.currentSettings().warnBeforeLoadingAudibleDeck;
    unsavedOut = engine.hasUnsavedChanges(deckIndex);

    return audibleOut || unsavedOut;
}

void ReadSortSpecs(SongLibrary& lib)
{
    ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
    if (!specs || !specs->SpecsDirty || specs->SpecsCount == 0) {
        return;
    }

    lib.sortColumn = static_cast<SongSortColumn>(specs->Specs[0].ColumnIndex);
    lib.sortAscending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
    lib.sortDirty = true;
    specs->SpecsDirty = false;
}

} // namespace

void DrawLibraryPanel(const UiContext& context, bool focusSearchRequested)
{
    AudioEngine& engine = context.engine;
    SongLibrary& lib = context.library;
    VstLibraryUiState& vstLibrary = context.vstLibrary;

    lib.maybeAutoRescan(ImGui::GetTime());

    AccentText(Tr("library.title"));
    DrawFolderSettings(lib, vstLibrary, context.uiSettings, context.notifications);

    AccentText(Tr("library.browse"));
    ImGui::BeginChild("folderTree", ImVec2(0, 130.0f), true);
    if (ImGui::Selectable(TrLabel("library.all_tracks"), lib.selectedFolderPath.empty())) {
        lib.selectedFolderPath.clear();
    }

    for (const FolderNode& root : lib.folderTree) {
        DrawFolderNode(lib, root);
    }

    ImGui::EndChild();

    if (focusSearchRequested) {
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", Tr("library.search_hint"), lib.searchBuffer, sizeof(lib.searchBuffer));
    ImGui::Separator();

    const std::string filter = ToLower(lib.searchBuffer);
    size_t visibleCount = 0;
    for (const SongEntry& song : lib.entries) {
        if (lib.matches(song, filter)) {
            ++visibleCount;
        }
    }

    if (visibleCount == lib.entries.size()) {
        ImGui::TextDisabled("%s", TrFormat("library.tracks_found", static_cast<int>(lib.entries.size())).c_str());
    }
    else {
        ImGui::TextDisabled(
            "%s",
            TrFormat("library.tracks_shown", static_cast<int>(visibleCount), static_cast<int>(lib.entries.size()))
                .c_str()
        );
    }

    static std::string reportedPreviewError;
    const std::string& previewError = lib.previewError.empty() ? engine.preview().lastError() : lib.previewError;
    context.notifications.errorOnChange(
        reportedPreviewError,
        previewError.empty() ? previewError : TrFormat("notify.preview_failed", previewError.c_str())
    );

    static PendingLoad pendingLoad;
    bool openConfirmLoad = false;

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Sortable;
    if (ImGui::BeginTable("songs", 5, tableFlags, ImGui::GetContentRegionAvail())) {
        ImGui::TableSetupColumn(
            TrLabel("library.col_track"), ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort
        );
        const ColumnWidths widths = MeasureColumns(engine.deckCount());
        ImGui::TableSetupColumn(TrLabel("library.col_bpm"), ImGuiTableColumnFlags_WidthFixed, widths.bpm);
        ImGui::TableSetupColumn(TrLabel("library.col_key"), ImGuiTableColumnFlags_WidthFixed, widths.key);
        ImGui::TableSetupColumn(
            TrLabel("library.col_preview"),
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
            widths.preview
        );
        ImGui::TableSetupColumn(
            TrLabel("library.col_load"), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, widths.load
        );
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ReadSortSpecs(lib);
        if (lib.sortDirty) {
            lib.applySort();
        }

        for (const SongEntry& song : lib.entries) {
            if (!lib.matches(song, filter)) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(song.path.c_str());

            ImGui::TableSetColumnIndex(0);
            ImGui::Selectable(
                "##row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, 0)
            );
            if (ImGui::IsItemHovered()) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
            }

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(song.label.c_str());

            ImGui::TableSetColumnIndex(1);
            if (song.info.bpm > 0.0) {
                ImGui::TextDisabled("%.1f", song.info.bpm);
            }

            ImGui::TableSetColumnIndex(2);
            if (song.info.key >= 0) {
                ImGui::TextDisabled("%s", KeyLabel(song.info.key));
            }

            ImGui::TableSetColumnIndex(3);
            if (engine.preview().isPlaying(song.path)) {
                DrawPreviewProgress(engine.preview());
            }
            else if (engine.preview().isLoading(song.path)) {
                ImGui::TextDisabled("...");
            }
            else if (ImGui::SmallButton(">")) {
                engine.preview().start(song.path);
                lib.previewError.clear();
            }

            ImGui::TableSetColumnIndex(4);
            for (int deckIndex = 0; deckIndex < engine.deckCount(); ++deckIndex) {
                if (deckIndex > 0) {
                    ImGui::SameLine();
                }

                char label[2] = {static_cast<char>('A' + deckIndex), '\0'};
                if (!ImGui::SmallButton(label)) {
                    continue;
                }

                bool audible = false;
                bool unsaved = false;
                if (DeckWouldBeInterrupted(engine, deckIndex, audible, unsaved)) {
                    pendingLoad = {deckIndex, song.path, song.label, audible, unsaved};
                    openConfirmLoad = true;
                }
                else {
                    engine.loadTrackAsync(deckIndex, song.path);
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (openConfirmLoad) {
        ImGui::OpenPopup(TrLabel("popup.confirm_load"));
    }

    DrawConfirmLoadPopup(engine, pendingLoad);
}

} // namespace imdj
