#include "DigitalIntervalSeriesDataView.hpp"

#include "IntervalTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

DigitalIntervalSeriesDataView::DigitalIntervalSeriesDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new IntervalTableModel(this)) {
    _setupUi();
    _connectSignals();
}

DigitalIntervalSeriesDataView::~DigitalIntervalSeriesDataView() {
    removeCallbacks();
}

void DigitalIntervalSeriesDataView::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<DigitalIntervalSeries>(key)) {
        return;
    }

    removeCallbacks();
    _active_key = key;

    auto interval_data = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (interval_data) {
        // Convert view to vector for table model
        std::vector<Interval> interval_vector;
        for (auto const & interval_with_id : interval_data->view()) {
            interval_vector.push_back(interval_with_id.value());
        }
        _table_model->setIntervals(interval_vector);
        _callback_id = interval_data->addObserver([this]() { _onDataChanged(); });
    } else {
        _table_model->setIntervals({});
    }
}

void DigitalIntervalSeriesDataView::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void DigitalIntervalSeriesDataView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto interval_data = dataManager()->getData<DigitalIntervalSeries>(_active_key);
        if (interval_data) {
            // Convert view to vector for table model
            std::vector<Interval> interval_vector;
            for (auto const & interval_with_id : interval_data->view()) {
                interval_vector.push_back(interval_with_id.value());
            }
            _table_model->setIntervals(interval_vector);
        } else {
            _table_model->setIntervals({});
        }
    }
}

void DigitalIntervalSeriesDataView::_setupUi() {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    _table_view = new QTableView(this);
    _table_view->setModel(_table_model);
    _table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table_view->setAlternatingRowColors(true);
    _table_view->setSortingEnabled(true);
    _table_view->horizontalHeader()->setStretchLastSection(true);

    _layout->addWidget(_table_view);
}

void DigitalIntervalSeriesDataView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &DigitalIntervalSeriesDataView::_handleTableViewDoubleClicked);
}

void DigitalIntervalSeriesDataView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {
        auto interval = _table_model->getInterval(index.row());
        // Navigate to start of interval
        emit frameSelected(static_cast<int>(interval.start));
    }
}

void DigitalIntervalSeriesDataView::_onDataChanged() {
    updateView();
}
