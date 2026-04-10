#ifndef PSTH_WIDGET_HPP
#define PSTH_WIDGET_HPP

/**
 * @file PSTHWidget.hpp
 * @brief Main widget for displaying PSTH plots
 * 
 * PSTHWidget displays Peri-Stimulus Time Histogram plots showing
 * event counts or rates aligned to a reference event.
 */

#include "DataTypeEnum/DM_DataType.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QWidget>

#include <map>
#include <memory>
#include <string>
#include <utility>

class DataManager;
class PSTHState;
class RelativeTimeAxisWidget;
class RelativeTimeAxisRangeControls;
class VerticalAxisWidget;
class VerticalAxisRangeControls;
class PSTHPlotOpenGLWidget;

namespace Ui {
class PSTHWidget;
}

/**
 * @brief Main widget for PSTH plot visualization
 */
class PSTHWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a PSTHWidget
     * @param data_manager Shared DataManager for data access
     * @param parent Parent widget
     */
    PSTHWidget(std::shared_ptr<DataManager> data_manager,
               QWidget * parent = nullptr);

    ~PSTHWidget() override;

    /**
     * @brief Set the PSTHState for this widget
     * 
     * The state manages all serializable settings. This widget shares
     * the state with the properties widget.
     * 
     * @param state Shared pointer to the state object
     */
    void setState(std::shared_ptr<PSTHState> state);

    /**
     * @brief Get the current PSTHState (const)
     * @return Shared pointer to the state object
     */
    [[nodiscard]] std::shared_ptr<PSTHState> state() const { return _state; }

    /**
     * @brief Get mutable state access
     * @return Raw pointer to state for modification
     */
    [[nodiscard]] PSTHState * state();

    /**
     * @brief Get the range controls widget (for placement in properties panel)
     * @return Pointer to the range controls widget, or nullptr if not created
     */
    [[nodiscard]] RelativeTimeAxisRangeControls * getRangeControls() const;


    /**
     * @brief Get the vertical axis range controls widget (for placement in properties panel)
     * @return Pointer to the vertical axis range controls widget, or nullptr if not created
     */
    [[nodiscard]] VerticalAxisRangeControls * getVerticalRangeControls() const;

signals:
    /**
     * @brief Emitted when a time position is selected in the view
     * @param position TimePosition to navigate to
     */
    void timePositionSelected(TimePosition position);

public slots:
    /**
     * @brief Open a file dialog and write the current scene to an SVG file.
     *
     * Uses `AppFileDialog` for session-aware path memory (dialog id
     * `"export_psth_svg"`). Shows an informational message box on success and an
     * error message box on failure. Does nothing if the scene is empty or the user
     * cancels the dialog.
     */
    void handleExportSVG();

    /**
     * @brief Open a file dialog and write PSTH histogram data to a CSV file.
     *
     * Uses `AppFileDialog` for session-aware path memory (dialog id
     * `"export_psth_csv"`). Re-computes per-series histograms and calls
     * `PlotDataExport::exportHistogramToCSV()`.
     */
    void handleExportCSV();

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    // -- setState decomposition --
    void createTimeAxisIfNeeded();
    void wireTimeAxis();
    void wireVerticalAxis();
    void connectViewChangeSignals();
    void syncTimeAxisRange();
    void syncVerticalAxisRange();

    // -- Visible range helpers --
    [[nodiscard]] std::pair<double, double> computeVisibleTimeRange() const;
    [[nodiscard]] std::pair<double, double> computeVisibleVerticalRange() const;

    std::shared_ptr<DataManager> _data_manager;
    Ui::PSTHWidget * ui;

    std::shared_ptr<PSTHState> _state;
    PSTHPlotOpenGLWidget * _opengl_widget;

    RelativeTimeAxisWidget * _axis_widget;
    RelativeTimeAxisRangeControls * _range_controls;

    VerticalAxisWidget * _vertical_axis_widget;
    VerticalAxisRangeControls * _vertical_range_controls;

    /// DataManager-level observer ID for detecting key additions/removals
    int _dm_observer_id = -1;

    /// Per-key callback IDs for data change notifications (DM key → callback ID)
    std::map<std::string, int> _data_callback_ids;

    /// Register a data-change callback for a single DM key
    void _registerDataCallback(std::string const & key);

    /// Unregister a data-change callback for a single DM key
    void _unregisterDataCallback(std::string const & key);

    /// Unregister all data-change callbacks
    void _unregisterAllDataCallbacks();

    /// Sync data callbacks with current state's plot event keys
    void _syncDataCallbacks();

    /// Check if any visualized keys have been removed from DataManager and clear them
    void _pruneRemovedKeys();
};

#endif// PSTH_WIDGET_HPP
