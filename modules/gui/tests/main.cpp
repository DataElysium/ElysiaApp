#include <array>
#include <elysia_app/runtime.hpp>
#include <elysia_gui/gui.hpp>
#include <iostream>
#include <stdexcept>
namespace {
int panels = 0;
bool clicked = false;
ImVec2 button;
void panel(elysia::gui::Context &) {
  ++panels;
  ImGui::TextUnformatted("Shared ImGui API");
  if (ImGui::Button("Shared-context input"))
    clicked = true;
  auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
  button = {(minimum.x + maximum.x) * .5f, (minimum.y + maximum.y) * .5f};
}
void window(elysia::gui::Context &context) {
  elysia::gui::registry(context.app).draw_panel("test.panel", context);
}
template <class F> void rejects(F action) {
  try {
    action();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("Invalid GUI operation succeeded");
}
} // namespace
struct SurfaceCheck {
  elysia::gui::Image image;
  int initialized = 0, drawn = 0, submitted = 0;
};
int surface_check() {
  elysia::app::Runtime runtime;
  auto &app = runtime.app();
  app.add_plugin(elysia::gui::GuiPlugin{
      {.title = "Elysia custom frame test", .vsync = false, .hidden = true}});
  app.world().resources().get_or_create<SurfaceCheck>();
  app.add_resource(elysia::gui::Callbacks{
      [](elysia::gui::Context &context) {
        auto &state = *context.app.world().resources().get<SurfaceCheck>();
        ++state.initialized;
        const std::array<uint8_t, 4> pixel{255, 0, 128, 255};
        state.image.upload(1, 1, pixel);
      },
      [](elysia::gui::Context &context) {
        auto &state = *context.app.world().resources().get<SurfaceCheck>();
        ++state.drawn;
        ImGui::Begin("Application-owned frame");
        ImGui::Image(state.image.view(), {64, 64});
        ImGui::End();
        if (state.drawn == 3)
          elysia::gui::quit(context.app);
      },
      [](elysia::gui::Context &context, bool rendered) {
        auto &state = *context.app.world().resources().get<SurfaceCheck>();
        if (!rendered || state.drawn != state.submitted + 1)
          throw std::runtime_error("Frame callback order failed");
        ++state.submitted;
      }});
  app.run();
  const auto &state = *app.world().resources().get<SurfaceCheck>();
  if (state.initialized != 1 || state.drawn != 3 || state.submitted != 3)
    throw std::runtime_error("Application frame ownership or quit failed");
  std::cout
      << "PASS custom application frame, image, submitted callback and quit\n";
  return 0;
}
int main(int argc, char **argv) {
  try {
    if (argc > 1 && std::string_view(argv[1]) == "--surface")
      return surface_check();
    elysia::app::Runtime runtime;
    runtime.app().add_plugin(elysia::gui::GuiPlugin{});
    rejects([&] { runtime.app().add_plugin(elysia::gui::GuiPlugin{}); });
    rejects([&] { elysia::gui::check_abi(elysia::gui::native_abi() ^ 1); });
    auto &windows = elysia::gui::registry(runtime.app());
    rejects([&] { windows.check_abi(elysia::gui::native_abi() ^ 1); });
    windows.add_panel("test.panel", panel);
    rejects([&] { windows.add_panel("test.panel", panel); });
    windows.add_window({"test.window", "Test", window});
    rejects([&] { windows.add_window({"test.window", "Duplicate", window}); });
    if (windows.set_open("absent", true))
      throw std::runtime_error("Missing window unexpectedly found");
    auto *context = ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1280, 800};
    io.DeltaTime = 1.f / 60;
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    elysia::gui::Context drawing{runtime.app()};
    for (int i = 0; i < 2; ++i) {
      ImGui::NewFrame();
      windows.draw(drawing);
      ImGui::Render();
    }
    if (panels != 2 || ImGui::GetDrawData()->TotalVtxCount == 0)
      throw std::runtime_error(
          "Cross-library ImGui context or panel composition failed");
    const auto frame = [&] {
      ImGui::NewFrame();
      windows.draw(drawing);
      ImGui::Render();
    };
    io.AddMousePosEvent(button.x, button.y);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame();
    if (!clicked)
      throw std::runtime_error(
          "ImGui input did not reach the registered panel");
    elysia::gui::Image image;
    rejects([&] { image.upload(0, 0, {}); });
    std::array<uint8_t, 4> pixel{};
    rejects([&] {
      image.upload(1, 1, pixel);
    }); // No GPU surface was created by this unit test.
    ImGui::DestroyContext(context);
    std::cout << "PASS shared ImGui, GUI ABI, window/panel registry and "
                 "host-owned frame composition\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
