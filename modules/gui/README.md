# Elysia GUI ecosystem module

This optional DLL provides the ImGui implementation, window/docking frame loop, named
panels/windows and backend-owned images. Application tools depend on this module's public
SDK, register ordinary Elysia plugins, and draw with ordinary ImGui calls.

The generic `elysia-app` executable links only the base SDK. It knows no ImGui, GLFW or
renderer types. A playset selects GUI or headless composition; later `set_runner` calls
replace earlier ones, exactly as in the existing App API.

## Public headers

For a normal GUI tool, include **`elysia_gui/gui.hpp`**. It is the SDK umbrella and the checked
`registry(app)` accessor; it does not contain the frame loop or backend implementation.
The feature declarations are organized as follows:

| Header | Purpose | Main declarations |
| --- | --- | --- |
| `gui.hpp` | Normal tool/application entry point; shared-build checks and registry access | `native_abi`, `check_abi`, `registry` |
| `application.hpp` | Configure/install GUI and compose the frame loop | `Context`, `Options`, `Callbacks`, `GuiPlugin`, `run`, `quit`, `capture`, `renderer_name` |
| `windows.hpp` | Register windows and reusable panel contents | `Window`, `Draw`, `Registry` |
| `image.hpp` | Own/upload an RGBA8 image and obtain its ImGui texture reference | `Image` |
| `mods.hpp` | Add the optional mod-manager menu to an application menu bar | `draw_mods_menu` |
| `config.hpp` | Compile/link macros shared with ImGui; not runtime settings | `ELYSIA_GUI_API`, `IMGUI_API` |

`Context` borrows the caller's App. `Registry` holds window descriptions, visibility and drawing
callbacks; business state belongs in App resources. `Callbacks` provides application-level frame
hooks, while window/panel callbacks draw their own contents. `Image` owns a texture, and must die
before its GUI session. Native backend device access is not part of `Image`.

A DLL tool also explicitly includes `elysia_app/module.hpp` for its query/export entry. If it reads
plugin CLI arguments, also include `elysia_app/application.hpp` explicitly. GUI application support
uses that resource header for its lifetime contract; it does not supply the DLL module entry contract.
The `src/render/` headers remain backend implementation details.

## Try the application

From the independent ElysiaApp project root:

```sh
xmake sdk
xmake
build/linux/x86_64/release/elysia-app --playset build/linux/x86_64/release/sandbox/gui.json
```

Windows, after building with clang-cl from this directory:

```powershell
.\build\windows\x64\release\elysia-app.exe --playset .\build\windows\x64\release\sandbox\gui.json
```

**Windows → Scope Settings** opens the second window. Both windows compose the same
registered settings panel and edit the same ECS resource. Gain, frequency, pause and reset
control a 512-sample signal updated by an ECS system. Windows can dock inside the application.
No Fleet component, document, case format or rendering code is used by this tool.

**Mods → Manage mods...** edits the playset and can restart this entire application.
Try disabling Signal Workbench, save/restart, then enable it again. The same executable stays
alive, but the old App/resources, retained GUI context and DLLs are destroyed before the next
App is built. Tool settings reset. Dependency/save errors appear in the window and keep the
current App running; native startup errors after restart exit through the host error handler.
The save button writes the displayed playset, so use a writable copy when experimenting.
Additional modules discovered under the playset's `Mods/` directory start disabled.

Application-owned menu bars can reuse `elysia::gui::draw_mods_menu(context)` inside their
`BeginMainMenuBar`/`EndMainMenuBar`. The standard GUI runner draws the associated editor window
and notices `app::ModSession::restart_requested` at the end of the frame. A custom runner
must likewise return after that request; its outer host must rebuild the Runtime. This menu
is available only for playset-loaded Runtimes. Direct `run_composed()` clients, including Fleet,
retain their existing behavior and do not acquire a misleading restart menu.

To save a layout for a particular application/profile:

```sh
build/linux/x86_64/release/elysia-app --playset build/linux/x86_64/release/sandbox/gui.json \
  -- --gui-layout /path/to/my-layout.ini
```

Lifecycle and frame-statistics logs are off by default. The host option `--diagnostics` enables
them explicitly; for example, put it before `--` in the command above. Test scripts that inspect
teardown logs opt in. A requested `--scope-report FILE` writes its report independently of logging.
Failures still report their errors. Direct hosts can set the `app::Diagnostics{true}` resource
before installing GUI/tool plugins; absent/disabled resources keep diagnostics off.

GUI-specific arguments belong to the GUI module: `--gui-font FILE`, `--gui-hidden`,
`--gui-no-vsync`, and `--gui-frames N`. For a bounded diagnostic run, add
`--gui-capture /path/to/frame.ppm`; capture requires `--gui-frames`.
`--scope-report FILE` belongs exclusively to the example tool and records final UI state
and context lifetime for automated checks. None of these options are understood by the host.

## Layered interfaces

```text
elysia-app-sdk / generic executable
    └─ elysia-gui.dll                manifest ID: elysia.gui
         ├─ one ImGui core + public API exports
         ├─ GUI registry / docking / default GUI runner
         ├─ private renderer implementation
         └─ elysia-workbench.dll     manifest ID: example.workbench
              includes elysia_gui/gui.hpp, links the GUI import/shared library
```

Consumers do not compile or statically link their own ImGui core. ImGui functions, context,
font atlas and allocator state come from the provider DLL. `Registry` uses an opaque
implementation, and the GUI ABI check supplements the base SDK check with ImGui version and
layout information. This remains a same-build SDK: ImGui does not guarantee ABI compatibility
across releases. Use the provider's shipped headers/configuration and matching toolchain/CRT.
Include `<elysia_gui/gui.hpp>` or propagate its `IMGUI_USER_CONFIG` before including ImGui.

Minimal tool installation:

```cpp
#include <elysia_gui/gui.hpp>

void draw(elysia::gui::Context&) {
    ImGui::TextUnformatted("My tool");
}
struct MyToolPlugin {
    void build(elysia::App& app) const {
        auto& windows = elysia::gui::registry(app); // Also verifies the GUI build fingerprint.
        windows.add_window({"my_tool.main", "My tool", draw});
    }
};
```

For a DLL entry point, add this separately from the window code:

```cpp
#include <elysia_app/module.hpp>

ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
    static constexpr auto module = elysia::app::module<MyToolPlugin>();
    return &module;
}
```

The normal module descriptor installs `MyToolPlugin` through `app.add_plugin(...)`.
Its manifest declares `"dependencies": ["elysia.gui"]` and the library locations. Module type
name, install callback and SDK metadata are generated at compile time; no duplicate descriptor
labels/release are required. Shared application data stays in
App resources/entities; named panels are function callbacks that can be embedded in different
windows. The GUI thread owns rendering and mutations to these registries. This introduces
no observer requirement or separate plugin object hierarchy.

A tool can upload RGBA8 pixels through `gui::Image` and pass `image.view()` to `ImGui::Image`.
It never receives an OpenGL/Vulkan object. The workbench's small gradient banner exercises
this path, including destruction of its GPU image while the context is still valid.

## Backend boundary

Only `src/render/opengl/`, `src/render/wgvk/` and upstream backend files include/call graphics APIs.
`src/render/surface.hpp` is private to the GUI provider. The registry, SDK consumers and
application code work with ImGui and backend-owned image objects.

GLFW + OpenGL 3.3 is the default. `--renderer=wgvk --wgvk=/installed/prefix`
selects the ported Fleet WGVK/Vulkan implementation; install it into a separate SDK prefix.
The WGVK static library must be built with PIC on Linux. Both renderer choices implement the
same private backend and public image/frame interfaces. Native Wayland WGVK, device-loss
recovery and detached multi-OS-window viewports remain unsupported. Docking inside the main
window is enabled. Windows code paths still require testing on Windows.

The pinned Dear ImGui v1.93.0-docking source subset and MIT license live in
`thirdparty/imgui/`; original source files are unchanged. GLFW is supplied by xmake.
Runtime platform/driver dependencies of the selected backend still need normal packaging.

## Runtime and teardown

The GUI provider installs a default runner. `gui-simulation.json` first installs the
simulation/headless runner and then GUI: GUI wins and drives App updates.
`headless-last.json` installs the simulation runner last: it wins, and the GUI never
initializes its window/context. A dedicated headless distribution should use a playset
without GUI modules, avoiding their platform library dependencies altogether.

Installation registers resources and callbacks without opening the display. The selected
GUI runner creates the context/backend, runs App startup, then advances systems and renders
registered windows each frame. A later custom runner may call `elysia::gui::run(app)`.

Applications with existing layouts install a `Callbacks` resource. `initialize` runs after
the surface exists but before App startup; `prepare` runs after input polling, before NewFrame;
`draw` replaces the stock menu/dockspace; `submitted` runs after backend submission, before
present (including false for minimized frames). The provider still draws registered tool windows.
Callbacks can request `quit(app)` or call `capture(app,path)` from submitted. No GLFW/GPU types
are passed to the application. `Options` configures title, size, fonts and layout at installation.
Call `check_abi(native_abi())` before passing SDK configuration across a directly linked DLL API.

Fleet now uses this boundary in its single `FleetClientPlugin`; simulation sessions, snapshots,
commands and fixed timesteps remain game-owned. It consumes an installed SDK rather than including
this project's xmake file. `Runtime::run_composed()` supports that direct plugin composition with
the same exception/lifetime protection as manifest-based Runtime::run().

The base SDK's `AfterAppLifetime` resource (`elysia_app/application.hpp`) retains infrastructure without adding GUI types
to the SDK. Destruction order is:

```text
App runners/executors/world/resources (including tool textures)
→ retained GUI session: renderer/window/context teardown
→ DLLs, in reverse dependency order
```

Retained owners must not dereference the destroyed App. Tools stop/join their own workers
before returning from the runner. GUI state belongs on the UI thread; simulation/worker
worlds remain separately owned by application plugins. Startup/runtime failures follow the
same order, and native exceptions are copied into host-owned errors before DLL unload.

## Installed SDK / independently built tool

```sh
xmake sdk
```

The prefix includes the base SDK, `elysia_gui/` headers, matching ImGui headers, the GUI
shared library and Windows import library where applicable. The runtime copy of the GUI
DLL stays in `Mods/gui/Assemblies/`; developer linking uses the SDK prefix.

Copy `examples/external_gui_tool/xmake.lua` and
`examples/workbench/src/workbench.cpp` into a folder OUTSIDE ElysiaApp. Build there:

```sh
xmake f -P . -y -m release --sdk=/home/cts/workspace/ElysiaApp/dist/sdk \
  --ecs=/home/cts/workspace/ElysiaECS_Hpp
xmake build -P . -y -j6
```

That module compiles no GUI provider, renderer, ImGui implementation or base SDK source.
It links `elysia-gui` and exports the normal ElysiaApp module descriptor. To test the separately
built library in a relocated application package:

```sh
python3 tools/gui_smoke.py --bin build/linux/x86_64/release \
  --tool-library /path/to/external/build/linux/x86_64/release/libelysia-workbench.so
```

On Linux, the provider has an explicit SONAME. The host preloads it by manifest dependency
order before the tool's linked dependency is resolved. The test uses no build-tree RPATH or
`LD_LIBRARY_PATH`; copying the application away from the build directory is part of validation.
Windows uses the GUI import library for builds and the preloaded provider DLL at runtime.
The Windows path is implemented but not runtime-tested here.

## Checks

```sh
build/linux/x86_64/release/elysia-gui-tests
build/linux/x86_64/release/elysia-app-tests
python3 tools/gui_smoke.py --bin build/linux/x86_64/release
python3 tools/mod_reload_smoke.py --bin build/linux/x86_64/release # Linux X11/XWayland input test
python3 tools/smoke.py --bin build/linux/x86_64/release
```

Tests cover shared ImGui state/input, GUI ABI rejection, panel composition, duplicate IDs,
real GPU rendering and textures, relocation, both runner orders, failure cleanup, and resource
teardown before context destruction. GUI smoke tests need a desktop/virtual graphics display.
Fleet and Radar Lab now consume this shared GUI SDK in the local integration. Fleet keeps its
existing editor interface/registries behind one client adapter; the scope example remains independent.
