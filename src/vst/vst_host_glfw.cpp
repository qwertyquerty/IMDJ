#if !defined(_WIN32)

#if defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "vst/vst_host_internal.h"

namespace imdj {

#if defined(__APPLE__)
const Steinberg::FIDString EDITOR_PLATFORM_TYPE = Steinberg::kPlatformTypeNSView;
#else
const Steinberg::FIDString EDITOR_PLATFORM_TYPE = Steinberg::kPlatformTypeX11EmbedWindowID;
#endif

struct PluginThread::State {};

PluginThread::PluginThread() = default;

PluginThread::~PluginThread() = default;

void PluginThread::post(std::function<void()> task) { task(); }

EditorWindow::EditorWindow(int width, int height, const std::string& title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
}

EditorWindow::~EditorWindow()
{
    if (window_) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window_));
    }
}

void* EditorWindow::parent() const
{
#if defined(__APPLE__)
    return glfwGetCocoaView(static_cast<GLFWwindow*>(window_));
#else
    return reinterpret_cast<void*>(glfwGetX11Window(static_cast<GLFWwindow*>(window_)));
#endif
}

void EditorWindow::resize(int width, int height)
{
    glfwSetWindowSize(static_cast<GLFWwindow*>(window_), width, height);
}

bool EditorWindow::closeRequested() const { return glfwWindowShouldClose(static_cast<GLFWwindow*>(window_)); }

} // namespace imdj

#endif
