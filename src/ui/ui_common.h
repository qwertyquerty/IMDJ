#pragma once

#include <string>
#include <string_view>

#include "audio/audio_constants.h"
#include "audio/level_meter.h"
#include "core/clamped_atomic.h"
#include "core/paths.h"
#include "core/text.h"
#include "imgui.h"
#include "ui/theme.h"

namespace imdj {

constexpr const char* KEY_LABELS[12] = {
    "Cmaj/Am",
    "Dbmaj/Bbm",
    "Dmaj/Bm",
    "Ebmaj/Cm",
    "Emaj/C#m",
    "Fmaj/Dm",
    "F#maj/D#m",
    "Gmaj/Em",
    "Abmaj/Fm",
    "Amaj/F#m",
    "Bbmaj/Gm",
    "Bmaj/G#m",
};
const char* KeyLabel(int key);

std::string FormatTime(double seconds);
float LinearToDbfsDisplay(float level);

void ColoredText(const ImVec4& color, std::string_view text);
inline void AccentText(std::string_view text) { ColoredText(Colors().accentBright, text); }
inline void BusyText(std::string_view text) { ColoredText(Colors().busyText, text); }

const char* DeckLabel(int deckIndex, bool shortVersion = false);

void SectionTitle(const char* title);

bool ToggleButton(const char* label, bool selected, const ImVec2& size = ImVec2(0, 0));
bool ToggleSmallButton(const char* label, bool selected);

void DrawLevelMeter(const char* label, float level, float peakHold, bool showPeakHold = true);
void DrawLevelMeter(const char* label, const LevelMeter& meter, bool showPeakHold = true);
void DrawStereoMeters(const StereoMeter& meter);

template <typename Apply>
bool BoundCheckbox(const char* label, bool current, Apply apply)
{
    if (!ImGui::Checkbox(label, &current)) {
        return false;
    }

    apply(current);
    return true;
}

template <typename Apply>
bool BoundSlider(const char* id, float current, float minimum, float maximum, Apply apply, const char* format = "%.3f")
{
    ImGui::SetNextItemWidth(-1);
    if (!ImGui::SliderFloat(id, &current, minimum, maximum, format)) {
        return false;
    }

    apply(current);
    return true;
}

template <typename Apply>
bool BoundSlider(const char* id, const ClampedAtomic<float>& bound, Apply apply, const char* format = "%.3f")
{
    return BoundSlider(id, bound.load(), bound.minimum(), bound.maximum(), apply, format);
}

template <typename Apply>
bool BoundInputFloat(const char* label, float current, Apply apply, float width = 100.0f, const char* format = "%.2f")
{
    ImGui::SetNextItemWidth(width);
    if (!ImGui::InputFloat(label, &current, 0.0f, 0.0f, format)) {
        return false;
    }

    apply(current);
    return true;
}

template <typename Enum, typename LabelFn, typename Apply>
bool EnumCombo(const char* id, Enum current, int count, LabelFn label, Apply apply)
{
    if (!ImGui::BeginCombo(id, label(current))) {
        return false;
    }

    bool changed = false;
    for (int i = 0; i < count; ++i) {
        Enum option = static_cast<Enum>(i);
        if (ImGui::Selectable(label(option), option == current)) {
            apply(option);
            changed = true;
        }
    }

    ImGui::EndCombo();
    return changed;
}

} // namespace imdj
