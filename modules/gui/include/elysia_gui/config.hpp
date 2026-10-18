#pragma once
/** @file config.hpp
 * DLL export/import and shared ImGui compilation macros. Runtime window/font settings are Options
 * in application.hpp. All tools must use the GUI provider's copy of ImGui.
 */
// All GUI modules consume the SAME ImGui implementation and configuration from
// elysia-gui.
#ifdef _WIN32
#ifdef ELYSIA_GUI_BUILD
#define ELYSIA_GUI_API __declspec(dllexport)
#else
#define ELYSIA_GUI_API __declspec(dllimport)
#endif
#else
#define ELYSIA_GUI_API __attribute__((visibility("default")))
#endif
#ifndef IMGUI_API
#define IMGUI_API ELYSIA_GUI_API
#endif
// Renderer backend entry points are private to the GUI provider.
#ifndef IMGUI_IMPL_API
#define IMGUI_IMPL_API
#endif
