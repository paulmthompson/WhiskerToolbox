#ifndef EDITOR_CREATION_CONTROLLER_HPP
#define EDITOR_CREATION_CONTROLLER_HPP

/**
 * @file EditorCreationController.hpp
 * @brief Controller for creating editors and placing them in appropriate zones
 * 
 * EditorCreationController encapsulates the logic for:
 * - Creating editor instances via EditorRegistry
 * - Wrapping views and properties in ADS dock widgets
 * - Placing dock widgets in appropriate zones via ZoneManager
 * 
 * This is part of the Phase 3 refactoring to move away from PropertiesHost
 * toward persistent, independently-tabbed property panels.
 * 
 * ## Usage
 * 
 * ```cpp
 * auto controller = new EditorCreationController(registry, zone_manager, dock_manager);
 * 
 * // Create and place an editor - view goes to preferred_zone, properties to properties_zone
 * auto [view_dock, props_dock, state] = controller->createAndPlace("MediaWidget");
 * 
 * // The dock widgets are already added to their zones
 * // State is registered with EditorRegistry
 * // Cleanup signals are connected
 * ```
 * 
 * @see EditorRegistry for type registration and editor creation
 * @see ZoneManager for zone placement logic
 * @see EditorTypeInfo for zone preference configuration
 */

#include "EditorState/ZoneTypes.hpp"
#include "EditorState/StrongTypes.hpp"

#include <QObject>

#include <memory>

// Forward declarations
namespace ads {
class CDockManager;
class CDockWidget;
}

class EditorRegistry;
class EditorState;
class ZoneManager;

/**
 * @brief Controller for unified editor creation and zone placement
 * 
 * EditorCreationController bridges EditorRegistry (which knows how to create
 * editor components) with ZoneManager (which knows where to place them).
 * 
 * ## Zone Placement Logic
 * 
 * The controller reads EditorTypeInfo to determine:
 * - `preferred_zone`: Where the view widget goes (typically Center)
 * - `properties_zone`: Where the properties widget goes (typically Right)
 * - `properties_as_tab`: Whether to add as tab (true) or replace content
 * - `auto_raise_properties`: Whether to bring properties to front on creation
 * 
 * ## Ownership
 * 
 * - The controller does NOT own the created widgets
 * - Dock widgets are managed by the ADS dock manager
 * - EditorState is registered with EditorRegistry
 * - Callers can connect to dock widget signals for lifecycle events
 */
class EditorCreationController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Result of creating and placing an editor
     * 
     * Contains pointers to the created dock widgets and the editor state.
     * Dock widgets may be nullptr if creation failed or was not applicable
     * (e.g., no properties factory for the editor type).
     */
    struct PlacedEditor {
        ads::CDockWidget * view_dock = nullptr;       ///< Dock widget containing the view
        ads::CDockWidget * properties_dock = nullptr; ///< Dock widget containing properties (may be null)
        std::shared_ptr<EditorState> state;           ///< The editor state (registered with registry)
        
        /**
         * @brief Check if the editor was successfully created
         * @return true if state and view_dock are valid
         */
        [[nodiscard]] bool isValid() const { return state != nullptr && view_dock != nullptr; }
    };

    /**
     * @brief Construct an EditorCreationController
     * 
     * @param registry The EditorRegistry for type lookup and editor creation
     * @param zone_manager The ZoneManager for zone placement
     * @param dock_manager The ADS dock manager for creating dock widgets
     * @param parent Parent QObject (typically MainWindow)
     */
    explicit EditorCreationController(EditorRegistry * registry,
                                       ZoneManager * zone_manager,
                                       ads::CDockManager * dock_manager,
                                       QObject * parent = nullptr);

    ~EditorCreationController() override = default;

    /**
     * @brief Create an editor and place it in appropriate zones
     * 
     * This method:
     * 1. Creates the editor via EditorRegistry::createEditor()
     * 2. Wraps the view in a CDockWidget and adds to preferred_zone
     * 3. Wraps properties (if any) in a CDockWidget and adds to properties_zone
     * 4. Connects cleanup signals for state unregistration on close
     * 
     * The dock widget titles are derived from the editor's display name.
     * Properties dock titles are suffixed with " Properties".
     * 
     * @param type_id The editor type to create
     * @param raise_view If true, make the view the active tab in its zone
     * @return PlacedEditor containing the created components
     */
    [[nodiscard]] PlacedEditor createAndPlace(EditorLib::EditorTypeId const & type_id,
                                               bool raise_view = true);

    /**
     * @brief Create an editor with a custom dock title
     * 
     * Same as createAndPlace() but allows specifying custom titles for
     * the dock widgets.
     * 
     * @param type_id The editor type to create
     * @param view_title Custom title for the view dock widget
     * @param raise_view If true, make the view the active tab in its zone
     * @return PlacedEditor containing the created components
     */
    [[nodiscard]] PlacedEditor createAndPlaceWithTitle(EditorLib::EditorTypeId const & type_id,
                                                        QString const & view_title,
                                                        bool raise_view = true);

    /**
     * @brief Get the number of editors created by this controller
     * 
     * Useful for generating unique dock titles (e.g., "MediaWidget_1").
     */
    [[nodiscard]] int createdCount(EditorLib::EditorTypeId const & type_id) const;

signals:
    /**
     * @brief Emitted when an editor is successfully created and placed
     * 
     * @param instance_id The instance ID of the created editor
     * @param type_id The type of the created editor
     */
    void editorPlaced(EditorLib::EditorInstanceId instance_id, 
                      EditorLib::EditorTypeId type_id);

    /**
     * @brief Emitted when an editor's view dock is closed
     * 
     * @param instance_id The instance ID of the closed editor
     */
    void editorClosed(EditorLib::EditorInstanceId instance_id);

private:
    EditorRegistry * _registry;
    ZoneManager * _zone_manager;
    ads::CDockManager * _dock_manager;
    
    /// Counter for generating unique dock titles per type
    std::map<EditorLib::EditorTypeId, int> _creation_counters;

    /**
     * @brief Create a dock widget for a widget
     * 
     * @param widget The widget to wrap
     * @param title The dock widget title
     * @param closable Whether the dock can be closed by the user
     * @return The created dock widget
     */
    ads::CDockWidget * createDockWidget(QWidget * widget, 
                                         QString const & title,
                                         bool closable = true);

    /**
     * @brief Connect cleanup signals for an editor
     * 
     * Connects the view dock's closed signal to unregister the state
     * from the EditorRegistry.
     */
    void connectCleanupSignals(PlacedEditor const & editor,
                                EditorLib::EditorInstanceId const & instance_id);

    /**
     * @brief Generate a unique title for a dock widget
     * 
     * @param base_name The base name (e.g., "Media Viewer")
     * @param type_id The editor type (for counter lookup)
     * @return Unique title (e.g., "Media Viewer 2")
     */
    QString generateUniqueTitle(QString const & base_name,
                                 EditorLib::EditorTypeId const & type_id);
};

#endif  // EDITOR_CREATION_CONTROLLER_HPP
