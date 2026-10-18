#pragma once
#include <elysia/prefab/prefab.hpp>
namespace demo {
/** Application extension, deliberately outside the generic SDK. Both DLLs use this same registry. */
struct Authoring { elysia::prefab::PrefabRegistry prefabs{}; };
} // namespace demo
