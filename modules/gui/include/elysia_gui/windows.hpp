#pragma once
/** @file windows.hpp
 * Register dockable windows and reusable panel callbacks. Registry owns window visibility;
 * callbacks draw contents and keep their application state in App resources.
 * Include gui.hpp for the checked registry(app) convenience accessor.
 */
#include "application.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace elysia::gui {
using Draw = void (*)(Context&);
struct Window {
    std::string id, title;
    Draw draw;
    bool open = true;
    float width = 900, height = 600;
};
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
} // namespace elysia::gui
