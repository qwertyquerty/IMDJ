#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/result.h"
#include "imgui.h"

namespace imdj {

struct ThemeColors {
    ImVec4 text;
    ImVec4 textDim;
    ImVec4 windowBg;
    ImVec4 childBg;
    ImVec4 popupBg;
    ImVec4 border;
    ImVec4 frameBg;
    ImVec4 frameBgHover;
    ImVec4 frameBgActive;
    ImVec4 titleBg;
    ImVec4 titleBgActive;
    ImVec4 menuBarBg;
    ImVec4 scrollbarGrab;
    ImVec4 scrollbarGrabHover;
    ImVec4 button;
    ImVec4 header;
    ImVec4 accent;
    ImVec4 accentSoft;
    ImVec4 accentHover;
    ImVec4 accentPressed;
    ImVec4 accentBright;
    ImVec4 plot;
    ImVec4 tab;
    ImVec4 tabActive;
    ImVec4 tabUnfocused;
    ImVec4 tabUnfocusedActive;
    ImVec4 tableHeaderBg;
    ImVec4 tableBorderLight;
    ImVec4 panelBg;
    ImVec4 panelBorder;
    ImVec4 errorText;
    ImVec4 infoText;
    ImVec4 busyText;
    ImVec4 meterLow;
    ImVec4 meterMid;
    ImVec4 meterHigh;
    ImVec4 meterPeak;
    ImVec4 toggleOn;
    ImVec4 loadOk;
    ImVec4 loadHigh;
    ImVec4 loadOver;
    ImVec4 endingSoon;
    ImVec4 toastErrorBg;
    ImVec4 toastInfoBg;
    ImVec4 toastText;
    ImVec4 toastClose;
    ImVec4 toastCloseHover;
    ImVec4 waveformLow;
    ImVec4 waveformMid;
    ImVec4 waveformHigh;
    ImVec4 waveformSilent;
    ImVec4 beatPulseDim;
    ImVec4 beatPulseBright;
    ImVec4 loopRegion;
    ImVec4 loopEdge;
    ImVec4 downbeat;
    ImVec4 beat;
    ImVec4 trackStart;
    ImVec4 trackEnd;
    ImVec4 startPoint;
    ImVec4 hotCue;
    ImVec4 marker;
    ImVec4 playhead;
};

struct ThemeInfo {
    std::string id;
    std::map<std::string, std::string> names;
};

std::vector<ThemeInfo> ListThemes();
std::string ResolveTheme(const std::string& themeId);
const char* ThemeDisplayName(const ThemeInfo& theme);

Status ApplyAppearance(const std::string& themeId, float scale);
bool FontHasJapanese();

const ThemeColors& Colors();

inline ImU32 ToU32(const ImVec4& color) { return ImGui::ColorConvertFloat4ToU32(color); }
inline ImVec4 WithAlpha(const ImVec4& color, float alpha) { return ImVec4(color.x, color.y, color.z, alpha); }

inline ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

} // namespace imdj
