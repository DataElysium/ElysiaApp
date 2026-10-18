#include <demo/model.hpp>
#include <demo/authoring.hpp>
#include <iostream>
#include <stdexcept>

namespace demo {
namespace {
struct Lifetime {
    std::shared_ptr<std::vector<std::string>> trace;
    explicit Lifetime(std::shared_ptr<std::vector<std::string>> value) : trace(std::move(value)) {}
    ~Lifetime() {
        if (trace)
            trace->push_back("simulation.destroyed");
    }
};
} // namespace
void SimulationPlugin::build(elysia::App& app) const {
    if (app.world().resources().get<Progress>())
        throw std::logic_error("Simulation plugin installed twice");
    app.add_resource(Settings{}).add_resource(Progress{}).add_resource(Authoring{});
    auto& components = app.world().resources().get<Authoring>()->prefabs.components();
    components.register_type<Position>("demo::Position");
    components.register_type<Velocity>("demo::Velocity");
    components.register_type<Label>("demo::Label");
    const auto* trace = app.world().resources().get<Trace>();
    app.add_resource(std::make_shared<Lifetime>(trace ? trace->entries : nullptr));
    if (trace)
        trace->entries->push_back("simulation.installed");
    app.system("demo.move")
        .run([](Position& position, const Velocity& velocity) {
            position.x += velocity.x;
        })
        .build();
    app.system("demo.tick")
        .after("demo.move")
        .run([](elysia::World* world) {
            ++world->resources().get<Progress>()->ticks;
        })
        .build();
    app.set_runner([](elysia::App& running) {
        running.init_serial(); // All startup systems see registrations from ALL installed modules.
        const auto& progress = *running.world().resources().get<Progress>();
        if (!progress.case_loaded)
            throw std::runtime_error("No case loader prepared this application");
        const auto steps = running.world().resources().get<Settings>()->steps;
        for (int step = 0; step < steps; ++step)
            running.update();
        std::cout << "ticks=" << progress.ticks << '\n';
        running.world().query<const Position, const Label>().each(
            [](const auto& position, const auto& label) {
                std::cout << label.name << " x=" << position.x << '\n';
            });
    });
}
const elysia::app::ModuleDescriptor& simulation_module() {
    static constexpr auto descriptor = elysia::app::module<SimulationPlugin>("demo.simulation");
    return descriptor;
}
} // namespace demo
