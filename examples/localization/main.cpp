#include <elysia_app/runtime.hpp>
#include <elysia_gui/gui.hpp>
#include <elysia_text/i18n.hpp>
#include <iostream>

namespace {
struct State {
    std::string language = "en";
    bool running = false;
};

void prepare(elysia::gui::Context& context) {
    auto resources = context.app.world().resources();
    resources.get<elysia::text::I18nManager>()->select(resources.get<State>()->language);
}

void draw(elysia::gui::Context& context) {
    auto resources = context.app.world().resources();
    const auto& TR = *resources.get<elysia::text::I18nManager>();
    auto& state = *resources.get<State>();

    ImGui::SetNextWindowSize({560, 340}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(TR.t_id("demo.window", "Localization example"))) {
        // Store the request now; prepare() changes the language before the next frame.
        if (ImGui::BeginCombo(TR.t_id("demo.language", "Language"), state.language.c_str())) {
            for (const auto& language : TR.languages())
                if (ImGui::Selectable(language.name.c_str(), state.language == language.id))
                    state.language = language.id;
            ImGui::EndCombo();
        }

        ImGui::TextUnformatted(TR("demo.greeting", "Welcome aboard."));
        ImGui::TextWrapped("%s", TR("demo.help", "Change language, then try the button. The "
                                                 "application state stays unchanged."));
        if (ImGui::Button(TR.t_id("demo.start", "Start")))
            state.running = !state.running;
        if (state.running)
            ImGui::TextUnformatted(TR("demo.running", "Running"));
        else
            ImGui::TextUnformatted(TR("demo.stopped", "Stopped"));
    }
    ImGui::End();
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || !std::filesystem::is_directory(argv[1]))
            throw std::invalid_argument("Usage: elysia-text-example LOCALE_DIRECTORY [--gui-font "
                                        "FONT_FILE] [--gui-frames N] [--gui-hidden]");

        elysia::app::Runtime runtime;
        auto& app = runtime.app();
        elysia::app::Arguments arguments;
        for (int i = 2; i < argc; ++i)
            arguments.values.emplace_back(argv[i]);
        app.add_resource(std::move(arguments));
        app.add_resource(State{});
        app.add_plugin(elysia::text::Plugin{});
        auto& words = *app.world().resources().get<elysia::text::I18nManager>();
        words.load(argv[1]);
        app.add_plugin(elysia::gui::GuiPlugin{{.title = "Elysia text example"}});
        app.add_resource(elysia::gui::Callbacks{.draw = draw, .prepare = prepare});
        runtime.run_composed();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
