-- Copy this folder OUTSIDE the SDK project before configuring it.
set_project("ElysiaAppSdkConsumer")
set_languages("cxxlatest")
add_rules("mode.debug", "mode.release")
option("sdk", {default="/path/to/installed-sdk", description="Installed SDK prefix"})
option("ecs", {default="/path/to/ElysiaECS_Hpp", description="Matching ECS checkout"})
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
end
add_requires("nameof","taskflow",{system=false})
add_requires("reflect-cpp 0.24.0",{system=false,configs={msgpack=true,flatbuffers=true,toml=true}})
add_requireconfs("reflect-cpp.**|cmake|ninja",{override=true,system=false})
local sdk=get_config("sdk") or "/path/to/installed-sdk"
local ecs=get_config("ecs") or "/path/to/ElysiaECS_Hpp"
target("external-elysia-host")
    set_kind("binary")
    add_files("main.cpp")
    add_includedirs(path.join(sdk,"include"),path.join(ecs,"include"),path.join(ecs,"thirdparty/ForkUnion/include"))
    add_linkdirs(path.join(sdk,"lib"))
    add_links("elysia-app-sdk")
    add_packages("nameof","taskflow","reflect-cpp")
    add_defines('ELYSIA_APP_BUILD_TAG="' .. (get_config("sdk_tag") or "experimental-1") .. '"')
    if has_config("perf_overlay") then add_defines("ELYSIA_PERF_OVERLAY") end
    set_pcxxheader(path.join(ecs,"include/elysia/elysia.hpp"))
    if is_plat("linux") then add_syslinks("dl","pthread") end
