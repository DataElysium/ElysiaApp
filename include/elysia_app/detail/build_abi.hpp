#pragma once
// Build compatibility implementation for the native SDK. Include module.hpp in mod entry points.
#include <elysia/elysia.hpp>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#ifndef ELYSIA_APP_BUILD_TAG
#define ELYSIA_APP_BUILD_TAG "experimental-1"
#endif

namespace elysia::app {
namespace detail {
// Increment when changing shared SDK contracts that are not represented by the sizes below.
inline constexpr uint32_t sdk_contract_revision = 3;
} // namespace detail
/** Detect common native-build mismatches, not arbitrary compiler/header compatibility.
 * All modules must use the host SDK, ECS, dependency headers, compiler, CRT and ABI options.
 * Change the shared build tag when changing that distribution's public contracts.
 */
constexpr uint64_t native_abi() {
    uint64_t hash = 14695981039346656037ull;
    auto mix = [&](uint64_t value) {
        hash = (hash ^ value) * 1099511628211ull;
    };
    for (const auto text : {std::string_view(__VERSION__), std::string_view(ELYSIA_APP_BUILD_TAG)})
        for (const unsigned char c : text)
            mix(c);
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
    mix(sizeof(void*));
    mix(std::endian::native == std::endian::little);
    mix(sizeof(App));
    mix(sizeof(World));
    mix(sizeof(Entity));
    mix(sizeof(std::string));
    mix(sizeof(std::filesystem::path));
    mix(sizeof(std::string_view));
    mix(detail::sdk_contract_revision);
    return hash;
}

} // namespace elysia::app
