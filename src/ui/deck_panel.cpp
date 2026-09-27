#include "ui/deck_panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>

#include "audio/filter.h"
#include "imgui.h"
#include "ui/modal.h"
#include "ui/ui_common.h"
#include "core/strings.h"

namespace imdj {

namespace {

bool DeckSection(const DeckView& view, const char* name)
{
    std::vector<std::string>& collapsed = view.uiSettings.collapsedSections;
    auto it = std::find(collapsed.begin(), collapsed.end(), name);
    const bool open = it == collapsed.end();

    ImGui::SetNextItemOpen(open, ImGuiCond_Always);
    const bool nowOpen = ImGui::CollapsingHeader(TrLabel(name));
    if (nowOpen != open) {
        if (nowOpen) {
            collapsed.erase(it);
        }
        else {
            collapsed.push_back(name);
        }

        SaveAppUiSettings(view.uiSettings);
    }

    return nowOpen;
}

void DrawMarkerPadAssignPopup(const DeckView& view, size_t markerIndex)
{
    Modal modal(TrLabel("popup.assign_pad"), ImVec2(300, 0));
    if (!modal) {
        return;
    }

    if (markerIndex >= view.deck.markers.size()) {
        ImGui::TextDisabled("%s", Tr("deck.marker_gone"));
        modal.closeButton();

        return;
    }

    const Marker& marker = view.deck.markers[markerIndex];
    ImGui::TextColored(Colors().accentBright, "%s", marker.name.c_str());
    ImGui::TextDisabled("%s", Tr("deck.pad_assign_hint"));

    for (int pad = 0; pad < PADS_PER_DECK; ++pad) {
        if (pad != 0) {
            ImGui::SameLine();
        }

        ImGui::PushID(pad);

        PadAction current = view.midi.padAssignment(view.index, pad);
        bool assignedHere =
            current.assigned && current.kind == PadActionKind::JumpToMarker && current.markerIndex == markerIndex;

        if (ToggleSmallButton(std::format("{}", pad + 1).c_str(), assignedHere)) {
            if (assignedHere) {
                view.midi.clearPadAssignment(view.index, pad);
            }
            else {
                PadAction action;
                action.kind = PadActionKind::JumpToMarker;
                action.markerIndex = markerIndex;
                action.label = marker.name;
                view.midi.assignPad(view.index, pad, action);
            }
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    modal.closeButton();
}

void DrawTempoRampPopup(const DeckView& view)
{
    Modal modal(TrLabel("popup.tempo_ramp"), ImVec2(280, 0));
    if (!modal) {
        return;
    }

    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat(TrLabel("deck.ramp_target"), &view.ui.rampTargetBpm, 0.0f, 0.0f, "%.2f");
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat(TrLabel("deck.ramp_duration"), &view.ui.rampDurationSeconds, 1.0f, 60.0f, "%.0f s");

    ImGui::Spacing();
    if (ImGui::Button(TrLabel("deck.ramp_start"), ImVec2(120, 0))) {
        view.deck.startTempoRamp(view.ui.rampTargetBpm, view.ui.rampDurationSeconds);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("common.cancel"), ImVec2(100, 0))) {
        ImGui::CloseCurrentPopup();
    }
}

void DrawCalcBpmPopup(const DeckView& view, int& numBeats)
{
    Modal modal(TrLabel("popup.calc_bpm"), ImVec2(280, 0));
    if (!modal) {
        return;
    }

    if (!view.deck.hasTrack() || view.deck.durationSeconds() <= 0.0) {
        ImGui::TextDisabled("%s", Tr("deck.no_track_short"));
        modal.closeButton();

        return;
    }

    ImGui::TextWrapped("%s", TrFormat("deck.calc_bpm_hint", FormatTime(view.deck.durationSeconds()).c_str()).c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputInt(TrLabel("deck.calc_bpm_beats"), &numBeats);
    numBeats = std::max(1, numBeats);

    double bpm = static_cast<double>(numBeats) * 60.0 / view.deck.durationSeconds();
    ImGui::TextDisabled("%s", TrFormat("deck.calc_bpm_result", bpm).c_str());

    ImGui::Spacing();
    if (ImGui::Button(TrLabel("deck.calc_bpm_apply"), ImVec2(100, 0))) {
        view.deck.setBpm(bpm);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("common.cancel"), ImVec2(100, 0))) {
        ImGui::CloseCurrentPopup();
    }
}

void DrawKnobAttachButton(const DeckView& view, KnobTargetKind kind)
{
    ImGui::PushID(static_cast<int>(kind));
    ImGui::SameLine();
    if (ImGui::SmallButton(TrLabel("deck.attach_knob"))) {
        ImGui::OpenPopup("AttachKnob");
    }

    if (ImGui::BeginPopup("AttachKnob")) {
        ImGui::TextDisabled("%s", Tr("deck.bind_to"));
        for (int knob = 0; knob < FX_KNOBS_PER_DECK; ++knob) {
            FxKnobAssignment current = view.midi.fxKnobAssignment(view.index, knob);
            bool assignedHere = current.assigned && current.targetKind == kind;

            if (ImGui::Selectable(TrFormat("common.knob_n", knob + 1).c_str(), assignedHere)) {
                if (assignedHere) {
                    view.midi.clearFxKnobAssignment(view.index, knob);
                }
                else {
                    view.midi.assignFxKnobBuiltin(view.index, knob, kind);
                }
            }
        }

        ImGui::EndPopup();
    }

    ImGui::PopID();
}

void DrawTrackHeader(const DeckView& view, SongLibrary& songLibrary)
{
    const Deck& deck = view.deck;
    ImGui::TextColored(Colors().accentBright, "%s", view.label());

    const bool loading = view.engine.isDeckLoading(view.index);
    if (deck.hasTrack() && !loading) {
        const char* save = TrLabel("deck.save_metadata");
        const float width = ImGui::CalcTextSize(save, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));
        if (ImGui::SmallButton(save)) {
            if (Status saved = view.engine.saveTrackMetadata(view.index); saved) {
                view.notifications.info(TrFormat("notify.metadata_saved", view.label()));
                songLibrary.setCachedInfo(deck.filePath, deck.metadata().info);
            }
            else {
                view.notifications.error(std::string(view.label()) + ": " + saved.error());
            }
        }
    }

    if (loading) {
        ImGui::TextWrapped("%s", Tr("deck.loading"));
        ImGui::ProgressBar(view.engine.deckLoadProgress(view.index), ImVec2(-1, ImGui::GetTextLineHeight()));
    }
    else if (!deck.hasTrack()) {
        ImGui::TextWrapped("%s", Tr("deck.no_track"));
    }
    else {
        ImGui::TextWrapped("%s", deck.displayTitle().c_str());
        if (!deck.info.artist.empty()) {
            ImGui::TextDisabled("%s", deck.info.artist.c_str());
        }
    }

    view.notifications.errorOnChange(view.ui.reportedLoadError, view.engine.lastLoadError(view.index));
}

void DrawEffectiveKey(int key, float rate)
{
    if (key < 0 || std::fabs(rate - 1.0f) <= 1e-4f) {
        return;
    }

    double semitones = 12.0 * std::log2(static_cast<double>(rate));
    int wholeSemitones = static_cast<int>(std::lround(semitones));
    double cents = (semitones - wholeSemitones) * 100.0;
    int effectiveKey = ((key + wholeSemitones) % 12 + 12) % 12;

    ImGui::SameLine();
    ImGui::TextDisabled("%s", TrFormat("deck.effective_key", KEY_LABELS[effectiveKey], cents).c_str());
}

void DrawKeyPicker(const DeckView& view);

template <size_t N>
void MetadataInput(const std::string& key, char (&buffer)[N], std::string& value)
{
    ImGui::SetNextItemWidth(-60);
    ImGui::InputTextWithHint(TrLabel(key), Tr(key + "_hint"), buffer, N);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        value = buffer;
    }
}

void DrawMetadataSection(const DeckView& view)
{
    if (!DeckSection(view, "deck.section.metadata")) {
        return;
    }

    Deck& deck = view.deck;
    DeckUiState& ui = view.ui;

    if (ui.metadataPath != deck.filePath) {
        ui.metadataPath = deck.filePath;
        std::snprintf(ui.titleBuf, sizeof(ui.titleBuf), "%s", deck.info.title.c_str());
        std::snprintf(ui.artistBuf, sizeof(ui.artistBuf), "%s", deck.info.artist.c_str());
    }

    ImGui::BeginDisabled(!deck.hasTrack());
    MetadataInput("deck.meta_title", ui.titleBuf, deck.info.title);
    MetadataInput("deck.meta_artist", ui.artistBuf, deck.info.artist);

    DrawKeyPicker(view);
    ImGui::EndDisabled();
}

void DrawKeyPicker(const DeckView& view)
{
    ImGui::SetNextItemWidth(140);
    if (ImGui::BeginCombo(TrLabel("deck.key"), KeyLabel(view.deck.info.key))) {
        if (ImGui::Selectable(TrLabel("deck.key_unknown"), view.deck.info.key < 0)) {
            view.deck.setKey(-1);
        }

        for (int k = 0; k < 12; ++k) {
            if (ImGui::Selectable(KEY_LABELS[k], view.deck.info.key == k)) {
                view.deck.setKey(k);
            }
        }

        ImGui::EndCombo();
    }

    DrawEffectiveKey(view.deck.info.key, view.deck.playbackRate.load());
}

void ApplyTap(const DeckView& view)
{
    constexpr size_t MAX_TAP_HISTORY = 64;
    constexpr double TAP_RESET_GAP_SECONDS = 3.0;

    std::vector<double>& taps = view.ui.tapTimes;
    double now = ImGui::GetTime();
    if (!taps.empty() && now - taps.back() > TAP_RESET_GAP_SECONDS) {
        taps.clear();
    }

    taps.push_back(now);
    if (taps.size() > MAX_TAP_HISTORY) {
        taps.erase(taps.begin());
    }

    if (taps.size() < 2) {
        return;
    }

    double total = 0.0;
    for (size_t i = 1; i < taps.size(); ++i) {
        total += taps[i] - taps[i - 1];
    }

    double average = total / static_cast<double>(taps.size() - 1);
    if (average > 0.02) {
        view.deck.setBpm(60.0 / average);
    }
}

void DrawTempoSection(const DeckView& view)
{
    if (!DeckSection(view, "deck.section.tempo")) {
        return;
    }

    Deck& deck = view.deck;

    BoundCheckbox(TrLabel("deck.metronome"), deck.metronomeEnabled.load(), [&](bool on) {
        deck.setMetronomeEnabled(on);
    });

    BoundInputFloat(TrLabel("deck.true_bpm"), static_cast<float>(deck.bpm.load()), [&](float bpm) {
        deck.setBpm(bpm);
    });
    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.tap"))) {
        ApplyTap(view);
    }

    ImGui::SameLine();

    static int calcBpmBeats = 16;
    if (ImGui::Button(TrLabel("deck.calc"))) {
        calcBpmBeats = 16;
        ImGui::OpenPopup(TrLabel("popup.calc_bpm"));
    }

    DrawCalcBpmPopup(view, calcBpmBeats);

    const bool locked = view.engine.syncEnabled(view.index);
    ImGui::BeginDisabled(locked);
    BoundInputFloat(TrLabel("deck.target_bpm"), static_cast<float>(deck.effectiveBpm()), [&](float bpm) {
        deck.setTargetBpm(bpm);
    });
    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.reset_rate"))) {
        deck.setPlaybackRate(1.0f);
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.sync"))) {
        view.engine.syncTempoToReferenceDeck(view.index);
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.ramp"))) {
        view.ui.rampTargetBpm = static_cast<float>(deck.effectiveBpm());
        ImGui::OpenPopup(TrLabel("popup.tempo_ramp"));
    }

    ImGui::EndDisabled();
    DrawTempoRampPopup(view);
    ImGui::SameLine();
    ImGui::TextDisabled("(%.1f%%)", deck.playbackRate.load() * 100.0f);

    BoundCheckbox(TrLabel("deck.keylock"), deck.keylock.load(), [&](bool on) { deck.setKeylock(on); });
    ImGui::SameLine();
    BoundCheckbox(TrLabel("deck.sync_lock"), locked, [&](bool on) { view.engine.setSyncEnabled(view.index, on); });

    if (locked) {
        ImGui::SameLine();
        int master = view.engine.syncMasterDeck();
        if (master >= 0) {
            ImGui::TextColored(Colors().accentBright, "%s", TrFormat("deck.sync_master", DeckLetter(master)).c_str());
        }
        else {
            ImGui::TextDisabled("%s", Tr("deck.sync_no_master"));
        }
    }

    if (deck.tempoRamp.active()) {
        BusyText(TrFormat("deck.ramping", deck.playbackRate.load() * 100.0f));
    }
}

void DrawBeatAlignSection(const DeckView& view)
{
    if (!DeckSection(view, "deck.section.beat_align")) {
        return;
    }

    float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button(TrLabel("deck.snap"), ImVec2(halfWidth, 0))) {
        view.engine.snapToNearestGrid(view.index);
    }

    ImGui::SameLine();

    const int reference = view.engine.tempoReferenceDeck(view.index);
    const std::string label = reference >= 0 ? TrFormat("deck.align_to", DeckLetter(reference)) : Tr("deck.align");
    ImGui::BeginDisabled(reference < 0);
    if (ImGui::Button(label.c_str(), ImVec2(halfWidth, 0))) {
        view.engine.alignBeatToOtherDeck(view.index);
    }

    ImGui::EndDisabled();
    ImGui::TextDisabled(
        "%s", TrFormat("deck.snap_resolution", SnapResolutionLabel(view.engine.snapResolution())).c_str()
    );
}

void DrawHotCueSection(const DeckView& view)
{
    if (!DeckSection(view, "deck.section.hot_cue")) {
        return;
    }

    if (ImGui::Button(TrLabel("deck.set_hot_cue"))) {
        view.deck.setHotCue();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!view.deck.hotCueSet.load());
    if (ImGui::Button(TrLabel("deck.go_hot_cue"))) {
        view.engine.jumpToHotCue(view.index);
    }

    ImGui::EndDisabled();
    if (view.deck.hotCueSet.load()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", FormatTime(view.deck.hotCue().count()).c_str());
    }
}

void DrawLoopSection(const DeckView& view)
{
    if (!DeckSection(view, "deck.section.loop")) {
        return;
    }

    Deck& deck = view.deck;

    if (ImGui::Button(TrLabel("deck.loop_in"))) {
        deck.setLoopInHere();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.loop_out"))) {
        deck.setLoopOutHere();
    }

    ImGui::SameLine();
    BoundCheckbox(TrLabel("deck.loop_on"), deck.loopEnabled.load(), [&](bool on) { deck.setLoopEnabled(on); });
    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.loop_clear"))) {
        deck.clearLoop();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.loop_all"))) {
        deck.setLoopToWholeTrack();
    }

    BoundCheckbox(TrLabel("deck.quantize"), deck.quantizeLoops.load(), [&](bool on) { deck.setQuantizeLoops(on); });

    ImGui::BeginDisabled(!(deck.loopInSet.load() && deck.loopOutSet.load()));
    if (ImGui::Button(TrLabel("deck.loop_halve"))) {
        deck.halveLoop();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.loop_double"))) {
        deck.doubleLoop();
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.loop_back"))) {
        deck.shiftLoop(-1);
    }

    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.loop_forward"))) {
        deck.shiftLoop(1);
    }

    ImGui::EndDisabled();

    ImGui::TextDisabled("%s", Tr("deck.quick_loop"));
    constexpr double QUICK_LOOP_BEATS[] = {1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0};
    for (double beats : QUICK_LOOP_BEATS) {
        ImGui::SameLine();
        if (ImGui::Button(std::format("{}B", beats).c_str())) {
            deck.setLoopLengthBeats(beats);
        }
    }
}

void DrawMarkersSection(const DeckView& view)
{
    if (!DeckSection(view, "deck.section.markers")) {
        return;
    }

    Deck& deck = view.deck;

    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint(
        "##markerName", Tr("deck.marker_name_hint"), view.ui.markerNameBuf, sizeof(view.ui.markerNameBuf)
    );
    ImGui::SameLine();
    if (ImGui::Button(TrLabel("deck.marker_add"))) {
        deck.addMarkerHere(view.ui.markerNameBuf);
        view.ui.markerNameBuf[0] = '\0';
    }

    static size_t padAssignMarker = 0;
    bool openPadAssign = false;

    for (size_t i = 0; i < deck.markers.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton(TrLabel("deck.marker_go"))) {
            deck.jumpToMarker(i);
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(deck.markers[i].name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", FormatTime(static_cast<double>(deck.markers[i].frame) / SAMPLE_RATE).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(TrLabel("deck.marker_pad"))) {
            padAssignMarker = i;
            openPadAssign = true;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            deck.removeMarker(i);
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (openPadAssign) {
        ImGui::OpenPopup(TrLabel("popup.assign_pad"));
    }

    DrawMarkerPadAssignPopup(view, padAssignMarker);
}

void DrawChannelSection(const DeckView& view)
{
    Deck& deck = view.deck;

    if (!ImGui::BeginTable("faderGainPan", 3, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", Tr("deck.fader"));
    ImGui::TableNextColumn();
    ImGui::Text("%s", Tr("deck.gain"));
    DrawKnobAttachButton(view, KnobTargetKind::DeckGain);
    ImGui::TableNextColumn();
    ImGui::Text("%s", Tr("deck.pan"));
    DrawKnobAttachButton(view, KnobTargetKind::DeckPan);
    ImGui::SameLine();
    if (ImGui::SmallButton(TrLabel("deck.pan_center"))) {
        deck.setPan(0.0f);
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    BoundSlider("##fader", deck.volume, [&](float v) { deck.setVolume(v); });
    ImGui::TableNextColumn();
    BoundSlider("##gain", deck.gain, [&](float v) { deck.setGain(v); });
    ImGui::TableNextColumn();
    BoundSlider("##pan", deck.pan, [&](float v) { deck.setPan(v); }, "%.2f");
    ImGui::EndTable();
}

void DrawEqSection(const DeckView& view)
{
    Deck& deck = view.deck;
    struct Band {
        const char* label;
        const char* id;
        KnobTargetKind knob;
        const ClampedAtomic<float>& gain;
        void (Deck::*setter)(float);
    };

    const Band bands[] = {
        {Tr("deck.eq_low"), "##eqLow", KnobTargetKind::EqLow, deck.eqLow, &Deck::setEqLow},
        {Tr("deck.eq_mid"), "##eqMid", KnobTargetKind::EqMid, deck.eqMid, &Deck::setEqMid},
        {Tr("deck.eq_high"), "##eqHigh", KnobTargetKind::EqHigh, deck.eqHigh, &Deck::setEqHigh},
    };

    ImGui::Spacing();
    if (!ImGui::BeginTable("eq", 3, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }

    ImGui::TableNextRow();
    for (const Band& band : bands) {
        ImGui::TableNextColumn();
        ImGui::Text("%s", band.label);
        DrawKnobAttachButton(view, band.knob);
        ImGui::SameLine();
        if (ImGui::SmallButton(std::format("C{}", band.id).c_str())) {
            (deck.*band.setter)(0.0f);
        }
    }

    ImGui::TableNextRow();
    for (const Band& band : bands) {
        ImGui::TableNextColumn();
        BoundSlider(band.id, band.gain, [&](float db) { (deck.*band.setter)(db); }, "%.1f dB");
    }

    ImGui::EndTable();
}

void DrawFilterSection(const DeckView& view)
{
    Deck& deck = view.deck;
    ImGui::Text("%s", Tr("deck.filter"));
    DrawKnobAttachButton(view, KnobTargetKind::Filter);
    ImGui::SameLine();
    if (ImGui::SmallButton(TrLabel("deck.filter_off"))) {
        deck.setFilter(0.0f);
    }

    const float position = deck.filter.load();
    const std::string overlay =
        OneKnobFilter::engaged(position)
            ? std::format("{} {:.0f} Hz", position < 0.0f ? "LP" : "HP", OneKnobFilter::cutoffHz(position))
            : Tr("deck.filter_off_state");
    BoundSlider("##filter", deck.filter, [&](float value) { deck.setFilter(value); }, overlay.c_str());
}

void DrawNormalizeToggle(const DeckView& view)
{
    Deck& deck = view.deck;
    BoundCheckbox(TrLabel("deck.normalize"), deck.normalizeEnabled.load(), [&](bool on) {
        deck.setNormalizeEnabled(on);
    });
    if (!deck.hasTrack()) {
        return;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(%+.1f dB)", 20.0f * std::log10(std::max(0.0001f, deck.normalizeGain.load())));
}

} // namespace

void DrawDeckPanel(const UiContext& context, int deckIndex)
{
    const DeckView view = context.deckView(deckIndex);

    DrawTrackHeader(view, context.library);
    DrawMetadataSection(view);
    if (DeckSection(view, "deck.section.channel")) {
        DrawChannelSection(view);
        DrawEqSection(view);
        DrawFilterSection(view);
        DrawNormalizeToggle(view);
    }

    DrawTempoSection(view);
    DrawLoopSection(view);
    DrawHotCueSection(view);
    DrawMarkersSection(view);
    DrawBeatAlignSection(view);
    if (DeckSection(view, "deck.section.effects")) {
        DrawVstChainEditor(
            view.deck.vstChain,
            context.vstLibrary,
            view.notifications,
            view.label(),
            view.midi,
            DeckFxChainTarget(deckIndex),
            view.engine.deckCount()
        );
    }
}

bool DeckNearingEnd(const AudioEngine& engine, int deckIndex)
{
    constexpr double ENDING_SOON_THRESHOLD_SEC = 20.0;
    const Deck& deck = engine.deck(deckIndex);
    if (!deck.playing.load() || !deck.hasTrack()) {
        return false;
    }

    double remaining = deck.durationSeconds() - deck.positionSeconds().count();
    return remaining > 0.0 && remaining <= ENDING_SOON_THRESHOLD_SEC;
}

} // namespace imdj
