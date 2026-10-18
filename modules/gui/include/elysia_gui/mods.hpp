#pragma once
/** @file mods.hpp
 * Optional Mods-menu entry for a custom application menu bar.
 * The GUI runner draws its editor; the outer Runtime host performs the requested restart.
 */
#include "config.hpp"

namespace elysia::gui {
struct Context;
/** Reusable Mods menu for application-owned menu bars. The GUI runner draws its
 * editor window and returns after a successful save/restart request. Available
 * only for a playset-loaded Runtime; rebuilding is the outer host's job.
 */
ELYSIA_GUI_API void draw_mods_menu(Context&);
} // namespace elysia::gui
