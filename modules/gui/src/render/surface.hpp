#pragma once
#include <filesystem>
#include <imgui.h>
#include <memory>
#include <string>
namespace elysia::gui::render {
struct Options {
    std::string title;
    int width = 1440, height = 900;
    bool hidden = false, vsync = true, x11 = false;
};
/** Private backend boundary: UI registries and consumer DLLs never include
 * graphics API headers. */
class Surface {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit Surface(const Options&);
    ~Surface();
    void poll();
    bool closing() const;
    void new_frame();
    bool draw(ImDrawData*);
    void present();
    void wait_events();
    void capture(const std::filesystem::path&);
    static const char* name();
};
} // namespace elysia::gui::render
