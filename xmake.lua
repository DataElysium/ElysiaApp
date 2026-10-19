set_project("ElysiaAppExperimental")
set_version("0.1.0")
set_languages("cxxlatest")
add_rules("mode.debug", "mode.release")
option("renderer", {default="opengl",values={"opengl","wgvk"},description="GUI renderer backend"})
option("wgvk", {default="../ElysiaFleetNextGen/build/deps/wgvk-install",description="Installed WGVK prefix"})
option("gui", {default=true, description="Build the optional ImGui provider and GUI tool modules"})
option("ecs", {default = "../ElysiaECS_Hpp", description = "Matching ElysiaECS header checkout"})
option("perf_overlay", {default = true, description = "Match host ELYSIA_PERF_OVERLAY"})
option("sdk_tag", {default = "experimental-1", description = "Shared SDK/header distribution fingerprint tag"})
if is_plat("windows") then
    set_toolchains("clang-cl")
    local runtime = is_mode("debug") and "MDd" or "MD"
    set_runtimes(runtime)
    add_requireconfs("**|cmake|ninja", {override=true, configs={toolchains="clang-cl", runtimes=runtime}})
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
else
    set_toolchains("clang")
    add_cxxflags("-fvisibility=hidden", "-fvisibility-inlines-hidden")
end
if has_config("gui") then add_requires("glfw 3.4", {system=false}) end
add_requires("nameof", "taskflow", "yaml-cpp", {system=false})
add_requires("reflect-cpp 0.24.0", {system=false, configs={msgpack=true, flatbuffers=true, toml=true}})
add_requireconfs("reflect-cpp.**|cmake|ninja", {override=true, system=false})
local ecs = path.absolute((get_config("ecs") or "../ElysiaECS_Hpp"))
add_includedirs("include", path.join(ecs,"include"), path.join(ecs,"prefab/include"), path.join(ecs,"thirdparty/ForkUnion/include"))
add_packages("nameof", "taskflow", "reflect-cpp")
add_defines('ELYSIA_APP_BUILD_TAG="' .. (get_config("sdk_tag") or "experimental-1") .. '"')
if has_config("perf_overlay") then add_defines("ELYSIA_PERF_OVERLAY") end
set_pcxxheader(path.join(ecs, "include/elysia/elysia.hpp"))

-- The SDK is a normal linkable static library; public headers contain no ImGui or domain types.
target("elysia-app-sdk")
    set_kind("static")
    add_files("src/*.cpp")
    add_headerfiles("include/(elysia_app/**.hpp)")
    add_includedirs("include", {public=true})
    if is_plat("linux") then add_syslinks("dl", "pthread", {public=true}) end

target("elysia-app")
    set_kind("binary")
    add_deps("elysia-app-sdk")
    add_files("host/*.cpp")

-- Existing static code can be wrapped by ONE plugin, and independently exposed as a DLL.
target("demo-simulation-static")
    set_kind("static")
    add_includedirs("examples/simulation/include", {public=true})
    add_files("examples/simulation/src/simulation.cpp")

-- Common package data is staged once; parallel DLL links copy only their own binaries.
target("elysia-app-sandbox")
    set_kind("phony")
    on_build(function (target)
        local output = path.join(target:targetdir(), "sandbox")
        os.mkdir(path.join(output, "Mods"))
        os.cp("packages/*.json", output)
        for _, folder in ipairs(os.dirs("packages/*")) do
            os.cp(folder, path.join(output,"Mods") .. path.sep())
        end
    end)

local function stage_mod(folder)
    return function (target)
        local assemblies = path.join(target:targetdir(), "sandbox/Mods", folder, "Assemblies")
        os.mkdir(assemblies)
        os.cp(target:targetfile(), assemblies)
    end
end

target("elysia-demo-simulation")
    set_kind("shared")
    add_includedirs("examples/simulation/include")
    add_files("examples/simulation/src/*.cpp")
    add_deps("elysia-app-sandbox")
    after_build(stage_mod("simulation"))

target("elysia-demo-case-loader")
    set_kind("shared")
    add_includedirs("examples/simulation/include")
    add_files("examples/case_loader/*.cpp")
    add_deps("elysia-app-sandbox")
    after_build(stage_mod("case-loader"))

target("elysia-app-static")
    set_kind("binary")
    add_deps("elysia-app-sdk", "demo-simulation-static")
    add_files("examples/static_host/*.cpp")

local fixtures = {abi=1, install=2, run=3, query=4}
for name, kind in pairs(fixtures) do
    target("elysia-test-" .. name)
        set_kind("shared")
        add_files("tests/fixtures/*.cpp")
        add_includedirs("examples/simulation/include")
        add_defines("FIXTURE_KIND=" .. kind)
end

target("elysia-app-tests")
    set_kind("binary")
    add_deps("elysia-app-sdk", "elysia-demo-simulation", "elysia-demo-case-loader", "demo-simulation-static")
    for name, _ in pairs(fixtures) do add_deps("elysia-test-" .. name) end
    add_files("tests/*.cpp")
    add_includedirs("examples/simulation/include")
    set_rundir("$(projectdir)")

-- GUI is an optional ecosystem module. The SDK and generic executable never link it.
if has_config("gui") then
    target("elysia-gui")
        set_kind("shared")
        add_deps("elysia-app-sandbox")
        add_includedirs("modules/gui/include", "thirdparty/imgui", {public=true})
        add_includedirs("thirdparty/imgui/backends")
        add_defines("ELYSIA_GUI_BUILD")
        add_defines('IMGUI_USER_CONFIG="elysia_gui/config.hpp"', {public=true})
        add_files("modules/gui/src/*.cpp", "modules/gui/src/render/*.cpp", "thirdparty/imgui/*.cpp")
        add_files("thirdparty/imgui/backends/imgui_impl_glfw.cpp")
        if get_config("renderer")=="wgvk" then
            local prefix=path.absolute(get_config("wgvk"))
            add_files("modules/gui/src/render/wgvk/*.cpp", "thirdparty/imgui/backends/imgui_impl_wgpu.cpp")
            add_includedirs(path.join(prefix,"include"))
            add_linkdirs(path.join(prefix,"lib"))
            add_links("wgvk")
            add_defines("IMGUI_IMPL_WEBGPU_BACKEND_WGVK")
        else
            add_files("modules/gui/src/render/opengl/*.cpp", "thirdparty/imgui/backends/imgui_impl_opengl3.cpp")
        end
        add_packages("glfw")
        add_headerfiles("modules/gui/include/(elysia_gui/*.hpp)")
        add_headerfiles("thirdparty/imgui/(*.h)")
        add_installfiles("thirdparty/imgui/LICENSE.txt", "thirdparty/imgui/VERSION.txt", {prefixdir="share/licenses/elysia-gui"})
        if is_plat("linux") then
            add_syslinks("X11", "dl", "pthread")
            if get_config("renderer")=="wgvk" then add_syslinks("wayland-client", "m")
            else add_syslinks("GL") end
            add_shflags("-Wl,-soname,libelysia-gui.so", "-Wl,--as-needed", "-Wl,--exclude-libs,ALL")
        end
        if is_plat("windows") and get_config("renderer")~="wgvk" then add_syslinks("opengl32") end
        after_build(stage_mod("gui"))

    target("elysia-workbench")
        set_kind("shared")
        add_deps("elysia-gui")
        add_files("examples/workbench/src/*.cpp")
        after_build(stage_mod("workbench"))

    target("elysia-gui-tests")
        set_kind("binary")
        add_deps("elysia-app-sdk", "elysia-gui", "elysia-text")
        add_files("modules/gui/tests/*.cpp")

    target("elysia-text-example")
        set_default(false)
        set_kind("binary")
        add_deps("elysia-app-sdk", "elysia-gui", "elysia-text")
        add_files("examples/localization/main.cpp")
        set_rundir("$(projectdir)")
        set_runargs(path.absolute("examples/localization/locales"))
        if is_plat("linux") then add_rpathdirs("$ORIGIN") end
end

-- Localization is independent of GUI/rendering; YAML stays private to this provider.
target("elysia-text")
    set_kind("shared")
    add_defines("ELYSIA_TEXT_BUILD")
    add_includedirs("modules/text/include", {public=true})
    add_files("modules/text/src/*.cpp")
    add_packages("yaml-cpp")
    add_headerfiles("modules/text/include/(elysia_text/*.hpp)")
    add_installfiles("modules/text/LICENSE-yaml-cpp.txt", {prefixdir="share/licenses/elysia-text"})
    if is_plat("linux") then
        add_shflags("-Wl,-soname,libelysia-text.so", "-Wl,--exclude-libs,ALL")
    end

target("elysia-text-tests")
    set_default(false)
    set_kind("binary")
    add_deps("elysia-text")
    add_files("modules/text/tests/*.cpp")
    if is_plat("linux") then add_rpathdirs("$ORIGIN") end

-- Normal consumer workflow: build and install the configured SDK without repeating defaults.
task("sdk")
    set_category("plugin")
    set_menu {usage="xmake sdk", description="Build and install the SDK into dist/sdk (dist/wgvk for WGVK).", options={}}
    on_run(function ()
        import("core.base.task")
        import("core.project.config")
        import("core.project.project")
        task.run("config", {yes=true}, {disable_dump=true})
        local targets={"elysia-app-sdk", "elysia-text"}
        if project.target("elysia-gui") then table.insert(targets,"elysia-gui") end
        task.run("build", {targets=targets, jobs=6, yes=true})
        local folder=config.get("renderer")=="wgvk" and "wgvk" or "sdk"
        local prefix=path.join(os.projectdir(),"dist",folder)
        -- Install the declared header tree without leaving headers removed by SDK reorganizations.
        os.rm(path.join(prefix,"include/elysia_app"))
        os.rm(path.join(prefix,"include/elysia_gui"))
        os.rm(path.join(prefix,"include/elysia_text"))
        task.run("install", {targets=targets, installdir=prefix, yes=true})
        cprint("${green}SDK ready: ${clear}%s",prefix)
    end)
task_end()
