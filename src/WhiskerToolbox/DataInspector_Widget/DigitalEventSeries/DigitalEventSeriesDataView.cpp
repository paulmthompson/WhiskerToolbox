#include "DigitalEventSeriesDataView.hpp"

#include "EventTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

DigitalEventSeriesDataView::DigitalEventSeriesDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new EventTableModel(this)) {
    _setupUi();
    _connectSignals();
}

DigitalEventSeriesDataView::~DigitalEventSeriesDataView() {
    removeCallbacks();
}

void DigitalEventSeriesDataView::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<DigitalEventSeries>(key)) {
        return;
    }

    removeCallbacks();
    _active_key = key;

    auto event_data = dataManager()->getData<DigitalEventSeries>(_active_key);
    if (event_data) {
        // Convert view to vector for table model
        std::vector<TimeFrameIndex> event_vector;
        for (auto const & event : event_data->view()) {
            event_vector.push_back(event.time());
        }
        _table_model->setEvents(event_vector);
        _callback_id = event_data->addObserver([this]() { _onDataChanged(); });
    } else {
        _table_model->setEvents({});
    }
}

void DigitalEventSeriesDataView::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void DigitalEventSeriesDataView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto event_data = dataManager()->getData<DigitalEventSeries>(_active_key);
        if (event_data) {
            // Convert view to vector for table model
            std::vector<TimeFrameIndex> event_vector;
            for (auto const & event : event_data->view()) {
                event_vector.push_back(event.time());
            }
            _table_model->setEvents(event_vector);
        } else {
            _table_model->setEvents({});
        }
    }
}

void DigitalEventSeriesDataView::_setupUi() {
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

void DigitalEventSeriesDataView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &DigitalEventSeriesDataView::_handleTableViewDoubleClicked);
}

void DigitalEventSeriesDataView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {

        auto tf = dataManager()->getData<DigitalEventSeries>(_active_key)->getTimeFrame();
        if (!tf) {
            std::cout << "DigitalEventSeriesDataView::_handleTableViewDoubleClicked: TimeFrame not found"
                      << _active_key << std::endl;
            return;
        }
        
        auto event = _table_model->getEvent(index.row());
        emit frameSelected(TimePosition(event, tf));
    }
}

void DigitalEventSeriesDataView::_onDataChanged() {
    updateView();
}
