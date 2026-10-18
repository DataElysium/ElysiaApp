#pragma once
/** @file module.hpp
 * DLL author entry point: export macro, module identity, install callback and package location.
 * The exported query returns ModuleDescriptor; install calls ordinary App::add_plugin.
 * Runtime loading, mod-selection data and application resources have their own headers.
 */
#include "detail/build_abi.hpp"
#include <cstdint>
#include <concepts>
#include <filesystem>
#include <nameof.hpp>
#include <span>
#include <string_view>

#ifdef _WIN32
#define ELYSIA_APP_EXPORT extern "C" __declspec(dllexport)
#else
#define ELYSIA_APP_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace elysia::app {
inline constexpr uint32_t sdk_version = 1;
inline constexpr const char* module_symbol = "elysia_app_module_v1";

/** Borrowed only during installation. Copy paths into callbacks/resources that need them. */
struct ModuleContext {
    std::string_view id;
    const std::filesystem::path& directory;
};
/** Read-only UTF-8, null-terminated strings owned by the module. No allocator crosses the ABI.
 * Keep the array and its strings alive while the descriptor is available, normally in static
 * storage. Keys identify application-specific properties; the SDK does not interpret values.
 */
struct MetadataEntry {
    const char* key;
    const char* value;
};
/** Native descriptor discovered through a C symbol; the interface itself uses the C++ ABI.
 * install normally calls app.add_plugin(P{...}). There are no GUI or case-format requirements.
 * Throwing installation is fatal to this Runtime: partial App writes cannot be rolled back.
 */
struct ModuleDescriptor {
    uint32_t size = sizeof(ModuleDescriptor);
    uint32_t version = sdk_version;
    uint64_t abi = native_abi();
    // Optional constraints/labels. Package identity and release normally live in the manifest.
    const char* id = nullptr;
    const char* name = nullptr;
    const char* release = nullptr;
    void (*install)(App&, const ModuleContext&) = nullptr;
    const MetadataEntry* metadata = nullptr;
    uint32_t metadata_count = 0;
};
using QueryModule = const ModuleDescriptor* (*)();

/** Generate a native descriptor at compile time from an ordinary App plugin.
 * nameof supplies its full C++ type name; SDK size/version/ABI and installation are automatic.
 * A plugin is default constructed, or constructed from const ModuleContext& when it needs its
 * package directory. The optional id pins an alias, useful for registered static modules.
 * Keep the returned value in static constexpr storage inside the exported query function.
 */
template <class Plugin>
consteval ModuleDescriptor module(std::span<const MetadataEntry> metadata,
                                  const char* id = nullptr) {
    static_assert(
        requires(Plugin& plugin, App& app) { plugin.build(app); },
        "Module plugin must provide build(elysia::App&)");
    static_assert(std::default_initializable<Plugin> ||
                      std::constructible_from<Plugin, const ModuleContext&>,
                  "Module plugin must be default constructible or accept const ModuleContext&");
    return {.id = id,
            .name = nameof::nameof_type<Plugin>().data(),
            .install =
                [](App& app, const ModuleContext& context) {
                    if constexpr (std::constructible_from<Plugin, const ModuleContext&>)
                        app.add_plugin(Plugin{context});
                    else
                        app.add_plugin(Plugin{});
                },
            .metadata = metadata.data(),
            .metadata_count = static_cast<uint32_t>(metadata.size())};
}

/** Ordinary modules need no metadata. The optional ID is only a package alias/constraint. */
template <class Plugin> consteval ModuleDescriptor module(const char* id = nullptr) {
    return module<Plugin>(std::span<const MetadataEntry>{}, id);
}

} // namespace elysia::app
