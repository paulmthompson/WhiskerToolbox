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
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"// For TimePosition
#include "TimeFrame/interval_data.hpp"

#include <functional>
#include <string>
#include <variant>// std::variant
#include <vector>

namespace Ui {
class DigitalIntervalSeriesInspector;
}

class DigitalIntervalSeriesDataView;

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

    /**
     * @brief Set the data view to use for selection and group coordination
     * @param view Pointer to the DigitalIntervalSeriesDataView (can be nullptr)
     * 
     * This connects the widget's selection operations to the view panel's table.
     * Also sets up group manager and context menu signals on the view.
     * Should be called when both the inspector and view are created.
     */
    void setDataView(DigitalIntervalSeriesDataView * view);

    /**
     * @brief Set the group manager for group features
     * @param group_manager Pointer to GroupManager (can be nullptr)
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Set a callback function to get selected intervals from the view panel
     * @param provider Function that returns currently selected intervals
     */
    void setSelectionProvider(std::function<std::vector<Interval>()> provider);

private:
    Ui::DigitalIntervalSeriesInspector * ui;
    DigitalIntervalSeriesDataView * _data_view{nullptr};
    bool _interval_epoch{false};
    int64_t _interval_start{0};
    int64_t _interval_end{0};// Track both start and end for bidirectional support
    std::function<std::vector<Interval>()> _selection_provider;

    void _connectSignals();

    void _calculateIntervals();
    void _assignCallbacks();

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
     * @brief Move selected intervals to a specific group
     * @param group_id The group ID to move intervals to
     */
    void _moveIntervalsToGroup(int group_id);

    /**
     * @brief Remove selected intervals from their groups
     */
    void _removeIntervalsFromGroup();

    /**
     * @brief Populate the group filter combo box
     */
    void _populateGroupFilterCombo();

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

    void _mergeIntervalsButton();
    void _cancelIntervalButton();
    void _createIntervalContextMenuRequested(QPoint const & position);

    void _onGroupFilterChanged(int index);
    void _onGroupChanged();
};

#endif// DIGITAL_INTERVAL_SERIES_INSPECTOR_HPP
