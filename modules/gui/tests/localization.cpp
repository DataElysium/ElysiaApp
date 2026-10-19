#include <elysia_gui/config.hpp>
#include <elysia_text/i18n.hpp>
#include <imgui.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>

// The caller owns the shared ImGui context and has already initialized its font atlas.
void localization_check() {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("elysia-gui-text-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    struct Cleanup {
        std::filesystem::path directory;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
    } cleanup{directory};
    std::ofstream(directory / "fr.yaml")
        << "test.literal: '100% %s %n'\ntest.button: Démarrer\ntest.tree: Détails\n";
    elysia::text::I18nManager i18n;
    const auto& TR = i18n;
    i18n.load(directory);
    ImVec2 button;
    bool clicked = false;
    bool first_frame = true;
    ImGuiID english_id = 0;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({40, 40});
        ImGui::SetNextWindowSize({500, 300});
        ImGui::Begin("Localized widgets");
        ImGui::TextUnformatted(TR("test.literal", "Literal text"));
        if (std::abs(ImGui::GetItemRectSize().x -
                     ImGui::CalcTextSize(TR("test.literal", "Literal text")).x) > 1)
            throw std::runtime_error("Localized text was interpreted as a format string");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 80);
        ImGui::TextWrapped("%s", TR("test.wrap", "This sentence must wrap onto several lines."));
        ImGui::PopTextWrapPos();
        if (ImGui::GetItemRectSize().y <= ImGui::GetTextLineHeight())
            throw std::runtime_error("Localized text ignored the wrap position");
        const auto label = TR.t_id("test.button", "Start");
        if (!english_id)
            english_id = ImGui::GetID(label);
        if (english_id != ImGui::GetID(label))
            throw std::runtime_error("Language switch changed the button ID");
        clicked |= ImGui::Button(label);
        const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
        button = {(minimum.x + maximum.x) * .5f, (minimum.y + maximum.y) * .5f};
        if (first_frame)
            ImGui::SetNextItemOpen(true);
        if (!ImGui::TreeNode(TR.t_id("test.tree", "Details")))
            throw std::runtime_error("Tree expansion was lost during language switch");
        ImGui::TreePop();
        first_frame = false;
        ImGui::End();
        ImGui::Render();
    };
    frame();
    frame();
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(button.x, button.y);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    i18n.select("fr");
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame();
    if (!clicked)
        throw std::runtime_error("Button lost its pending click during language switch");
}
