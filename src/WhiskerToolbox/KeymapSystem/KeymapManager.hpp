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
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Commands/Core/CommandDescriptor.hpp"

class DataManager;
class EditorRegistry;
class QEvent;

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
 * When installed as a QApplication event filter, it intercepts key events and
 * dispatches them through the scope resolution pipeline. When focus is in a
 * text-input widget (QLineEdit, QTextEdit, etc.), keybindings are not dispatched
 * so normal typing is not interrupted.
 */
class KeymapManager : public QObject {
    Q_OBJECT
public:
    explicit KeymapManager(QObject * parent = nullptr);
    ~KeymapManager() override;

    // --- Event filter (installed on QApplication by MainWindow) ---

    /**
     * @brief Application-wide event filter for keyboard dispatch
     *
     * Intercepts QEvent::KeyPress events and applies scope resolution.
     * If focus is in a text-input widget, matched shortcuts are not dispatched.
     * If no action matches, the event passes through to Qt.
     */
    bool eventFilter(QObject * obj, QEvent * event) override;

    // --- Editor registry integration ---

    /**
     * @brief Set the editor registry used for focus tracking
     * @param registry Non-owning pointer to the EditorRegistry (must outlive KeymapManager)
     */
    void setEditorRegistry(EditorRegistry * registry);

    /**
     * @brief Set application DataManager for command-sequence execution (mutation / navigation)
     * @param dm Shared DataManager from MainWindow (typically); null clears the pointer
     */
    void setDataManager(std::shared_ptr<DataManager> dm);

    // --- Registration (called during widget type setup) ---

    /**
     * @brief Register an action descriptor
     * @param descriptor The action metadata to register
     * @return true if registered successfully, false if action_id already exists
     *
     * @pre descriptor.action_id must not be empty (enforcement: assert)
     * @pre descriptor.scope.editor_type_id must be non-empty when scope.kind is
     *      AlwaysRouted or EditorFocused — an empty type_id is accepted silently
     *      but causes dispatchAction to match the wrong adapter class (enforcement: none)
     * @pre descriptor.action_id must not already be registered; if it is, false is
     *      returned and the caller's descriptor is silently ignored (enforcement: runtime_check)
     * @pre descriptor.display_name should be non-empty for usable keybinding UI (enforcement: none)
     * @pre descriptor.category should be non-empty for correct grouping in the
     *      keybinding editor (enforcement: none)
     */
    bool registerAction(KeyActionDescriptor const & descriptor);

    /**
     * @brief Register a shortcut that executes a CommandSequenceDescriptor (command architecture)
     * @param sequence Serialized sequence; substitutions use @c ctx.runtime_variables (current_frame,
     *        current_time_key) populated from EditorRegistry via KeymapCommandBridge.
     * @return true when registered (same duplicate rules as registerAction)
     *
     * @pre When scope is AlwaysRouted or EditorFocused, @c scope.editor_type_id must be non-empty.
     */
    bool registerCommandAction(QString const & action_id,
                               QString const & display_name,
                               QString const & category,
                               KeyActionScope const & scope,
                               QKeySequence const & default_binding,
                               commands::CommandSequenceDescriptor sequence);

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

    // --- Active target management (for multi-instance AlwaysRouted dispatch) ---

    /**
     * @brief Cycle the active target to the next adapter of the given type
     * @param type_id The editor type to cycle through
     *
     * When multiple adapters of the same type are registered, AlwaysRouted
     * actions dispatch to the "active target" for that type. This method
     * advances the active target to the next adapter in registration order,
     * wrapping around. If no active target is set, selects the first adapter.
     *
     * Emits activeTargetChanged.
     */
    void cycleActiveTarget(EditorLib::EditorTypeId const & type_id);

    /**
     * @brief Set the active target instance for a given editor type
     * @param type_id The editor type
     * @param instance_id The instance to make active (empty to clear)
     *
     * Emits activeTargetChanged.
     */
    void setActiveTarget(EditorLib::EditorTypeId const & type_id,
                         EditorLib::EditorInstanceId const & instance_id);

    /**
     * @brief Get the active target instance for a given editor type
     * @param type_id The editor type to query
     * @return The active instance ID, or invalid if none set
     */
    [[nodiscard]] EditorLib::EditorInstanceId activeTarget(
            EditorLib::EditorTypeId const & type_id) const;

    /**
     * @brief Get all registered adapters for a given editor type
     * @param type_id The editor type to query
     * @return List of (instance_id, adapter) pairs
     */
    [[nodiscard]] std::vector<EditorLib::EditorInstanceId> adapterInstancesForType(
            EditorLib::EditorTypeId const & type_id) const;

    // --- Serialization ---

    /// Export all user overrides for persistence
    [[nodiscard]] std::vector<KeymapOverrideEntry> exportOverrides() const;

    /// Import user overrides (e.g. from AppPreferencesData on startup)
    void importOverrides(std::vector<KeymapOverrideEntry> const & overrides);

signals:
    /// Emitted when any binding changes (override set/cleared, action registered/removed)
    void bindingsChanged();

    /// Emitted when the active target for a type changes (cycling or explicit set)
    void activeTargetChanged(EditorLib::EditorTypeId type_id,
                             EditorLib::EditorInstanceId instance_id);

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

    /// Per-type active target for AlwaysRouted dispatch with multiple instances
    std::map<EditorLib::EditorTypeId, EditorLib::EditorInstanceId> _active_targets;

    /// Non-owning pointer to the editor registry for focus tracking
    EditorRegistry * _editor_registry = nullptr;

    /// Application DataManager — set by MainWindow for executeSequence command actions
    std::shared_ptr<DataManager> _data_manager;

    /// Check if the currently focused widget is a text-input widget
    [[nodiscard]] static bool _isTextInputWidget(QWidget * widget);

    /// Remove an adapter from the routing table (called on QObject::destroyed)
    void _removeAdapter(QObject * adapter);
};

}// namespace KeymapSystem

#endif// KEYMAP_SYSTEM_KEYMAP_MANAGER_HPP
