#include <elysia_app/runtime.hpp>
#include <demo/model.hpp>
int main(int argc, char** argv) {
    const elysia::app::ModuleDescriptor* builtins[]{&demo::simulation_module()};
    return elysia::app::run_cli(argc, argv, builtins);
}
