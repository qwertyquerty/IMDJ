#include "ui/ui_common.h"

#include "core/strings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace imdj {

namespace {

template <typename Press>
bool Highlighted(bool selected, Press press)
{
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, Colors().toggleOn);
    }

    const bool clicked = press();
    if (selected) {
        ImGui::PopStyleColor();
    }

    return clicked;
}

} // namespace

void ColoredText(const ImVec4& color, std::string_view text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopStyleColor();
}

bool ToggleButton(const char* label, bool selected, const ImVec2& size)
{
    return Highlighted(selected, [&] { return ImGui::Button(label, size); });
}

bool ToggleSmallButton(const char* label, bool selected)
{
    return Highlighted(selected, [&] { return ImGui::SmallButton(label); });
}

const char* DeckLabel(int deckIndex, bool shortVersion)
{
    static std::string labels[2][MAX_DECK_COUNT];
    if (deckIndex < 0 || deckIndex >= MAX_DECK_COUNT) {
        return Tr("common.deck_unknown");
    }

    std::string& label = labels[shortVersion ? 1 : 0][deckIndex];
    label = TrFormat(shortVersion ? "common.deck_label_short" : "common.deck_label", DeckLetter(deckIndex));
    return label.c_str();
}

std::string FormatTime(double seconds)
{
    if (seconds < 0.0 || !std::isfinite(seconds)) {
        seconds = 0.0;
    }

    int total = static_cast<int>(seconds + 0.5);
    int m = total / 60;
    int s = total % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", m, s);

    return buf;
}

void SectionTitle(const char* title)
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted(title);
}

const char* KeyLabel(int key)
{
    if (key < 0 || key >= 12) {
        return "?";
    }

    return KEY_LABELS[key];
}

float LinearToDbfsDisplay(float level)
{
    float db = level > 0.0005f ? 20.0f * std::log10(level) : -60.0f;
    return std::max(db, -60.0f);
}

void DrawLevelMeter(const char* label, float level, float peakHold, bool showPeakHold)
{
    ImGui::TextUnformatted(label);
    ImGui::SameLine(60.0f);

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x - 60.0f, 14.0f);
    ImVec2 p1(p0.x + size.x, p0.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, p1, ToU32(Colors().panelBg));

    float lvl = std::clamp(level, 0.0f, 1.0f);
    float fillW = lvl * size.x;
    ImU32 col = lvl > 0.89f  ? ToU32(Colors().meterHigh)
                : lvl > 0.7f ? ToU32(Colors().meterMid)
                             : ToU32(Colors().meterLow);
    if (fillW > 0.0f) {
        dl->AddRectFilled(p0, ImVec2(p0.x + fillW, p1.y), col);
    }

    if (showPeakHold) {
        float peakX = p0.x + std::clamp(peakHold, 0.0f, 1.0f) * size.x;
        dl->AddLine(ImVec2(peakX, p0.y), ImVec2(peakX, p1.y), ToU32(Colors().meterPeak), 2.0f);
    }

    dl->AddRect(p0, p1, ToU32(Colors().panelBorder));

    ImGui::Dummy(size);
    ImGui::SameLine();
    ImGui::Text("%5.1f dB", LinearToDbfsDisplay(level));
}

void DrawLevelMeter(const char* label, const LevelMeter& meter, bool showPeakHold)
{
    DrawLevelMeter(label, meter.level(), meter.peakHold(), showPeakHold);
}

void DrawStereoMeters(const StereoMeter& meter)
{
    DrawLevelMeter("L", meter.channel(0));
    DrawLevelMeter("R", meter.channel(1));
}

} // namespace imdj
