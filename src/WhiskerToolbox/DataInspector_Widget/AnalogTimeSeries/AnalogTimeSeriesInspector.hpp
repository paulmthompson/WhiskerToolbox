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

#include <memory>
#include <string>

class QComboBox;
class QLineEdit;
class QStackedWidget;
class CSVAnalogSaver_Widget;

namespace Ui {
class AnalogTimeSeriesInspector;
}

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
    void _connectSignals();

    Ui::AnalogTimeSeriesInspector * ui;

private slots:
    void _onExportTypeChanged(int index);
};

#endif// ANALOG_TIME_SERIES_INSPECTOR_HPP
