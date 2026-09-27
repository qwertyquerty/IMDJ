#include "ui/mixer_panel.h"

#include <format>

#include "app/app_settings.h"
#include "imgui.h"
#include "ui/ui_common.h"
#include "core/strings.h"

namespace imdj {

void DrawMixerPanel(const UiContext& context, float& transitionSeconds)
{
    AudioEngine& engine = context.engine;
    ImGui::TextColored(Colors().accentBright, "%s", Tr("mixer.title"));

    ImGui::Text("%s", Tr("mixer.crossfader"));
    const float xfade = engine.crossfade();
    BoundSlider("##xfade", xfade, 0.0f, 1.0f, [&](float value) { engine.setCrossfade(value); }, "");
    ImGui::TextDisabled("A %.0f%%   |   B %.0f%%", (1.0f - xfade) * 100.0f, xfade * 100.0f);

    ImGui::SetNextItemWidth(-1);
    EnumCombo(
        "##xfadeCurve",
        engine.crossfaderCurve(),
        CROSSFADER_CURVE_COUNT,
        CrossfaderCurveLabel,
        [&](CrossfaderCurve curve) {
            engine.setCrossfaderCurve(curve);
            SaveAppSettings(engine.currentSettings());
        }
    );

    SectionTitle(Tr("mixer.auto_transition"));
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##duration", &transitionSeconds, 1.0f, 32.0f, "%.0f s");
    if (ImGui::Button(TrLabel("mixer.transition_a"), ImVec2(-1, 0))) {
        engine.startAutoTransition(0.0f, transitionSeconds);
    }

    if (ImGui::Button(TrLabel("mixer.transition_b"), ImVec2(-1, 0))) {
        engine.startAutoTransition(1.0f, transitionSeconds);
    }

    if (engine.isTransitioning()) {
        BusyText(Tr("mixer.transitioning"));
    }

    SectionTitle(Tr("mixer.decks"));
    for (int count : {2, MAX_DECK_COUNT}) {
        if (count > 2) {
            ImGui::SameLine();
        }

        if (ToggleButton(std::format("{}##decks", count).c_str(), engine.deckCount() == count, ImVec2(40, 0))) {
            engine.setDeckCount(count);
        }
    }

    SectionTitle(Tr("mixer.snap_resolution"));
    int snapIndex = static_cast<int>(engine.snapResolution());
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderInt(
            "##snapResolution",
            &snapIndex,
            0,
            SNAP_RESOLUTION_COUNT - 1,
            SnapResolutionLabel(static_cast<SnapResolution>(snapIndex))
        )) {
        engine.setSnapResolution(static_cast<SnapResolution>(snapIndex));
    }

    SectionTitle(Tr("mixer.preview_volume"));
    BoundSlider("##previewVolume", engine.preview().volume(), 0.0f, 1.5f, [&](float value) {
        engine.preview().setVolume(value);
    });

    SectionTitle(Tr("mixer.monitoring"));
    ImGui::BeginDisabled(!engine.headphonesConnected());
    for (int i = 0; i < engine.deckCount(); ++i) {
        if (i > 0) {
            ImGui::SameLine();
        }

        ImGui::PushID(i);
        Deck& deck = engine.deck(i);
        BoundCheckbox(std::format("{}##cue", DeckLabel(i, true)).c_str(), deck.cue.load(), [&](bool on) {
            deck.setCue(on);
        });
        ImGui::PopID();
    }

    BoundCheckbox(TrLabel("mixer.cue_master"), engine.cueMaster(), [&](bool on) { engine.setCueMaster(on); });
    ImGui::EndDisabled();
    if (!engine.headphonesConnected()) {
        ImGui::TextDisabled("%s", Tr("mixer.no_monitor_device"));
    }
}

void DrawMasterPanel(const UiContext& context)
{
    AudioEngine& engine = context.engine;
    const VstLibraryUiState& vstLibrary = context.vstLibrary;
    MidiController& midi = context.midi;
    ImGui::TextColored(Colors().accentBright, "%s", Tr("mixer.master"));

    ImGui::TextUnformatted(Tr("mixer.master_volume"));
    BoundSlider("##master", engine.masterVolume(), 0.0f, 1.5f, [&](float value) { engine.setMasterVolume(value); });

    SectionTitle(Tr("mixer.master_effects"));
    DrawVstChainEditor(
        engine.masterVstChain(),
        vstLibrary,
        context.notifications,
        "Master",
        midi,
        FxChainTarget::Master,
        engine.deckCount()
    );
}

void DrawPeakMonitorPanel(const UiContext& context)
{
    const AudioEngine& engine = context.engine;
    ImGui::TextColored(Colors().accentBright, "%s", Tr("mixer.peak_monitor"));
    ImGui::Separator();

    ImGui::TextColored(Colors().accentBright, "%s", Tr("mixer.master"));
    const MasterMeter& masterMeter = engine.masterMeter();
    DrawStereoMeters(masterMeter.stereo());
    ImGui::Spacing();
    DrawLevelMeter(Tr("meter.mid"), masterMeter.mid());
    DrawLevelMeter(Tr("meter.side"), masterMeter.side());
    ImGui::Spacing();
    DrawLevelMeter(Tr("meter.loud"), masterMeter.loudness(), 0.0f, false);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(Colors().accentBright, "%s", Tr("mixer.monitor"));
    DrawStereoMeters(engine.cueMeter());

    for (int i = 0; i < engine.deckCount(); ++i) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(Colors().accentBright, "%s", DeckLabel(i));
        DrawStereoMeters(engine.deckMeter(i));
    }
}

} // namespace imdj
