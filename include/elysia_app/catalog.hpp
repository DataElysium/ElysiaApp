#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace elysia::app {
/** libraries maps windows/linux/macos to paths inside this package.
 * Empty libraries selects an explicitly registered static implementation of the same ID.
 */
struct Manifest {
    int schema = 1;
    std::string id, name, version, author, description;
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> libraries;
};
struct Selection { std::string manifest; bool enabled = true; };
struct Playset { int schema = 1; std::vector<Selection> mods; };
struct CatalogEntry {
    std::filesystem::path path;
    Manifest manifest;
    std::string error;
};
/** Read and validate metadata without executing native code. */
Manifest read_manifest(const std::filesystem::path&);
Playset read_playset(const std::filesystem::path&);
void save_playset(const std::filesystem::path&, const Playset&);
/** Sorted child folders containing manifest.json; malformed entries remain visible with errors. */
std::vector<CatalogEntry> discover(const std::filesystem::path&);
} // namespace elysia::app
