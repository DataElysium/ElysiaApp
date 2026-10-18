#include <elysia_app/runtime.hpp>
#include <elysia_app/catalog.hpp>
#include <iostream>
#include <stdexcept>

namespace elysia::app {
int run_cli(int argc, char** argv, std::span<const ModuleDescriptor* const> builtins) {
    try {
        std::filesystem::path playset, listing;
        Arguments arguments;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--") {
                while (++i < argc) arguments.values.emplace_back(argv[i]);
                break;
            }
            if (option == "--help") {
                std::cout << "Usage: elysia-app --playset FILE [-- PLUGIN_ARGUMENTS...]\n"
                             "       elysia-app --list MOD_DIRECTORY\n";
                return 0;
            }
            if ((option == "--playset" || option == "--list") && i + 1 < argc) {
                auto& target = option == "--playset" ? playset : listing;
                target = argv[++i];
            } else throw std::invalid_argument("Unknown or incomplete host argument: " + std::string(option));
        }
        if (!listing.empty()) {
            for (const auto& entry : discover(listing)) {
                if (entry.error.empty()) std::cout << entry.manifest.id << " " << entry.manifest.version << " " << entry.manifest.name << '\n';
                else std::cout << entry.path.string() << ": " << entry.error << '\n';
            }
            return 0;
        }
        if (playset.empty()) throw std::invalid_argument("Supply --playset FILE; plugin arguments follow '--'");
        playset = std::filesystem::absolute(playset).lexically_normal();
        bool restart;
        do {
            // A distinct scope is essential: App, retained services and DLLs all die
            // before constructing the next Runtime. Never reload from inside a callback.
            Runtime runtime;
            runtime.app().add_resource(arguments);
            for (const auto* descriptor : builtins) {
                if (!descriptor) throw std::invalid_argument("Null static descriptor");
                runtime.register_static(*descriptor);
            }
            runtime.load(playset);
            runtime.run();
            restart = runtime.app().world().resources().get<ModSession>()->restart_requested;
        } while (restart);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ElysiaApp: " << error.what() << '\n';
        return 1;
    }
}
} // namespace elysia::app
