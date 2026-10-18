#include "support.hpp"
#include <rfl/json.hpp>
#include <iostream>
using namespace elysia::app;
namespace {
struct Log {
    std::vector<std::string> events;
};
void alpha(elysia::App& app, const ModuleContext&) {
    app.world().resources().get<Log>()->events.push_back("alpha");
    app.add_startup_system("catalog.ready", [](elysia::World* world) {
        test::require(world->resources().get<Log>()->events ==
                          std::vector<std::string>{"alpha", "beta"},
                      "Startup ran before all installations completed");
        world->resources().get<Log>()->events.push_back("startup");
    });
}
void beta(elysia::App& app, const ModuleContext&) {
    app.world().resources().get<Log>()->events.push_back("beta");
}
ModuleDescriptor descriptor(const char* id, void (*install)(elysia::App&, const ModuleContext&)) {
    return {.id = id, .name = "Static test", .release = "1", .install = install};
}
void write(const std::filesystem::path& file, const Manifest& value) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << rfl::json::write(value);
}
} // namespace
void catalog_tests() {
    test::Temporary temporary;
    const auto file = temporary.path / "playset.json";
    const auto a = temporary.path / "Mods/alpha/manifest.json",
               b = temporary.path / "Mods/beta/manifest.json";
    Manifest first{.id = "alpha", .name = "Alpha", .version = "1"};
    Manifest second{.id = "beta", .name = "Beta", .version = "1", .dependencies = {"alpha"}};
    write(a, first);
    write(b, second);
    const auto alpha_api = descriptor("alpha", alpha), beta_api = descriptor("beta", beta);
    Playset plan{.mods = {{"Mods/beta/manifest.json", true}, {"Mods/alpha/manifest.json", true}}};
    save_playset(file, plan);
    test::require(read_playset(file).mods.front().manifest == plan.mods.front().manifest,
                  "Playset round trip lost ordering");
    {
        Runtime runtime;
        runtime.app().add_resource(Log{});
        {
            std::string id = "alpha", name = "Temporary metadata", release = "1";
            auto temporary_api = alpha_api;
            temporary_api.id = id.c_str();
            temporary_api.name = name.c_str();
            temporary_api.release = release.c_str();
            runtime.register_static(temporary_api);
        }
        runtime.register_static(beta_api);
        test::rejects(
            [&] {
                runtime.register_static(alpha_api);
            },
            "Duplicate static");
        test::rejects(
            [&] {
                runtime.run();
            },
            "Run requires");
        runtime.load(file);
        test::require(runtime.modules()[0].id == "alpha" && !runtime.modules()[0].dynamic,
                      "Static dependency-first loading failed");
        auto& session = *runtime.app().world().resources().get<ModSession>();
        session.selected.mods[1].enabled = false;
        test::rejects(
            [&] {
                session.save_and_restart(session);
            },
            "requires enabled alpha");
        test::require(!session.restart_requested && read_playset(file).mods[1].enabled,
                      "Rejected restart altered saved/running selection");
        session.selected = session.active;
        std::filesystem::create_directory(file.string() + ".tmp");
        test::rejects(
            [&] {
                session.save_and_restart(session);
            },
            "Cannot save playset");
        test::require(!session.restart_requested, "Failed save requested restart");
        std::filesystem::remove(file.string() + ".tmp");
        runtime.run();
        test::require(runtime.app().world().resources().get<Log>()->events.back() == "startup",
                      "Startup graph did not run");
        test::rejects(
            [&] {
                runtime.load(file);
            },
            "one playset");
        test::rejects(
            [&] {
                runtime.register_static(alpha_api);
            },
            "before loading");
        test::rejects(
            [&] {
                runtime.run();
            },
            "Run requires");
    }
    const auto preflight_reject = [&](std::string_view message) {
        Runtime runtime;
        runtime.app().add_resource(Log{});
        runtime.register_static(alpha_api);
        runtime.register_static(beta_api);
        test::rejects(
            [&] {
                runtime.load(file);
            },
            message);
        test::require(runtime.app().world().resources().get<Log>()->events.empty(),
                      "Preflight rejection installed a module");
        test::rejects(
            [&] {
                runtime.run();
            },
            "Run requires");
    };
    plan.mods[1].enabled = false;
    save_playset(file, plan);
    preflight_reject("requires enabled alpha");
    plan.mods[1].enabled = true;
    save_playset(file, plan);
    first.dependencies = {"beta"};
    write(a, first);
    preflight_reject("dependency cycle");
    first.dependencies.clear();
    write(a, first);
    plan.mods.push_back(plan.mods[1]);
    save_playset(file, plan);
    preflight_reject("Duplicate selected");
    plan.mods.pop_back();
    save_playset(file, plan);
    first.version = "2";
    write(a, first);
    preflight_reject("release mismatch");
    first.version = "1";
    first.libraries = {{test::platform(), "../outside"}};
    write(a, first);
    preflight_reject("escapes");
    first.libraries = {{"not-this-platform", "Assemblies/unused"}};
    write(a, first);
    preflight_reject("No library for this platform");
    first.libraries.clear();
    write(a, first);
    {
        auto bad = alpha_api;
        bad.abi ^= 1;
        Runtime runtime;
        test::rejects(
            [&] {
                runtime.register_static(bad);
            },
            "ABI mismatch");
        bad = alpha_api;
        ++bad.version;
        test::rejects(
            [&] {
                runtime.register_static(bad);
            },
            "descriptor/version");
    }
    plan.mods = {{"absent-and-disabled.json", false}};
    save_playset(file, plan);
    {
        Runtime empty;
        empty.load(file);
        empty.run();
        test::require(empty.modules().empty(), "Disabled entry was loaded");
    }
    std::ofstream(b) << "broken JSON";
    auto entries = discover(temporary.path / "Mods");
    test::require(entries.size() == 2 && !entries[1].error.empty(),
                  "Malformed catalog entry disappeared");
    first.schema = 99;
    write(a, first);
    test::rejects(
        [&] {
            read_manifest(a);
        },
        "schema");
    std::cout << "PASS catalog, persistent playset, static plugins, dependencies, preflight and "
                 "lifecycle guards\n";
}
