#include "playset.hpp"
#include <functional>
#include <stdexcept>

namespace elysia::app::detail {
Plan resolve(const std::filesystem::path& file, const Playset& playset) {
    if (playset.schema != 1) throw std::runtime_error("Unsupported playset schema");
    Plan plan;
    std::map<std::string, size_t> ids;
    for (const auto& selection : playset.mods) {
        if (selection.manifest.empty()) throw std::runtime_error("Playset entry has no manifest path");
        if (!selection.enabled) continue;
        const auto path = std::filesystem::absolute(file).parent_path() / selection.manifest;
        auto manifest = read_manifest(path);
        if (!ids.emplace(manifest.id, plan.packages.size()).second)
            throw std::runtime_error("Duplicate selected module: " + manifest.id);
        plan.packages.push_back({std::move(manifest), path.parent_path()});
    }
    std::vector<int> visited(plan.packages.size());
    std::function<void(size_t)> visit = [&](size_t i) {
        if (visited[i] == 2) return;
        if (visited[i] == 1) throw std::runtime_error("Module dependency cycle at " + plan.packages[i].manifest.id);
        visited[i] = 1;
        for (const auto& dependency : plan.packages[i].manifest.dependencies) {
            const auto found = ids.find(dependency);
            if (found == ids.end()) throw std::runtime_error("Module " + plan.packages[i].manifest.id + " requires enabled " + dependency);
            visit(found->second);
        }
        visited[i] = 2;
        plan.order.push_back(i);
    };
    for (size_t i = 0; i < plan.packages.size(); ++i) visit(i);
    return plan;
}
} // namespace elysia::app::detail
