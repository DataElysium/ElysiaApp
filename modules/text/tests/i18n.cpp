#include <elysia_text/i18n.hpp>
#include <elysia/elysia.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* reason) {
    if (!value)
        throw std::runtime_error(reason);
}
void write(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << text;
}
} // namespace
int main() {
    try {
        using namespace elysia::text;
        auto root = std::filesystem::temp_directory_path() /
                    ("elysia-text-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup {
            std::filesystem::path root;
            ~Cleanup() {
                std::error_code ignored;
                std::filesystem::remove_all(root, ignored);
            }
        } cleanup{root};
        write(root / "base/en.yaml", "_name: English\nmenu.play: Play\nmenu.quit: Quit\n");
        write(root / "base/zh-CN.yaml",
              "_name: 简体中文\nmenu.play: 开始\nbriefing: |\n  第一行\n  第二行\n");
        I18nManager text;
        const auto& TR = text;
        require(std::string_view(TR("Untranslated")) == "Untranslated",
                "Single-argument source fallback failed");
        require(std::string_view(TR("absent.key", "Fallback")) == "Fallback",
                "Explicit source fallback failed");
        require(std::string_view(TR.t_id("Start")) == "Start###Start",
                "Single-argument stable label failed");
        text.load(root / "absent");
        text.load(root / "base");
        require(std::string_view(text("menu.play", "fallback")) == "Play",
                "English default failed");
        auto english_label = std::string(text.t_id("menu.play", "Play"));
        text.select("zh-CN");
        require(std::string_view(text("menu.play", "Play")) == "开始", "Chinese lookup failed");
        require(std::string_view(text("menu.quit", "fallback")) == "Quit",
                "English fallback failed");
        require(std::string_view(text("mod.new", "Source fallback")) == "Source fallback",
                "Source fallback failed");
        require(std::string_view(text("briefing", "")) == "第一行\n第二行\n",
                "YAML multiline changed");
        const std::string chinese_label = text.t_id("menu.play", "Play");
        require(english_label.substr(english_label.find("###")) ==
                    chinese_label.substr(chinese_label.find("###")),
                "Language changed the widget ID");
        write(root / "mod/zh-CN.yaml", "mod.window: 工具窗口\n");
        text.load(root / "mod");
        require(std::string_view(text("mod.window", "Tool")) == "工具窗口",
                "Mod dictionary failed");
        I18nManager other;
        require(other.language() == "en", "I18nManagers shared locale state");
        require(text.languages().size() == 2, "Mod invented another language entry");
        write(root / "extra/ja.yaml", "_name: 日本語\nStart: 開始\n");
        text.load(root / "extra");
        text.select("ja");
        require(std::string_view(TR("Start")) == "開始" && text.languages().size() == 3,
                "Data-defined language or English-key lookup failed");
        text.select("zh-CN");
        const auto revision = text.revision();
        write(root / "bad/a.yaml", "menu.play: Changed\n");
        write(root / "bad/z.yaml", "duplicate: one\nduplicate: two\n");
        bool rejected = false;
        try {
            text.load(root / "bad");
        } catch (const std::exception&) {
            rejected = true;
        }
        require(rejected && text.revision() == revision &&
                    std::string_view(text("menu.play", "Play")) == "开始",
                "Bad package partially replaced live text");
        text.select("unavailable");
        require(std::string_view(text("menu.play", "fallback")) == "Play",
                "Unavailable locale fallback failed");
        {
            elysia::App first, second;
            first.add_plugin(Plugin{});
            second.add_plugin(Plugin{});
            auto& first_i18n = *first.world().resources().get<I18nManager>();
            auto& second_i18n = *second.world().resources().get<I18nManager>();
            first_i18n.load(root / "base");
            first_i18n.select("zh-CN");
            require(&first_i18n != &second_i18n && second_i18n.language() == "en" &&
                        std::string_view(first_i18n("menu.play")) == "开始" &&
                        std::string_view(second_i18n("menu.play", "Play")) == "Play",
                    "App resources unexpectedly shared translation state");
        }
        elysia::App rebuilt;
        rebuilt.add_plugin(Plugin{});
        const auto& fresh = *rebuilt.world().resources().get<I18nManager>();
        require(fresh.language() == "en" && std::string_view(fresh("menu.play")) == "menu.play",
                "Rebuilt App inherited old dictionaries or language");
        std::cout << "PASS callable I18nManager, YAML, language packs, stable labels, atomic load "
                     "and App resource isolation/rebuild\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
