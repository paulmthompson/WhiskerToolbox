#ifndef DATA_IMPORT_WIDGET_HPP
#define DATA_IMPORT_WIDGET_HPP

/**
 * @file DataImport_Widget.hpp
 * @brief Main container widget for data import functionality
 * 
 * DataImport_Widget serves as the primary interface for importing data into
 * WhiskerToolbox. It implements the "Passive Awareness" pattern via DataFocusAware,
 * automatically switching to the appropriate loader widget when data focus changes.
 * 
 * ## Architecture
 * 
 * The widget contains:
 * - A data type selector combo box (for manual type selection)
 * - A QStackedWidget containing type-specific import widgets
 * - Integration with SelectionContext for automatic type switching
 * 
 * ## Zone Placement
 * 
 * This widget is designed to function as a properties panel in Zone::Right,
 * providing context-sensitive import options based on the currently focused data.
 * 
 * ## Widget Lifecycle
 * 
 * Type-specific widgets are created lazily on first access and cached for reuse.
 * This preserves user input (file paths, options) across type switches.
 * 
 * @see DataImportWidgetState for state management
 * @see DataImportTypeRegistry for type registration
 * @see DataFocusAware for passive awareness pattern
 */

#include "EditorState/DataFocusAware.hpp"
#include "DataImportWidgetState.hpp"

#include <QWidget>

#include <map>
#include <memory>

// Forward declarations
class DataManager;
class SelectionContext;
class QStackedWidget;
class QComboBox;
class QLabel;

namespace Ui {
class DataImport_Widget;
}

/**
 * @brief Main container widget for data import functionality
 * 
 * DataImport_Widget provides:
 * 1. Automatic type switching based on SelectionContext data focus
 * 2. Manual type selection via combo box
 * 3. Lazy-loaded, cached type-specific import widgets
 * 4. State persistence via DataImportWidgetState
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state and widget
 * auto state = std::make_shared<DataImportWidgetState>();
 * auto widget = new DataImport_Widget(state, data_manager, selection_context, this);
 * 
 * // Widget automatically responds to data focus changes
 * // User can also manually select data type via combo box
 * ```
 */
class DataImport_Widget : public QWidget, public DataFocusAware {
    Q_OBJECT

public:
    /**
     * @brief Construct the DataImport_Widget
     * 
     * @param state State object for persistence and change tracking
     * @param data_manager DataManager for data access
     * @param selection_context SelectionContext for data focus awareness
     * @param parent Parent widget
     */
    explicit DataImport_Widget(
        std::shared_ptr<DataImportWidgetState> state,
        std::shared_ptr<DataManager> data_manager,
        SelectionContext * selection_context,
        QWidget * parent = nullptr);

    ~DataImport_Widget() override;

    // === DataFocusAware Interface ===

    /**
     * @brief Handle data focus changes from SelectionContext
     * 
     * Switches the stacked widget to show the appropriate import widget
     * for the focused data type.
     * 
     * @param data_key The newly focused data key (may be invalid)
     * @param data_type The type of the focused data (e.g., "LineData")
     */
    void onDataFocusChanged(EditorLib::SelectedDataKey const & data_key,
                            QString const & data_type) override;

    // === Accessors ===

    /**
     * @brief Get the state object
     * @return Shared pointer to state
     */
    [[nodiscard]] std::shared_ptr<DataImportWidgetState> state() const { return _state; }

signals:
    /**
     * @brief Emitted when an import operation completes successfully
     * @param data_key Key of the newly imported data in DataManager
     * @param data_type Type of the imported data
     */
    void importCompleted(QString const & data_key, QString const & data_type);

private slots:
    /**
     * @brief Handle manual data type selection from combo box
     * @param index Selected index in combo box
     */
    void _onDataTypeComboChanged(int index);

    /**
     * @brief Handle state's selected type changing
     * @param type New selected type
     */
    void _onStateTypeChanged(QString const & type);

private:
    Ui::DataImport_Widget * ui;
    std::shared_ptr<DataImportWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    SelectionContext * _selection_context;

    /// Cached type-specific import widgets (created lazily)
    std::map<QString, QWidget *> _type_widgets;

    /**
     * @brief Set up the data type combo box with registered types
     */
    void _setupDataTypeCombo();

    /**
     * @brief Switch the stacked widget to show import widget for given type
     * @param data_type Data type to switch to
     */
    void _switchToDataType(QString const & data_type);

    /**
     * @brief Get or create the import widget for a data type
     * 
     * Creates the widget on first access, caches for subsequent access.
     * 
     * @param data_type Data type
     * @return Widget for that type, or nullptr if type not registered
     */
    QWidget * _getOrCreateWidgetForType(QString const & data_type);

    /**
     * @brief Update the UI header based on current type
     * @param data_type Current data type (empty for no selection)
     */
    void _updateHeader(QString const & data_type);
};

#endif // DATA_IMPORT_WIDGET_HPP
