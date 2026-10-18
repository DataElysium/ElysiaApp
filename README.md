# ElysiaApp — experimental standard host and native SDK

An independent application experiment built on the existing Elysia `App`, plugin API,
startup scheduler and runner. It does not change or replace the ECS or Fleet entry points.

The same application plugin can be linked as a static library or installed from a DLL.
The generic host has no simulation, prefab, case-loader or ImGui code. Application DLLs
register those capabilities themselves. This follows the reusable runner/case-loader
approach in EMT-RS; it is broader than an editor-only extension API.

## Use from Fleet or another application

With ElysiaApp beside the matching ElysiaECS_Hpp and Fleet checkouts:

```sh
cd /home/cts/workspace/ElysiaApp
xmake sdk
cd ../ElysiaFleetNextGen
xmake
```

`sdk` builds the base library and enabled GUI provider, then installs their headers,
libraries and licenses into `dist/sdk`. It uses the configured compiler/backend and six
build jobs. A fresh checkout uses release mode, the sibling ECS and OpenGL defaults.
WGVK configurations install into `dist/wgvk`; headless configurations build the base SDK only.
The command runs xmake's existing build/install tasks in one process, without configuring Fleet.
Use ordinary `xmake f` only when changing a default or selecting optional features.
Players need only the packaged game, not these checkouts or build commands.

## Build in this directory

Project-owned C++ follows the root `.clang-format`. Format it, excluding vendored sources:

```sh
rg --files -0 include src host examples modules tests -g '*.cpp' -g '*.hpp' -g '*.h' | xargs -0 clang-format -i
rg --files -0 include src host examples modules tests -g '*.cpp' -g '*.hpp' -g '*.h' | xargs -0 clang-format --dry-run --Werror
```

```sh
cd /home/cts/workspace/ElysiaApp
xmake f -P . -y -m release --ecs=/home/cts/workspace/ElysiaECS_Hpp
xmake build -P . -y -j6
build/linux/x86_64/release/elysia-app-tests
python3 tools/smoke.py --bin build/linux/x86_64/release
```

This folder has its own `.xmake/` and `build/`. No parent project is included or reconfigured.
Clang is used on Linux and clang-cl with matching MD/MDd dependencies on Windows.
Set `--gui=n` for base SDK/headless builds without the optional GLFW dependency.
The SDK currently follows the ECS checkout's `cxxlatest` build setting. Base dependencies are
nameof, Taskflow and reflect-cpp 0.24.0. The base SDK/executable link no GUI or networking library;
the optional GUI module adds GLFW and its renderer dependencies.
Windows source/build branches exist but have not been executed in this experiment.

## Run the two application compositions

```sh
# A generic executable: simulation AND case loader come from DLLs.
build/linux/x86_64/release/elysia-app \
  --playset build/linux/x86_64/release/sandbox/dynamic.json

# The simulation is linked as a static library; the exact same case-loader DLL is used.
build/linux/x86_64/release/elysia-app-static \
  --playset build/linux/x86_64/release/sandbox/mixed.json
```

Both produce:

```text
ticks=4
Alpha x=8
Beta x=22
```

The simulation plugin registers Position/Velocity/Label, a shared prefab registry, systems
and the runner. The case-loader DLL accesses that registry through the shared public
application contract, reads the packaged prefab case, and spawns its two instances.
The host only knows that it must install modules and call `App::run()`.

Windows PowerShell, after building from this directory:

```powershell
.\build\windows\x64\release\elysia-app.exe --playset .\build\windows\x64\release\sandbox\dynamic.json
.\build\windows\x64\release\elysia-app-static.exe --playset .\build\windows\x64\release\sandbox\mixed.json
```

Plugin-specific arguments are passed after `--`; the host does not parse them:

```sh
build/linux/x86_64/release/elysia-app --playset /path/to/dynamic.json -- --case /path/to/custom.json
```

Without `--case`, the example loads `cases/default.json` relative to its own mod directory.
Playset descriptor paths are relative to the playset, and library paths to the mod folder.
The packaged application can move and run from an unrelated working directory. Explicit
relative `--case` paths belong to the plugin CLI and are relative to the working directory.

## Files and contracts

```text
include/elysia_app/
  module.hpp        DLL query/export, descriptor and install context
  runtime.hpp       Host-owned Runtime and reusable executable entry (run_cli)
  application.hpp   Plugin arguments and infrastructure lifetime resources
  mod_catalog.hpp   Package metadata, discovery and enabled/order lists
  mod_selection.hpp Next-start selection and deferred whole-App restart request
  detail/           Native build fingerprint implementation; not an application-facing feature
src/               Platform library loading, metadata, preflight and lifecycle implementation
host/              Generic executable, one call to run_cli
examples/
  simulation/      Ordinary SimulationPlugin implementation; compiled static and shared
  case_loader/     Independent DLL using the simulation's public model/registry headers
  static_host/     Registers one linked plugin adapter, then uses the generic CLI
  external_host/   A separate build consuming only the installed SDK and matching dependencies
  workbench/       GUI tool DLL using shared ImGui and application resources
  external_gui_tool/ Separate tool build consuming the installed GUI SDK
modules/gui/       Optional GUI provider, public SDK and private renderer
packages/          Source manifests, prefab case and application playsets
build/.../sandbox/ Relocatable application data and DLL packages
```

Choose headers by the work you are doing:

| Task | Include | Types/functions to read |
| --- | --- | --- |
| Export a DLL mod | `elysia_app/module.hpp` | `ELYSIA_APP_EXPORT`, `ModuleDescriptor`, `ModuleContext` |
| Write a host executable | `elysia_app/runtime.hpp` | `Runtime`, `run_cli` |
| Read plugin arguments | `elysia_app/application.hpp` | `Arguments` |
| Retain renderer infrastructure through App teardown | `elysia_app/application.hpp` | `AfterAppLifetime` |
| Browse packages or save enabled/load-order lists | `elysia_app/mod_catalog.hpp` | `Manifest`, `Playset`, `discover`, read/save functions |
| Build a mod-selection/restart UI | `elysia_app/mod_selection.hpp` | `ModSession` |
| Write a GUI tool | `elysia_gui/gui.hpp` | GUI umbrella; see the GUI header map below |

`module.hpp` no longer includes mod-selection/catalog data, CLI arguments or lifetime resources.
`application.hpp` is a set of shared resources, not another App implementation. Ordinary modules
keep using the existing `elysia::App` and `Plugin::build(App&)`. The module query/export and
installation signatures are unchanged. The old `catalog.hpp` and `session.hpp` include names are
replaced by the explicit mod feature names above. Rebuild the SDK and native modules together;
the build fingerprint now uses an explicit SDK contract revision instead of a dependency on
`sizeof(ModSession)`.

A DLL is a distribution form of a normal Elysia plugin:

```cpp
#include <elysia_app/module.hpp>
#include <my_app/plugin.hpp>

ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
    static constexpr auto descriptor = elysia::app::module<MySimulationPlugin>();
    return &descriptor;
}
```

`module<Plugin>()` is evaluated at compile time. It generates the descriptor size, SDK version,
build fingerprint, full type name via `nameof`, and installation callback. There is no per-plugin
registration macro, separate traits class, static registration side effect, or manual install lambda.
The plugin only provides its ordinary `build(App&)`. If it needs the package directory, give it a
constructor taking `const ModuleContext&`; installation passes that context automatically. Copy
borrowed paths/strings into state that survives installation.

The package ID defaults to the manifest's folder name; it can be overridden by `id` for dependency
naming. That alias is independent of the C++ type name. A manifest can override the display `name`;
otherwise a loaded module supplies its inferred full type name. An omitted release stays unknown:
the compiler cannot infer a product version, and the SDK does not fabricate one.

For static use, write `module<Plugin>("package.id")` once to associate the linked implementation
with a package. Keep the descriptor in the static library and register it with
`Runtime::register_static()`. The install function is unchanged. Metadata is copied by
the SDK; callback code must remain available. The simulation example compiles the SAME
`simulation.cpp` into a static library and into a DLL, with a tiny export wrapper for the latter.
A future Fleet wrapper can first retain its existing application/world composition behind one
plugin, rather than splitting every existing algorithm into a DLL at once.

A package looks like:

```text
Mods/case-loader/
  manifest.json
  Assemblies/libelysia-demo-case-loader.so  # .dll on Windows
  cases/default.json
```

Example manifest:

```json
{
  "schema": 1,
  "id": "demo.case_loader",
  "name": "Prefab case loader",
  "version": "0.1.0",
  "dependencies": ["demo.simulation"],
  "libraries": {
    "linux": "Assemblies/libelysia-demo-case-loader.so",
    "windows": "Assemblies/elysia-demo-case-loader.dll"
  }
}
```

Only packaging data needs to be written. For example, `Mods/my_tool/manifest.json` can be:

```json
{
  "dependencies": ["elysia.gui"],
  "libraries": {"linux": "Assemblies/libmy_tool.so", "windows": "Assemblies/my_tool.dll"}
}
```

This package's default ID is `my_tool`. `schema`, `id`, `name`, `version`, `author`, and `description`
need not be repeated to make it load. A registered static package with no dependencies can use `{}`.
Dependencies and the actual platform library filenames remain explicit; neither follows from the
C++ plugin type. The longer manifest above illustrates optional human-facing overrides.

An empty/omitted `libraries` map selects a registered static implementation of that ID.
A nonempty map selects a DLL for the current platform. With generated descriptors, package
identity and release are not repeated in C++. Custom descriptors may explicitly pin an ID or
release; supplied manifest values must then match those constraints. Dependencies must be explicitly
enabled in the playset; the host orders them first. Independent entries keep playset order. No implicit enabling or automatic override.

```json
{
  "schema": 1,
  "mods": [
    {"manifest": "Mods/case-loader/manifest.json", "enabled": true},
    {"manifest": "Mods/simulation/manifest.json", "enabled": true}
  ]
}
```

`discover()` lists manifest metadata without loading native code. Invalid manifests remain
visible with an error. The GUI provider now exposes **Mods → Manage mods...**. It lists
selected entries and discovers additional manifests in `Mods/` beside the playset; discoveries
start disabled. Checkboxes edit the next selection without changing the current App.
**Save selection & restart App** validates metadata/dependencies, atomically replaces the
explicitly displayed playset file, then requests runner exit. Use a writable playset; a failed
save or dependency check leaves the current App running. Installation order still follows the
playset/dependencies; this first menu does not provide ordering or per-mod settings editors.

`run_cli()` destroys the old Runtime completely before constructing the next one, registering
the static adapters again and replaying the original plugin arguments. Running data resets.
No App/context/library is destroyed on the draw callback stack. Libraries and ABI are checked
at next startup; if that startup fails, the host reports the error and exits, without rollback.
This reloads the application composition, not live machine code or an individual module.

## Lifecycle and ownership

1. The caller supplies initial resources/arguments and registers available static descriptors.
2. The host reads enabled manifests and validates dependency order, paths and all descriptors.
3. All DLLs are pinned, then installation callbacks run in dependency order.
4. A plugin's `set_runner` controls execution. The example calls `init_serial()` first, so
   startup case-loading systems run AFTER every plugin has registered its types/resources.
5. After execution or failure, the App (runners, executors, world and resources) is destroyed
   before the libraries are unloaded in reverse dependency order.

Do not call App init/update/run during installation. `ModuleContext` is borrowed during
install only: copy its directory into callbacks/resources that need it later. Registration
and execution happen on the host thread; plugins owning async workers must stop/join them
before their state is destroyed. No in-place module unload, state migration or install rollback is promised.
Whole-App restart happens only after the runner returns and the previous Runtime is destroyed.
A Runtime that failed installation/execution cannot be run or loaded again.

Exceptions can carry DLL vtables, destructors and message storage. Query/install/run calls
therefore translate exceptions into host-owned errors while code is still pinned. The original
exception is destroyed BEFORE unload; custom DLL exception types do not escape the SDK.
This is error propagation, not automatic recovery or rollback of a partially changed world.

The exported C name discovers a native C++ descriptor. This is a same-build SDK, not a universal
C ABI. Use matching SDK/ECS/application contract headers, dependencies, compiler, CRT and ABI
options. `--perf_overlay` and `--sdk_tag` must match; update the distribution tag when changing
public contracts. The ABI fingerprint catches common mismatches; it is not a proof of every
possible schema/layout mismatch. Base SDK APIs contain no editor or ImGui types.

## Install and link the SDK from a separate project

```sh
xmake install -P . -y -o /home/cts/workspace/ElysiaApp/dist/sdk elysia-app-sdk
```

The prefix contains `include/elysia_app/` (including its `detail/` implementation headers) and
`lib/libelysia-app-sdk.a` (`.lib` on Windows).
ECS headers and third-party dependencies remain external, with the same versions/configuration.

Copy `examples/external_host/` to a directory OUTSIDE this checkout, then build in that copy:

```sh
xmake f -P . -y -m release \
  --sdk=/home/cts/workspace/ElysiaApp/dist/sdk \
  --ecs=/home/cts/workspace/ElysiaECS_Hpp
xmake build -P . -y -j6
./build/linux/x86_64/release/external-elysia-host --playset /absolute/path/to/sandbox/dynamic.json
```

That project includes no SDK implementation, demo model or Fleet files. It links the installed
static SDK. To verify it together with a relocated application package:

```sh
python3 tools/smoke.py --bin build/linux/x86_64/release \
  --external /absolute/path/to/external/build/linux/x86_64/release/external-elysia-host
```

## Validation and experiment boundary

`elysia-app-tests` covers static/DLL outcomes, real shared-registry prefab instantiation,
startup ordering, enabled selections, dependencies/cycles/duplicates, ABI and query rejection,
partial installation, system failure and teardown. Custom DLL exceptions are retained as
host-owned errors and inspected AFTER actual DLL unload. It does not hide failures by leaking
library handles. The smoke script relocates the application, changes CWD, checks both
compositions, overrides the case through plugin arguments and verifies clean error exit.

The optional [GUI ecosystem module](modules/gui/README.md) now provides a shared ImGui DLL,
docking/windows/panels, backend-owned images and a separately compiled tool DLL. Its default backend
is OpenGL 3.3; the existing Fleet WGVK backend is also available behind the same interface.
Fleet now consumes the installed SDK through FleetClientPlugin in a local integration.
The graphical playset/restart experiment is available in ElysiaApp; Fleet has not adopted
this menu or the restart loop. Windows validation and production SDK version policy remain subsequent work. ECS module/Hpp repositories are unchanged.
