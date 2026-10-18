#include <demo/model.hpp>
ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
    return &demo::simulation_module();
}
