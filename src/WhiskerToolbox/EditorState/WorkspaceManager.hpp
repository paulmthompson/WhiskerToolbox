#ifndef WORKSPACE_MANAGER_HPP
#define WORKSPACE_MANAGER_HPP

/**
 * @file WorkspaceManager.hpp
 * @brief Central manager for all editor states and inter-widget communication
 * 
 * WorkspaceManager serves as the central hub for:
 * - Registry of all active EditorState instances
 * - Access to SelectionContext for inter-widget communication
 * - Workspace serialization (save/restore complete state)
 * - State factory registration
 * 
 * @see EditorState for individual widget states
 * @see SelectionContext for selection management
 */

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class DataManager;
class EditorState;
class SelectionContext;

/**
 * @brief Factory function type for creating EditorState instances
 */
using EditorStateFactory = std::function<std::shared_ptr<EditorState>()>;

/**
 * @brief Metadata about a registered editor type
 */
struct EditorTypeInfo {
    QString type_name;       ///< Unique type identifier
    QString display_name;    ///< Human-readable name for UI
    QString icon_path;       ///< Path to icon resource (optional)
    QString default_zone;    ///< Default dock zone (e.g., "main", "properties")
};

/**
 * @brief Central manager for workspace state
 * 
 * WorkspaceManager is the hub that connects the various parts of the
 * EditorState architecture:
 * 
 * 1. **State Registry**: All EditorState instances are registered here
 * 2. **Selection Context**: Single SelectionContext for the application
 * 3. **State Factory**: Creates new editor states by type
 * 4. **Serialization**: Saves/restores complete workspace
 * 
 * ## Ownership Model
 * 
 * - WorkspaceManager owns SelectionContext
 * - EditorState instances are owned via shared_ptr (can be shared with widgets)
 * - DataManager is a weak dependency (not owned)
 * 
 * ## Typical Usage
 * 
 * ```cpp
 * // In MainWindow setup
 * _workspace_manager = std::make_unique<WorkspaceManager>(_data_manager);
 * 
 * // Register editor type factory
 * _workspace_manager->registerEditorType(
 *     EditorTypeInfo{"MediaWidget", "Media Viewer", ":/icons/media.png", "main"},
 *     []() { return std::make_shared<MediaWidgetState>(); }
 * );
 * 
 * // Create a new editor
 * auto state = _workspace_manager->createState("MediaWidget");
 * auto* widget = new Media_Widget(state, _data_manager,
 *                                 _workspace_manager->selectionContext());
 * 
 * // Access selection context
 * connect(_workspace_manager->selectionContext(), &SelectionContext::selectionChanged,
 *         this, &MainWindow::onSelectionChanged);
 * ```
 */
class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct WorkspaceManager
     * @param data_manager Shared DataManager instance
     * @param parent Parent QObject
     */
    explicit WorkspaceManager(std::shared_ptr<DataManager> data_manager, QObject * parent = nullptr);

    /**
     * @brief Destructor
     */
    ~WorkspaceManager() override;

    // === Editor Type Registration ===

    /**
     * @brief Register an editor type with its factory
     * 
     * This allows WorkspaceManager to create new instances of editor
     * states by type name. Must be called during application startup.
     * 
     * @param info Metadata about the editor type
     * @param factory Function that creates new instances
     */
    void registerEditorType(EditorTypeInfo const & info, EditorStateFactory factory);

    /**
     * @brief Get information about registered editor types
     * @return Vector of EditorTypeInfo for all registered types
     */
    [[nodiscard]] std::vector<EditorTypeInfo> registeredEditorTypes() const;

    /**
     * @brief Check if an editor type is registered
     * @param type_name Type name to check
     * @return true if type is registered
     */
    [[nodiscard]] bool isEditorTypeRegistered(QString const & type_name) const;

    // === State Factory ===

    /**
     * @brief Create a new editor state of the specified type
     * 
     * Creates a new state instance and registers it automatically.
     * 
     * @param type_name Type of editor to create
     * @return Shared pointer to new state, or nullptr if type not found
     */
    [[nodiscard]] std::shared_ptr<EditorState> createState(QString const & type_name);

    // === State Registry ===

    /**
     * @brief Register an externally-created editor state
     * 
     * Use this when you've created a state outside of createState(),
     * e.g., when deserializing.
     * 
     * @param state State to register
     */
    void registerState(std::shared_ptr<EditorState> state);

    /**
     * @brief Unregister an editor state
     * 
     * Call this when closing an editor. The state may still exist
     * if other code holds a shared_ptr, but it won't be part of
     * workspace serialization.
     * 
     * @param instance_id Instance ID of state to unregister
     */
    void unregisterState(QString const & instance_id);

    /**
     * @brief Get state by instance ID
     * @param instance_id Instance ID to look up
     * @return Shared pointer to state, or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<EditorState> getState(QString const & instance_id) const;

    /**
     * @brief Get all states of a specific type
     * @param type_name Type name to filter by
     * @return Vector of matching states
     */
    [[nodiscard]] std::vector<std::shared_ptr<EditorState>>
    getStatesByType(QString const & type_name) const;

    /**
     * @brief Get all registered states
     * @return Vector of all states
     */
    [[nodiscard]] std::vector<std::shared_ptr<EditorState>> getAllStates() const;

    /**
     * @brief Get number of registered states
     * @return Count of states
     */
    [[nodiscard]] size_t stateCount() const;

    // === Selection Context ===

    /**
     * @brief Get the global selection context
     * 
     * All widgets should use this single SelectionContext for
     * inter-widget communication.
     * 
     * @return Pointer to SelectionContext (never null after construction)
     */
    [[nodiscard]] SelectionContext * selectionContext() const;

    // === Data Manager Access ===

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const;

    // === Serialization ===

    /**
     * @brief Serialize entire workspace to JSON
     * 
     * The JSON includes:
     * - List of all editor states (type and state JSON)
     * - Current selection state
     * - Workspace metadata
     * 
     * @return JSON string representation of workspace
     */
    [[nodiscard]] std::string toJson() const;

    /**
     * @brief Restore workspace from JSON
     * 
     * This will:
     * 1. Clear existing states
     * 2. Create new states from JSON
     * 3. Restore selection state
     * 
     * @param json JSON string to parse
     * @return true if parsing succeeded
     */
    bool fromJson(std::string const & json);

    /**
     * @brief Check if any state has unsaved changes
     * @return true if any state is dirty
     */
    [[nodiscard]] bool hasUnsavedChanges() const;

    /**
     * @brief Mark all states as clean
     * 
     * Called after saving workspace.
     */
    void markAllClean();

signals:
    /**
     * @brief Emitted when a new state is registered
     * @param instance_id Instance ID of new state
     * @param type_name Type of the new state
     */
    void stateRegistered(QString const & instance_id, QString const & type_name);

    /**
     * @brief Emitted when a state is unregistered
     * @param instance_id Instance ID of removed state
     */
    void stateUnregistered(QString const & instance_id);

    /**
     * @brief Emitted when any state changes
     * 
     * Useful for updating UI that shows workspace overview.
     */
    void workspaceChanged();

    /**
     * @brief Emitted when dirty state changes for any state
     * @param has_unsaved true if any state is now dirty
     */
    void unsavedChangesChanged(bool has_unsaved);

private slots:
    /**
     * @brief Handle state change from any registered state
     */
    void onStateChanged();

    /**
     * @brief Handle dirty change from any registered state
     * @param is_dirty New dirty state
     */
    void onStateDirtyChanged(bool is_dirty);

private:
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<SelectionContext> _selection_context;

    // State registry (instance_id -> state)
    std::unordered_map<std::string, std::shared_ptr<EditorState>> _states;

    // Editor type factories (type_name -> factory)
    std::unordered_map<std::string, EditorStateFactory> _factories;

    // Editor type info (type_name -> info)
    std::unordered_map<std::string, EditorTypeInfo> _type_info;

    // Connect signals from a state
    void connectStateSignals(EditorState * state);
};

#endif // WORKSPACE_MANAGER_HPP
