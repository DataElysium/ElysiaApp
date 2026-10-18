#include <demo/model.hpp>
#include <demo/authoring.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace demo {
namespace p = elysia::prefab;
struct CaseLoaderPlugin {
    std::filesystem::path directory;
    void build(elysia::App& app) const {
        // Installation only registers behavior; case I/O/spawning waits until every module is installed.
        app.add_startup_system("demo.case", [directory=directory](elysia::World* world) {
            auto path = directory / "cases/default.json";
            if (const auto* args = world->resources().get<elysia::app::Arguments>()) {
                for (size_t i = 0; i < args->values.size(); ++i) {
                    if (args->values[i] != "--case") continue;
                    if (i + 1 == args->values.size()) throw std::invalid_argument("Case loader expects --case FILE");
                    path = args->values[++i];
                }
            }
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Cannot read case: " + path.string());
            std::ostringstream bytes; bytes << file.rdbuf();
            const auto value = p::parse_json(bytes.str());
            const auto* fields = std::get_if<p::Object>(&value.get());
            const auto* steps_value = fields ? p::detail::find(*fields, "steps") : nullptr;
            const auto steps = steps_value ? elysia::reflect::from_generic<int>(*steps_value) : rfl::Result<int>(1);
            if (!steps || steps.value() < 1 || steps.value() > 1000000)
                throw std::invalid_argument("Case steps must be 1..1000000");
            const auto* scene = fields ? p::detail::find(*fields, "scene") : nullptr;
            if (!scene) throw std::invalid_argument("Case requires a prefab scene document");
            auto document = p::read_document(*scene);
            auto* authoring = world->resources().get<Authoring>();
            if (!authoring) throw std::runtime_error("Simulation authoring registry is not installed");
            authoring->prefabs.load_library(std::move(document.prefabs));
            for (const auto& instance : document.instances) {
                auto spawned = authoring->prefabs.spawn_class(*world, instance.prefab, instance.params, instance.id + ".");
                if (spawned.roots.size() != 1) throw std::runtime_error("Example case requires one root per instance");
            }
            world->resources().get<Settings>()->steps = steps.value();
            world->resources().get<Progress>()->case_loaded = true;
            if (auto* trace = world->resources().get<Trace>()) trace->entries->push_back("case.loaded");
        });
    }
};
}
ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
    static const elysia::app::ModuleDescriptor descriptor{
        .id="demo.case_loader", .name="Prefab case loader", .release="0.1.0",
        .install=[](elysia::App& app, const elysia::app::ModuleContext& context) {
            app.add_plugin(demo::CaseLoaderPlugin{context.directory});
        }};
    return &descriptor;
}
