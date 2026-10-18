#include <elysia_app/catalog.hpp>
#include "library.hpp"
#include <rfl/json.hpp>
#include <rfl/DefaultIfMissing.hpp>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace elysia::app {
namespace {
template<class T> T read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open " + path.string());
    auto value = rfl::json::read<T, rfl::DefaultIfMissing>(input);
    if (!value) throw std::runtime_error("Invalid " + path.string() + ": " + value.error().what());
    return std::move(value.value());
}
}
Manifest read_manifest(const std::filesystem::path& path) {
    auto value = read<Manifest>(path);
    if (value.schema != 1) throw std::runtime_error("Unsupported manifest schema: " + path.string());
    if (!detail::valid_id(value.id) || value.name.empty() || value.version.empty())
        throw std::runtime_error("Manifest requires id, name and version: " + path.string());
    for (const auto& dependency : value.dependencies)
        if (!detail::valid_id(dependency) || dependency == value.id)
            throw std::runtime_error("Invalid module dependency: " + dependency);
    return value;
}
Playset read_playset(const std::filesystem::path& path) {
    auto value = read<Playset>(path);
    if (value.schema != 1) throw std::runtime_error("Unsupported playset schema");
    for (const auto& entry : value.mods)
        if (entry.manifest.empty()) throw std::runtime_error("Playset entry has no manifest path");
    return value;
}
void save_playset(const std::filesystem::path& path, const Playset& value) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    auto temporary = path; temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << rfl::json::write(value);
        output.close();
        if (!output) throw std::runtime_error("Cannot save playset: " + path.string());
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace playset: Windows error " + std::to_string(GetLastError()));
#else
    std::filesystem::rename(temporary, path);
#endif
}
std::vector<CatalogEntry> discover(const std::filesystem::path& root) {
    std::vector<CatalogEntry> entries;
    if (!std::filesystem::exists(root)) return entries;
    for (const auto& directory : std::filesystem::directory_iterator(root)) {
        const auto path = directory.path() / "manifest.json";
        if (!directory.is_directory() || !std::filesystem::exists(path)) continue;
        CatalogEntry entry{std::filesystem::absolute(path), {}, {}};
        try { entry.manifest = read_manifest(path); }
        catch (const std::exception& error) { entry.error = error.what(); }
        entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    return entries;
}
} // namespace elysia::app
