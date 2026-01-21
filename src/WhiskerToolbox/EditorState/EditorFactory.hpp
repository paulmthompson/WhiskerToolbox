#ifndef EDITOR_FACTORY_HPP
#define EDITOR_FACTORY_HPP

/**
 * @file EditorFactory.hpp
 * @brief Centralized widget creation and registration
 * 
 * EditorFactory provides a unified system for creating editor widgets.
 * Instead of scattered widget creation code in MainWindow, all editor
 * types are registered with their factories, enabling:
 * 
 * 1. **Consistent creation**: All editors created through single interface
 * 2. **View/Properties split**: Factory knows how to create both components
 * 3. **Metadata storage**: Icon, menu location, zone, etc.
 * 4. **Single-instance tracking**: Prevent duplicates for single-instance editors
 * 
 * ## Registration Pattern
 * 
 * During application startup, MainWindow registers all editor types:
 * 
 * ```cpp
 * _editor_factory->registerEditorType(
 *     EditorFactory::EditorTypeInfo{
 *         .type_id = "TestWidget",
 *         .display_name = "Test Widget",
 *         .default_zone = "main",
 *         .allow_multiple = false
 *     },
 *     // State factory
 *     [dm]() { return std::make_shared<TestWidgetState>(dm); },
 *     // View factory  
 *     [](auto state) {
 *         return new TestWidgetView(
 *             std::dynamic_pointer_cast<TestWidgetState>(state));
 *     },
 *     // Properties factory (optional)
 *     [](auto state) {
 *         return new TestWidgetProperties(
 *             std::dynamic_pointer_cast<TestWidgetState>(state));
 *     }
 * );
 * ```
 * 
 * ## Creation Pattern
 * 
 * ```cpp
 * auto instance = _editor_factory->createEditor("TestWidget");
 * // instance.state is registered with WorkspaceManager
 * // instance.view is the main widget
 * // instance.properties may be nullptr if editor doesn't split
 * ```
 * 
 * @see EditorState for state base class
 * @see WorkspaceManager for state registry
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
class WorkspaceManager;

/**
 * @brief Centralized factory for creating editor widgets
 * 
 * EditorFactory manages the registration and creation of editor types.
 * Each editor type consists of:
 * - A state class (derived from EditorState)
 * - A view widget (the main visualization)
 * - An optional properties widget (for the properties panel)
 */
class EditorFactory : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Metadata describing an editor type
     */
    struct EditorTypeInfo {
        QString type_id;           ///< Unique identifier (e.g., "MediaWidget", "TestWidget")
        QString display_name;      ///< User-visible name (e.g., "Media Viewer")
        QString icon_path;         ///< Path to icon resource (optional)
        QString menu_path;         ///< Menu location (e.g., "View/Widgets")
        QString default_zone;      ///< Default dock zone ("main", "left", "right", etc.)
        bool allow_multiple;       ///< Can user open multiple instances?
    };

    /**
     * @brief Result of creating an editor instance
     */
    struct EditorInstance {
        std::shared_ptr<EditorState> state;  ///< The editor state (registered with WorkspaceManager)
        QWidget * view;                       ///< The main view widget
        QWidget * properties;                 ///< Properties widget (may be nullptr)
    };

    /**
     * @brief Factory function type for creating state
     */
    using StateFactory = std::function<std::shared_ptr<EditorState>()>;

    /**
     * @brief Factory function type for creating view from state
     */
    using ViewFactory = std::function<QWidget *(std::shared_ptr<EditorState>)>;

    /**
     * @brief Factory function type for creating properties from state
     */
    using PropertiesFactory = std::function<QWidget *(std::shared_ptr<EditorState>)>;

    /**
     * @brief Construct EditorFactory
     * @param workspace_manager Workspace manager for state registration
     * @param data_manager Data manager for widgets that need it
     * @param parent Parent QObject
     */
    explicit EditorFactory(WorkspaceManager * workspace_manager,
                           std::shared_ptr<DataManager> data_manager,
                           QObject * parent = nullptr);

    ~EditorFactory() override = default;

    // === Registration ===

    /**
     * @brief Register an editor type with its factories
     * 
     * @param info Metadata about the editor type
     * @param state_factory Function that creates the EditorState subclass
     * @param view_factory Function that creates the view widget given state
     * @param properties_factory Function that creates properties widget (optional)
     * @return true if registration succeeded, false if type_id already registered
     */
    bool registerEditorType(EditorTypeInfo const & info,
                            StateFactory state_factory,
                            ViewFactory view_factory,
                            PropertiesFactory properties_factory = nullptr);

    /**
     * @brief Unregister an editor type
     * @param type_id Type ID to unregister
     * @return true if type was registered and removed
     */
    bool unregisterEditorType(QString const & type_id);

    // === Creation ===

    /**
     * @brief Create a new editor instance (state + view + optional properties)
     * 
     * This method:
     * 1. Creates the state via the registered factory
     * 2. Registers the state with WorkspaceManager
     * 3. Creates the view widget with the state
     * 4. Creates the properties widget if factory was provided
     * 
     * @param type_id Type of editor to create
     * @return EditorInstance with all components, or empty if type not found
     */
    [[nodiscard]] EditorInstance createEditor(QString const & type_id);

    /**
     * @brief Create only the state (for deserialization or special cases)
     * 
     * The state is NOT automatically registered with WorkspaceManager.
     * Use WorkspaceManager::registerState() after calling this.
     * 
     * @param type_id Type of editor state to create
     * @return Shared pointer to state, or nullptr if type not found
     */
    [[nodiscard]] std::shared_ptr<EditorState> createState(QString const & type_id);

    /**
     * @brief Create view widget for an existing state
     * 
     * @param state Existing state (must match the editor type)
     * @return View widget, or nullptr if state type not registered
     */
    [[nodiscard]] QWidget * createView(std::shared_ptr<EditorState> state);

    /**
     * @brief Create properties widget for an existing state
     * 
     * @param state Existing state (must match the editor type)
     * @return Properties widget, or nullptr if no factory registered
     */
    [[nodiscard]] QWidget * createProperties(std::shared_ptr<EditorState> state);

    // === Queries ===

    /**
     * @brief Check if an editor type is registered
     * @param type_id Type ID to check
     * @return true if type is registered
     */
    [[nodiscard]] bool hasEditorType(QString const & type_id) const;

    /**
     * @brief Get information about a registered editor type
     * @param type_id Type ID to look up
     * @return EditorTypeInfo, or default-constructed if not found
     */
    [[nodiscard]] EditorTypeInfo getEditorInfo(QString const & type_id) const;

    /**
     * @brief Get all registered editor types
     * @return Vector of EditorTypeInfo for all registered types
     */
    [[nodiscard]] std::vector<EditorTypeInfo> availableEditors() const;

    /**
     * @brief Get editor types filtered by menu path
     * @param menu_path Menu path to filter by
     * @return Vector of matching EditorTypeInfo
     */
    [[nodiscard]] std::vector<EditorTypeInfo> editorsByMenuPath(QString const & menu_path) const;

    // === Accessors ===

    /**
     * @brief Get the WorkspaceManager
     * @return Pointer to WorkspaceManager
     */
    [[nodiscard]] WorkspaceManager * workspaceManager() const { return _workspace_manager; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

signals:
    /**
     * @brief Emitted when a new editor type is registered
     * @param type_id The type ID that was registered
     */
    void editorTypeRegistered(QString type_id);

    /**
     * @brief Emitted when an editor type is unregistered
     * @param type_id The type ID that was unregistered
     */
    void editorTypeUnregistered(QString type_id);

    /**
     * @brief Emitted when an editor instance is created
     * @param instance_id The instance ID of the created editor's state
     * @param type_id The type ID of the created editor
     */
    void editorCreated(QString instance_id, QString type_id);

private:
    WorkspaceManager * _workspace_manager;
    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Internal registration structure
     */
    struct EditorRegistration {
        EditorTypeInfo info;
        StateFactory state_factory;
        ViewFactory view_factory;
        PropertiesFactory properties_factory;
    };

    /// Registered editor types (type_id -> registration)
    std::map<QString, EditorRegistration> _registrations;
};

#endif // EDITOR_FACTORY_HPP
