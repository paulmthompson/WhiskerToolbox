#ifndef DIGITALINTERVALSERIES_WIDGET_HPP
#define DIGITALINTERVALSERIES_WIDGET_HPP

#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"// For CSVIntervalSaverOptions
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"               // For context menu utilities

#include <QMenu>
#include <QWidget>

#include <memory>
#include <string>
#include <variant>// std::variant


namespace Ui {
class DigitalIntervalSeries_Widget;
}
class DataManager;
class IntervalTableModel;
class CSVIntervalSaver_Widget;


using IntervalSaverOptionsVariant = std::variant<CSVIntervalSaverOptions>;// Will expand if more types are added

class DigitalIntervalSeries_Widget : public QWidget {
    Q_OBJECT
public:
    explicit DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~DigitalIntervalSeries_Widget() override;

    void openWidget();// Call to open the widget

    void setActiveKey(std::string key);
    void removeCallbacks();

signals:
    void frameSelected(int frame_id);

private:
    Ui::DigitalIntervalSeries_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _active_key;
    int _callback_id{-1};// Corrected initialization
    bool _interval_epoch{false};
    int64_t _interval_start{0};
    int64_t _interval_end{0};// Track both start and end for bidirectional support
    IntervalTableModel * _interval_table_model;

    enum SaverType { CSV };// Enum for different saver types

    void _calculateIntervals();
    void _assignCallbacks();

    void _initiateSaveProcess(SaverType saver_type, IntervalSaverOptionsVariant & options_variant);
    bool _performActualCSVSave(CSVIntervalSaverOptions & options);

    std::vector<Interval> _getSelectedIntervals();
    void _showContextMenu(QPoint const & position);

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

private slots:

    void _createIntervalButton();
    void _removeIntervalButton();
    void _flipIntervalButton();
    void _handleCellClicked(QModelIndex const & index);
    void _extendInterval();

    void _onExportTypeChanged(int index);
    void _handleSaveIntervalCSVRequested(CSVIntervalSaverOptions options);

    void _mergeIntervalsButton();
    void _cancelIntervalButton();
    void _createIntervalContextMenuRequested(QPoint const & position);
};

#endif// DIGITALINTERVALSERIES_WIDGET_HPP
