#include "library.hpp"
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace elysia::app::detail {
Library::Library(const std::filesystem::path& path) {
#ifdef _WIN32
    handle_ = LoadLibraryExW(path.c_str(), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!handle_)
        throw std::runtime_error("Cannot load " + path.string() + ": Windows error " +
                                 std::to_string(GetLastError()));
#else
    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_)
        throw std::runtime_error("Cannot load " + path.string() + ": " + dlerror());
#endif
}
Library::~Library() {
#ifdef _WIN32
    if (handle_)
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
    if (handle_)
        dlclose(handle_);
#endif
}
QueryModule Library::query() const {
#ifdef _WIN32
    return reinterpret_cast<QueryModule>(
        GetProcAddress(static_cast<HMODULE>(handle_), module_symbol));
#else
    return reinterpret_cast<QueryModule>(dlsym(handle_, module_symbol));
#endif
}
bool valid_id(std::string_view id) {
    return !id.empty() && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}
const char* platform() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}
std::filesystem::path package_file(const std::filesystem::path& root, const std::string& file) {
    const std::filesystem::path relative(file);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name())
        throw std::runtime_error("Module library must be a relative package path");
    const auto directory = std::filesystem::weakly_canonical(root);
    const auto path = std::filesystem::weakly_canonical(directory / relative);
    const auto local = path.lexically_relative(directory);
    if (local.empty() || *local.begin() == "..")
        throw std::runtime_error("Module library escapes its package");
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error("Missing module library: " + path.string());
    return path;
}
void validate(const ModuleDescriptor& api) {
    if (api.size != sizeof(ModuleDescriptor) || api.version != sdk_version)
        throw std::runtime_error("Module SDK descriptor/version mismatch");
    if (api.abi != native_abi())
        throw std::runtime_error("Module native ABI mismatch; rebuild with the host SDK/toolchain");
    if ((api.id && !valid_id(api.id)) || (api.name && !*api.name) ||
        (api.release && !*api.release) || !api.install)
        throw std::runtime_error("Invalid native module descriptor");
}
} // namespace elysia::app::detail
