#include <elysia_app/runtime.hpp>
#include <elysia_app/catalog.hpp>
#include <demo/model.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <map>
void catalog_tests();
void lifetime_tests(const std::filesystem::path&);

int main(int argc, char** argv) {
    try {
        const auto bin = std::filesystem::absolute(argv[0]).parent_path();
        catalog_tests();
        lifetime_tests(bin);
        auto trace = std::make_shared<std::vector<std::string>>();
        for (bool dynamic : {false,true}) {
            {
                elysia::app::Runtime runtime;
                runtime.app().add_resource(demo::Trace{trace});
                if (!dynamic) runtime.register_static(demo::simulation_module());
                runtime.load(bin / "sandbox" / (dynamic ? "dynamic.json" : "mixed.json"));
                runtime.run();
                const auto* progress = runtime.app().world().resources().get<demo::Progress>();
                if (!progress || progress->ticks != 4 || !progress->case_loaded) throw std::runtime_error("Case/runner did not execute");
                std::map<std::string,double> positions;
                runtime.app().world().query<const demo::Position,const demo::Label>().each([&](const auto& position,const auto& label) {
                    positions.emplace(label.name, position.x);
                });
                if (positions != std::map<std::string,double>{{"Alpha",8},{"Beta",22}})
                    throw std::runtime_error("Cross-module prefab registration, spawn or simulation mismatch");
                if (runtime.modules().size() != 2 || runtime.modules()[0].id != "demo.simulation")
                    throw std::runtime_error("Dependency order was not respected");
            }
            if (trace->empty() || trace->back() != "simulation.destroyed") throw std::runtime_error("DLL-owned resource was not destroyed");
        }
        std::cout << "PASS static/DLL parity, shared prefab registry, case-loader DLL, runner and destruction\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
