#include "support.hpp"
#include <demo/model.hpp>
#include <rfl/json.hpp>
#include <exception>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
using namespace elysia::app;
bool loaded(const std::filesystem::path& library) {
#ifdef _WIN32
    return GetModuleHandleW(library.c_str()) != nullptr;
#else
    auto* handle = dlopen(library.c_str(), RTLD_NOW | RTLD_NOLOAD);
    if (!handle) return false;
    dlclose(handle);
    return true;
#endif
}
std::filesystem::path package(const std::filesystem::path& root, const std::filesystem::path& bin,
                              const std::string& fixture) {
    const auto directory=root/fixture;
    const auto library=test::library_name("elysia-test-"+fixture);
    std::filesystem::create_directories(directory/"Assemblies");
    std::filesystem::copy_file(bin/library,directory/"Assemblies"/library);
    Manifest manifest{.id="test."+fixture,.name="Fixture",.version="0.1.0",
                      .libraries={{test::platform(),"Assemblies/"+library}}};
    std::ofstream(directory/"manifest.json")<<rfl::json::write(manifest);
    return directory/"Assemblies"/library;
}
}
namespace {
struct EndMarker {
    std::shared_ptr<std::vector<std::string>> trace;
    std::string value;
    EndMarker(std::shared_ptr<std::vector<std::string>> log,std::string name):trace(std::move(log)),value(std::move(name)){}
    ~EndMarker(){trace->push_back(value);}
};
struct AppMarker {
    std::shared_ptr<EndMarker> owner;
};
}
namespace {
struct RestartProbe {
    int installs = 0, runs = 0, destroyed = 0;
    bool fail_next = false;
    std::filesystem::path library;
};
RestartProbe* restart_probe = nullptr;
struct RestartMarker {
    ~RestartMarker() { ++restart_probe->destroyed; }
};
void install_restart(elysia::App& app, const elysia::app::ModuleContext&) {
    auto& probe = *restart_probe;
    test::require(probe.destroyed == probe.installs, "Next install began before old App destruction");
    if (probe.installs) test::require(!loaded(probe.library), "Old DLL remained loaded at next install");
    ++probe.installs;
    app.add_resource(std::make_shared<RestartMarker>());
    app.set_runner([](elysia::App& running) {
        auto& probe = *restart_probe;
        ++probe.runs;
        test::require(running.world().resources().get<elysia::app::Arguments>()->values ==
            std::vector<std::string>{"preserved-argument"}, "Plugin arguments disappeared on restart");
        if (probe.runs != 1) return;
        auto& session = *running.world().resources().get<elysia::app::ModSession>();
        test::require(session.catalog.size() == session.selected.mods.size(), "Catalog rows do not match selection");
        // Metadata-only discovery must expose an optional module without installing it.
        test::require(!session.selected.mods.back().enabled, "Discovered module was enabled automatically");
        session.selected.mods[0].enabled = false; // Remove the DLL for the next generation.
        session.selected.mods.back().enabled = probe.fail_next;
        session.save_and_restart(session);
        std::filesystem::current_path(session.file.parent_path()); // A plugin may change CWD.
        test::require(probe.destroyed == 0 && loaded(probe.library), "Restart destroyed code on the runner stack");
        test::require(session.restart_requested, "Restart request was not delivered");
    });
}
void restart_tests(const std::filesystem::path& bin) {
    using namespace elysia::app;
    test::Temporary temporary;
    const auto file = temporary.path / "playset.json";
    const auto root = temporary.path / "Mods";
    const auto library = test::library_name("elysia-demo-simulation");
    std::filesystem::create_directories(root / "simulation/Assemblies");
    std::filesystem::copy_file(bin / library, root / "simulation/Assemblies" / library);
    const auto manifest = [&](const std::string& folder, const Manifest& value) {
        std::filesystem::create_directories(root / folder);
        std::ofstream(root / folder / "manifest.json") << rfl::json::write(value);
    };
    manifest("simulation", {.id="demo.simulation",.name="Demo",.version="0.1.0",
        .libraries={{test::platform(),"Assemblies/"+library}}});
    manifest("restart", {.id="test.restart",.name="Restart",.version="1"});
    manifest("optional", {.id="test.unavailable",.name="Not installed",.version="1"});
    const ModuleDescriptor api{.id="test.restart",.name="Restart",.release="1",.install=install_restart};
    const ModuleDescriptor* apis[] = {&api};
    const auto original_cwd = std::filesystem::current_path();
    for (bool fail : {false, true}) {
        save_playset(file, {.mods={{"Mods/simulation/manifest.json",true},{"Mods/restart/manifest.json",true}}});
        RestartProbe probe{.fail_next=fail,.library=root / "simulation/Assemblies" / library};
        restart_probe = &probe;
        std::vector<std::string> values{"host", "--playset", file.lexically_relative(original_cwd).string(), "--", "preserved-argument"};
        std::vector<char*> args; for (auto& value : values) args.push_back(value.data());
        const auto result = run_cli(static_cast<int>(args.size()), args.data(), apis);
        std::filesystem::current_path(original_cwd);
        test::require(result == (fail ? 1 : 0), "Restart startup failure was not returned by host");
        test::require(probe.installs == (fail ? 1 : 2) && probe.runs == probe.installs &&
            probe.destroyed == probe.installs, "Host rebuild/destruction count mismatch");
        test::require(!loaded(probe.library), "Restart leaked a native DLL");
        test::require(!read_playset(file).mods[0].enabled, "Selected modules were not persisted");
        restart_probe = nullptr;
    }
    std::cout << "PASS whole-App restart, deferred teardown, DLL unload, persisted selection, arguments and failed next startup\n";
}
}
void lifetime_tests(const std::filesystem::path& bin) {
    using namespace elysia::app;
    restart_tests(bin);
    test::Temporary temporary;
    const auto file=temporary.path/"playset.json";
    {
        auto trace=std::make_shared<std::vector<std::string>>();
        {
            Runtime runtime;
            runtime.app().add_resource(AppMarker{std::make_shared<EndMarker>(trace,"App resource")});
            auto& lifetime=*runtime.app().world().resources().get<AfterAppLifetime>();
            lifetime.hold(std::make_shared<EndMarker>(trace,"first owner"));
            lifetime.hold(std::make_shared<EndMarker>(trace,"second owner"));
        }
        test::require(*trace==std::vector<std::string>{"App resource","second owner","first owner"},
                      "Infrastructure must outlive App resources and release owners in reverse order");
    }

    {
        Runtime runtime;
        int runs=0;
        runtime.app().set_runner([&](auto&){++runs;});
        runtime.run_composed();
        test::require(runs==1,"Directly composed App runner was not executed");
        test::rejects([&]{runtime.run_composed();},"unused Runtime");
        test::rejects([&]{runtime.load(file);},"Load one playset");
    }
    {
        Runtime runtime;
        runtime.app().set_runner([](auto&){throw std::logic_error("composed runner failed");});
        test::rejects([&]{runtime.run_composed();},"composed runner failed");
        test::rejects([&]{runtime.run_composed();},"unused Runtime");
    }
    std::filesystem::create_directories(temporary.path/"simulation");
    std::ofstream(temporary.path/"simulation/manifest.json")<<rfl::json::write(
        Manifest{.id="demo.simulation",.name="Simulation",.version="0.1.0"});
    for (const auto& fixture : {std::string("abi"),std::string("query")}) {
        const auto library=package(temporary.path,bin,fixture);
        save_playset(file,Playset{.mods={{"simulation/manifest.json",true},{fixture+"/manifest.json",true}}});
        auto trace=std::make_shared<std::vector<std::string>>();
        {
            Runtime runtime;runtime.register_static(demo::simulation_module());
            runtime.app().add_resource(demo::Trace{trace});
            test::rejects([&]{runtime.load(file);},fixture=="abi"?"ABI mismatch":"fixture query failed");
            test::require(trace->empty(),"Preflight failure installed an earlier valid module");
        }
        test::require(!loaded(library),"Rejected library was left loaded");
    }
    for (const auto& fixture : {std::string("install"),std::string("run")}) {
        const auto library=package(temporary.path,bin,fixture);
        save_playset(file,Playset{.mods={{fixture+"/manifest.json",true}}});
        auto trace=std::make_shared<std::vector<std::string>>();
        std::exception_ptr saved;
        {
            Runtime runtime;runtime.app().add_resource(demo::Trace{trace});
            try { runtime.load(file);runtime.run(); }
            catch (...) { saved=std::current_exception(); }
            test::require(bool(saved),"Throwing DLL unexpectedly succeeded");
            test::require(loaded(library),"Failure unloaded code before App destruction");
            test::rejects([&]{runtime.run();},"Run requires");
        }
        test::require(*trace==std::vector<std::string>{"fixture.destroyed"},"DLL resource did not die before unloading");
        test::require(!loaded(library),"Failed module was leaked instead of unloaded");
        // The DLL's custom exception vtable and literal are now gone. This must be a HOST-owned error.
        test::rejects([&]{std::rethrow_exception(saved);},fixture=="install"?"installation failed":"system failed");
        saved=nullptr;
    }
    std::cout<<"PASS ABI/query preflight, DLL-defined exceptions, partial install, system failure and actual library unload\n";
}
