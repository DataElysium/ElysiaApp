#include "support.hpp"
#include <iostream>

namespace {
struct Installed {
    bool plain = false;
    std::filesystem::path directory;
};
struct PlainPlugin {
    void build(elysia::App& app) const {
        app.world().resources().get<Installed>()->plain = true;
    }
};
struct PackagePlugin {
    std::filesystem::path directory;
    explicit PackagePlugin(const elysia::app::ModuleContext& context)
        : directory(context.directory) {}
    void build(elysia::App& app) const {
        app.world().resources().get<Installed>()->directory = directory;
    }
};
constexpr auto plain = elysia::app::module<PlainPlugin>("plain");
constexpr auto package = elysia::app::module<PackagePlugin>("package");
constexpr auto automatic = elysia::app::module<PlainPlugin>();
static_assert(plain.size == sizeof(plain) && plain.version == elysia::app::sdk_version);
static_assert(plain.abi == elysia::app::native_abi());
static_assert(std::string_view(plain.name) == nameof::nameof_type<PlainPlugin>());
static_assert(automatic.id == nullptr && automatic.release == nullptr);
} // namespace

void module_tests() {
    using namespace elysia::app;
    test::Temporary temporary;
    const auto file = temporary.path / "playset.json";
    for (const auto* folder : {"plain", "package"}) {
        const auto directory = temporary.path / folder;
        std::filesystem::create_directories(directory);
        std::ofstream(directory / "manifest.json") << "{}";
    }
    // A separate disabled package can use the same alias without inheriting loaded metadata.
    std::filesystem::create_directories(temporary.path / "disabled");
    std::ofstream(temporary.path / "disabled/manifest.json")
        << R"({"id":"plain","name":"Disabled alternative","version":"42"})";
    save_playset(file, {.mods = {{"package/manifest.json", true},
                                 {"plain/manifest.json", true},
                                 {"disabled/manifest.json", false}}});
    const auto manifest = read_manifest(temporary.path / "plain/manifest.json");
    test::require(manifest.id == "plain" && manifest.name.empty() && manifest.version.empty(),
                  "Empty manifest did not infer only its package folder");
    Runtime runtime;
    runtime.app().add_resource(Installed{});
    test::rejects(
        [&] {
            runtime.register_static(automatic);
        },
        "requires a package id");
    auto invalid = plain;
    invalid.install = nullptr;
    test::rejects(
        [&] {
            runtime.register_static(invalid);
        },
        "Invalid native module descriptor");
    runtime.register_static(plain);
    runtime.register_static(package);
    runtime.load(file);
    runtime.run();
    const auto& installed = *runtime.app().world().resources().get<Installed>();
    test::require(installed.plain && installed.directory == temporary.path / "package",
                  "Generated install callback did not construct the plugin correctly");
    const auto& session = *runtime.app().world().resources().get<ModSession>();
    test::require(session.catalog[1].manifest.name == plain.name &&
                      session.catalog[1].manifest.version.empty(),
                  "Loaded module did not supply its inferred name without inventing a release");
    test::require(session.catalog[2].manifest.name == "Disabled alternative" &&
                      session.catalog[2].manifest.version == "42",
                  "Loaded metadata leaked into a different package with the same alias");
    std::cout << "PASS compile-time module descriptors, inferred names, context constructors and "
                 "empty manifests\n";
}
