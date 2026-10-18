# ElysiaApp — isolated native application experiment

- This is an independent xmake project. Configure from THIS directory using `xmake f -P .`;
  do not include it in ElysiaECS_Module, ElysiaECS_Hpp or Fleet's build.
- Public SDK: `include/elysia_app/`. Linkable implementation: `src/`. Generic executable: `host/`.
- Keep `Plugin::build(elysia::App&)`, the existing startup scheduler and `App::set_runner`.
  The host must not know game/simulation case formats, prefab component types, GUI or rendering.
- Optional GUI ecosystem: `modules/gui/` owns the shared ImGui implementation and private renderer;
  tools include its public SDK and link its DLL. Never link a second ImGui core into a tool.
- Examples may share their public `demo/` contracts. The SDK must not include them.
- Native plugins require matching SDK/ECS/dependencies/toolchain/CRT/ABI options. The C query symbol
  discovers a native C++ descriptor; it does not establish a universal C ABI.
- Startup-only loading. Preflight all descriptors before any installation. App must be destroyed
  before libraries, including failure paths. Copy DLL exceptions into host-owned errors while pinned.
- One xmake process at a time, `-y -j6`. Keep the ECS umbrella PCH. Do not modify the ECS/Fleet
  repositories merely to make this experiment compile; report an actual upstream dependency bug first.
- Checks: `elysia-app-tests`, `python3 tools/smoke.py --bin build/<platform>/<arch>/<mode>`.
  Public header changes also need standalone header compilation. SDK changes need the installed-SDK
  external consumer check described in README. GUI changes also require `elysia-gui-tests` and
  `tools/gui_smoke.py` with a desktop display, plus the separately built GUI-tool SDK consumer.
  Retained infrastructure uses AfterAppLifetime so contexts outlive App resource destructors.
- Git identity, if commits are requested: Klein <klein@elysia.dev>.
- Format project-owned C++ with the root `.clang-format`; preserve include order and never
  reformat vendored `thirdparty/` sources. Run clang-format's dry-run check before finishing C++ edits.
