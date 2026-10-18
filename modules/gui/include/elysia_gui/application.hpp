#pragma once
/** @file application.hpp
 * Install GuiPlugin, configure the OS window, and integrate an application's frame loop.
 * Context identifies the App passed to GUI callbacks. This header owns no renderer device API.
 */
#include "config.hpp"
#include <elysia_app/application.hpp>
#include <elysia/elysia.hpp>
#include <filesystem>
#include <string>

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
/** Reusable GUI runner. GuiPlugin installs it with set_runner; later modules
 * may replace it. A custom runner can perform its own preparation and then call
 * this function.
 */
ELYSIA_GUI_API void run(App&);
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
