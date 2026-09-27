#pragma once

#include <string>
#include "core/result.h"

struct GLFWwindow;

namespace imdj {

class AppShell {
public:
    ~AppShell();

    AppShell(const AppShell&) = delete;
    AppShell& operator=(const AppShell&) = delete;
    AppShell() = default;

    Status init(const char* title, int width, int height);
    void shutdown();

    bool shouldClose() const;
    bool beginFrame();
    void endFrame();

    double time() const;
    GLFWwindow* window() const { return window_; }

private:
    GLFWwindow* window_ = nullptr;
    bool imguiInitialized_ = false;
    std::string iniPath_;
};

} // namespace imdj
