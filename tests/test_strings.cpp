#include <regex>
#include <string>
#include <vector>

#include "core/json_io.h"
#include "core/strings.h"
#include "doctest/doctest.h"

using namespace imdj;

namespace {

const std::string LANG_DIR = std::string(IMDJ_SOURCE_DIR) + "/res/lang";

std::vector<std::string> Placeholders(const std::string& text)
{
    static const std::regex token(R"(\{\{|\}\}|\{[^{}]*\}|%%|%[-+ #0]*\d*(?:\.\d+)?[sdcfugx])");
    std::vector<std::string> found;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), token); it != std::sregex_iterator(); ++it) {
        const std::string match = it->str();
        if (match != "{{" && match != "}}" && match != "%%") {
            found.push_back(match);
        }
    }

    return found;
}

} // namespace

TEST_CASE("Missing strings fall back to English, then to the key")
{
    const char* english = R"({"greeting": "Hello", "only_en": "English only", "count": "{} items"})";
    const char* other = R"({"greeting": "Salut", "count": "{} choses"})";

    REQUIRE(LoadLanguageFromText(english, other, "xx").ok());
    CHECK(std::string(Tr("greeting")) == "Salut");
    CHECK(std::string(Tr("only_en")) == "English only");
    CHECK(std::string(Tr("nowhere")) == "nowhere");
    CHECK(TrFormat("count", 3) == "3 choses");
    CHECK(std::string(TrLabel("greeting")) == "Salut##greeting");

    REQUIRE(LoadLanguageFromText(english, "", "en").ok());
    CHECK(std::string(Tr("greeting")) == "Hello");
    CHECK(std::string(TrLabel("greeting")) == "Hello##greeting");

    CHECK_FALSE(LoadLanguageFromText(english, "{ not json", "yy").ok());
    CHECK(CurrentLanguage() == "en");

    CHECK_FALSE(LoadLanguage(LANG_DIR, "does_not_exist").ok());
    REQUIRE(LoadLanguage(LANG_DIR, "en").ok());
}

TEST_CASE("Every shipped language matches English keys and placeholders")
{
    const std::vector<std::string> codes = AvailableLanguages(LANG_DIR);
    CHECK(codes.size() >= 5);

    Json english;
    REQUIRE(LoadJsonFile(LANG_DIR + "/en.json", english));

    for (const std::string& code : codes) {
        Json table;
        REQUIRE(LoadJsonFile(LANG_DIR + "/" + code + ".json", table));
        for (const auto& [key, value] : table.items()) {
            INFO(code << ": " << key);
            REQUIRE(english.contains(key));
            CHECK(Placeholders(value.get<std::string>()) == Placeholders(english[key].get<std::string>()));
        }
    }
}
