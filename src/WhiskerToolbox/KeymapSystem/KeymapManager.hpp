/**
 * @file KeymapManager.hpp
 * @brief Central manager for keyboard action registration, dispatch, and override management
 *
 * KeymapManager is the core of the Keymap System. It owns the action registry,
 * handles event filtering, resolves scope priority, manages user overrides,
 * and detects binding conflicts.
 *
 * Owned by MainWindow and installed as a QApplication event filter.
 *
 * @see KeyAction.hpp for action descriptors and scopes
 * @see KeyActionAdapter for widget-side dispatch handling
 * @see Keymap.hpp for binding and override types
 */

#ifndef KEYMAP_SYSTEM_KEYMAP_MANAGER_HPP
#define KEYMAP_SYSTEM_KEYMAP_MANAGER_HPP

#include <QKeySequence>
#include <QObject>
#include <QString>

#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/Keymap.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace KeymapSystem {

class KeyActionAdapter;

/**
 * @brief A detected conflict between two actions sharing the same key in the same scope
 */
struct KeyConflict {
    QString action_id_a; ///< First conflicting action
    QString action_id_b; ///< Second conflicting action
    QKeySequence key;    ///< The shared key sequence
    KeyActionScope scope;///< The scope in which the conflict occurs
};

/**
 * @brief Central manager for keyboard shortcut registration, resolution, and dispatch
 *
 * Responsibilities:
 * - Action registry: stores all KeyActionDescriptors
 * - Override management: merges defaults with user customizations
 * - Conflict detection: reports same key bound to multiple actions in the same scope
 * - Scope resolution: resolves key presses using priority order
 *   (EditorFocused > AlwaysRouted > Global)
 * - Adapter routing: dispatches actions to registered KeyActionAdapters
 *
 * This class does NOT install itself as an event filter — that is done in Step 1.3.
 * For Step 1.1, it provides the registration and resolution APIs only.
 */
class KeymapManager : public QObject {
    Q_OBJECT
public:
    explicit KeymapManager(QObject * parent = nullptr);
    ~KeymapManager() override;

    // --- Registration (called during widget type setup) ---

    /**
     * @brief Register an action descriptor
     * @param descriptor The action metadata to register
     * @return true if registered successfully, false if action_id already exists
     * @pre descriptor.action_id must not be empty
     */
    bool registerAction(KeyActionDescriptor const & descriptor);

    /**
     * @brief Unregister an action by ID
     * @param action_id The action to remove
     * @return true if the action was found and removed
     */
    bool unregisterAction(QString const & action_id);

    // --- Query ---

    /// Get all registered action descriptors
    [[nodiscard]] std::vector<KeyActionDescriptor> allActions() const;

    /// Get a specific action descriptor by ID
    [[nodiscard]] std::optional<KeyActionDescriptor> action(QString const & action_id) const;

    /// Get the effective binding for an action (override if set, otherwise default)
    [[nodiscard]] QKeySequence bindingFor(QString const & action_id) const;

    /// Check whether an action is enabled
    [[nodiscard]] bool isActionEnabled(QString const & action_id) const;

    // --- Override management ---

    /**
     * @brief Set a user override for an action's key binding
     * @param action_id The action to override
     * @param key The new key sequence (empty to unbind)
     */
    void setUserOverride(QString const & action_id, QKeySequence const & key);

    /**
     * @brief Clear a user override, reverting to the default binding
     * @param action_id The action to reset
     */
    void clearUserOverride(QString const & action_id);

    /// Clear all user overrides
    void clearAllOverrides();

    /**
     * @brief Set whether an action is enabled
     * @param action_id The action to enable/disable
     * @param enabled Whether the action should be active
     */
    void setActionEnabled(QString const & action_id, bool enabled);

    // --- Conflict detection ---

    /// Detect all binding conflicts (same key + same scope)
    [[nodiscard]] std::vector<KeyConflict> detectConflicts() const;

    // --- Scope resolution ---

    /**
     * @brief Resolve a key press to an action ID given the current context
     * @param key The key sequence pressed
     * @param focused_type_id The editor type that currently has focus (may be empty)
     * @return The action_id to dispatch, or std::nullopt if no match
     *
     * Resolution order:
     * 1. EditorFocused actions for the focused editor type
     * 2. AlwaysRouted actions
     * 3. Global actions
     */
    [[nodiscard]] std::optional<QString> resolveKeyPress(
            QKeySequence const & key,
            EditorLib::EditorTypeId const & focused_type_id) const;

    // --- Adapter management ---

    /**
     * @brief Register a KeyActionAdapter for dispatch routing
     *
     * The adapter's typeId() is used to route actions to the correct widget.
     * Deregistration is automatic via QObject::destroyed.
     */
    void registerAdapter(KeyActionAdapter * adapter);

    /**
     * @brief Dispatch an action to the appropriate adapter
     * @param action_id The action to dispatch
     * @param target_type_id The editor type to route to
     * @param target_instance_id Optional specific instance (empty = first matching type)
     * @return true if an adapter handled the action
     */
    bool dispatchAction(
            QString const & action_id,
            EditorLib::EditorTypeId const & target_type_id,
            EditorLib::EditorInstanceId const & target_instance_id = {}) const;

    // --- Serialization ---

    /// Export all user overrides for persistence
    [[nodiscard]] std::vector<KeymapOverrideEntry> exportOverrides() const;

    /// Import user overrides (e.g. from AppPreferencesData on startup)
    void importOverrides(std::vector<KeymapOverrideEntry> const & overrides);

signals:
    /// Emitted when any binding changes (override set/cleared, action registered/removed)
    void bindingsChanged();

private:
    struct OverrideState {
        QKeySequence key_sequence;
        bool enabled = true;
    };

    /// Registered action descriptors, keyed by action_id
    std::map<QString, KeyActionDescriptor> _actions;

    /// User overrides, keyed by action_id
    std::map<QString, OverrideState> _overrides;

    /// Registered adapters (non-owning; cleaned up via QObject::destroyed)
    std::vector<KeyActionAdapter *> _adapters;

    /// Remove an adapter from the routing table (called on QObject::destroyed)
    void _removeAdapter(QObject * adapter);
};

}// namespace KeymapSystem

#endif// KEYMAP_SYSTEM_KEYMAP_MANAGER_HPP
