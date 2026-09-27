#include "core/json_io.h"

#include <filesystem>
#include <fstream>

#include "core/paths.h"
#include "core/strings.h"

namespace imdj {

namespace {

const Json EMPTY_ARRAY = Json::array();

} // namespace

bool LoadJsonFile(const std::string& utf8Path, Json& out)
{
    std::ifstream file(PathFromUtf8(utf8Path));
    if (!file) {
        return false;
    }

    try {
        out = Json::parse(file, nullptr, false, true);
    }
    catch (const Json::exception&) {
        return false;
    }
    return !out.is_discarded();
}

Status SaveJsonFile(const std::string& utf8Path, const Json& value)
{
    std::filesystem::path path = PathFromUtf8(utf8Path);
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream file(path, std::ios::trunc);
    if (file) {
        file << value.dump(2) << "\n";
    }

    if (!file) {
        return Status::Fail(TrFormat("error.file_write", utf8Path));
    }

    return Status::Ok();
}

Status SaveVersionedJson(const std::string& utf8Path, Json root, int version)
{
    root["version"] = version;
    return SaveJsonFile(utf8Path, root);
}

int JsonVersion(const Json& root) { return JsonValue(root, "version", 0); }

const Json& JsonArray(const Json& object, const char* key)
{
    if (!object.is_object()) {
        return EMPTY_ARRAY;
    }

    auto it = object.find(key);
    return (it != object.end() && it->is_array()) ? *it : EMPTY_ARRAY;
}

} // namespace imdj
