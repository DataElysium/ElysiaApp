#include <elysia_app/module.hpp>
#include "render/surface.hpp"
#include <elysia_app/application.hpp>
#include <elysia_app/mod_selection.hpp>
#include "mods.hpp"
#include <cstdio>
#include <cstdlib>
#include <elysia_gui/gui.hpp>
#include <fstream>
#include <stdexcept>

namespace elysia::gui {
namespace {
struct Settings {
    render::Options surface;
    int frames = 0;
    bool diagnostics = false;
    std::filesystem::path capture, layout, font;
};
Settings settings(App& app, const Options& defaults) {
    Settings result;
    result.surface = {defaults.title,  defaults.width, defaults.height,
                      defaults.hidden, defaults.vsync, defaults.x11};
    result.layout = defaults.layout;
    result.font = defaults.font;
    if (const auto* diagnostics = app.world().resources().get<app::Diagnostics>())
        result.diagnostics = diagnostics->enabled;
    if (const auto* args = app.world().resources().get<app::Arguments>()) {
        for (size_t i = 0; i < args->values.size(); ++i) {
            const auto& option = args->values[i];
            if (!option.starts_with("--gui-"))
                continue;
            if (option == "--gui-hidden") {
                result.surface.hidden = true;
                continue;
            }
            if (option == "--gui-no-vsync") {
                result.surface.vsync = false;
                continue;
            }
            if (i + 1 == args->values.size())
                throw std::invalid_argument("Missing value for " + option);
            const auto& value = args->values[++i];
            if (option == "--gui-frames") {
                size_t consumed = 0;
                result.frames = std::stoi(value, &consumed);
                if (consumed != value.size() || result.frames < 1)
                    throw std::invalid_argument("--gui-frames must be positive");
            } else if (option == "--gui-capture")
                result.capture = value;
            else if (option == "--gui-layout")
                result.layout = value;
            else if (option == "--gui-font")
                result.font = value;
            else
                throw std::invalid_argument("Unknown GUI option: " + option);
        }
    }
    if (!result.capture.empty() && !result.frames)
        throw std::invalid_argument("--gui-capture requires --gui-frames");
    return result;
}
struct Session {
    Settings options;
    std::string layout;
    ImGuiContext* context = nullptr;
    std::unique_ptr<render::Surface> surface;
    bool ran = false, quit = false;
    explicit Session(Settings value) : options(std::move(value)) {}
    ~Session() {
        if (context) {
            ImGui::SetCurrentContext(context);
            surface.reset();
            ImGui::DestroyContext(context);
            if (options.diagnostics) {
                std::fprintf(stdout, "GUI context destroyed after App resources\n");
                std::fflush(stdout);
            }
        }
    }
    void initialize() {
        context = ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        if (!options.layout.empty()) {
            if (!options.layout.parent_path().empty())
                std::filesystem::create_directories(options.layout.parent_path());
            layout = options.layout.string();
            io.IniFilename = layout.c_str();
        } else
            io.IniFilename = nullptr; // Persistence is explicit per application/profile.
        auto font = options.font;
        if (font.empty()) {
#ifdef _WIN32
            // Microsoft YaHei ships with Windows and carries CJK glyphs Segoe UI lacks.
            if (const char* directory = std::getenv("WINDIR")) {
                font = std::filesystem::path(directory) / "Fonts/msyh.ttc";
                if (!std::filesystem::is_regular_file(font))
                    font = std::filesystem::path(directory) / "Fonts/segoeui.ttf";
            }
#else
            for (const char* path : {"/usr/share/fonts/noto/NotoSans-Regular.ttf",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"})
                if (std::filesystem::is_regular_file(path)) {
                    font = path;
                    break;
                }
#endif
        }
        if (!font.empty() && !io.Fonts->AddFontFromFileTTF(font.string().c_str(), 18))
            throw std::runtime_error("Cannot load GUI font: " + font.string());
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.WindowPadding = {14, 12};
        style.FramePadding = {8, 5};
        style.ItemSpacing = {8, 8};
        style.WindowRounding = 3;
        style.FrameRounding = 3;
        surface = std::make_unique<render::Surface>(options.surface);
    }
};
struct Service {
    std::shared_ptr<Session> session;
};
} // namespace
void GuiPlugin::build(App& app) const {
    if (app.world().resources().get<Registry>())
        throw std::logic_error("GUI plugin installed twice");
    const auto* lifetime = app.world().resources().get<app::AfterAppLifetime>();
    if (!lifetime)
        throw std::logic_error("GUI module requires an ElysiaApp Runtime lifetime owner");
    if (options.width < 100 || options.height < 100 || options.title.empty())
        throw std::invalid_argument("Invalid GUI window options");
    auto session = std::make_shared<Session>(settings(app, options));
    lifetime->hold(session);
    app.add_resource(Service{std::move(session)})
        .add_resource(Registry{})
        .add_resource(ModEditor{});
    app.set_runner([](App& running) {
        run(running);
    }); // Normal last-installed-runner semantics.
}
void run(App& app) {
    auto* service = app.world().resources().get<Service>();
    if (!service)
        throw std::logic_error("GUI plugin is not installed");
    auto& session = *service->session;
    if (session.ran)
        throw std::logic_error("Run a GUI session once per App");
    session.ran = true;
    session.initialize();
    auto& windows = registry(app);
    Context context{app};
    const auto callbacks = app.world().resources().get<Callbacks>()
                               ? *app.world().resources().get<Callbacks>()
                               : Callbacks{};
    if (callbacks.initialize)
        callbacks.initialize(context);
    app.init_serial();
    int frames = 0;
    uint64_t vertices = 0;
    while (!session.quit && !session.surface->closing() &&
           (!session.options.frames || frames < session.options.frames)) {
        session.surface->poll();
        if (callbacks.prepare)
            callbacks.prepare(context);
        session.surface->new_frame();
        ImGui::NewFrame();
        try {
            app.update();
            if (callbacks.draw)
                callbacks.draw(context);
            else {
                if (ImGui::BeginMainMenuBar()) {
                    ImGui::TextUnformatted("ELYSIA APP");
                    if (ImGui::BeginMenu("File")) {
                        if (ImGui::MenuItem("Quit"))
                            session.quit = true;
                        ImGui::EndMenu();
                    }
                    windows.draw_menu();
                    draw_mods_menu(context);
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", render::Surface::name());
                    ImGui::EndMainMenuBar();
                }
                ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                             ImGuiDockNodeFlags_PassthruCentralNode);
            }
            windows.draw(context);
            draw_mods_window(context);
            ImGui::Render();
        } catch (...) {
            ImGui::EndFrame();
            throw;
        }
        if (session.options.diagnostics)
            vertices += ImGui::GetDrawData()->TotalVtxCount;
        const bool rendered = session.surface->draw(ImGui::GetDrawData());
        ++frames;
        if (rendered && !session.options.capture.empty() && frames == session.options.frames)
            session.surface->capture(session.options.capture);
        if (callbacks.submitted)
            callbacks.submitted(context, rendered);
        if (rendered)
            session.surface->present();
        else
            session.surface->wait_events();
        if (const auto* mods = app.world().resources().get<app::ModSession>();
            mods && mods->restart_requested)
            session.quit = true;
    }
    if (session.options.diagnostics)
        std::fprintf(stdout, "GUI frames=%d vertices=%llu windows=%zu renderer=%s\n", frames,
                     static_cast<unsigned long long>(vertices), windows.window_count(),
                     render::Surface::name());
}
void quit(App& app) {
    auto* service = app.world().resources().get<Service>();
    if (!service)
        throw std::logic_error("GUI plugin is not installed");
    service->session->quit = true;
}
void capture(App& app, const std::filesystem::path& path) {
    auto* service = app.world().resources().get<Service>();
    if (!service || !service->session->surface)
        throw std::logic_error("GUI surface is not initialized");
    service->session->surface->capture(path);
}
const char* renderer_name() {
    return render::Surface::name();
}
} // namespace elysia::gui
ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
    static constexpr auto descriptor = elysia::app::module<elysia::gui::GuiPlugin>();
    return &descriptor;
}
