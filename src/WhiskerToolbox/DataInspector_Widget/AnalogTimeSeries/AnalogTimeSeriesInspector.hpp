#ifndef ANALOG_TIME_SERIES_INSPECTOR_HPP
#define ANALOG_TIME_SERIES_INSPECTOR_HPP

/**
 * @file AnalogTimeSeriesInspector.hpp
 * @brief Inspector widget for AnalogTimeSeries
 * 
 * AnalogTimeSeriesInspector provides inspection capabilities for AnalogTimeSeries objects.
 * It wraps the existing AnalogTimeSeries_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Analog time series information display
 * - Export to CSV
 * 
 * @see AnalogTimeSeries_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class AnalogTimeSeries_Widget;

/**
 * @brief Inspector widget for AnalogTimeSeries
 * 
 * Wraps AnalogTimeSeries_Widget to provide IDataInspector interface while reusing
 * existing functionality for analog time series data inspection and export.
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
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<AnalogTimeSeries_Widget> _analog_widget;
};

#endif // ANALOG_TIME_SERIES_INSPECTOR_HPP
