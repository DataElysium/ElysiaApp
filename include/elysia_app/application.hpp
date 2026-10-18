#pragma once
/** @file application.hpp
 * Resources shared between the host and installed plugins.
 * Arguments exposes plugin CLI arguments; AfterAppLifetime retains infrastructure until App
 * teardown. Ordinary plugins keep their own state in App resources; only infrastructure needs the
 * latter.
 */
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace elysia::app {
/** Hold infrastructure until AFTER App destruction and BEFORE DLL unload.
 * The host owns the callbacks/storage. Only resources tied to this Runtime may retain owners.
 * Retained objects are released in reverse registration order. Stop/join workers before returning
 * from run.
 */
struct AfterAppLifetime {
    void* state;
    void (*retain)(void*, std::shared_ptr<void>);
    template <class T> void hold(std::shared_ptr<T> value) const {
        retain(state, std::move(value));
    }
};

/** Arguments after the host's '--' delimiter; interpreted exclusively by application plugins. */
struct Arguments {
    std::vector<std::string> values;
};
} // namespace elysia::app
