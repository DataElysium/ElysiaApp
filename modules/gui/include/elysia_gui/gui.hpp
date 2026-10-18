#pragma once
#include "config.hpp"
#include <elysia_app/module.hpp>
#include <imgui.h>
#include <memory>
#include <span>

namespace elysia::gui {
struct Context {
    App& app;
};
/** Application-owned frame composition. Callbacks run on the GUI thread with a
 * current context. initialize runs once after renderer creation, before App
 * startup. draw replaces the stock menu/dockspace; registered tool windows are
 * still drawn afterwards. submitted runs after rendering and before present,
 * including false when the window is minimized.
 */
struct Callbacks {
    void (*initialize)(Context&) = nullptr;
    void (*draw)(Context&) = nullptr;
    void (*submitted)(Context&, bool rendered) = nullptr;
    void (*prepare)(Context&) = nullptr; // After event polling, before NewFrame.
};
struct Options {
    std::string title = "ElysiaApp / ImGui Workbench";
    int width = 1440, height = 900;
    bool vsync = true, hidden = false, x11 = false;
    std::filesystem::path layout, font;
};
using Draw = void (*)(Context&);
struct Window {
    std::string id, title;
    Draw draw;
    bool open = true;
    float width = 900, height = 600;
};
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

/** GUI-thread registry, shared by all UI DLLs through the App resource.
 * Register during installation. Host owns Begin/End; draw supplies only window
 * contents. Named panels can be composed inside any registered window.
 * Modifying the registry during draw is rejected.
 */
class ELYSIA_GUI_API Registry {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    Registry();
    ~Registry();
    Registry(const Registry&) = delete;
    Registry(Registry&&) noexcept;
    void check_abi(uint64_t) const;
    void add_window(Window);
    void add_panel(std::string id, Draw);
    bool draw_panel(std::string_view id, Context&);
    bool set_open(std::string_view id, bool);
    size_t window_count() const;
    void draw_menu();
    void draw(Context&);
};
/** Check GUI ABI before calling ImGui or touching GUI data in a consumer DLL.
 */
inline Registry& registry(App& app) {
    auto* value = app.world().resources().get<Registry>();
    if (!value)
        throw std::logic_error("Install elysia.gui before GUI tools");
    value->check_abi(native_abi());
    return *value;
}
/** Backend-owned RGBA8 texture. Upload on the GUI thread after the surface
 * exists. Keep images in App/module resources so they die before the retained
 * GUI session. Consumer code receives an ImGui texture reference, never an
 * OpenGL/Vulkan handle.
 */
class ELYSIA_GUI_API Image {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    Image();
    ~Image();
    Image(const Image&) = delete;
    void upload(int width, int height, std::span<const uint8_t> rgba);
    ImTextureRef view() const;
};

/** Reusable GUI runner. GuiPlugin installs it with set_runner; later modules
 * may replace it. A custom runner can perform its own preparation and then call
 * this function.
 */
ELYSIA_GUI_API void run(App&);
/** Reusable Mods menu for application-owned menu bars. The GUI runner draws its
 * editor window and returns after a successful save/restart request. Available
 * only for a playset-loaded Runtime; rebuilding is the outer host's job.
 */
ELYSIA_GUI_API void draw_mods_menu(Context&);
/** Request loop exit after the current frame; does not terminate the simulation
 * process. */
ELYSIA_GUI_API void quit(App&);
/** Capture the rendered frame from a submitted callback, before presentation.
 */
ELYSIA_GUI_API void capture(App&, const std::filesystem::path&);
ELYSIA_GUI_API const char* renderer_name();
struct ELYSIA_GUI_API GuiPlugin {
    Options options{};
    void build(App&) const;
};
} // namespace elysia::gui
