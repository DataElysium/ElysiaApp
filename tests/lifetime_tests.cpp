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
void lifetime_tests(const std::filesystem::path& bin) {
    using namespace elysia::app;
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
