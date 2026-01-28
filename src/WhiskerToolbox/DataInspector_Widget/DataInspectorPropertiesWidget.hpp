#ifndef DATA_INSPECTOR_PROPERTIES_WIDGET_HPP
#define DATA_INSPECTOR_PROPERTIES_WIDGET_HPP

/**
 * @file DataInspectorPropertiesWidget.hpp
 * @brief Properties panel for data inspection (Right zone)
 * 
 * DataInspectorPropertiesWidget displays the properties and controls for
 * inspecting a single data item. It responds to SelectionContext changes
 * unless pinned.
 * 
 * ## Features
 * - Header with data key display and pin button
 * - Type-specific content area (stacked widget)
 * - Export options (delegated to type-specific inspectors)
 * 
 * ## Zone Placement
 * This widget is placed in the Right zone as a properties panel.
 * The corresponding DataInspectorViewWidget goes in the Center zone.
 * 
 * @see DataInspectorState for state management
 * @see DataInspectorViewWidget for the view component
 */

#include "DataManager/DataManagerFwd.hpp"  // For DM_DataType

#include <QWidget>

#include <memory>
#include <string>

class DataManager;
class DataInspectorState;
class DataInspectorViewWidget;
class GroupManager;
class SelectionContext;
struct SelectionSource;

namespace Ui {
class DataInspectorPropertiesWidget;
}

/**
 * @brief Properties panel for inspecting data items
 * 
 * This widget displays controls and properties for inspecting a data item.
 * When unpinned, it responds to SelectionContext changes.
 * When pinned, it maintains its current data key regardless of selection.
 */
class DataInspectorPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the properties widget
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group-aware features
     * @param parent Parent widget
     */
    explicit DataInspectorPropertiesWidget(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~DataInspectorPropertiesWidget() override;

    /**
     * @brief Set the state object for this widget
     * @param state Shared state (also used by DataInspectorViewWidget)
     */
    void setState(std::shared_ptr<DataInspectorState> state);

    /**
     * @brief Get the current state
     * @return State object, or nullptr if not set
     */
    [[nodiscard]] std::shared_ptr<DataInspectorState> state() const { return _state; }

    /**
     * @brief Set the SelectionContext for responding to selection changes
     * @param context SelectionContext (typically from EditorRegistry)
     */
    void setSelectionContext(SelectionContext * context);

    /**
     * @brief Directly set the data key to inspect (bypasses SelectionContext)
     * @param key Data key in DataManager
     */
    void inspectData(QString const & key);

    /**
     * @brief Set the view widget to connect inspectors to views
     * @param view_widget The view widget (can be nullptr)
     * 
     * This allows inspectors that need selection from views to connect to them.
     */
    void setViewWidget(DataInspectorViewWidget * view_widget);

signals:
    /**
     * @brief Emitted when a frame is selected in the inspector
     * @param frame_id Frame index to navigate to
     */
    void frameSelected(int frame_id);

private slots:
    void _onSelectionChanged(SelectionSource const & source);
    void _onPinToggled(bool checked);
    void _onInspectedKeyChanged(QString const & key);
    void _onStateChanged();

private:
    void _setupUi();
    void _connectSignals();
    void _updateInspectorForKey(QString const & key);
    void _updateHeaderDisplay();
    void _clearInspector();
    void _createInspectorForType(DM_DataType type);

    std::unique_ptr<Ui::DataInspectorPropertiesWidget> ui;
    std::shared_ptr<DataManager> _data_manager;
    std::shared_ptr<DataInspectorState> _state;
    SelectionContext * _selection_context{nullptr};
    GroupManager * _group_manager{nullptr};
    
    // Current inspector (type-specific, created by InspectorFactory)
    std::unique_ptr<class BaseInspector> _current_inspector;
    std::string _current_key;
    DM_DataType _current_type{DM_DataType::Unknown};
    DataInspectorViewWidget * _view_widget{nullptr};
    
    void _connectInspectorToView();
};

#endif // DATA_INSPECTOR_PROPERTIES_WIDGET_HPP
