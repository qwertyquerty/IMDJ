#include "core/strings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <unordered_map>

#include "core/json_io.h"
#include "core/paths.h"

namespace imdj {

namespace {

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view text) const { return std::hash<std::string_view>{}(text); }
};

using Table = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;

struct Tables {
    Table english;
    Table current;
    Table labels;
    Table missing;
    std::string currentCode = "en";
    std::mutex mutex;
};

Tables& State()
{
    static Tables tables;
    return tables;
}

Status ParseTable(std::string_view json, const std::string& name, Table& table)
{
    Json root = Json::parse(json, nullptr, false, true);
    if (root.is_discarded() || !root.is_object()) {
        return Status::Fail("Could not parse " + name);
    }

    table.clear();
    for (const auto& [key, value] : root.items()) {
        if (value.is_string()) {
            table[key] = value.get<std::string>();
        }
    }

    return Status::Ok();
}

Status ReadFile(const std::string& path, std::string& text)
{
    std::ifstream file(PathFromUtf8(path), std::ios::binary);
    if (!file) {
        return Status::Fail("Could not read " + path);
    }

    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return Status::Ok();
}

} // namespace

Status LoadLanguageFromText(std::string_view englishJson, std::string_view languageJson, const std::string& code)
{
    Table english;
    if (Status parsed = ParseTable(englishJson, "en", english); !parsed) {
        return parsed;
    }

    Table table;
    if (code != "en") {
        if (Status parsed = ParseTable(languageJson, code, table); !parsed) {
            return parsed;
        }
    }

    Tables& state = State();
    std::lock_guard lock(state.mutex);
    state.english = std::move(english);
    state.current = std::move(table);
    state.labels.clear();
    state.currentCode = code;

    return Status::Ok();
}

Status LoadLanguage(const std::string& dir, const std::string& code)
{
    std::string english;
    if (Status read = ReadFile(dir + "/en.json", english); !read) {
        return read;
    }

    std::string language;
    if (code != "en") {
        if (Status read = ReadFile(dir + "/" + code + ".json", language); !read) {
            return read;
        }
    }

    return LoadLanguageFromText(english, language, code);
}

std::vector<std::string> AvailableLanguages(const std::string& dir)
{
    std::vector<std::string> codes;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(PathFromUtf8(dir), ec)) {
        if (entry.path().extension() == ".json") {
            codes.push_back(PathToUtf8(entry.path().stem()));
        }
    }

    std::sort(codes.begin(), codes.end());
    return codes;
}

const std::string& CurrentLanguage() { return State().currentCode; }

namespace {

const char* Lookup(Tables& state, std::string_view key)
{
    if (auto it = state.current.find(key); it != state.current.end()) {
        return it->second.c_str();
    }

    if (auto it = state.english.find(key); it != state.english.end()) {
        return it->second.c_str();
    }

    auto [it, inserted] = state.missing.try_emplace(std::string(key), std::string(key));
    return it->second.c_str();
}

} // namespace

const char* Tr(std::string_view key)
{
    Tables& state = State();
    std::lock_guard lock(state.mutex);

    return Lookup(state, key);
}

const char* TrLabel(std::string_view key)
{
    Tables& state = State();
    std::lock_guard lock(state.mutex);
    if (auto it = state.labels.find(key); it != state.labels.end()) {
        return it->second.c_str();
    }

    std::string label = std::string(Lookup(state, key)) + "##" + std::string(key);
    return state.labels.emplace(std::string(key), std::move(label)).first->second.c_str();
}

std::string TrFormatArgs(std::string_view key, std::format_args args)
{
    Tables& state = State();
    std::lock_guard lock(state.mutex);

    return FormatTranslation(Lookup(state, key), args);
}

std::string FormatTranslation(std::string_view pattern, std::format_args args)
{
    try {
        return std::vformat(pattern, args);
    }
    catch (const std::format_error&) {
        // A translator's broken placeholder shows the raw key text
        return std::string(pattern);
    }
}

} // namespace imdj
