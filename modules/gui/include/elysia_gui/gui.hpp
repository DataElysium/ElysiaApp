#pragma once
/** @file gui.hpp
 * GUI SDK entry point for application/tool authors. Includes frame-loop integration,
 * windows/panels, images and the optional Mods menu. Native module entry: elysia_app/module.hpp.
 * The inline helpers below check the shared SDK build before accessing its Registry.
 */
#include "application.hpp"
#include "windows.hpp"
#include "image.hpp"
#include "mods.hpp"
#include <elysia_app/detail/build_abi.hpp>
#include <stdexcept>
#include <string_view>

namespace elysia::gui {
/** Extra GUI build check: the generic App SDK intentionally has no ImGui
 * dependency. */
constexpr uint64_t native_abi() {
    uint64_t hash = app::native_abi();
    for (unsigned char c : std::string_view(IMGUI_VERSION))
        hash = (hash ^ c) * 1099511628211ull;
    for (auto size : {sizeof(ImGuiIO), sizeof(ImGuiStyle), sizeof(ImDrawVert), sizeof(ImDrawIdx),
                      sizeof(ImWchar), sizeof(Window), sizeof(Callbacks), sizeof(Options)})
        hash = (hash ^ size) * 1099511628211ull;
    return hash;
}
/** Verify the SDK before passing configuration/callback structures across the
 * DLL boundary. */
ELYSIA_GUI_API void check_abi(uint64_t);

/** Check GUI ABI before calling ImGui or touching GUI data in a consumer DLL.
 */
inline Registry& registry(App& app) {
    auto* value = app.world().resources().get<Registry>();
    if (!value)
        throw std::logic_error("Install elysia.gui before GUI tools");
    value->check_abi(native_abi());
    return *value;
}
} // namespace elysia::gui
