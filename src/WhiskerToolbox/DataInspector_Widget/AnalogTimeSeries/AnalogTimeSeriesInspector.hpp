#ifndef ANALOG_TIME_SERIES_INSPECTOR_HPP
#define ANALOG_TIME_SERIES_INSPECTOR_HPP

/**
 * @file AnalogTimeSeriesInspector.hpp
 * @brief Inspector widget for AnalogTimeSeries
 * 
 * AnalogTimeSeriesInspector provides inspection capabilities for AnalogTimeSeries objects.
 * 
 * ## Features
 * - Analog time series information display
 * - Export to CSV
 * 
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"

#include <memory>
#include <string>
#include <variant>

class QComboBox;
class QLineEdit;
class QStackedWidget;
class CSVAnalogSaver_Widget;

namespace Ui {
class AnalogTimeSeriesInspector;
}

// Define the variant type for saver options
using AnalogSaverOptionsVariant = std::variant<CSVAnalogSaverOptions>;// Will expand if more types are added

/**
 * @brief Inspector widget for AnalogTimeSeries
 * 
 * Provides functionality for analog time series data inspection and export.
 */
class AnalogTimeSeriesInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the analog time series inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features (not used)
     * @param parent Parent widget
     */
    explicit AnalogTimeSeriesInspector(
            std::shared_ptr<DataManager> data_manager,
            GroupManager * group_manager = nullptr,
            QWidget * parent = nullptr);

    ~AnalogTimeSeriesInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Analog; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Analog Time Series"); }

    [[nodiscard]] bool supportsExport() const override { return true; }
    [[nodiscard]] bool supportsGroupFiltering() const override { return false; }

private:
    enum SaverType { CSV };// Enum for different saver types

    void _connectSignals();
    void _initiateSaveProcess(SaverType saver_type, AnalogSaverOptionsVariant & options_variant);
    bool _performActualCSVSave(CSVAnalogSaverOptions & options);

    Ui::AnalogTimeSeriesInspector * ui;

private slots:
    void _onExportTypeChanged(int index);
    void _handleSaveAnalogCSVRequested(CSVAnalogSaverOptions options);
};

#endif// ANALOG_TIME_SERIES_INSPECTOR_HPP
