#include "support.hpp"
#include <iostream>
#include <type_traits>

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
constexpr elysia::app::MetadataEntry properties[] = {
    {"fleet.side", "client"}, {"custom.note", "逗号,引号\"和等号="}, {"custom.empty", ""}};
constexpr auto package = elysia::app::module<PackagePlugin>(properties, "package");
constexpr auto automatic = elysia::app::module<PlainPlugin>();
constexpr auto annotated = elysia::app::module<PlainPlugin>(properties);
static_assert(std::is_standard_layout_v<elysia::app::MetadataEntry> &&
              std::is_trivial_v<elysia::app::MetadataEntry>);
static_assert(plain.size == sizeof(plain) && plain.version == elysia::app::sdk_version);
static_assert(plain.abi == elysia::app::native_abi());
static_assert(std::string_view(plain.name) == nameof::nameof_type<PlainPlugin>());
static_assert(automatic.id == nullptr && automatic.release == nullptr);
static_assert(automatic.metadata == nullptr && automatic.metadata_count == 0);
static_assert(annotated.id == nullptr && annotated.metadata_count == 3);
static_assert(std::string_view(package.metadata[0].value) == "client");
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
    invalid = plain;
    invalid.metadata_count = 1;
    test::rejects(
        [&] {
            runtime.register_static(invalid);
        },
        "metadata array is null");
    for (const MetadataEntry entry : {MetadataEntry{nullptr, "value"}, MetadataEntry{"", "value"},
                                      MetadataEntry{"key", nullptr}}) {
        invalid.metadata = &entry;
        test::rejects(
            [&] {
                runtime.register_static(invalid);
            },
            "Module metadata requires");
    }
    runtime.register_static(plain);
    runtime.register_static(package);
    runtime.load(file);
    const auto& loaded = runtime.modules()[0];
    test::require(loaded.id == "package" && loaded.metadata.size() == 3 &&
                      loaded.metadata[0].key == "fleet.side" &&
                      loaded.metadata[0].value == "client" &&
                      loaded.metadata[1].value == properties[1].value &&
                      loaded.metadata[2].value.empty() && runtime.modules()[1].metadata.empty(),
                  "Runtime lost opaque metadata, empty values or an unannotated module");
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
                 "empty manifests and native metadata\n";
}
