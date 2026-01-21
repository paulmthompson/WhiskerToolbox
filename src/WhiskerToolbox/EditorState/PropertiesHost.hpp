#ifndef PROPERTIES_HOST_HPP
#define PROPERTIES_HOST_HPP

/**
 * @file PropertiesHost.hpp
 * @brief Context-sensitive properties container for the right panel
 * 
 * PropertiesHost observes SelectionContext and displays the appropriate
 * properties panel based on the active editor. It serves as a unified
 * container for all editor-specific properties widgets.
 * 
 * ## Design Philosophy
 * 
 * PropertiesHost is **editor-centric**, not data-type-centric:
 * - It shows properties for the currently active **editor** (e.g., MediaWidget, TestWidget)
 * - Each editor's properties widget is responsible for its own internal layout
 * - If an editor needs to show data-type-specific properties, that logic lives
 *   in the editor's properties widget, not in PropertiesHost
 * 
 * This simplifies PropertiesHost significantly:
 * - No data-type factories needed
 * - No complex routing based on selected data
 * - Just: "show the properties for the active editor"
 * 
 * ## View/Properties Split Pattern
 * 
 * Widgets that support the split pattern have:
 * - A **View** component: goes in the Center zone, shows visualization
 * - A **Properties** component: goes in PropertiesHost, shows controls
 * - Both share the same **EditorState** instance
 * 
 * When a View becomes active (gains focus), PropertiesHost automatically
 * shows the corresponding Properties widget.
 * 
 * ## Usage
 * 
 * ```cpp
 * // In MainWindow
 * _properties_host = new PropertiesHost(_editor_registry);
 * _zone_manager->addToZone(properties_dock, Zone::Right);
 * 
 * // Register properties factory for an editor type
 * _editor_registry->registerType({
 *     .type_id = "TestWidget",
 *     .create_properties = [](auto state) {
 *         auto test_state = std::dynamic_pointer_cast<TestWidgetState>(state);
 *         return new TestWidgetProperties(test_state);
 *     }
 * });
 * 
 * // When TestWidgetView gains focus, PropertiesHost automatically
 * // shows TestWidgetProperties
 * ```
 * 
 * ## Caching
 * 
 * PropertiesHost caches created properties widgets to avoid recreation:
 * - Key: editor instance ID
 * - Value: properties widget pointer
 * 
 * When an editor is unregistered, its cached properties widget is removed.
 * 
 * @see EditorRegistry for widget type registration
 * @see SelectionContext for active editor tracking
 * @see ZoneManager for UI zone management
 */

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QVBoxLayout>

#include <map>
#include <memory>

// Forward declarations
class EditorRegistry;
class EditorState;
class SelectionContext;
struct SelectionSource;

/**
 * @brief Container widget that displays properties for the active editor
 * 
 * PropertiesHost:
 * - Observes SelectionContext for active editor changes
 * - Uses EditorRegistry to create properties widgets
 * - Caches properties widgets for efficiency
 * - Shows placeholder when no editor is active
 */
class PropertiesHost : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a PropertiesHost
     * @param editor_registry The EditorRegistry to use for creating properties
     * @param parent Parent widget
     */
    explicit PropertiesHost(EditorRegistry * editor_registry,
                            QWidget * parent = nullptr);

    ~PropertiesHost() override;

    /**
     * @brief Get the currently displayed properties widget
     * @return Current widget or nullptr if showing placeholder
     */
    [[nodiscard]] QWidget * currentProperties() const;

    /**
     * @brief Get the instance ID of the currently displayed properties
     * @return Instance ID or empty string if showing placeholder
     */
    [[nodiscard]] QString currentInstanceId() const;

    /**
     * @brief Force display of properties for a specific editor
     * 
     * Useful for programmatic control, bypasses normal active editor tracking.
     * 
     * @param instance_id The editor instance ID
     */
    void showPropertiesFor(QString const & instance_id);

    /**
     * @brief Clear cached properties widget for an editor
     * 
     * Called when an editor is being destroyed.
     * 
     * @param instance_id The editor instance ID to clear
     */
    void clearCachedProperties(QString const & instance_id);

    /**
     * @brief Clear all cached properties widgets
     */
    void clearAllCached();

signals:
    /**
     * @brief Emitted when the displayed properties widget changes
     * @param instance_id The editor instance ID, or empty if placeholder
     */
    void propertiesChanged(QString const & instance_id);

private slots:
    /**
     * @brief Handle active editor changes from SelectionContext
     * @param instance_id The new active editor instance ID
     */
    void onActiveEditorChanged(QString const & instance_id);

    /**
     * @brief Handle selection changes from SelectionContext
     * @param source The selection source
     */
    void onSelectionChanged(SelectionSource const & source);

    /**
     * @brief Handle editor unregistration from EditorRegistry
     * @param instance_id The unregistered editor instance ID
     */
    void onEditorUnregistered(QString const & instance_id);

private:
    EditorRegistry * _editor_registry;
    SelectionContext * _selection_context;

    QStackedWidget * _stack;
    QWidget * _placeholder;
    
    // Cached properties widgets (instance_id -> widget)
    std::map<QString, QWidget *> _cached_widgets;
    
    // Currently displayed instance ID (empty for placeholder)
    QString _current_instance_id;

    /**
     * @brief Set up the UI
     */
    void setupUi();

    /**
     * @brief Connect to SelectionContext and EditorRegistry signals
     */
    void connectSignals();

    /**
     * @brief Get or create properties widget for an editor
     * @param instance_id The editor instance ID
     * @return Properties widget or nullptr if editor has no properties
     */
    QWidget * getOrCreateProperties(QString const & instance_id);

    /**
     * @brief Show the placeholder widget
     */
    void showPlaceholder();

    /**
     * @brief Create the placeholder widget
     */
    QWidget * createPlaceholder();
};

#endif  // PROPERTIES_HOST_HPP
