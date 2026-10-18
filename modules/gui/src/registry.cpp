#include <algorithm>
#include <cmath>
#include <elysia_gui/gui.hpp>
#include <map>
#include <stdexcept>

namespace elysia::gui {
struct Registry::Impl {
  std::vector<Window> windows;
  std::map<std::string, Draw, std::less<>> panels;
  bool drawing = false;
  void installing() const {
    if (drawing)
      throw std::logic_error("Register GUI callbacks outside drawing");
  }
};
Registry::Registry() : impl_(std::make_unique<Impl>()) {}
Registry::~Registry() = default;
Registry::Registry(Registry &&) noexcept = default;
void check_abi(uint64_t abi) {
  if (abi != native_abi())
    throw std::runtime_error("GUI SDK/ImGui configuration mismatch");
}
void Registry::check_abi(uint64_t abi) const { gui::check_abi(abi); }
void Registry::add_window(Window window) {
  impl_->installing();
  if (window.id.empty() || window.title.empty() || !window.draw ||
      !std::isfinite(window.width) || !std::isfinite(window.height) ||
      window.width < 200 || window.height < 100)
    throw std::invalid_argument("Invalid GUI window");
  for (const auto &previous : impl_->windows)
    if (previous.id == window.id)
      throw std::invalid_argument("Duplicate GUI window: " + window.id);
  impl_->windows.push_back(std::move(window));
}
void Registry::add_panel(std::string id, Draw draw) {
  impl_->installing();
  if (id.empty() || !draw)
    throw std::invalid_argument("Invalid GUI panel");
  if (!impl_->panels.emplace(id, draw).second)
    throw std::invalid_argument("Duplicate GUI panel: " + id);
}
bool Registry::draw_panel(std::string_view id, Context &context) {
  const auto found = impl_->panels.find(id);
  if (found == impl_->panels.end())
    return false;
  const bool previous = impl_->drawing;
  impl_->drawing = true;
  struct Restore {
    bool &value;
    bool previous;
    ~Restore() { value = previous; }
  } restore{impl_->drawing, previous};
  ImGui::PushID(id.data(), id.data() + id.size());
  try {
    found->second(context);
  } catch (...) {
    ImGui::PopID();
    throw;
  }
  ImGui::PopID();
  return true;
}
bool Registry::set_open(std::string_view id, bool open) {
  for (auto &window : impl_->windows)
    if (window.id == id) {
      window.open = open;
      return true;
    }
  return false;
}
size_t Registry::window_count() const { return impl_->windows.size(); }
void Registry::draw_menu() {
  if (ImGui::BeginMenu("Windows")) {
    for (auto &window : impl_->windows)
      ImGui::MenuItem(window.title.c_str(), nullptr, &window.open);
    ImGui::EndMenu();
  }
}
void Registry::draw(Context &context) {
  if (impl_->drawing)
    throw std::logic_error("Recursive GUI window drawing");
  impl_->drawing = true;
  struct Reset {
    bool &value;
    ~Reset() { value = false; }
  } reset{impl_->drawing};
  size_t index = 0;
  for (auto &window : impl_->windows) {
    if (!window.open)
      continue;
    const auto *viewport = ImGui::GetMainViewport();
    const float offset = 28.f * float(index++);
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + 30 + offset, viewport->WorkPos.y + 30 + offset},
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        {std::min(window.width, viewport->WorkSize.x - 60),
         std::min(window.height, viewport->WorkSize.y - 60)},
        ImGuiCond_FirstUseEver);
    const auto title = window.title + "###" + window.id;
    const bool visible = ImGui::Begin(title.c_str(), &window.open);
    try {
      if (visible)
        window.draw(context);
    } catch (...) {
      ImGui::End();
      throw;
    }
    ImGui::End();
  }
}
} // namespace elysia::gui
