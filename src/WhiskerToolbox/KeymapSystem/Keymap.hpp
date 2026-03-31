/**
 * @file Keymap.hpp
 * @brief Keymap binding types: resolved entries and serializable user overrides
 *
 * Defines the runtime binding representation and the persistable override format.
 *
 * @see KeymapManager for override management and conflict detection
 * @see KeyAction.hpp for action descriptors
 */

#ifndef KEYMAP_SYSTEM_KEYMAP_HPP
#define KEYMAP_SYSTEM_KEYMAP_HPP

#include <QKeySequence>
#include <QString>

#include <string>

namespace KeymapSystem {

/**
 * @brief A resolved binding mapping an action to a key sequence
 *
 * This is the runtime representation used by KeymapManager for dispatch.
 */
struct KeymapEntry {
    QString action_id;        ///< The action this binding belongs to
    QKeySequence key_sequence;///< The currently active key sequence
    bool enabled = true;      ///< Whether this binding is active
    bool is_override = false; ///< True when user has customized this binding
};

/**
 * @brief Serializable user override for a keybinding
 *
 * Stored in AppPreferencesData for persistence across sessions.
 * Uses std::string for reflect-cpp compatibility (no QString).
 */
struct KeymapOverrideEntry {
    std::string action_id;   ///< The action ID being overridden
    std::string key_sequence;///< The key sequence as a portable string
    bool enabled = true;     ///< Whether the override is active
};

}// namespace KeymapSystem

#endif// KEYMAP_SYSTEM_KEYMAP_HPP
