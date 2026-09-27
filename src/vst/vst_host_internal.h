#pragma once

#include <functional>
#include <memory>
#include <string>

#include "core/crash_guard.h"
#include "pluginterfaces/gui/iplugview.h"

namespace imdj {

extern const Steinberg::FIDString EDITOR_PLATFORM_TYPE;

class PluginThread {
public:
    PluginThread();
    ~PluginThread();
    PluginThread(const PluginThread&) = delete;
    PluginThread& operator=(const PluginThread&) = delete;

    void post(std::function<void()> task);
    void run(const std::function<void()>& task);

private:
    struct State;
    std::unique_ptr<State> state_;
};

class EditorWindow {
public:
    EditorWindow(int width, int height, const std::string& title);
    ~EditorWindow();
    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;

    bool valid() const { return window_ != nullptr; }
    void* parent() const;
    void resize(int width, int height);
    bool closeRequested() const;

private:
    void* window_ = nullptr;
#if defined(_WIN32)
    bool closeRequested_ = false;
#endif
};

} // namespace imdj
