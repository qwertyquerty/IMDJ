#include "ui/theme.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <map>

#include "app/app_paths.h"
#include "core/json_io.h"
#include "core/paths.h"
#include "core/strings.h"
#include "imgui.h"

namespace imdj {

namespace {

constexpr float DEFAULT_FONT_SIZE = 13.0f;
constexpr ImWchar HIRAGANA_A = 0x3042;

struct ThemeFont {
    std::string path;
    float size = DEFAULT_FONT_SIZE;
};

struct ThemeField {
    const char* key;
    ImVec4 ThemeColors::* member;
    ImGuiCol fallback = ImGuiCol_COUNT;
    const char* hex = nullptr;
};

constexpr ThemeField FIELDS[] = {
    {"text", &ThemeColors::text, ImGuiCol_Text},
    {"text_dim", &ThemeColors::textDim, ImGuiCol_TextDisabled},
    {"window_bg", &ThemeColors::windowBg, ImGuiCol_WindowBg},
    {"child_bg", &ThemeColors::childBg, ImGuiCol_ChildBg},
    {"popup_bg", &ThemeColors::popupBg, ImGuiCol_PopupBg},
    {"border", &ThemeColors::border, ImGuiCol_Border},
    {"frame_bg", &ThemeColors::frameBg, ImGuiCol_FrameBg},
    {"frame_bg_hover", &ThemeColors::frameBgHover, ImGuiCol_FrameBgHovered},
    {"frame_bg_active", &ThemeColors::frameBgActive, ImGuiCol_FrameBgActive},
    {"title_bg", &ThemeColors::titleBg, ImGuiCol_TitleBg},
    {"title_bg_active", &ThemeColors::titleBgActive, ImGuiCol_TitleBgActive},
    {"menu_bar_bg", &ThemeColors::menuBarBg, ImGuiCol_MenuBarBg},
    {"scrollbar_grab", &ThemeColors::scrollbarGrab, ImGuiCol_ScrollbarGrab},
    {"scrollbar_grab_hover", &ThemeColors::scrollbarGrabHover, ImGuiCol_ScrollbarGrabHovered},
    {"button", &ThemeColors::button, ImGuiCol_Button},
    {"header", &ThemeColors::header, ImGuiCol_Header},
    {"accent", &ThemeColors::accent, ImGuiCol_CheckMark},
    {"accent_soft", &ThemeColors::accentSoft, ImGuiCol_SliderGrab},
    {"accent_hover", &ThemeColors::accentHover, ImGuiCol_ButtonHovered},
    {"accent_pressed", &ThemeColors::accentPressed, ImGuiCol_ButtonActive},
    {"accent_bright", &ThemeColors::accentBright, ImGuiCol_CheckMark},
    {"plot", &ThemeColors::plot, ImGuiCol_PlotHistogram},
    {"tab", &ThemeColors::tab, ImGuiCol_Tab},
    {"tab_active", &ThemeColors::tabActive, ImGuiCol_TabSelected},
    {"tab_unfocused", &ThemeColors::tabUnfocused, ImGuiCol_TabDimmed},
    {"tab_unfocused_active", &ThemeColors::tabUnfocusedActive, ImGuiCol_TabDimmedSelected},
    {"table_header_bg", &ThemeColors::tableHeaderBg, ImGuiCol_TableHeaderBg},
    {"table_border_light", &ThemeColors::tableBorderLight, ImGuiCol_TableBorderLight},
    {"panel_bg", &ThemeColors::panelBg, ImGuiCol_WindowBg},
    {"panel_border", &ThemeColors::panelBorder, ImGuiCol_Border},
    {"error_text", &ThemeColors::errorText, ImGuiCol_COUNT, "#FF7373"},
    {"info_text", &ThemeColors::infoText, ImGuiCol_COUNT, "#80E680"},
    {"busy_text", &ThemeColors::busyText, ImGuiCol_COUNT, "#FFCC4D"},
    {"meter_low", &ThemeColors::meterLow, ImGuiCol_COUNT, "#46C86E"},
    {"meter_mid", &ThemeColors::meterMid, ImGuiCol_COUNT, "#E6C83C"},
    {"meter_high", &ThemeColors::meterHigh, ImGuiCol_COUNT, "#E63C3C"},
    {"meter_peak", &ThemeColors::meterPeak, ImGuiCol_COUNT, "#FFFFFFDC"},
    {"toggle_on", &ThemeColors::toggleOn, ImGuiCol_COUNT, "#4DB366"},
    {"load_ok", &ThemeColors::loadOk, ImGuiCol_COUNT, "#99D999"},
    {"load_high", &ThemeColors::loadHigh, ImGuiCol_COUNT, "#FFBF4D"},
    {"load_over", &ThemeColors::loadOver, ImGuiCol_COUNT, "#FF6666"},
    {"ending_soon", &ThemeColors::endingSoon, ImGuiCol_COUNT, "#FF2626"},
    {"toast_error_bg", &ThemeColors::toastErrorBg, ImGuiCol_COUNT, "#46181CEB"},
    {"toast_info_bg", &ThemeColors::toastInfoBg, ImGuiCol_COUNT, "#182C1EEB"},
    {"toast_text", &ThemeColors::toastText, ImGuiCol_COUNT, "#F0F0F0"},
    {"toast_close", &ThemeColors::toastClose, ImGuiCol_COUNT, "#C8C8C8C8"},
    {"toast_close_hover", &ThemeColors::toastCloseHover, ImGuiCol_COUNT, "#FFFFFF"},
    {"waveform_low", &ThemeColors::waveformLow, ImGuiCol_COUNT, "#598CF2"},
    {"waveform_mid", &ThemeColors::waveformMid, ImGuiCol_COUNT, "#9E6BE6"},
    {"waveform_high", &ThemeColors::waveformHigh, ImGuiCol_COUNT, "#FF73BF"},
    {"waveform_silent", &ThemeColors::waveformSilent, ImGuiCol_COUNT, "#333D57"},
    {"beat_pulse_dim", &ThemeColors::beatPulseDim, ImGuiCol_COUNT, "#FF503C78"},
    {"beat_pulse_bright", &ThemeColors::beatPulseBright, ImGuiCol_COUNT, "#FFFF3C"},
    {"loop_region", &ThemeColors::loopRegion, ImGuiCol_COUNT, "#FFC83C37"},
    {"loop_edge", &ThemeColors::loopEdge, ImGuiCol_COUNT, "#FFC83CB4"},
    {"downbeat", &ThemeColors::downbeat, ImGuiCol_COUNT, "#E63232C8"},
    {"beat", &ThemeColors::beat, ImGuiCol_COUNT, "#FFFFFF37"},
    {"track_start", &ThemeColors::trackStart, ImGuiCol_COUNT, "#78FF785A"},
    {"track_end", &ThemeColors::trackEnd, ImGuiCol_COUNT, "#FF7878A0"},
    {"start_point", &ThemeColors::startPoint, ImGuiCol_COUNT, "#FFBE28E6"},
    {"hot_cue", &ThemeColors::hotCue, ImGuiCol_COUNT, "#50FF82E6"},
    {"marker", &ThemeColors::marker, ImGuiCol_COUNT, "#46BEFFDC"},
    {"playhead", &ThemeColors::playhead, ImGuiCol_COUNT, "#FF4646"},
};

ThemeColors current;

std::string ThemesDir() { return ResourcePath("themes"); }

// "#RRGGBB" or "#RRGGBBAA".
bool ParseColor(const std::string& text, ImVec4& out)
{
    if (text.empty() || text[0] != '#' || (text.size() != 7 && text.size() != 9)) {
        return false;
    }

    int channels[4] = {0, 0, 0, 255};
    for (size_t i = 0; i * 2 + 1 < text.size(); ++i) {
        const char* first = text.data() + 1 + i * 2;
        if (std::from_chars(first, first + 2, channels[i], 16).ptr != first + 2) {
            return false;
        }
    }

    out = ImVec4(channels[0] / 255.0f, channels[1] / 255.0f, channels[2] / 255.0f, channels[3] / 255.0f);
    return true;
}

Status ReadTheme(const std::string& id, ThemeColors& colors, ThemeFont& font)
{
    const ImGuiStyle defaults;
    for (const ThemeField& field : FIELDS) {
        if (field.hex) {
            ParseColor(field.hex, colors.*field.member);
        }
        else {
            colors.*field.member = defaults.Colors[field.fallback];
        }
    }

    Json root;
    const std::string path = ThemesDir() + "/" + id + ".json";
    if (!LoadJsonFile(path, root) || !root.is_object()) {
        return Status::Fail(TrFormat("error.file_read", path));
    }

    const Json empty = Json::object();
    const Json& values = root.contains("colors") ? root["colors"] : empty;
    for (const ThemeField& field : FIELDS) {
        ParseColor(JsonValue(values, field.key, std::string()), colors.*field.member);
    }

    font.path = JsonValue(root, "font", std::string());
    font.size = std::clamp(JsonValue(root, "font_size", DEFAULT_FONT_SIZE), 6.0f, 48.0f);

    return Status::Ok();
}

ImFont* LoadFont(const std::string& relativePath)
{
    static std::map<std::string, ImFont*> fonts;
    if (auto found = fonts.find(relativePath); found != fonts.end()) {
        return found->second;
    }

    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    const std::string path = ResourcePath(relativePath);
    std::error_code ec;
    ImFont* font = nullptr;
    if (!relativePath.empty() && std::filesystem::exists(PathFromUtf8(path), ec)) {
        font = atlas->AddFontFromFileTTF(path.c_str());
    }

    if (!font) {
        font = relativePath.empty() ? atlas->AddFontDefault() : LoadFont("");
    }

    fonts[relativePath] = font;
    return font;
}

void ApplyFont(const ThemeFont& font)
{
    ImGui::GetIO().FontDefault = LoadFont(font.path);

    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = font.size;
    style._NextFrameFontSizeBase = font.size;
}

void ApplyColors(const ThemeColors& t)
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* c = style.Colors;

    c[ImGuiCol_Text] = t.text;
    c[ImGuiCol_TextDisabled] = t.textDim;
    c[ImGuiCol_WindowBg] = t.windowBg;
    c[ImGuiCol_ChildBg] = t.childBg;
    c[ImGuiCol_PopupBg] = t.popupBg;
    c[ImGuiCol_Border] = t.border;
    c[ImGuiCol_FrameBg] = t.frameBg;
    c[ImGuiCol_FrameBgHovered] = t.frameBgHover;
    c[ImGuiCol_FrameBgActive] = t.frameBgActive;
    c[ImGuiCol_TitleBg] = t.titleBg;
    c[ImGuiCol_TitleBgActive] = t.titleBgActive;
    c[ImGuiCol_TitleBgCollapsed] = WithAlpha(t.titleBg, 0.75f);
    c[ImGuiCol_MenuBarBg] = t.menuBarBg;
    c[ImGuiCol_ScrollbarBg] = WithAlpha(t.windowBg, 0.60f);
    c[ImGuiCol_ScrollbarGrab] = t.scrollbarGrab;
    c[ImGuiCol_ScrollbarGrabHovered] = t.scrollbarGrabHover;
    c[ImGuiCol_ScrollbarGrabActive] = t.accent;
    c[ImGuiCol_CheckMark] = t.accent;
    c[ImGuiCol_SliderGrab] = t.accentSoft;
    c[ImGuiCol_SliderGrabActive] = t.accent;
    c[ImGuiCol_Button] = t.button;
    c[ImGuiCol_ButtonHovered] = t.accentHover;
    c[ImGuiCol_ButtonActive] = t.accentPressed;
    c[ImGuiCol_Header] = t.header;
    c[ImGuiCol_HeaderHovered] = WithAlpha(t.accentHover, 0.80f);
    c[ImGuiCol_HeaderActive] = t.accentPressed;
    c[ImGuiCol_Separator] = t.border;
    c[ImGuiCol_SeparatorHovered] = WithAlpha(t.accentSoft, 0.78f);
    c[ImGuiCol_SeparatorActive] = t.accent;
    c[ImGuiCol_ResizeGrip] = WithAlpha(t.accentSoft, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = WithAlpha(t.accentSoft, 0.67f);
    c[ImGuiCol_ResizeGripActive] = WithAlpha(t.accent, 0.95f);
    c[ImGuiCol_Tab] = t.tab;
    c[ImGuiCol_TabHovered] = WithAlpha(t.accentHover, 0.80f);
    c[ImGuiCol_TabSelected] = t.tabActive;
    c[ImGuiCol_TabDimmed] = t.tabUnfocused;
    c[ImGuiCol_TabDimmedSelected] = t.tabUnfocusedActive;
    c[ImGuiCol_PlotLines] = t.plot;
    c[ImGuiCol_PlotLinesHovered] = t.accent;
    c[ImGuiCol_PlotHistogram] = t.plot;
    c[ImGuiCol_PlotHistogramHovered] = t.accent;
    c[ImGuiCol_TableHeaderBg] = t.tableHeaderBg;
    c[ImGuiCol_TableBorderStrong] = WithAlpha(t.border, 1.00f);
    c[ImGuiCol_TableBorderLight] = t.tableBorderLight;
    c[ImGuiCol_TableRowBg] = WithAlpha(t.childBg, 0.00f);
    c[ImGuiCol_TableRowBgAlt] = WithAlpha(t.accentBright, 0.04f);
    c[ImGuiCol_TextSelectedBg] = WithAlpha(t.accent, 0.35f);
    c[ImGuiCol_DragDropTarget] = WithAlpha(t.accentBright, 0.90f);
    c[ImGuiCol_NavHighlight] = t.accent;
    c[ImGuiCol_NavWindowingHighlight] = WithAlpha(t.accentBright, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = WithAlpha(t.tableHeaderBg, 0.35f);
    c[ImGuiCol_ModalWindowDimBg] = WithAlpha(t.tableHeaderBg, 0.45f);

    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    current = t;
}

} // namespace

const ThemeColors& Colors() { return current; }

std::vector<ThemeInfo> ListThemes()
{
    std::vector<ThemeInfo> themes;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(PathFromUtf8(ThemesDir()), ec)) {
        if (entry.path().extension() != ".json") {
            continue;
        }

        ThemeInfo theme;
        theme.id = PathToUtf8(entry.path().stem());
        Json root;
        LoadJsonFile(PathToUtf8(entry.path()), root);
        theme.names = JsonValue(root, "name", std::map<std::string, std::string>());
        themes.push_back(std::move(theme));
    }

    std::sort(themes.begin(), themes.end(), [](const ThemeInfo& a, const ThemeInfo& b) { return a.id < b.id; });
    return themes;
}

std::string ResolveTheme(const std::string& themeId)
{
    const std::vector<ThemeInfo> themes = ListThemes();
    const bool known = std::ranges::any_of(themes, [&](const ThemeInfo& theme) { return theme.id == themeId; });

    return known || themes.empty() ? themeId : themes.front().id;
}

const char* ThemeDisplayName(const ThemeInfo& theme)
{
    for (const std::string& code : {CurrentLanguage(), std::string("en")}) {
        if (auto name = theme.names.find(code); name != theme.names.end()) {
            return name->second.c_str();
        }
    }

    return theme.id.c_str();
}

Status ApplyAppearance(const std::string& themeId, float scale)
{
    ImGui::GetStyle() = ImGuiStyle();
    ThemeColors colors;
    ThemeFont font;
    Status loaded = ReadTheme(themeId, colors, font);
    ApplyColors(colors);
    ApplyFont(font);
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleMain = scale;

    return loaded;
}

bool FontHasJapanese()
{
    ImFont* font = ImGui::GetIO().FontDefault;
    return font && font->IsGlyphInFont(HIRAGANA_A);
}

} // namespace imdj
