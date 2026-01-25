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
 *     .preferred_zone = Zone::Center,      // View goes to center
 *     .properties_zone = Zone::Right,      // Properties as persistent tab
 *     .auto_raise_properties = false,      // Don't obscure other tools
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

#include "StrongTypes.hpp"
#include "ZoneTypes.hpp"

#include <QObject>
#include <QString>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using EditorLib::EditorInstanceId;
using EditorLib::EditorTypeId;

class DataManager;
class EditorState;
class SelectionContext;

namespace EditorLib {
class OperationContext;
}

/**
 * @brief UI display configuration for a data item
 * 
 * This is a lightweight struct used by EditorRegistry signals to communicate
 * display hints (colors, styles) from data loading to UI widgets.
 * It mirrors DataInfo but is defined here to avoid circular dependencies.
 */
struct DataDisplayConfig {
    std::string key;        ///< Data key in DataManager
    std::string data_class; ///< Type of data (e.g., "PointData", "LineData")
    std::string color;      ///< Hex color for display (e.g., "#00FF00")
};

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
        
        // === Zone Placement (Phase 1 Refactoring) ===
        
        Zone preferred_zone = Zone::Center;     ///< Where View widget goes
        Zone properties_zone = Zone::Right;     ///< Where Properties widget goes
        bool prefers_split = false;             ///< Hint for transient operations (split zone if needed)
        bool properties_as_tab = true;          ///< Add properties as tab vs replace content
        bool auto_raise_properties = false;     ///< Bring properties to front on editor activation
        
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
    bool unregisterType(EditorTypeId const & type_id);

    /**
     * @brief Check if a type is registered
     */
    [[nodiscard]] bool hasType(EditorTypeId const & type_id) const;

    /**
     * @brief Get type info (returns default-constructed if not found)
     */
    [[nodiscard]] EditorTypeInfo typeInfo(EditorTypeId const & type_id) const;

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
    [[nodiscard]] EditorInstance createEditor(EditorTypeId const & type_id);

    /**
     * @brief Create only the state (not auto-registered)
     *
     * Use registerState() after calling this to add to the registry.
     * Useful for deserialization where you need to restore state before registering.
     */
    [[nodiscard]] std::shared_ptr<EditorState> createState(EditorTypeId const & type_id);

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
    void unregisterState(EditorInstanceId const & instance_id);

    /**
     * @brief Get state by instance ID
     * @return State or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<EditorState> state(EditorInstanceId const & instance_id) const;

    /**
     * @brief Get all states of a specific type
     */
    [[nodiscard]] std::vector<std::shared_ptr<EditorState>>
    statesByType(EditorTypeId const & type_id) const;

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
     * @brief Get the operation context for transient data pipes
     * 
     * OperationContext manages temporary connections where one widget
     * requests output from another (e.g., LinePlot requesting transform
     * configuration from DataTransformWidget).
     */
    [[nodiscard]] EditorLib::OperationContext * operationContext() const;

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

    // ========== Global Time ==========

    /**
     * @brief Set the current visualization time
     * 
     * This represents which point in time the UI is currently displaying.
     * All time-aware widgets should connect to timeChanged() to update their views.
     * 
     * Note: This is a UI/visualization concept, not data storage. The actual
     * time data lives in DataManager's TimeFrame objects.
     * 
     * @param time The frame index to display
     */
    void setCurrentTime(int64_t time);

    /**
     * @brief Get the current visualization time
     * @return Current frame index being displayed
     */
    [[nodiscard]] int64_t currentTime() const { return _current_time; }

signals:
    /// Emitted when a new type is registered
    void typeRegistered(EditorTypeId type_id);

    /// Emitted when a type is unregistered
    void typeUnregistered(EditorTypeId type_id);

    /// Emitted when a state is registered
    void stateRegistered(EditorInstanceId instance_id, EditorTypeId type_id);

    /// Emitted when a state is unregistered
    void stateUnregistered(EditorInstanceId instance_id);

    /// Emitted when createEditor() succeeds
    void editorCreated(EditorInstanceId instance_id, EditorTypeId type_id);

    /// Emitted when any state changes
    void workspaceChanged();

    /// Emitted when dirty state changes
    void unsavedChangesChanged(bool has_unsaved);

    /**
     * @brief Emitted when the visualization time changes
     * 
     * Connect to this signal to update views when the user scrubs
     * through time (e.g., via TimeScrollBar).
     * 
     * @param time The new frame index to display
     */
    void timeChanged(int64_t time);

    /**
     * @brief Emitted after data is loaded from external sources (JSON config, batch processing)
     * 
     * This signal carries UI configuration hints (colors, display styles) that widgets
     * should apply to their visualizations. This is separate from DataManager's observer
     * notifications which handle data existence changes.
     * 
     * Typical flow:
     * 1. DataManager::load() adds data → DataManager notifies observers (data exists)
     * 2. This signal is emitted → widgets apply UI configuration (colors, styles)
     * 
     * @param config Vector of DataDisplayConfig containing keys and display configuration
     */
    void applyDataDisplayConfig(std::vector<DataDisplayConfig> const & config);

private slots:
    void onStateChanged();
    void onStateDirtyChanged(bool is_dirty);

private:
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<SelectionContext> _selection_context;
    std::unique_ptr<EditorLib::OperationContext> _operation_context;

    /// Registered types (type_id -> info)
    std::map<EditorTypeId, EditorTypeInfo> _types;

    /// Active states (instance_id -> state)
    std::map<EditorInstanceId, std::shared_ptr<EditorState>> _states;

    /// Current visualization time (frame index)
    int64_t _current_time{0};

    void connectStateSignals(EditorState * state);
};

#endif  // EDITOR_REGISTRY_HPP
