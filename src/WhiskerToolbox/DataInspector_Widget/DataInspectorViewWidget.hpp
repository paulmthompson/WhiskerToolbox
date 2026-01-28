#ifndef DATA_INSPECTOR_VIEW_WIDGET_HPP
#define DATA_INSPECTOR_VIEW_WIDGET_HPP

/**
 * @file DataInspectorViewWidget.hpp
 * @brief View panel for data inspection (Center zone)
 * 
 * DataInspectorViewWidget displays the data view/visualization for the
 * currently inspected data item. This widget shares state with
 * DataInspectorPropertiesWidget.
 * 
 * ## Phase 1 Implementation
 * 
 * Initially, this widget shows a placeholder message. In Phase 3, it will
 * be populated with:
 * - Data tables (migrated from type-specific widgets)
 * - Visual representations
 * - Interactive data editing
 * 
 * ## Zone Placement
 * This widget is placed in the Center zone.
 * The corresponding DataInspectorPropertiesWidget goes in the Right zone.
 * 
 * @see DataInspectorState for state management
 * @see DataInspectorPropertiesWidget for the properties component
 */

#include <QWidget>

#include <memory>
#include <string>

class DataManager;
class DataInspectorState;

namespace Ui {
class DataInspectorViewWidget;
}

/**
 * @brief View panel for data visualization
 * 
 * This widget displays data tables and visualizations for the inspected item.
 * It shares state with DataInspectorPropertiesWidget.
 * 
 * ## Phase 1 (Current)
 * Shows a placeholder message indicating that custom views will be added later.
 * 
 * ## Phase 3 (Future)
 * Will contain:
 * - Type-specific data tables
 * - Visual representations
 * - Frame navigation
 */
class DataInspectorViewWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the view widget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    explicit DataInspectorViewWidget(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~DataInspectorViewWidget() override;

    /**
     * @brief Set the state object for this widget
     * @param state Shared state (also used by DataInspectorPropertiesWidget)
     */
    void setState(std::shared_ptr<DataInspectorState> state);

    /**
     * @brief Get the current state
     * @return State object, or nullptr if not set
     */
    [[nodiscard]] std::shared_ptr<DataInspectorState> state() const { return _state; }

signals:
    /**
     * @brief Emitted when a frame is selected in the view
     * @param frame_id Frame index to navigate to
     */
    void frameSelected(int frame_id);

private slots:
    void _onInspectedKeyChanged(QString const & key);

private:
    void _setupUi();
    void _updateViewForKey(QString const & key);
    void _clearView();

    std::unique_ptr<Ui::DataInspectorViewWidget> ui;
    std::shared_ptr<DataManager> _data_manager;
    std::shared_ptr<DataInspectorState> _state;
    
    QWidget * _current_view{nullptr};
    std::string _current_key;
};

#endif // DATA_INSPECTOR_VIEW_WIDGET_HPP
