#pragma once
#include <elysia_gui/gui.hpp>
namespace elysia::gui {
struct ModEditor { bool open = false; std::string error; };
void draw_mods_window(Context &);
}
