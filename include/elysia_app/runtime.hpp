#pragma once
#include "module.hpp"
#include <memory>
#include <span>

namespace elysia::app {
struct LoadedModule {
    std::string id;
    std::filesystem::path directory;
    bool dynamic;
};
/** Owns the App and all loaded libraries. App callbacks, executors, entities and resources
 * are destroyed before any library unloads, including after failed installation or execution.
 * Startup is single-threaded. Plugins must stop/join their own workers before destruction.
 * Register static implementations, load one playset, then run once. Rebuilding requires
 * returning from the runner and destroying this entire Runtime first.
 */
class Runtime {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    /** Configure resources/plugins before load; do not init/update the App until loading completes.
     */
    App& app();
    /** Copies descriptor metadata; the install function must remain valid for this Runtime. */
    void register_static(const ModuleDescriptor&);
    /** Preflight all enabled modules/dependencies/ABIs before installing any of them. */
    void load(const std::filesystem::path& playset);
    void run();
    /** Run plugins installed directly through app().add_plugin, without a playset.
     * Seals configuration and uses the same exception/lifetime guards as run().
     * Registered module descriptors are not installed by this path.
     */
    void run_composed();
    std::span<const LoadedModule> modules() const;
};
/** Minimal reusable executable entry. Host flags: --playset FILE, --list DIRECTORY, --help.
 * The host rebuilds the entire Runtime when ModSession requests restart.
 * Plugin arguments follow '--'. Errors return nonzero; no domain-specific case parsing occurs here.
 */
int run_cli(int argc, char** argv, std::span<const ModuleDescriptor* const> builtins = {});
} // namespace elysia::app
