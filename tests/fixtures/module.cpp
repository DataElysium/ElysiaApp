#include <elysia_app/module.hpp>
#include <demo/model.hpp>
#include <exception>

namespace {
class ModuleError final : public std::exception {
    const char* message_;
  public:
    explicit ModuleError(const char* message) : message_(message) {}
    const char* what() const noexcept override { return message_; }
    ~ModuleError() noexcept override = default;
};
struct Probe {
    std::shared_ptr<std::vector<std::string>> trace;
    explicit Probe(std::shared_ptr<std::vector<std::string>> value) : trace(std::move(value)) {}
    ~Probe() { if (trace) trace->push_back("fixture.destroyed"); }
};
#if FIXTURE_KIND == 1
constexpr const char* id = "test.abi";
#elif FIXTURE_KIND == 2
constexpr const char* id = "test.install";
#elif FIXTURE_KIND == 3
constexpr const char* id = "test.run";
#else
constexpr const char* id = "test.query";
#endif
void install(elysia::App& app, const elysia::app::ModuleContext&) {
    auto* trace = app.world().resources().get<demo::Trace>();
    app.add_resource(std::make_shared<Probe>(trace ? trace->entries : nullptr));
#if FIXTURE_KIND == 2
    throw ModuleError("fixture installation failed");
#elif FIXTURE_KIND == 3
    app.system("fixture.throw").run([](elysia::World*) { throw ModuleError("fixture system failed"); }).build();
#endif
}
}
ELYSIA_APP_EXPORT const elysia::app::ModuleDescriptor* elysia_app_module_v1() {
#if FIXTURE_KIND == 4
    throw ModuleError("fixture query failed");
#else
    static const elysia::app::ModuleDescriptor descriptor{
#if FIXTURE_KIND == 1
        .abi = elysia::app::native_abi() ^ 1,
#endif
        .id=id, .name="Test fixture", .release="0.1.0", .install=install};
    return &descriptor;
#endif
}
