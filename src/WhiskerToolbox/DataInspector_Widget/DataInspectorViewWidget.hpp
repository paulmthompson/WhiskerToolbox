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
 * ## Phase 3 Implementation
 * 
 * This widget uses ViewFactory to create type-specific data views (tables)
 * based on the data type being inspected.
 * 
 * ## Zone Placement
 * This widget is placed in the Center zone.
 * The corresponding DataInspectorPropertiesWidget goes in the Right zone.
 * 
 * @see DataInspectorState for state management
 * @see DataInspectorPropertiesWidget for the properties component
 * @see ViewFactory for creating type-specific views
 */

#include "DataManager/DataManagerFwd.hpp"  // For DM_DataType
#include "TimeFrame/TimeFrame.hpp"  // For TimePosition

#include <QWidget>

#include <memory>
#include <string>

class DataManager;
class DataInspectorState;
class BaseDataView;

namespace Ui {
class DataInspectorViewWidget;
}

/**
 * @brief View panel for data visualization
 * 
 * This widget displays data tables and visualizations for the inspected item.
 * It shares state with DataInspectorPropertiesWidget.
 * 
 * Uses ViewFactory to create type-specific views:
 * - PointDataView for Points
 * - LineDataView for Lines
 * - MaskDataView for Masks
 * - ImageDataView for Images/Video
 * - AnalogTimeSeriesDataView for AnalogTimeSeries
 * - DigitalEventSeriesDataView for DigitalEventSeries
 * - DigitalIntervalSeriesDataView for DigitalIntervalSeries
 * - TensorDataView for Tensor
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

    /**
     * @brief Get the current data view widget
     * @return Pointer to current view, or nullptr if none
     */
    [[nodiscard]] BaseDataView * currentView() const { return _current_data_view.get(); }

signals:
    /**
     * @brief Emitted when a frame is selected in the view
     * @param frame_id Frame index to navigate to
     */
    void frameSelected(TimePosition position);

private slots:
    void _onInspectedKeyChanged(QString const & key);

private:
    void _setupUi();
    void _updateViewForKey(QString const & key);
    void _createViewForType(DM_DataType type);
    void _clearView();

    std::unique_ptr<Ui::DataInspectorViewWidget> ui;
    std::shared_ptr<DataManager> _data_manager;
    std::shared_ptr<DataInspectorState> _state;
    
    // Current data view (type-specific, created by ViewFactory)
    std::unique_ptr<BaseDataView> _current_data_view;
    QWidget * _placeholder_widget{nullptr};
    std::string _current_key;
    DM_DataType _current_type{DM_DataType::Unknown};
};

#endif // DATA_INSPECTOR_VIEW_WIDGET_HPP
