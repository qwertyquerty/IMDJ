#include "ui/notifications.h"

#include "imgui.h"
#include "ui/theme.h"

namespace imdj {

namespace {

constexpr double INFO_LIFETIME_SECONDS = 4.0;
constexpr float TOAST_WIDTH = 360.0f;
constexpr float TOAST_PADDING = 10.0f;
constexpr float TOAST_GAP = 6.0f;
constexpr float MARGIN = 14.0f;
constexpr float CLOSE_SIZE = 14.0f;
constexpr size_t MAX_VISIBLE = 6;

} // namespace

void Notifications::info(std::string message) { entries_.push_back({std::move(message), false, ImGui::GetTime()}); }

void Notifications::error(std::string message) { entries_.push_back({std::move(message), true, ImGui::GetTime()}); }

void Notifications::errorOnChange(std::string& reported, const std::string& current)
{
    if (current == reported) {
        return;
    }

    reported = current;
    if (!current.empty()) {
        error(current);
    }
}

void Notifications::draw()
{
    const double now = ImGui::GetTime();
    std::erase_if(entries_, [&](const Entry& e) { return !e.isError && now - e.postedAt > INFO_LIFETIME_SECONDS; });
    if (entries_.empty()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const float textWidth = TOAST_WIDTH - TOAST_PADDING * 2.0f - CLOSE_SIZE - TOAST_PADDING;
    const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const ImVec2 mouse = io.MousePos;

    float bottom = io.DisplaySize.y - MARGIN;
    size_t dismissed = entries_.size();
    const size_t first = entries_.size() > MAX_VISIBLE ? entries_.size() - MAX_VISIBLE : 0;

    for (size_t i = entries_.size(); i-- > first;) {
        const Entry& entry = entries_[i];
        ImVec2 textSize = ImGui::CalcTextSize(entry.message.c_str(), nullptr, false, textWidth);
        float height = textSize.y + TOAST_PADDING * 2.0f;

        ImVec2 max(io.DisplaySize.x - MARGIN, bottom);
        ImVec2 min(max.x - TOAST_WIDTH, max.y - height);
        const ThemeColors& colors = Colors();
        ImU32 fill = ToU32(entry.isError ? colors.toastErrorBg : colors.toastInfoBg);
        ImU32 border = ImGui::ColorConvertFloat4ToU32(entry.isError ? Colors().errorText : Colors().infoText);

        drawList->AddRectFilled(min, max, fill, 6.0f);
        drawList->AddRect(min, max, border, 6.0f);
        drawList->AddText(
            nullptr,
            0.0f,
            ImVec2(min.x + TOAST_PADDING, min.y + TOAST_PADDING),
            ToU32(colors.toastText),
            entry.message.c_str(),
            nullptr,
            textWidth
        );

        ImVec2 closeMin(max.x - TOAST_PADDING - CLOSE_SIZE, min.y + TOAST_PADDING);
        ImVec2 closeMax(closeMin.x + CLOSE_SIZE, closeMin.y + CLOSE_SIZE);
        bool hover = mouse.x >= closeMin.x && mouse.x <= closeMax.x && mouse.y >= closeMin.y && mouse.y <= closeMax.y;
        ImU32 closeColor = ToU32(hover ? colors.toastCloseHover : colors.toastClose);
        drawList->AddLine(closeMin, closeMax, closeColor, 1.5f);
        drawList->AddLine(ImVec2(closeMin.x, closeMax.y), ImVec2(closeMax.x, closeMin.y), closeColor, 1.5f);
        if (hover && clicked) {
            dismissed = i;
        }

        bottom = min.y - TOAST_GAP;
    }

    if (dismissed < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(dismissed));
    }
}

} // namespace imdj
