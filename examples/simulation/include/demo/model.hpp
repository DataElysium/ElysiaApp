#pragma once
#include <elysia_app/module.hpp>
#include <memory>

namespace demo {
struct Position { double x = 0; };
struct Velocity { double x = 0; };
struct Label { std::string name; };
struct Settings { int steps = 1; };
struct Progress { int ticks = 0; bool case_loaded = false; };
/** Optional external lifetime probe used by the integration test. */
struct Trace { std::shared_ptr<std::vector<std::string>> entries; };
struct SimulationPlugin { void build(elysia::App&) const; };
const elysia::app::ModuleDescriptor& simulation_module();
} // namespace demo
