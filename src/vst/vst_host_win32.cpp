#if defined(_WIN32)

#include <windows.h>

#include <chrono>
#include <future>
#include <mutex>
#include <thread>

#include "core/paths.h"
#include "vst/vst_host_internal.h"

namespace imdj {

namespace {

constexpr UINT RUN_TASK = WM_APP + 1;
constexpr wchar_t TASK_WINDOW_CLASS[] = L"ImdjVstTaskWindow";
constexpr wchar_t EDITOR_WINDOW_CLASS[] = L"ImdjVstEditorWindow";

LRESULT CALLBACK TaskWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg != RUN_TASK) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    std::unique_ptr<std::function<void()>> task(reinterpret_cast<std::function<void()>*>(lp));
    (*task)();

    return 0;
}

LRESULT CALLBACK EditorWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg != WM_CLOSE) {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    if (auto* closeRequested = reinterpret_cast<bool*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA))) {
        *closeRequested = true;
    }

    return 0;
}

void RegisterWindowClasses()
{
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &TaskWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = TASK_WINDOW_CLASS;
        RegisterClassExW(&wc);

        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &EditorWindowProc;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = EDITOR_WINDOW_CLASS;
        RegisterClassExW(&wc);
    });
}

} // namespace

const Steinberg::FIDString EDITOR_PLATFORM_TYPE = Steinberg::kPlatformTypeHWND;

struct PluginThread::State {
    std::thread thread;
    std::future<void> finished;
    HWND window = nullptr;
};

PluginThread::PluginThread() : state_(std::make_unique<State>())
{
    std::promise<HWND> ready;
    std::future<HWND> window = ready.get_future();
    std::promise<void> finished;
    state_->finished = finished.get_future();
    state_->thread = std::thread([ready = std::move(ready), finished = std::move(finished)]() mutable {
        RegisterWindowClasses();
        HWND window = CreateWindowExW(
            0, TASK_WINDOW_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr
        );
        ready.set_value(window);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        DestroyWindow(window);
        finished.set_value();
    });
    state_->window = window.get();
}

PluginThread::~PluginThread()
{
    post([] { PostQuitMessage(0); });
    if (state_->finished.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
        state_->thread.join();
    }
    else {
        state_->thread.detach();
    }
}

void PluginThread::post(std::function<void()> task)
{
    auto message = std::make_unique<std::function<void()>>(std::move(task));
    if (state_->window && PostMessageW(state_->window, RUN_TASK, 0, reinterpret_cast<LPARAM>(message.get()))) {
        message.release();
        return;
    }

    (*message)();
}

EditorWindow::EditorWindow(int width, int height, const std::string& title)
{
    RegisterWindowClasses();
    const DWORD style = WS_OVERLAPPEDWINDOW & ~static_cast<DWORD>(WS_THICKFRAME | WS_MAXIMIZEBOX);
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        EDITOR_WINDOW_CLASS,
        PathFromUtf8(title).wstring().c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );
    if (!hwnd) {
        return;
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&closeRequested_));
    ShowWindow(hwnd, SW_SHOW);
    window_ = hwnd;
}

EditorWindow::~EditorWindow()
{
    if (window_) {
        DestroyWindow(static_cast<HWND>(window_));
    }
}

void* EditorWindow::parent() const { return window_; }

void EditorWindow::resize(int width, int height)
{
    HWND hwnd = static_cast<HWND>(window_);
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE);
    SetWindowPos(
        hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
    );
}

bool EditorWindow::closeRequested() const { return closeRequested_; }

} // namespace imdj

#endif
