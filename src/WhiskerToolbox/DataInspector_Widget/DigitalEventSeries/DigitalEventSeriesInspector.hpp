#ifndef DIGITAL_EVENT_SERIES_INSPECTOR_HPP
#define DIGITAL_EVENT_SERIES_INSPECTOR_HPP

/**
 * @file DigitalEventSeriesInspector.hpp
 * @brief Inspector widget for DigitalEventSeries
 * 
 * DigitalEventSeriesInspector provides inspection capabilities for DigitalEventSeries objects.
 * It wraps the existing DigitalEventSeries_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Event table with timestamps
 * - Add/remove events
 * - Export to CSV
 * - Frame navigation via click
 * 
 * @see DigitalEventSeries_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class DigitalEventSeries_Widget;

/**
 * @brief Inspector widget for DigitalEventSeries
 * 
 * Wraps DigitalEventSeries_Widget to provide IDataInspector interface while reusing
 * existing functionality for digital event series inspection and management.
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
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<DigitalEventSeries_Widget> _event_widget;
};

#endif // DIGITAL_EVENT_SERIES_INSPECTOR_HPP
