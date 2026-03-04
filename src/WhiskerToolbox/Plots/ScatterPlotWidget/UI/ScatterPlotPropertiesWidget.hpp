#ifndef SCATTER_PLOT_PROPERTIES_WIDGET_HPP
#define SCATTER_PLOT_PROPERTIES_WIDGET_HPP

/**
 * @file ScatterPlotPropertiesWidget.hpp
 * @brief Properties panel for the Scatter Plot Widget
 *
 * Provides data source selection (X and Y axis), axis range controls,
 * and reference line toggle. Uses DataManager observer to keep combo
 * boxes synchronized with available data keys.
 *
 * @see ScatterPlotWidget, ScatterPlotState, ScatterPlotWidgetRegistration
 */

#include "Core/ScatterPlotState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class GlyphStyleControls;
class HorizontalAxisRangeControls;
class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class ScatterPlotWidget;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class ScatterPlotPropertiesWidget;
}

/**
 * @brief Properties panel for Scatter Plot Widget
 *
 * Contains data source selection (key, column, offset) for both axes,
 * a compatibility status label, reference line toggle, and axis range controls.
 */
class ScatterPlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScatterPlotPropertiesWidget(std::shared_ptr<ScatterPlotState> state,
                                          std::shared_ptr<DataManager> data_manager,
                                          QWidget * parent = nullptr);
    ~ScatterPlotPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<ScatterPlotState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the ScatterPlotWidget to connect axis range controls
     * @param plot_widget The ScatterPlotWidget instance
     */
    void setPlotWidget(ScatterPlotWidget * plot_widget);

private:
    void _createDataSourceUI();
    void _populateKeyComboBoxes();
    void _populateColumnComboBox(QComboBox * combo, std::string const & data_key);
    void _onXKeyChanged();
    void _onYKeyChanged();
    void _onXColumnChanged();
    void _onYColumnChanged();
    void _onXOffsetChanged(int value);
    void _onYOffsetChanged(int value);
    void _onReferenceLineToggled(bool checked);
    void _onColorByGroupToggled(bool checked);
    void _updateCompatibilityLabel();
    void _updateUIFromState();
    void _applyXSourceToState();
    void _applyYSourceToState();
    void _onSelectionModeChanged(int index);
    void _updateSelectionInstructions();

    Ui::ScatterPlotPropertiesWidget * ui;
    std::shared_ptr<ScatterPlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    int _dm_observer_id{-1};
    ScatterPlotWidget * _plot_widget{nullptr};

    // Data source UI elements
    Section * _data_source_section{nullptr};
    QComboBox * _x_key_combo{nullptr};
    QComboBox * _x_column_combo{nullptr};
    QSpinBox * _x_offset_spin{nullptr};
    QComboBox * _y_key_combo{nullptr};
    QComboBox * _y_column_combo{nullptr};
    QSpinBox * _y_offset_spin{nullptr};
    QLabel * _compatibility_label{nullptr};

    // Reference line
    Section * _reference_line_section{nullptr};
    QCheckBox * _reference_line_checkbox{nullptr};

    // Glyph style
    Section * _glyph_style_section{nullptr};
    GlyphStyleControls * _glyph_style_controls{nullptr};

    // Group coloring
    QCheckBox * _color_by_group_checkbox{nullptr};

    // Selection mode
    Section * _selection_section{nullptr};
    QComboBox * _selection_mode_combo{nullptr};
    QLabel * _selection_instructions_label{nullptr};

    // Axis range controls
    HorizontalAxisRangeControls * _horizontal_range_controls{nullptr};
    Section * _horizontal_range_controls_section{nullptr};
    VerticalAxisRangeControls * _vertical_range_controls{nullptr};
    Section * _vertical_range_controls_section{nullptr};

    // Guard to prevent feedback loops during combo box population
    bool _updating_combos{false};
};

#endif  // SCATTER_PLOT_PROPERTIES_WIDGET_HPP
