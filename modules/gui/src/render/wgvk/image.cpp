#include "../backend.hpp"
#include <elysia_gui/image.hpp>
namespace elysia::gui {
struct Image::Impl {
    render::backend::Texture texture;
};
Image::Image() : impl_(std::make_unique<Impl>()) {}
Image::~Image() = default;
void Image::upload(int width, int height, std::span<const uint8_t> pixels) {
    impl_->texture.upload(width, height, pixels);
}
ImTextureRef Image::view() const {
    return ImTextureRef(impl_->texture.id());
}
} // namespace elysia::gui
