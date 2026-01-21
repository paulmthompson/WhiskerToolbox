#ifndef EDITOR_REGISTRY_HPP
#define EDITOR_REGISTRY_HPP

/**
 * @file EditorRegistry.hpp
 * @brief Central registry for editor types, instances, and selection
 *
 * EditorRegistry consolidates what was previously split across WorkspaceManager
 * and EditorFactory into a single coherent class:
 *
 * - **Type Registration**: Metadata + factory functions for each editor type
 * - **State Registry**: Active EditorState instances
 * - **Selection Context**: Inter-widget communication
 * - **Serialization**: Save/restore workspace state
 *
 * ## Design Philosophy
 *
 * Factory functions are stored as part of type metadata, not in a separate
 * factory class. This keeps type registration cohesive: "Here's what a
 * MediaWidget is, and here's how to create one."
 *
 * ## Usage Example
 *
 * ```cpp
 * EditorRegistry registry(data_manager);
 *
 * registry.registerType({
 *     .type_id = "MediaWidget",
 *     .display_name = "Media Viewer",
 *     .default_zone = "main",
 *     .allow_multiple = true,
 *     .create_state = []() { return std::make_shared<MediaWidgetState>(); },
 *     .create_view = [](auto s) { return new MediaWidgetView(s); },
 *     .create_properties = [](auto s) { return new MediaWidgetProperties(s); }
 * });
 *
 * auto [state, view, props] = registry.createEditor("MediaWidget");
 * // state is auto-registered, view and props are ready to use
 * ```
 *
 * @see EditorState for the base state class
 * @see SelectionContext for inter-widget selection
 */

#include <QObject>
#include <QString>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class EditorState;
class SelectionContext;

/**
 * @brief Central registry for editor types, instances, and selection
 *
 * Single source of truth for:
 * - What editor types exist (type_id → metadata + factories)
 * - What editor instances are active (instance_id → state)
 * - Current selection state (via SelectionContext)
 */
class EditorRegistry : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Complete editor type definition including factories
     *
     * Factory functions are part of the type definition. This ensures
     * that serialization can always recreate editors of registered types.
     */
    // Forward declaration for custom factory signature
    struct EditorInstance;

    struct EditorTypeInfo {
        QString type_id;       ///< Unique identifier (e.g., "MediaWidget")
        QString display_name;  ///< User-visible name (e.g., "Media Viewer")
        QString icon_path;     ///< Path to icon resource (optional)
        QString menu_path;     ///< Menu location (e.g., "View/Widgets")
        QString default_zone;  ///< Default dock zone ("main", "left", "right")
        bool allow_multiple = true;  ///< Can user open multiple instances?

        /// Creates the EditorState subclass
        std::function<std::shared_ptr<EditorState>()> create_state;

        /// Creates the view widget given state (required unless create_editor_custom is set)
        std::function<QWidget *(std::shared_ptr<EditorState>)> create_view;

        /// Creates the properties widget given state (optional, may be null)
        std::function<QWidget *(std::shared_ptr<EditorState>)> create_properties;

        /**
         * @brief Custom factory for complex editor creation (optional)
         * 
         * When set, this function is called instead of the standard
         * create_state + create_view + create_properties sequence.
         * 
         * Use this when:
         * - View and properties widgets need to share resources
         * - Complex initialization order is required
         * - Widgets need cross-references (e.g., properties needs view's child)
         * 
         * The custom factory is responsible for:
         * - Creating the state
         * - Creating view and properties widgets
         * - Registering the state with the registry
         * 
         * @param registry Pointer to the EditorRegistry (for registerState)
         * @return Complete EditorInstance with state, view, and optional properties
         */
        std::function<EditorInstance(EditorRegistry *)> create_editor_custom;
    };

    /**
     * @brief Result of creating an editor instance
     */
    struct EditorInstance {
        std::shared_ptr<EditorState> state;  ///< The state (auto-registered)
        QWidget * view = nullptr;            ///< Main view widget
        QWidget * properties = nullptr;      ///< Properties widget (may be null)
    };

    /**
     * @brief Construct EditorRegistry
     * @param data_manager Shared DataManager instance (may be nullptr for tests)
     * @param parent Parent QObject
     */
    explicit EditorRegistry(std::shared_ptr<DataManager> data_manager,
                            QObject * parent = nullptr);

    ~EditorRegistry() override;

    // ========== Type Registration ==========

    /**
     * @brief Register an editor type
     *
     * @param info Complete type definition including factories
     * @return true if registered, false if type_id empty or already registered
     */
    bool registerType(EditorTypeInfo info);

    /**
     * @brief Unregister an editor type
     * @param type_id Type to unregister
     * @return true if was registered and removed
     */
    bool unregisterType(QString const & type_id);

    /**
     * @brief Check if a type is registered
     */
    [[nodiscard]] bool hasType(QString const & type_id) const;

    /**
     * @brief Get type info (returns default-constructed if not found)
     */
    [[nodiscard]] EditorTypeInfo typeInfo(QString const & type_id) const;

    /**
     * @brief Get all registered types
     */
    [[nodiscard]] std::vector<EditorTypeInfo> allTypes() const;

    /**
     * @brief Get types filtered by menu path
     */
    [[nodiscard]] std::vector<EditorTypeInfo> typesByMenuPath(QString const & path) const;

    // ========== Editor Creation ==========

    /**
     * @brief Create a complete editor instance
     *
     * Creates state + view + optional properties. The state is automatically
     * registered with the registry.
     *
     * @param type_id Type of editor to create
     * @return EditorInstance with all components, or empty if type not found
     */
    [[nodiscard]] EditorInstance createEditor(QString const & type_id);

    /**
     * @brief Create only the state (not auto-registered)
     *
     * Use registerState() after calling this to add to the registry.
     * Useful for deserialization where you need to restore state before registering.
     */
    [[nodiscard]] std::shared_ptr<EditorState> createState(QString const & type_id);

    /**
     * @brief Create view widget for an existing state
     */
    [[nodiscard]] QWidget * createView(std::shared_ptr<EditorState> state);

    /**
     * @brief Create properties widget for an existing state
     * @return Properties widget, or nullptr if type has no properties factory
     */
    [[nodiscard]] QWidget * createProperties(std::shared_ptr<EditorState> state);

    // ========== State Registry ==========

    /**
     * @brief Register an externally-created state
     *
     * Use for states created via createState() or deserialized.
     */
    void registerState(std::shared_ptr<EditorState> state);

    /**
     * @brief Unregister a state by instance ID
     */
    void unregisterState(QString const & instance_id);

    /**
     * @brief Get state by instance ID
     * @return State or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<EditorState> state(QString const & instance_id) const;

    /**
     * @brief Get all states of a specific type
     */
    [[nodiscard]] std::vector<std::shared_ptr<EditorState>>
    statesByType(QString const & type_id) const;

    /**
     * @brief Get all registered states
     */
    [[nodiscard]] std::vector<std::shared_ptr<EditorState>> allStates() const;

    /**
     * @brief Get number of registered states
     */
    [[nodiscard]] size_t stateCount() const;

    // ========== Selection & Data ==========

    /**
     * @brief Get the selection context for inter-widget communication
     */
    [[nodiscard]] SelectionContext * selectionContext() const;

    /**
     * @brief Get the DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const;

    // ========== Serialization ==========

    /**
     * @brief Serialize workspace to JSON
     *
     * Includes all registered states and current selection.
     */
    [[nodiscard]] std::string toJson() const;

    /**
     * @brief Restore workspace from JSON
     *
     * Clears existing states and recreates from JSON.
     * Types must be registered before calling this.
     *
     * @return true if successful
     */
    bool fromJson(std::string const & json);

    /**
     * @brief Check if any state has unsaved changes
     */
    [[nodiscard]] bool hasUnsavedChanges() const;

    /**
     * @brief Mark all states as clean
     */
    void markAllClean();

signals:
    /// Emitted when a new type is registered
    void typeRegistered(QString type_id);

    /// Emitted when a type is unregistered
    void typeUnregistered(QString type_id);

    /// Emitted when a state is registered
    void stateRegistered(QString instance_id, QString type_id);

    /// Emitted when a state is unregistered
    void stateUnregistered(QString instance_id);

    /// Emitted when createEditor() succeeds
    void editorCreated(QString instance_id, QString type_id);

    /// Emitted when any state changes
    void workspaceChanged();

    /// Emitted when dirty state changes
    void unsavedChangesChanged(bool has_unsaved);

private slots:
    void onStateChanged();
    void onStateDirtyChanged(bool is_dirty);

private:
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<SelectionContext> _selection_context;

    /// Registered types (type_id -> info)
    std::map<QString, EditorTypeInfo> _types;

    /// Active states (instance_id -> state)
    std::map<QString, std::shared_ptr<EditorState>> _states;

    void connectStateSignals(EditorState * state);
};

#endif  // EDITOR_REGISTRY_HPP
