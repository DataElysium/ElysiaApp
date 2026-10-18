#pragma once
#include <elysia_gui/application.hpp>
#include <elysia_gui/mods.hpp>
namespace elysia::gui {
struct ModEditor {
    bool open = false;
    std::string error;
};
void draw_mods_window(Context&);
} // namespace elysia::gui
