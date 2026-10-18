#pragma once
#include <filesystem>
#include <imgui.h>
#include <span>
struct GLFWwindow;
namespace elysia::gui::render::backend {
void window_hints();
void initialize(GLFWwindow *, bool);
void shutdown();
void new_frame();
bool draw(ImDrawData *);
void present();
void capture(const std::filesystem::path &);
const char *name();
// Private WGVK image ownership; not part of the GUI SDK.
class Texture {
  uintptr_t handle_ = 0;

public:
  ~Texture();
  void upload(int, int, std::span<const uint8_t>);
  ImTextureID id() const { return ImTextureID(handle_); }
};
} // namespace elysia::gui::render::backend
