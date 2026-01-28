#ifndef ANALOG_TIME_SERIES_DATA_VIEW_HPP
#define ANALOG_TIME_SERIES_DATA_VIEW_HPP

/**
 * @file AnalogTimeSeriesDataView.hpp
 * @brief Info view widget for AnalogTimeSeries data
 * 
 * AnalogTimeSeriesDataView provides an informational display for AnalogTimeSeries
 * objects in the Center zone. Since analog time series data is continuous and
 * doesn't have a natural tabular representation by frame, this view shows
 * summary statistics and metadata about the signal.
 * 
 * @see BaseDataView for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include <memory>

class QLabel;
class QVBoxLayout;

/**
 * @brief Info view widget for AnalogTimeSeries
 * 
 * Displays summary information about analog time series data including
 * sample count, time range, and basic statistics.
 */
class AnalogTimeSeriesDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit AnalogTimeSeriesDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~AnalogTimeSeriesDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Analog; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Analog Time Series Info"); }

private:
    void _setupUi();

    QVBoxLayout * _layout{nullptr};
    QLabel * _info_label{nullptr};
};

#endif // ANALOG_TIME_SERIES_DATA_VIEW_HPP
