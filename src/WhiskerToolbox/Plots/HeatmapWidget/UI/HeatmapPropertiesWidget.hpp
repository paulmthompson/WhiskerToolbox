#ifndef HEATMAP_PROPERTIES_WIDGET_HPP
#define HEATMAP_PROPERTIES_WIDGET_HPP

/**
 * @file HeatmapPropertiesWidget.hpp
 * @brief Properties panel for the Heatmap Widget
 *
 * HeatmapPropertiesWidget is the properties/inspector panel for HeatmapWidget.
 * Axis range controls are provided via RelativeTimeAxisRangeControls and
 * VerticalAxisRangeControls in collapsible sections (set when setPlotWidget is
 * called).
 *
 * @see HeatmapWidget for the view component
 * @see HeatmapState for shared state
 * @see HeatmapWidgetRegistration for factory registration
 */

#include "Core/HeatmapState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class ColormapControls;
class EstimationMethodControls;
class Feature_Tree_Widget;
class HeatmapWidget;
class PlotAlignmentWidget;
class QCheckBox;
class QComboBox;
class QLabel;
class RelativeTimeAxisRangeControls;
class ScalingModeControls;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class HeatmapPropertiesWidget;
}

/**
 * @brief Properties panel for Heatmap Widget
 *
 * Displays plot settings and configuration options.
 * Shares state with HeatmapWidget (view) via HeatmapState.
 */
class HeatmapPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a HeatmapPropertiesWidget
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit HeatmapPropertiesWidget(std::shared_ptr<HeatmapState> state,
                                     std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent = nullptr);

    ~HeatmapPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<HeatmapState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the HeatmapWidget to connect axis range controls in collapsible sections
     * @param plot_widget The HeatmapWidget instance
     */
    void setPlotWidget(HeatmapWidget * plot_widget);

signals:
    /**
     * @brief Emitted when the user requests SVG export from the properties panel
     */
    void exportSVGRequested();

    /**
     * @brief Emitted when the user requests CSV export from the properties panel
     */
    void exportCSVRequested();

private:
    Ui::HeatmapPropertiesWidget * ui;
    std::shared_ptr<HeatmapState> _state;
    std::shared_ptr<DataManager> _data_manager;
    PlotAlignmentWidget * _alignment_widget;
    Feature_Tree_Widget * _unit_tree_widget;
    HeatmapWidget * _plot_widget;
    RelativeTimeAxisRangeControls * _range_controls;
    Section * _range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;

    // Scaling UI
    Section * _scaling_section{nullptr};
    EstimationMethodControls * _estimation_controls{nullptr};
    ScalingModeControls * _scaling_controls{nullptr};
    ColormapControls * _colormap_controls{nullptr};

    // Sorting UI
    Section * _sorting_section{nullptr};
    QComboBox * _sort_mode_combo{nullptr};
    QCheckBox * _sort_ascending_check{nullptr};

    Section * _export_section{nullptr};

    void _setupUnitTree();
    void _connectUnitTreeSignals();
    void _syncTreeFromState();
    void _setupScalingSection();
    void _syncScalingFromState();
    void _setupSortingSection();
    void _syncSortingFromState();
    void _setupExportSection();
};

#endif// HEATMAP_PROPERTIES_WIDGET_HPP
