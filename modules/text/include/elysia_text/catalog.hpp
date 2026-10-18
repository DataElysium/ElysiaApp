#pragma once
/** @file catalog.hpp
 * UI-thread localization resource. YAML is parsed only by load(); drawing reads owned strings.
 * Call load/select between frames. Returned pointers remain valid until the next load/select.
 * Text and widget labels are separate: label() appends a stable ###message-id for ImGui.
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
struct Message {
    const char* id;
    const char* fallback;
};
struct Language {
    std::string id, name;
};
class ELYSIA_TEXT_API Catalog {
    struct State;
    std::unique_ptr<State> state_;

public:
    Catalog();
    ~Catalog();
    Catalog(const Catalog&) = delete;
    Catalog& operator=(const Catalog&) = delete;
    /// Load *.yaml/*.yml dictionaries. Missing folders are optional. Invalid files leave the
    /// catalog unchanged. Later packages may override keys in dependency/load order.
    void load(const std::filesystem::path& directory);
    /// Select an available locale; unresolved locales use English until their pack is loaded.
    void select(std::string language);
    std::string_view language() const;
    std::vector<Language> languages() const;
    uint64_t revision() const;
    /// Selected language -> English -> source fallback. Message strings are borrowed per call.
    const char* get(Message) const;
    /// Cached translated label plus ###id; repeated rows still need their ordinary PushID scope.
    const char* label(Message) const;
};
struct ELYSIA_TEXT_API Plugin {
    void build(App&) const;
};
} // namespace elysia::text
