#include <elysia_text/catalog.hpp>
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
        Catalog text;
        text.load(root / "absent");
        text.load(root / "base");
        require(std::string_view(text.get({"menu.play", "fallback"})) == "Play",
                "English default failed");
        auto english_label = std::string(text.label({"menu.play", "Play"}));
        text.select("zh-CN");
        require(std::string_view(text.get({"menu.play", "Play"})) == "开始",
                "Chinese lookup failed");
        require(std::string_view(text.get({"menu.quit", "fallback"})) == "Quit",
                "English fallback failed");
        require(std::string_view(text.get({"mod.new", "Source fallback"})) == "Source fallback",
                "Source fallback failed");
        require(std::string_view(text.get({"briefing", ""})) == "第一行\n第二行\n",
                "YAML multiline changed");
        const std::string chinese_label = text.label({"menu.play", "Play"});
        require(english_label.substr(english_label.find("###")) ==
                    chinese_label.substr(chinese_label.find("###")),
                "Language changed the widget ID");
        write(root / "mod/zh-CN.yaml", "mod.window: 工具窗口\n");
        text.load(root / "mod");
        require(std::string_view(text.get({"mod.window", "Tool"})) == "工具窗口",
                "Mod dictionary failed");
        Catalog other;
        require(other.language() == "en", "Catalogs shared locale state");
        require(text.languages().size() == 2, "Mod invented another language entry");
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
                    std::string_view(text.get({"menu.play", "Play"})) == "开始",
                "Bad package partially replaced live text");
        text.select("unavailable");
        require(std::string_view(text.get({"menu.play", "fallback"})) == "Play",
                "Unavailable locale fallback failed");
        std::cout << "PASS YAML, fallback, language switch, stable labels, mod catalogs and atomic "
                     "load\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
