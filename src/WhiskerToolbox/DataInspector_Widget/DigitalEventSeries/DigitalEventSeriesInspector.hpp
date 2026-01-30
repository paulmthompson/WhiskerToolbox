#ifndef DIGITAL_EVENT_SERIES_INSPECTOR_HPP
#define DIGITAL_EVENT_SERIES_INSPECTOR_HPP

/**
 * @file DigitalEventSeriesInspector.hpp
 * @brief Inspector widget for DigitalEventSeries
 * 
 * DigitalEventSeriesInspector provides inspection capabilities for DigitalEventSeries objects.
 * 
 * ## Features
 * - Add/remove events
 * - Export to CSV
 * 
 * Note: Event table view is provided by DigitalEventSeriesDataView in the view panel.
 * 
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"

#include <memory>
#include <string>
#include <variant>

namespace Ui {
class DigitalEventSeriesInspector;
}

class CSVEventSaver_Widget;

using EventSaverOptionsVariant = std::variant<CSVEventSaverOptions>;

/**
 * @brief Inspector widget for DigitalEventSeries
 * 
 * Provides functionality for digital event series inspection and management.
 */
class DigitalEventSeriesInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the digital event series inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features (not used)
     * @param parent Parent widget
     */
    explicit DigitalEventSeriesInspector(
            std::shared_ptr<DataManager> data_manager,
            GroupManager * group_manager = nullptr,
            QWidget * parent = nullptr);

    ~DigitalEventSeriesInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::DigitalEvent; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Digital Event Series"); }

    [[nodiscard]] bool supportsExport() const override { return true; }
    [[nodiscard]] bool supportsGroupFiltering() const override { return false; }

private:
    enum SaverType { CSV };

    void _connectSignals();
    void _calculateEvents();
    void _assignCallbacks();

    void _initiateSaveProcess(SaverType saver_type, EventSaverOptionsVariant & options_variant);
    bool _performActualCSVSave(CSVEventSaverOptions const & options);

    /**
     * @brief Generate appropriate filename based on active key and export type
     * 
     * @return String containing the filename with appropriate extension
     */
    std::string _generateFilename() const;

    /**
     * @brief Update the filename field based on current active key and export type
     */
    void _updateFilename();

    /**
     * @brief Get the current time converted to the DigitalEventSeries timeframe
     * @return The current time index in the series' timeframe, or -1 if conversion fails
     * 
     * Gets the time_position from the state variable, converts it to the
     * DigitalEventSeries timeframe, and returns the TimeFrameIndex value.
     */
    [[nodiscard]] int64_t _getCurrentTimeInSeriesFrame() const;

    Ui::DigitalEventSeriesInspector * ui;

private slots:
    void _addEventButton();
    void _removeEventButton();

    void _onExportTypeChanged(int index);
    void _handleSaveEventCSVRequested(CSVEventSaverOptions options);
};

#endif// DIGITAL_EVENT_SERIES_INSPECTOR_HPP
