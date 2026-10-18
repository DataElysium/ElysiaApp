#pragma once
/** @file image.hpp
 * Owned RGBA8 image for ImGui. Upload pixels on the GUI thread and use view() when drawing.
 * Destroy images before their GUI session. Native device interoperation is not provided here.
 */
#include "config.hpp"
#include <imgui.h>
#include <cstdint>
#include <memory>
#include <span>

namespace elysia::gui {
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

} // namespace elysia::gui
