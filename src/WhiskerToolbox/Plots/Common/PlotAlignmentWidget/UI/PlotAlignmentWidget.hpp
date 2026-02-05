#ifndef PLOT_ALIGNMENT_WIDGET_HPP
#define PLOT_ALIGNMENT_WIDGET_HPP

/**
 * @file PlotAlignmentWidget.hpp
 * @brief Reusable widget for plot alignment controls
 * 
 * PlotAlignmentWidget provides a reusable UI component for selecting alignment
 * events/intervals, configuring window size and offset, and displaying event counts.
 * This widget can be embedded in plot properties panels.
 * 
 * @see PlotAlignmentState for the state class
 */

#include "Core/PlotAlignmentData.hpp"
#include "Core/PlotAlignmentState.hpp"

#include "DataManager/DataManagerFwd.hpp"

#include <QWidget>

#include <memory>

class DataManager;

namespace Ui {
class PlotAlignmentWidget;
}

/**
 * @brief Reusable widget for plot alignment controls
 * 
 * This widget provides UI controls for:
 * - Selecting alignment event/interval series
 * - Configuring window size and offset
 * - Displaying total event/interval count
 * - Selecting interval alignment (beginning/end) when applicable
 * 
 * The widget automatically populates combo boxes from DataManager and
 * handles observer registration/cleanup.
 */
class PlotAlignmentWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a PlotAlignmentWidget
     * 
     * @param state State object for alignment settings (must outlive this widget)
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit PlotAlignmentWidget(PlotAlignmentState * state,
                                 std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent = nullptr);

    ~PlotAlignmentWidget() override;

    /**
     * @brief Update UI elements from current state
     * 
     * Call this after setting the state or when state changes externally.
     */
    void updateUIFromState();

private slots:
    /**
     * @brief Handle alignment event combo box selection change
     * @param index Selected index
     */
    void _onAlignmentEventChanged(int index);

    /**
     * @brief Handle interval alignment combo box selection change
     * @param index Selected index (0 = Beginning, 1 = End)
     */
    void _onIntervalAlignmentChanged(int index);

    /**
     * @brief Handle offset spinbox value change
     * @param value New offset value
     */
    void _onOffsetChanged(double value);

    /**
     * @brief Handle window size spinbox value change
     * @param value New window size value
     */
    void _onWindowSizeChanged(double value);

    /**
     * @brief Handle state alignment event key change
     * @param key New alignment event key
     */
    void _onStateAlignmentEventKeyChanged(QString const & key);

    /**
     * @brief Handle state interval alignment type change
     * @param type New interval alignment type
     */
    void _onStateIntervalAlignmentTypeChanged(IntervalAlignmentType type);

    /**
     * @brief Handle state offset change
     * @param offset New offset value
     */
    void _onStateOffsetChanged(double offset);

    /**
     * @brief Handle state window size change
     * @param window_size New window size value
     */
    void _onStateWindowSizeChanged(double window_size);

private:
    /**
     * @brief Populate the alignment event combo box with available event/interval series
     */
    void _populateAlignmentEventComboBox();

    /**
     * @brief Update the event/interval count labels based on current selection
     */
    void _updateEventCount();

    Ui::PlotAlignmentWidget * ui;
    PlotAlignmentState * _state;
    std::shared_ptr<DataManager> _data_manager;

    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif// PLOT_ALIGNMENT_WIDGET_HPP
