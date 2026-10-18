#include "surface.hpp"
#include "backend.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif
#include <fstream>
#include <stdexcept>
#include <vector>
namespace elysia::gui::render {
struct Surface::Impl {
    GLFWwindow* window = nullptr;
    bool glfw = false, initialized = false;
    ~Impl() {
        if (initialized)
            backend::shutdown();
        if (window)
            glfwDestroyWindow(window);
        if (glfw)
            glfwTerminate();
    }
};
Surface::Surface(const Options& options) : impl_(std::make_unique<Impl>()) {
    if (options.x11)
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit())
        throw std::runtime_error("Cannot initialize GLFW");
    impl_->glfw = true;
    backend::window_hints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    impl_->window =
        glfwCreateWindow(options.width, options.height, options.title.c_str(), nullptr, nullptr);
    if (!impl_->window)
        throw std::runtime_error("Cannot create GUI window");
    backend::initialize(impl_->window, options.vsync);
    impl_->initialized = true;
    if (!options.hidden) {
#if defined(__linux__)
        if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
            XMapRaised(glfwGetX11Display(), glfwGetX11Window(impl_->window));
            XFlush(glfwGetX11Display());
        } else
#endif
            glfwShowWindow(impl_->window);
    }
}
Surface::~Surface() = default;
void Surface::poll() {
    glfwPollEvents();
}
void Surface::wait_events() {
    glfwWaitEventsTimeout(.05);
}
bool Surface::closing() const {
    return glfwWindowShouldClose(impl_->window);
}
void Surface::new_frame() {
    backend::new_frame();
}
bool Surface::draw(ImDrawData* data) {
    return backend::draw(data);
}
void Surface::present() {
    backend::present();
}
const char* Surface::name() {
    return backend::name();
}
void Surface::capture(const std::filesystem::path& path) {
    backend::capture(path);
}
} // namespace elysia::gui::render
