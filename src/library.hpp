#pragma once
#include <elysia_app/module.hpp>

namespace elysia::app::detail {
class Library {
    void* handle_ = nullptr;
  public:
    explicit Library(const std::filesystem::path&);
    ~Library();
    Library(const Library&) = delete;
    QueryModule query() const;
};
bool valid_id(std::string_view);
const char* platform();
std::filesystem::path package_file(const std::filesystem::path&, const std::string&);
void validate(const ModuleDescriptor&);
} // namespace elysia::app::detail
