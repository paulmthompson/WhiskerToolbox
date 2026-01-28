#ifndef DIGITAL_EVENT_SERIES_DATA_VIEW_HPP
#define DIGITAL_EVENT_SERIES_DATA_VIEW_HPP

/**
 * @file DigitalEventSeriesDataView.hpp
 * @brief Table view widget for DigitalEventSeries data
 * 
 * DigitalEventSeriesDataView provides a table view for DigitalEventSeries
 * objects in the Center zone. It displays events in a table format with
 * frame/time information.
 * 
 * @see BaseDataView for the base class
 * @see EventTableModel for the underlying data model
 */

#include "DataInspector_Widget/Inspectors/BaseDataView.hpp"

#include <memory>

class EventTableModel;

class QTableView;
class QVBoxLayout;

/**
 * @brief Table view widget for DigitalEventSeries
 */
class DigitalEventSeriesDataView : public BaseDataView {
    Q_OBJECT

public:
    explicit DigitalEventSeriesDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~DigitalEventSeriesDataView() override;

    // IDataView Interface
    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::DigitalEvent; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Event Table"); }

    [[nodiscard]] QTableView * tableView() const { return _table_view; }

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();

private:
    void _setupUi();
    void _connectSignals();

    QVBoxLayout * _layout{nullptr};
    QTableView * _table_view{nullptr};
    EventTableModel * _table_model{nullptr};
    int _callback_id{-1};
};

#endif // DIGITAL_EVENT_SERIES_DATA_VIEW_HPP
