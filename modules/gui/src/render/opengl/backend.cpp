#include "../backend.hpp"
#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdexcept>
#include <vector>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
namespace elysia::gui::render::backend {
namespace {
GLFWwindow *window = nullptr;
}
const char *name() { return "OpenGL 3.3"; }
void window_hints() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}
void initialize(GLFWwindow *target, bool vsync) {
  window = target;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(vsync ? 1 : 0);
  if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    throw std::runtime_error("Cannot initialize OpenGL renderer");
  if (!ImGui_ImplOpenGL3_Init("#version 330")) {
    ImGui_ImplGlfw_Shutdown();
    throw std::runtime_error("Cannot initialize OpenGL ImGui backend");
  }
}
void shutdown() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  window = nullptr;
}
void new_frame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
}
bool draw(ImDrawData *data) {
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  if (w <= 0 || h <= 0)
    return false;
  glViewport(0, 0, w, h);
  glClearColor(.02, .05, .07, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(data);
  return true;
}
void present() { glfwSwapBuffers(window); }
void capture(const std::filesystem::path &path) {
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path());
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  std::vector<uint8_t> pixels(size_t(w) * h * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  std::ofstream out(path, std::ios::binary);
  if (!out)
    throw std::runtime_error("Cannot write render capture");
  out << "P6\n" << w << " " << h << "\n255\n";
  for (int y = h - 1; y >= 0; --y)
    out.write(reinterpret_cast<const char *>(pixels.data() + size_t(y) * w * 3),
              w * 3);
}
} // namespace elysia::gui::render::backend
