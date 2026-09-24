#ifndef MEDIA_KEYMAP_ACTIONS_HPP
#define MEDIA_KEYMAP_ACTIONS_HPP

/**
 * @file Media_KeymapActions.hpp
 * @brief Keymap dispatch helpers for Media Viewer keyboard actions
 */

#include "Core/MediaWidgetStateData.hpp"

#include "Entity/EntityId.hpp"

#include <QString>

#include <functional>
#include <unordered_set>

class GroupManager;
class MediaWidgetState;

/**
 * @brief Runtime context for Media Viewer keymap action dispatch
 */
struct MediaKeymapContext {
    MediaWidgetState * state = nullptr;
    GroupManager * group_manager = nullptr;
    std::unordered_set<EntityId> const * selected_entities = nullptr;
    std::function<void()> refresh_canvas;
    std::function<void()> clear_selection;
};

/**
 * @brief Advance pen append endpoint policy to the next value
 * @param endpoint Current append endpoint
 * @return Next endpoint in Tip → Base → Nearest → Tip cycle
 */
[[nodiscard]] LineAppendEndpoint cycleLineAppendEndpoint(LineAppendEndpoint endpoint);

/**
 * @brief Dispatch a Media Viewer keymap action
 * @param action_id Registered keymap action identifier
 * @param ctx Dispatch context with state and canvas side-effect callbacks
 * @return True when the action was handled and the key event should be consumed
 */
[[nodiscard]] bool handleMediaKeyAction(QString const & action_id, MediaKeymapContext const & ctx);

#endif// MEDIA_KEYMAP_ACTIONS_HPP
