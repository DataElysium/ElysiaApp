#pragma once
#include "session.hpp"
#include <elysia/elysia.hpp>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define ELYSIA_APP_EXPORT extern "C" __declspec(dllexport)
#else
#define ELYSIA_APP_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#ifndef ELYSIA_APP_BUILD_TAG
#define ELYSIA_APP_BUILD_TAG "experimental-1"
#endif

namespace elysia::app {
inline constexpr uint32_t sdk_version = 1;
inline constexpr const char* module_symbol = "elysia_app_module_v1";

/** Detect common native-build mismatches, not arbitrary compiler/header compatibility.
 * All modules must use the host SDK, ECS, dependency headers, compiler, CRT and ABI options.
 * Change the shared build tag when changing that distribution's public contracts.
 */
constexpr uint64_t native_abi() {
    uint64_t hash = 14695981039346656037ull;
    auto mix = [&](uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };
    for (const auto text : {std::string_view(__VERSION__), std::string_view(ELYSIA_APP_BUILD_TAG)})
        for (const unsigned char c : text) mix(c);
#ifdef _MSC_VER
    mix(_MSC_VER);
#endif
#ifdef _ITERATOR_DEBUG_LEVEL
    mix(_ITERATOR_DEBUG_LEVEL);
#endif
#ifdef _DLL
    mix(1);
#endif
#ifdef _DEBUG
    mix(2);
#endif
#ifdef _GLIBCXX_USE_CXX11_ABI
    mix(_GLIBCXX_USE_CXX11_ABI);
#endif
#ifdef _LIBCPP_VERSION
    mix(_LIBCPP_VERSION);
#endif
#ifdef ELYSIA_PERF_OVERLAY
    mix(3);
#endif
    mix(sizeof(void*)); mix(std::endian::native == std::endian::little);
    mix(sizeof(App)); mix(sizeof(World)); mix(sizeof(Entity));
    mix(sizeof(std::string)); mix(sizeof(std::filesystem::path)); mix(sizeof(std::string_view));
    mix(sizeof(ModSession));
    return hash;
}

/** Borrowed only during installation. Copy paths into callbacks/resources that need them. */
struct ModuleContext {
    std::string_view id;
    const std::filesystem::path& directory;
};
/** Native descriptor discovered through a C symbol; the interface itself uses the C++ ABI.
 * install normally calls app.add_plugin(P{...}). There are no GUI or case-format requirements.
 * Throwing installation is fatal to this Runtime: partial App writes cannot be rolled back.
 */
struct ModuleDescriptor {
    uint32_t size = sizeof(ModuleDescriptor);
    uint32_t version = sdk_version;
    uint64_t abi = native_abi();
    const char* id;
    const char* name;
    const char* release;
    void (*install)(App&, const ModuleContext&);
};
using QueryModule = const ModuleDescriptor* (*)();

/** Hold infrastructure until AFTER App destruction and BEFORE DLL unload.
 * The host owns the callbacks/storage. Only resources tied to this Runtime may retain owners.
 * Retained objects are released in reverse registration order. Stop/join workers before returning from run.
 */
struct AfterAppLifetime {
    void* state;
    void (*retain)(void*, std::shared_ptr<void>);
    template<class T> void hold(std::shared_ptr<T> value) const { retain(state, std::move(value)); }
};

/** Arguments after the host's '--' delimiter; interpreted exclusively by application plugins. */
struct Arguments { std::vector<std::string> values; };
} // namespace elysia::app
