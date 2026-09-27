#include "app/app_paths.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>

#include "core/paths.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace imdj {

namespace {

std::string GetEnvVar(const char* name)
{
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

std::string ComputeAppDataDir()
{
#if defined(_WIN32)
    std::string base = GetEnvVar("APPDATA");
    if (base.empty()) {
        return ".";
    }

    return base + "\\imdj";
#elif defined(__APPLE__)
    std::string home = GetEnvVar("HOME");
    if (home.empty()) {
        return ".";
    }

    return home + "/Library/Application Support/imdj";
#else
    std::string base = GetEnvVar("XDG_DATA_HOME");
    if (base.empty()) {
        std::string home = GetEnvVar("HOME");
        if (home.empty()) {
            return ".";
        }

        base = home + "/.local/share";
    }

    return base + "/imdj";
#endif
}

std::string EnsureDir(const std::string& dir, const std::string& fallback)
{
    std::error_code ec;
    std::filesystem::create_directories(PathFromUtf8(dir), ec);

    return ec ? fallback : dir;
}

std::filesystem::path ExecutableDir()
{
    std::filesystem::path exe;
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        exe = buffer;
    }
#elif defined(__APPLE__)
    char buffer[4096];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        exe = buffer;
    }
#else
    std::error_code ec;
    exe = std::filesystem::read_symlink("/proc/self/exe", ec);
#endif
    return exe.parent_path();
}

} // namespace

std::string ResourcePath(const std::string& relative)
{
    static const std::filesystem::path executableDir = ExecutableDir();
    std::error_code ec;
    const std::filesystem::path beside = executableDir / PathFromUtf8(relative);
    if (!executableDir.empty() && std::filesystem::exists(beside, ec)) {
        return PathToUtf8(beside);
    }

    return relative;
}

std::string AppDataDir()
{
    static const std::string dir = EnsureDir(ComputeAppDataDir(), ".");
    return dir;
}

std::string SessionPresetsDir()
{
    static const std::string dir = EnsureDir(AppDataDir() + "/presets", AppDataDir());
    return dir;
}

void OpenFolderInFileManager(const std::string& path)
{
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::system(("open \"" + path + "\"").c_str());
#else
    std::system(("xdg-open \"" + path + "\"").c_str());
#endif
}

} // namespace imdj
