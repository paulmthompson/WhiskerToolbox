#ifndef DIGITAL_INTERVAL_SERIES_DATA_VIEW_HPP
#define DIGITAL_INTERVAL_SERIES_DATA_VIEW_HPP

/**
 * @file DigitalIntervalSeriesDataView.hpp
 * @brief Table view widget for DigitalIntervalSeries data
 * 
 * DigitalIntervalSeriesDataView provides a table view for DigitalIntervalSeries
 * objects in the Center zone. It displays intervals in a table format with
 * start and end frame/time information.
 * 
 * @see BaseDataView for the base class
 * @see IntervalTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include "TimeFrame/interval_data.hpp"

#include <memory>
#include <vector>

class IntervalTableModel;

class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for DigitalIntervalSeries
 */
class DigitalIntervalSeriesDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit DigitalIntervalSeriesDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~DigitalIntervalSeriesDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::DigitalInterval; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Interval Table"); }

    [[nodiscard]] QTableView * tableView() const { return _table_view; }

    /**
     * @brief Get the currently selected intervals from the table view
     * @return Vector of selected intervals
     */
    [[nodiscard]] std::vector<Interval> getSelectedIntervals() const;

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();

private:
    void _setupUi();
    void _connectSignals();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    IntervalTableModel * _table_model{nullptr};
    int _callback_id{-1};
};

#endif // DIGITAL_INTERVAL_SERIES_DATA_VIEW_HPP
