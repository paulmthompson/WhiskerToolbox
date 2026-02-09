#ifndef TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP
#define TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP

/**
 * @file TemporalProjectionViewPropertiesWidget.hpp
 * @brief Properties panel for the Temporal Projection View Widget
 *
 * Provides controls for:
 * - Adding/removing point and line data keys from DataManager
 * - Point size and line width rendering controls
 * - Selection mode toggle (None / Point / Line)
 * - Axis range controls via HorizontalAxisRangeControls and VerticalAxisRangeControls
 *   in collapsible sections (set when setPlotWidget is called)
 *
 * @see TemporalProjectionViewWidget, TemporalProjectionViewState,
 * TemporalProjectionViewWidgetRegistration
 */

#include "Core/TemporalProjectionViewState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class HorizontalAxisRangeControls;
class TemporalProjectionViewWidget;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class TemporalProjectionViewPropertiesWidget;
}

/**
 * @brief Properties panel for Temporal Projection View Widget
 */
class TemporalProjectionViewPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a TemporalProjectionViewPropertiesWidget
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit TemporalProjectionViewPropertiesWidget(
        std::shared_ptr<TemporalProjectionViewState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~TemporalProjectionViewPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<TemporalProjectionViewState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the TemporalProjectionViewWidget to connect axis range controls
     * @param plot_widget The TemporalProjectionViewWidget instance
     */
    void setPlotWidget(TemporalProjectionViewWidget * plot_widget);

private slots:
    // Point data key management
    void _onAddPointClicked();
    void _onRemovePointClicked();
    void _onPointTableSelectionChanged();

    // Line data key management
    void _onAddLineClicked();
    void _onRemoveLineClicked();
    void _onLineTableSelectionChanged();

    // Rendering controls
    void _onPointSizeChanged(double value);
    void _onLineWidthChanged(double value);

    // Selection mode
    void _onSelectionModeChanged(int index);
    void _onClearSelectionClicked();

    // State change handlers
    void _onStatePointKeyAdded(QString const & key);
    void _onStatePointKeyRemoved(QString const & key);
    void _onStateLineKeyAdded(QString const & key);
    void _onStateLineKeyRemoved(QString const & key);

private:
    void _populatePointComboBox();
    void _populateLineComboBox();
    void _updatePointDataTable();
    void _updateLineDataTable();
    void _updateUIFromState();

    Ui::TemporalProjectionViewPropertiesWidget * ui;
    std::shared_ptr<TemporalProjectionViewState> _state;
    std::shared_ptr<DataManager> _data_manager;
    TemporalProjectionViewWidget * _plot_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    Section * _horizontal_range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;

    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif  // TEMPORAL_PROJECTION_VIEW_PROPERTIES_WIDGET_HPP
