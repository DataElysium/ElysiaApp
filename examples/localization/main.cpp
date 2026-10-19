#include <elysia_app/runtime.hpp>
#include <elysia_gui/gui.hpp>
#include <elysia_gui/text.hpp>
#include <iostream>

namespace {
struct State {
    std::string language = "en";
    bool running = false;
};

void prepare(elysia::gui::Context& context) {
    auto resources = context.app.world().resources();
    resources.get<elysia::text::Catalog>()->select(resources.get<State>()->language);
}

void draw(elysia::gui::Context& context) {
    auto resources = context.app.world().resources();
    const auto& words = *resources.get<elysia::text::Catalog>();
    auto& state = *resources.get<State>();

    ImGui::SetNextWindowSize({560, 340}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(words.label({"demo.window", "Localization example"}))) {
        // Store the request now; prepare() changes the catalog before the next frame.
        if (ImGui::BeginCombo(words.label({"demo.language", "Language"}), state.language.c_str())) {
            for (const auto& language : words.languages())
                if (ImGui::Selectable(language.name.c_str(), state.language == language.id))
                    state.language = language.id;
            ImGui::EndCombo();
        }

        elysia::gui::text(words, {"demo.greeting", "Welcome aboard."});
        elysia::gui::text_wrapped(words, {"demo.help", "Change language, then try the button. The "
                                                       "application state stays unchanged."});
        if (ImGui::Button(words.label({"demo.start", "Start"})))
            state.running = !state.running;
        if (state.running)
            elysia::gui::text(words, {"demo.running", "Running"});
        else
            elysia::gui::text(words, {"demo.stopped", "Stopped"});
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
        auto& words = *app.world().resources().get<elysia::text::Catalog>();
        words.load(argv[1]);
        app.add_plugin(elysia::gui::GuiPlugin{{.title = "Elysia text example"}});
        app.add_resource(elysia::gui::Callbacks{.draw = draw, .prepare = prepare});
        runtime.run_composed();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
