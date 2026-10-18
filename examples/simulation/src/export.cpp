#include <demo/model.hpp>
ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
    static constexpr auto descriptor =
        elysia::app::module<demo::SimulationPlugin>(demo::simulation_metadata);
    return &descriptor;
}
