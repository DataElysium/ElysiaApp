-- Copy this folder and the workbench source outside ElysiaApp to test the layered SDK.
set_project("ExternalElysiaGuiTool")
set_languages("cxxlatest")
add_rules("mode.debug", "mode.release")
option("sdk", {default="/path/to/installed-sdk", description="SDK prefix containing elysia_app, elysia_gui and the GUI import/shared library"})
option("ecs", {default="/path/to/ElysiaECS_Hpp"})
option("perf_overlay", {default=true})
option("sdk_tag", {default="experimental-1"})
if is_plat("windows") then
    set_toolchains("clang-cl")
    local runtime=is_mode("debug") and "MDd" or "MD"
    set_runtimes(runtime)
    add_requireconfs("**|cmake|ninja", {override=true, configs={toolchains="clang-cl",runtimes=runtime}})
    add_defines("NOMINMAX","WIN32_LEAN_AND_MEAN")
else
    set_toolchains("clang")
    add_cxxflags("-fvisibility=hidden","-fvisibility-inlines-hidden")
end
add_requires("nameof","taskflow",{system=false})
local sdk=get_config("sdk") or "/path/to/installed-sdk"
local ecs=get_config("ecs") or "/path/to/ElysiaECS_Hpp"
target("elysia-workbench")
    set_kind("shared")
    add_files("workbench.cpp")
    add_includedirs(path.join(sdk,"include"),path.join(ecs,"include"),path.join(ecs,"thirdparty/ForkUnion/include"))
    add_linkdirs(path.join(sdk,"lib"))
    add_links("elysia-gui")
    add_packages("nameof","taskflow")
    add_defines('IMGUI_USER_CONFIG="elysia_gui/config.hpp"')
    add_defines('ELYSIA_APP_BUILD_TAG="' .. (get_config("sdk_tag") or "experimental-1") .. '"')
    if has_config("perf_overlay") then add_defines("ELYSIA_PERF_OVERLAY") end
    set_pcxxheader(path.join(ecs,"include/elysia/elysia.hpp"))
