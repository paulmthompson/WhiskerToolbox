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
 * EditorRegistry registry();
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
#include "TimeFrame/TimeFrame.hpp"// For TimePosition
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
    std::string key;       ///< Data key in DataManager
    std::string data_class;///< Type of data (e.g., "PointData", "LineData")
    std::string color;     ///< Hex color for display (e.g., "#00FF00")
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
        QString type_id;     ///< Unique identifier (e.g., "MediaWidget")
        QString display_name;///< User-visible name (e.g., "Media Viewer")
        QString icon_path;   ///< Path to icon resource (optional)
        QString menu_path;   ///< Menu location (e.g., "View/Widgets")

        // === Zone Placement (Phase 1 Refactoring) ===

        Zone preferred_zone = Zone::Center;///< Where View widget goes
        Zone properties_zone = Zone::Right;///< Where Properties widget goes
        bool prefers_split = false;        ///< Hint for transient operations (split zone if needed)
        bool properties_as_tab = true;     ///< Add properties as tab vs replace content
        bool auto_raise_properties = false;///< Bring properties to front on editor activation

        bool allow_multiple = true;///< Can user open multiple instances?

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
        std::shared_ptr<EditorState> state;///< The state (auto-registered)
        QWidget * view = nullptr;          ///< Main view widget
        QWidget * properties = nullptr;    ///< Properties widget (may be null)
    };

    /**
     * @brief Construct EditorRegistry
     * @param parent Parent QObject
     */
    explicit EditorRegistry(QObject * parent = nullptr);

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
     * @brief Set the current visualization time with TimePosition
     * 
     * This is the preferred method for setting the current visualization time.
     * It includes the TimeFrame pointer for clock identity, allowing widgets
     * to efficiently check if they're on the same clock.
     * 
     * This represents which point in time the UI is currently displaying.
     * All time-aware widgets should connect to timeChanged(TimePosition) to update their views.
     * 
     * Note: This is a UI/visualization concept, not data storage. The actual
     * time data lives in DataManager's TimeFrame objects.
     * 
     * This method includes cycle prevention to avoid infinite loops when widgets
     * respond to time changes by calling setCurrentTime() again.
     * 
     * @param position The TimePosition (index + TimeFrame pointer) to set
     */
    void setCurrentTime(TimePosition position);

    /**
     * @brief Set the current visualization time with TimeFrameIndex and TimeFrame
     * 
     * Convenience overload for widgets that already have a TimeFrame pointer.
     * 
     * @param index The TimeFrameIndex within the TimeFrame
     * @param time_frame The TimeFrame this index belongs to
     */
    void setCurrentTime(TimeFrameIndex index, std::shared_ptr<TimeFrame> time_frame);


    /**
     * @brief Set the active TimeKey
     * 
     * Changes which TimeFrame is considered active. This will emit
     * activeTimeKeyChanged() if the key actually changes.
     * 
     * @param key The TimeKey to make active
     */
    void setActiveTimeKey(TimeKey key);

    /**
     * @brief Get the active TimeKey
     * @return The currently active TimeKey (defaults to "time")
     */
    [[nodiscard]] TimeKey activeTimeKey() const;

    /**
     * @brief Get the current time position (includes TimeFrame context)
     * @return Current TimePosition being displayed
     */
    [[nodiscard]] TimePosition currentPosition() const;

    /**
     * @brief Get the current time index
     * @return Current TimeFrameIndex being displayed
     */
    [[nodiscard]] TimeFrameIndex currentTimeIndex() const {
        return _current_position.index;
    }

    /**
     * @brief Get the current TimeFrame (may be nullptr)
     * @return The TimeFrame for the current position
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> currentTimeFrame() const {
        return _current_position.time_frame;
    }

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
     * This is the preferred signal that includes TimePosition (index + TimeFrame pointer).
     * Widgets can use TimePosition::sameClock() to check if they're on the same clock
     * and TimePosition::convertTo() to convert indices between different TimeFrames.
     * 
     * @param position The new TimePosition being displayed
     */
    void timeChanged(TimePosition position);


    /**
     * @brief Emitted when the active TimeKey changes
     * 
     * This signal is emitted when setActiveTimeKey() is called and the
     * TimeKey actually changes.
     * 
     * @param new_key The new active TimeKey
     * @param old_key The previous active TimeKey
     */
    void activeTimeKeyChanged(TimeKey new_key, TimeKey old_key);


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
    std::unique_ptr<SelectionContext> _selection_context;
    std::unique_ptr<EditorLib::OperationContext> _operation_context;

    /// Registered types (type_id -> info)
    std::map<EditorTypeId, EditorTypeInfo> _types;

    /// Active states (instance_id -> state)
    std::map<EditorInstanceId, std::shared_ptr<EditorState>> _states;

    /// Current visualization time state
    TimePosition _current_position;           ///< Current time position (index + TimeFrame pointer)
    TimeKey _active_time_key{TimeKey("time")};///< Currently selected TimeFrame key (defaults to "time")
    bool _time_update_in_progress{false};     ///< Guard flag to prevent re-entrant time updates

    void connectStateSignals(EditorState * state);

public:
    /**
     * @brief Set the current visualization time (deprecated)
     * 
     * @deprecated Use setCurrentTime(TimeKey, TimeFrameIndex) instead
     * 
     * This method is kept for backward compatibility during migration.
     * It uses the active TimeKey and converts the int64_t to TimeFrameIndex.
     * 
     * @param time The frame index to display
     */
    [[deprecated("Use setCurrentTime(TimeKey, TimeFrameIndex) instead")]]
    void setCurrentTime(int64_t time);
};

#endif// EDITOR_REGISTRY_HPP
