#include <elysia_app/runtime.hpp>
#include <elysia_app/catalog.hpp>
#include "library.hpp"
#include <functional>
#include <map>
#include <stdexcept>

namespace elysia::app {
namespace {
// Exception vtables/destructors can belong to a DLL too. Destroy the original exception while
// its library is pinned; only host-owned exceptions may escape Runtime's public call boundary.
template<class F> decltype(auto) native_call(std::string_view owner, F&& function) {
    try { return function(); }
    catch (const std::exception& error) {
        throw std::runtime_error(std::string(owner) + ": " + error.what());
    } catch (...) {
        throw std::runtime_error(std::string(owner) + ": unknown native exception");
    }
}
}
struct Runtime::Impl {
    enum class Phase { configuring, loading, ready, running, stopped, failed };
    // Declaration order is deliberate: App dies first, before any module library.
    struct Libraries {
        std::vector<std::unique_ptr<detail::Library>> items;
        ~Libraries() { while (!items.empty()) items.pop_back(); } // Dependents unload before dependencies.
    } libraries;
    struct Owners {
        std::vector<std::shared_ptr<void>> items;
        ~Owners() { while (!items.empty()) items.pop_back(); }
    } owners;
    struct StaticModule {
        std::string name, release;
        ModuleDescriptor api;
        explicit StaticModule(const ModuleDescriptor& descriptor)
            : name(descriptor.name), release(descriptor.release), api(descriptor) {}
    };
    std::map<std::string, StaticModule> statics;
    std::vector<LoadedModule> modules;
    App app;
    Phase phase = Phase::configuring;
};
Runtime::Runtime() : impl_(std::make_unique<Impl>()) {
    impl_->app.add_resource(AfterAppLifetime{&impl_->owners,
        [](void* state, std::shared_ptr<void> owner) {
            static_cast<Impl::Owners*>(state)->items.push_back(std::move(owner));
        }});
}
Runtime::~Runtime() = default;
App& Runtime::app() { return impl_->app; }
std::span<const LoadedModule> Runtime::modules() const { return impl_->modules; }
void Runtime::register_static(const ModuleDescriptor& api) {
    if (impl_->phase != Impl::Phase::configuring) throw std::logic_error("Register static modules before loading");
    detail::validate(api);
    const auto [entry, inserted] = impl_->statics.try_emplace(api.id, api);
    if (!inserted) throw std::invalid_argument("Duplicate static module: " + std::string(api.id));
    entry->second.api.id = entry->first.c_str();
    entry->second.api.name = entry->second.name.c_str();
    entry->second.api.release = entry->second.release.c_str();
}
void Runtime::load(const std::filesystem::path& playset_file) {
    if (impl_->phase != Impl::Phase::configuring) throw std::logic_error("Load one playset per Runtime");
    impl_->phase = Impl::Phase::loading;
    try {
        const auto playset = read_playset(playset_file);
        struct Package { Manifest manifest; std::filesystem::path root; };
        std::vector<Package> packages;
        std::map<std::string, size_t> ids;
        for (const auto& selection : playset.mods) {
            if (!selection.enabled) continue;
            const auto path = std::filesystem::absolute(playset_file).parent_path() / selection.manifest;
            auto manifest = read_manifest(path);
            if (!ids.emplace(manifest.id, packages.size()).second)
                throw std::runtime_error("Duplicate selected module: " + manifest.id);
            packages.push_back({std::move(manifest), path.parent_path()});
        }
        std::vector<int> visited(packages.size());
        std::vector<size_t> order;
        std::function<void(size_t)> visit = [&](size_t i) {
            if (visited[i] == 2) return;
            if (visited[i] == 1) throw std::runtime_error("Module dependency cycle at " + packages[i].manifest.id);
            visited[i] = 1;
            for (const auto& dependency : packages[i].manifest.dependencies) {
                const auto found = ids.find(dependency);
                if (found == ids.end()) throw std::runtime_error("Module " + packages[i].manifest.id + " requires enabled " + dependency);
                visit(found->second);
            }
            visited[i] = 2;
            order.push_back(i);
        };
        for (size_t i = 0; i < packages.size(); ++i) visit(i);

        struct Prepared { const ModuleDescriptor* api; size_t package; };
        std::vector<Prepared> prepared;
        // Preflight every descriptor before the first install callback can mutate the App.
        for (const auto i : order) {
            const auto& package = packages[i];
            const ModuleDescriptor* api = nullptr;
            if (package.manifest.libraries.empty()) {
                const auto found = impl_->statics.find(package.manifest.id);
                if (found == impl_->statics.end()) throw std::runtime_error("No registered static module: " + package.manifest.id);
                api = &found->second.api;
            } else {
                const auto found = package.manifest.libraries.find(detail::platform());
                if (found == package.manifest.libraries.end()) throw std::runtime_error("No library for this platform: " + package.manifest.id);
                auto library = std::make_unique<detail::Library>(detail::package_file(package.root, found->second));
                auto* loaded = library.get();
                // Keep every loaded library pinned through both preflight and App destruction on failure.
                impl_->libraries.items.push_back(std::move(library));
                auto query = loaded->query();
                if (!query) throw std::runtime_error("Missing elysia_app_module_v1: " + package.manifest.id);
                api = native_call(package.manifest.id, query);
                if (!api) throw std::runtime_error("Module returned no descriptor: " + package.manifest.id);
            }
            detail::validate(*api);
            if (package.manifest.id != api->id || package.manifest.version != api->release)
                throw std::runtime_error("Manifest/descriptor identity or release mismatch: " + package.manifest.id);
            prepared.push_back({api, i});
        }
        // ALL descriptors passed preflight and ALL libraries are pinned before installation begins.
        for (const auto& module : prepared) {
            const auto& package = packages[module.package];
            impl_->modules.push_back({package.manifest.id, package.root, !package.manifest.libraries.empty()});
            const ModuleContext context{package.manifest.id, package.root};
            native_call(package.manifest.id, [&] { module.api->install(impl_->app, context); });
        }
        impl_->phase = Impl::Phase::ready;
    } catch (...) {
        impl_->phase = Impl::Phase::failed;
        throw;
    }
}
void Runtime::run_composed() {
    if (impl_->phase != Impl::Phase::configuring)
        throw std::logic_error("Run composed requires an unused Runtime without a playset");
    impl_->phase = Impl::Phase::ready;
    run();
}
void Runtime::run() {
    if (impl_->phase != Impl::Phase::ready) throw std::logic_error("Run requires a successfully loaded, unused Runtime");
    impl_->phase = Impl::Phase::running;
    try {
        native_call("App runner", [&] { impl_->app.run(); });
        impl_->phase = Impl::Phase::stopped;
    } catch (...) {
        impl_->phase = Impl::Phase::failed;
        throw;
    }
}
} // namespace elysia::app
