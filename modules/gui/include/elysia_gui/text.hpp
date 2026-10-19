#pragma once
/** @file text.hpp
 * Optional localized ImGui text functions. Include explicitly and link elysia-text.
 * The caller supplies the catalog; these functions own no language or global state.
 * Use Catalog::label() with normal ImGui controls for stable widget IDs.
 */
#include "config.hpp"
#include <elysia_text/catalog.hpp>
#include <imgui.h>

namespace elysia::gui {
/// Draw a translated string literally, including any percent signs in its value.
inline void text(const elysia::text::Catalog& catalog, elysia::text::Message message) {
    ImGui::TextUnformatted(catalog.get(message));
}

/// Draw a translated string with ImGui's current text wrapping settings.
inline void text_wrapped(const elysia::text::Catalog& catalog, elysia::text::Message message) {
    ImGui::TextWrapped("%s", catalog.get(message));
}
} // namespace elysia::gui
