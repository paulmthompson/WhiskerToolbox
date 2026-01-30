#ifndef DIGITAL_INTERVAL_SERIES_INSPECTOR_HPP
#define DIGITAL_INTERVAL_SERIES_INSPECTOR_HPP

/**
 * @file DigitalIntervalSeriesInspector.hpp
 * @brief Inspector widget for DigitalIntervalSeries
 * 
 * DigitalIntervalSeriesInspector provides inspection capabilities for DigitalIntervalSeries objects.
 * 
 * ## Features
 * - Interval table with start/end times
 * - Create/delete intervals
 * - Export to CSV
 * - Frame navigation via click
 * 
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"// For CSVIntervalSaverOptions
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"               // For context menu utilities
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"  // For TimePosition

#include <functional>
#include <string>
#include <variant>// std::variant
#include <vector>

namespace Ui {
class DigitalIntervalSeriesInspector;
}
class CSVIntervalSaver_Widget;
class DigitalIntervalSeriesDataView;

using IntervalSaverOptionsVariant = std::variant<CSVIntervalSaverOptions>;// Will expand if more types are added

/**
 * @brief Inspector widget for DigitalIntervalSeries
 * 
 * Provides functionality for digital interval series inspection and management.
 */
class DigitalIntervalSeriesInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the digital interval series inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features (not used)
     * @param parent Parent widget
     */
    explicit DigitalIntervalSeriesInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~DigitalIntervalSeriesInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::DigitalInterval; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Digital Interval Series"); }

    [[nodiscard]] bool supportsExport() const override { return true; }
    [[nodiscard]] bool supportsGroupFiltering() const override { return false; }

    /**
     * @brief Set the data view to use for selection
     * @param view Pointer to the DigitalIntervalSeriesDataView (can be nullptr)
     * 
     * This connects the widget's selection operations to the view panel's table.
     * Should be called when both the inspector and view are created.
     */
    void setDataView(DigitalIntervalSeriesDataView * view);

    /**
     * @brief Set a callback function to get selected intervals from the view panel
     * @param provider Function that returns currently selected intervals
     */
    void setSelectionProvider(std::function<std::vector<Interval>()> provider);

private:
    Ui::DigitalIntervalSeriesInspector * ui;
    bool _interval_epoch{false};
    int64_t _interval_start{0};
    int64_t _interval_end{0};// Track both start and end for bidirectional support
    std::function<std::vector<Interval>()> _selection_provider;

    enum SaverType { CSV };// Enum for different saver types

    void _connectSignals();

    void _calculateIntervals();
    void _assignCallbacks();

    void _initiateSaveProcess(SaverType saver_type, IntervalSaverOptionsVariant & options_variant);
    bool _performActualCSVSave(CSVIntervalSaverOptions & options);

    std::vector<Interval> _getSelectedIntervals();

    /**
     * @brief Update the start frame label display
     * 
     * @param frame_number The frame number to display, or -1 to clear
     */
    void _updateStartFrameLabel(int64_t frame_number = -1);

    /**
     * @brief Cancel the current interval creation process
     */
    void _cancelIntervalCreation();

    /**
     * @brief Show context menu for create interval button
     * 
     * @param position The position where the menu should appear
     */
    void _showCreateIntervalContextMenu(QPoint const & position);

    /**
     * @brief Generate appropriate filename based on active key and export type
     * 
     * @return String containing the filename with appropriate extension
     * @note Returns the active key with the appropriate file extension for the current export type
     */
    std::string _generateFilename() const;

    /**
     * @brief Update the filename field based on current active key and export type
     */
    void _updateFilename();

    /**
     * @brief Move selected intervals to the specified target key
     * 
     * @param target_key The key to move intervals to
     */
    void _moveIntervalsToTarget(std::string const & target_key);

    /**
     * @brief Copy selected intervals to the specified target key
     * 
     * @param target_key The key to copy intervals to
     */
    void _copyIntervalsToTarget(std::string const & target_key);

    /**
     * @brief Delete selected intervals from the current data
     */
    void _deleteSelectedIntervals();

    /**
     * @brief Get the current time converted to the DigitalIntervalSeries timeframe
     * @return The current time index in the series' timeframe, or -1 if conversion fails
     * 
     * Gets the time_position from the state variable, converts it to the
     * DigitalIntervalSeries timeframe, and returns the TimeFrameIndex value.
     */
    [[nodiscard]] int64_t _getCurrentTimeInSeriesFrame() const;

private slots:

    void _createIntervalButton();
    void _removeIntervalButton();
    void _flipIntervalButton();
    void _extendInterval();

    void _onExportTypeChanged(int index);
    void _handleSaveIntervalCSVRequested(CSVIntervalSaverOptions options);

    void _mergeIntervalsButton();
    void _cancelIntervalButton();
    void _createIntervalContextMenuRequested(QPoint const & position);
};

#endif // DIGITAL_INTERVAL_SERIES_INSPECTOR_HPP
