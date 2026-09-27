#include "ui/modal.h"

#include "core/strings.h"

namespace imdj {

Modal::Modal(const char* name, ImVec2 size, bool fixedSize)
{
    if (size.x > 0.0f || size.y > 0.0f) {
        ImGui::SetNextWindowSize(size, fixedSize ? ImGuiCond_Always : ImGuiCond_Appearing);
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;
    if (size.x <= 0.0f && size.y <= 0.0f) {
        flags |= ImGuiWindowFlags_AlwaysAutoResize;
    }

    open_ = ImGui::BeginPopupModal(name, nullptr, flags);
}

Modal::~Modal()
{
    if (open_) {
        ImGui::EndPopup();
    }
}

bool Modal::closeButton(const char* label, float width) const
{
    if (!ImGui::Button(label ? label : TrLabel("common.close"), ImVec2(width, 0))) {
        return false;
    }

    ImGui::CloseCurrentPopup();
    return true;
}

Modal CenteredModal(const char* name, float widthFraction, float heightFraction)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    return Modal(name, ImVec2(viewport->Size.x * widthFraction, viewport->Size.y * heightFraction), true);
}

} // namespace imdj
