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
  module.hpp       Native descriptor, install context, raw application arguments
  catalog.hpp      Manifest, enabled playset, metadata discovery; no ImGui required
  runtime.hpp      Linkable Runtime and reusable run_cli entry
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

A DLL is a distribution form of a normal Elysia plugin:

```cpp
#include <elysia_app/module.hpp>
#include <my_app/plugin.hpp>

ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
    static const elysia::app::ModuleDescriptor descriptor{
        .id="my_app.simulation", .name="My simulation", .release="0.1.0",
        .install=[](elysia::App& app, const elysia::app::ModuleContext& context) {
            app.add_plugin(MySimulationPlugin{});
        }};
    return &descriptor;
}
```

For static use, keep the descriptor in the static library and register it with
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

An empty/omitted `libraries` map selects a registered static implementation of that ID.
A nonempty map selects a DLL for the current platform. The descriptor ID and release must
match the manifest. Dependencies must be explicitly enabled in the playset; the host orders
them first. Independent entries keep playset order. No implicit enabling or automatic override.

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
visible with an error. `read_playset()`/`save_playset()` support a future launcher's checkboxes;
saves replace the old selection only after the new file has been written. This experiment
provides those APIs and `--list DIRECTORY`, not a second graphical launcher.

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
before their state is destroyed. No runtime unload, hot reload or install rollback is promised.
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

The prefix contains `include/elysia_app/*.hpp` and `lib/libelysia-app-sdk.a` (`.lib` on Windows).
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
A graphical playset manager, Windows validation and production SDK version policy remain
subsequent work. ECS module/Hpp repositories are unchanged.
