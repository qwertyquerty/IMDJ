#include "app/session_state.h"

#include "app/app_paths.h"
#include "core/json_io.h"

namespace imdj {

namespace {

constexpr int SESSION_FILE_VERSION = 1;

std::string SessionFilePath() { return AppDataDir() + "/session.json"; }

} // namespace

std::string SessionValue(const char* key)
{
    Json root;
    LoadJsonFile(SessionFilePath(), root);

    return JsonValue(root, key, std::string());
}

void SetSessionValue(const char* key, const std::string& value)
{
    Json root;
    if (!LoadJsonFile(SessionFilePath(), root) || !root.is_object()) {
        root = Json::object();
    }

    root[key] = value;
    SaveVersionedJson(SessionFilePath(), root, SESSION_FILE_VERSION);
}

} // namespace imdj
