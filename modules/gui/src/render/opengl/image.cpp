#include <elysia_gui/gui.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
namespace elysia::gui {
struct Image::Impl {
  unsigned handle = 0;
  ~Impl() {
    if (handle)
      glDeleteTextures(1, &handle);
  }
};
Image::Image() : impl_(std::make_unique<Impl>()) {}
Image::~Image() = default;
void Image::upload(int width, int height, std::span<const uint8_t> rgba) {
  if (width <= 0 || height <= 0 || rgba.size() != size_t(width) * height * 4)
    throw std::invalid_argument("Invalid RGBA8 image");
  if (!glfwGetCurrentContext())
    throw std::logic_error(
        "Upload images after the GUI surface is initialized");
  unsigned texture = impl_->handle;
  if (!texture)
    glGenTextures(1, &texture);
  impl_->handle = texture;
  int previous = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, rgba.data());
  glBindTexture(GL_TEXTURE_2D, unsigned(previous));
}
ImTextureRef Image::view() const {
  return ImTextureRef(ImTextureID(impl_->handle));
}
} // namespace elysia::gui
