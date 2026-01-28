#ifndef DIGITAL_INTERVAL_SERIES_INSPECTOR_HPP
#define DIGITAL_INTERVAL_SERIES_INSPECTOR_HPP

/**
 * @file DigitalIntervalSeriesInspector.hpp
 * @brief Inspector widget for DigitalIntervalSeries
 * 
 * DigitalIntervalSeriesInspector provides inspection capabilities for DigitalIntervalSeries objects.
 * It wraps the existing DigitalIntervalSeries_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Interval table with start/end times
 * - Create/delete intervals
 * - Export to CSV
 * - Frame navigation via click
 * 
 * @see DigitalIntervalSeries_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class DigitalIntervalSeries_Widget;

/**
 * @brief Inspector widget for DigitalIntervalSeries
 * 
 * Wraps DigitalIntervalSeries_Widget to provide IDataInspector interface while reusing
 * existing functionality for digital interval series inspection and management.
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

private:
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<DigitalIntervalSeries_Widget> _interval_widget;
};

#endif // DIGITAL_INTERVAL_SERIES_INSPECTOR_HPP
