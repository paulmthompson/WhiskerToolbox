#ifndef LINE_PLOT_PROPERTIES_WIDGET_HPP
#define LINE_PLOT_PROPERTIES_WIDGET_HPP

/**
 * @file LinePlotPropertiesWidget.hpp
 * @brief Properties panel for the Line Plot Widget
 * 
 * LinePlotPropertiesWidget is the properties/inspector panel for LinePlotWidget.
 * It displays controls for managing plot settings and options.
 * 
 * @see LinePlotWidget for the view component
 * @see LinePlotState for shared state
 * @see LinePlotWidgetRegistration for factory registration
 */

#include "Core/LinePlotState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class LinePlotWidget;
class PlotAlignmentWidget;
class RelativeTimeAxisRangeControls;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class LinePlotPropertiesWidget;
}

/**
 * @brief Properties panel for Line Plot Widget
 * 
 * Displays plot settings and configuration options.
 * Shares state with LinePlotWidget (view) via LinePlotState.
 */
class LinePlotPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a LinePlotPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit LinePlotPropertiesWidget(std::shared_ptr<LinePlotState> state,
                                      std::shared_ptr<DataManager> data_manager,
                                      QWidget * parent = nullptr);

    ~LinePlotPropertiesWidget() override;

    /**
     * @brief Get the shared state
     * @return Shared pointer to LinePlotState
     */
    [[nodiscard]] std::shared_ptr<LinePlotState> state() const { return _state; }

    /**
     * @brief Get the DataManager
     * @return Shared pointer to DataManager
     */
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the LinePlotWidget to connect range controls
     * @param plot_widget The LinePlotWidget instance
     */
    void setPlotWidget(LinePlotWidget * plot_widget);

private slots:
    /**
     * @brief Handle add series button click
     */
    void _onAddSeriesClicked();

    /**
     * @brief Handle remove series button click
     */
    void _onRemoveSeriesClicked();

    /**
     * @brief Handle plot series table selection change
     */
    void _onPlotSeriesSelectionChanged();

    /**
     * @brief Handle state plot series added
     * @param series_name Name of the added series
     */
    void _onStatePlotSeriesAdded(QString const & series_name);

    /**
     * @brief Handle state plot series removed
     * @param series_name Name of the removed series
     */
    void _onStatePlotSeriesRemoved(QString const & series_name);

    /**
     * @brief Handle state plot series options changed
     * @param series_name Name of the updated series
     */
    void _onStatePlotSeriesOptionsChanged(QString const & series_name);

    /**
     * @brief Handle line thickness spinbox value change
     * @param value New thickness value
     */
    void _onLineThicknessChanged(double value);

    /**
     * @brief Handle color button click
     */
    void _onColorButtonClicked();

private:
    /**
     * @brief Populate the add series combo box with available AnalogTimeSeries keys
     */
    void _populateAddSeriesComboBox();

    /**
     * @brief Update the plot series table from state
     */
    void _updatePlotSeriesTable();

    /**
     * @brief Update the series options display for the selected series
     * @param series_name Name of the series to display options for
     */
    void _updateSeriesOptions(QString const & series_name);

    /**
     * @brief Update UI elements from current state
     */
    void _updateUIFromState();

    /**
     * @brief Get the currently selected series name from the table
     * @return Series name if selection exists, empty string otherwise
     */
    [[nodiscard]] QString _getSelectedSeriesName() const;

    /**
     * @brief Update the color display button with a hex color
     * @param hex_color Hex color string (e.g., "#000000")
     */
    void _updateColorDisplay(QString const & hex_color);

    Ui::LinePlotPropertiesWidget * ui;
    std::shared_ptr<LinePlotState> _state;
    std::shared_ptr<DataManager> _data_manager;
    PlotAlignmentWidget * _alignment_widget;
    LinePlotWidget * _plot_widget;
    RelativeTimeAxisRangeControls * _range_controls;
    Section * _range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;

    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif// LINE_PLOT_PROPERTIES_WIDGET_HPP
