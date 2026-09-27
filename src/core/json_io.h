#pragma once

#include <string>
#include <type_traits>
#include <utility>

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include "core/result.h"

namespace imdj {

using Json = nlohmann::json;

bool LoadJsonFile(const std::string& utf8Path, Json& out);

Status SaveJsonFile(const std::string& utf8Path, const Json& value);

Status SaveVersionedJson(const std::string& utf8Path, Json root, int version);
int JsonVersion(const Json& root);

template <typename T>
T JsonAs(const Json& json, T fallback)
{
    try {
        return json.is_null() ? fallback : json.get<T>();
    }
    catch (const Json::exception&) {
        return fallback;
    }
}

template <typename T>
T JsonValue(const Json& object, const char* key, T fallback)
{
    if (!object.is_object()) {
        return fallback;
    }

    auto it = object.find(key);
    return it == object.end() ? fallback : JsonAs(*it, std::move(fallback));
}

template <typename T>
T LoadJsonAs(const std::string& utf8Path, T fallback = T{})
{
    Json root;
    return LoadJsonFile(utf8Path, root) ? JsonAs(root, std::move(fallback)) : fallback;
}

const Json& JsonArray(const Json& object, const char* key);

template <typename Enum>
    requires std::is_enum_v<Enum>
void to_json(Json& json, Enum value)
{
    json = magic_enum::enum_name(value);
}

template <typename Enum>
    requires std::is_enum_v<Enum>
void from_json(const Json& json, Enum& value)
{
    value = magic_enum::enum_cast<Enum>(json.is_string() ? json.get<std::string>() : std::string()).value_or(Enum{});
}

template <typename Enum>
Enum JsonEnum(const Json& object, const char* key, Enum fallback, int count)
{
    int value = JsonValue(object, key, static_cast<int>(fallback));
    return value >= 0 && value < count ? static_cast<Enum>(value) : fallback;
}

} // namespace imdj
