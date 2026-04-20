/**
 * @file KeyAction.hpp
 * @brief Core types for the Keymap System: action scopes and descriptors
 *
 * Defines how keyboard actions are categorized by scope (global, editor-focused,
 * always-routed) and how action metadata is represented.
 *
 * @see KeymapManager for the central dispatch and override manager
 * @see KeyActionAdapter for the composition-based widget handler
 */

#ifndef KEYMAP_SYSTEM_KEY_ACTION_HPP
#define KEYMAP_SYSTEM_KEY_ACTION_HPP

#include <QKeySequence>
#include <QString>

#include "Commands/Core/CommandDescriptor.hpp"
#include "EditorState/StrongTypes.hpp"

#include <optional>

namespace KeymapSystem {

/**
 * @brief The kind of scope that determines when an action is eligible for dispatch
 */
enum class KeyActionScopeKind {
    Global,       ///< Always available, no target widget
    EditorFocused,///< Only when a specific editor type has keyboard focus
    AlwaysRouted  ///< Always available; dispatched to a specific editor type regardless of focus
};

/**
 * @brief Scope for a key action, determining when and where it can be dispatched
 *
 * Combines a scope kind with an optional editor type identifier.
 * - Global: no editor_type_id needed
 * - EditorFocused: action only fires when this editor type has focus
 * - AlwaysRouted: action always fires, dispatched to this editor type
 */
struct KeyActionScope {
    KeyActionScopeKind kind = KeyActionScopeKind::Global;
    EditorLib::EditorTypeId editor_type_id;

    /// Create a Global scope (no target widget)
    static KeyActionScope global() {
        return {KeyActionScopeKind::Global, {}};
    }

    /// Create an EditorFocused scope for the given editor type
    static KeyActionScope editorFocused(EditorLib::EditorTypeId const & type_id) {
        return {KeyActionScopeKind::EditorFocused, type_id};
    }

    /// Create an AlwaysRouted scope for the given editor type
    static KeyActionScope alwaysRouted(EditorLib::EditorTypeId const & type_id) {
        return {KeyActionScopeKind::AlwaysRouted, type_id};
    }

    bool operator==(KeyActionScope const & other) const = default;
};

/**
 * @brief Metadata describing a bindable keyboard action
 *
 * Pure data — no callback stored. Registered with KeymapManager during
 * widget type setup. The actual handler is installed via KeyActionAdapter
 * at widget construction time.
 */
struct KeyActionDescriptor {
    QString action_id;           ///< Unique, dot-namespaced ID (e.g. "media.assign_group_1")
    QString display_name;        ///< Human-readable name (e.g. "Assign to Group 1")
    QString category;            ///< Grouping for the keybinding editor (e.g. "Media Viewer")
    KeyActionScope scope;        ///< When this action is eligible for dispatch
    QKeySequence default_binding;///< Shipped default key (may be empty = unbound)
    /// When set, key dispatch runs @c commands::executeSequence() instead of adapters
    std::optional<commands::CommandSequenceDescriptor> command_sequence;
};

}// namespace KeymapSystem

#endif// KEYMAP_SYSTEM_KEY_ACTION_HPP
