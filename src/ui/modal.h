#pragma once

#include "imgui.h"

namespace imdj {

class Modal {
public:
    explicit Modal(const char* name, ImVec2 size = ImVec2(0.0f, 0.0f), bool fixedSize = false);
    ~Modal();

    Modal(const Modal&) = delete;
    Modal& operator=(const Modal&) = delete;

    explicit operator bool() const { return open_; }

    void close() const { ImGui::CloseCurrentPopup(); }
    bool closeButton(const char* label = nullptr, float width = 80.0f) const;

private:
    bool open_ = false;
};

Modal CenteredModal(const char* name, float widthFraction, float heightFraction);

} // namespace imdj
