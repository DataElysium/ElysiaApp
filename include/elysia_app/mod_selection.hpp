#pragma once
/** @file mod_selection.hpp
 * Editable next-start mod selection and a deferred whole-App restart request.
 * Used by mod-management interfaces, not by ordinary module entry points.
 */
#include "mod_catalog.hpp"

namespace elysia::app {
/** Host-owned playset editing session, available to any runner after load().
 * Editing selected does not change the running App. save_and_restart validates
 * metadata/dependencies and atomically saves file before setting restart_requested.
 * It throws on failure and leaves the running App intact. The runner must return;
 * only the outer host may destroy this Runtime and build a fresh one.
 * catalog rows correspond to active/selected entries, including disabled discoveries
 * from Mods/ beside the playset. Keep entry order/paths intact when toggling.
 * Calls and edits belong to the runner thread. No state migration or DLL hot swap.
 */
struct ModSession {
    std::filesystem::path file;
    Playset active, selected;
    std::vector<CatalogEntry> catalog;
    bool restart_requested = false;
    void (*save_and_restart)(ModSession&) = nullptr;
};
} // namespace elysia::app
