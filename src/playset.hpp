#pragma once
#include <elysia_app/mod_catalog.hpp>

namespace elysia::app::detail {
struct Package {
    Manifest manifest;
    std::filesystem::path root;
};
struct Plan {
    std::vector<Package> packages;
    std::vector<size_t> order;
};
// Metadata-only resolution shared by startup and the restart request. Never loads code.
Plan resolve(const std::filesystem::path& file, const Playset&);
} // namespace elysia::app::detail
