#pragma once
/** @file i18n.hpp
 * UI-thread localization resource. YAML is parsed only by load(); drawing reads owned strings.
 * Call load/select between frames. Returned pointers remain valid until the next load/select.
 * Text and widget labels are separate: t_id() appends a stable ###message-id for ImGui.
 */
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifdef ELYSIA_TEXT_BUILD
#define ELYSIA_TEXT_API __declspec(dllexport)
#else
#define ELYSIA_TEXT_API __declspec(dllimport)
#endif
#else
#define ELYSIA_TEXT_API __attribute__((visibility("default")))
#endif

namespace elysia {
class App;
}
namespace elysia::text {
struct Language {
    std::string id, name;
};
class ELYSIA_TEXT_API I18nManager {
    struct State;
    std::unique_ptr<State> state_;

public:
    I18nManager();
    ~I18nManager();
    I18nManager(const I18nManager&) = delete;
    I18nManager& operator=(const I18nManager&) = delete;
    /// Load *.yaml/*.yml dictionaries. Missing folders are optional. Invalid files leave the
    /// manager unchanged. Later packages may override keys in dependency/load order.
    void load(const std::filesystem::path& directory);
    /// Select an available locale; unresolved locales use English until their pack is loaded.
    void select(std::string language);
    std::string_view language() const;
    std::vector<Language> languages() const;
    uint64_t revision() const;
    /// Selected language -> English -> fallback (or key itself when fallback is omitted).
    /// Arguments are borrowed for this call; an unresolved fallback/key is returned as supplied.
    const char* translate(const char* key, const char* fallback = nullptr) const;
    /// Plain translated text with no ImGui ID suffix. Bind a local resource reference named TR.
    const char* operator()(const char* key, const char* fallback = nullptr) const {
        return translate(key, fallback);
    }
    /// For controls only: translated label plus ###key. TextUnformatted does not hide that suffix.
    /// Repeated rows still need their ordinary PushID scope.
    const char* t_id(const char* key, const char* fallback = nullptr) const;
};
struct ELYSIA_TEXT_API Plugin {
    void build(App&) const;
};
} // namespace elysia::text
