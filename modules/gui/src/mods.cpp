#include "mods.hpp"

namespace elysia::gui {
void draw_mods_menu(Context& context) {
    auto* session = context.app.world().resources().get<app::ModSession>();
    auto* editor = context.app.world().resources().get<ModEditor>();
    if (!session || !editor)
        return;
    if (ImGui::BeginMenu("Mods")) {
        ImGui::MenuItem("Manage mods...", nullptr, &editor->open);
        ImGui::EndMenu();
    }
}
void draw_mods_window(Context& context) {
    auto* session = context.app.world().resources().get<app::ModSession>();
    auto* editor = context.app.world().resources().get<ModEditor>();
    if (!session || !editor || !editor->open)
        return;
    ImGui::SetNextWindowSize({850, 620}, ImGuiCond_FirstUseEver);
    const bool visible = ImGui::Begin("Mods / App restart", &editor->open);
    if (visible) {
        ImGui::TextWrapped("Select modules for the next App. Saving restarts the whole App and "
                           "resets running data.");
        ImGui::TextWrapped("Playset: %s", session->file.string().c_str());
        if (ImGui::Button("Save selection & restart App")) {
            try {
                session->save_and_restart(*session);
                editor->error.clear();
            } catch (const std::exception& error) {
                editor->error = error.what();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset selection")) {
            session->selected = session->active;
            editor->error.clear();
        }
        if (!editor->error.empty())
            ImGui::TextWrapped("Cannot restart: %s", editor->error.c_str());
        ImGui::TextWrapped(
            "Dependencies must be enabled explicitly. Disabling a runner may close the GUI. Native "
            "library/ABI checks run at next startup; a failed startup exits with an error.");
        ImGui::Separator();
        if (ImGui::BeginChild("Module list", {0, 0})) {
            for (size_t i = 0; i < session->selected.mods.size(); ++i) {
                auto& choice = session->selected.mods[i];
                const auto& entry = session->catalog.at(i);
                ImGui::PushID(static_cast<int>(i));
                ImGui::Checkbox("##enabled", &choice.enabled);
                ImGui::SameLine();
                ImGui::TextUnformatted(entry.error.empty() ? entry.manifest.name.c_str()
                                                           : choice.manifest.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled(session->active.mods[i].enabled ? "(loaded)" : "(not loaded)");
                if (entry.error.empty()) {
                    ImGui::TextDisabled("%s / %s", entry.manifest.id.c_str(),
                                        entry.manifest.version.c_str());
                    if (!entry.manifest.description.empty())
                        ImGui::TextWrapped("%s", entry.manifest.description.c_str());
                    for (const auto& dependency : entry.manifest.dependencies)
                        ImGui::TextUnformatted(("Requires: " + dependency).c_str());
                } else
                    ImGui::TextWrapped("%s", entry.error.c_str());
                ImGui::Separator();
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
} // namespace elysia::gui
