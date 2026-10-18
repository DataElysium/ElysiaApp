#pragma once
#include <elysia_app/runtime.hpp>
#include <elysia_app/catalog.hpp>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace test {
inline void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> std::string rejects(F&& action, std::string_view expected) {
    try {
        action();
    } catch (const std::exception& error) {
        const std::string message = error.what();
        if (message.find(expected) == std::string::npos)
            throw std::runtime_error("Unexpected error: " + message + ", expected " +
                                     std::string(expected));
        return message;
    }
    throw std::runtime_error("Expected rejection: " + std::string(expected));
}
struct Temporary {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("elysia-app-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temporary() {
        std::filesystem::create_directories(path);
    }
    ~Temporary() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
inline const char* platform() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}
inline std::string library_name(std::string_view name) {
#ifdef _WIN32
    return std::string(name) + ".dll";
#elif defined(__APPLE__)
    return "lib" + std::string(name) + ".dylib";
#else
    return "lib" + std::string(name) + ".so";
#endif
}
} // namespace test
