#include "app/app_shell.h"

#include <cstdio>

#include "app/app_paths.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include "core/strings.h"

#include <GLFW/glfw3.h>

namespace imdj {

namespace {

void GlfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

#if defined(__APPLE__)
constexpr const char* GLSL_VERSION = "#version 150";
#else
constexpr const char* GLSL_VERSION = "#version 130";
#endif

} // namespace

AppShell::~AppShell() { shutdown(); }

Status AppShell::init(const char* title, int width, int height)
{
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        return Status::Fail(TrFormat("error.glfw_init"));
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return Status::Fail(TrFormat("error.window_create"));
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    iniPath_ = AppDataDir() + "/imgui.ini";
    ImGui::GetIO().IniFilename = iniPath_.c_str();

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);
    imguiInitialized_ = true;

    return Status::Ok();
}

void AppShell::shutdown()
{
    if (imguiInitialized_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imguiInitialized_ = false;
    }

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

bool AppShell::shouldClose() const { return !window_ || glfwWindowShouldClose(window_); }

bool AppShell::beginFrame()
{
    glfwPollEvents();
    if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED)) {
        glfwWaitEventsTimeout(1.0 / 30.0);
        return false;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return true;
}

void AppShell::endFrame()
{
    ImGui::Render();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);

    const ImVec4 background = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    glClearColor(background.x, background.y, background.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

double AppShell::time() const { return glfwGetTime(); }

} // namespace imdj
