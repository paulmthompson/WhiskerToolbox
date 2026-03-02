#include "TimeFrameDataView.hpp"

#include "TimeFrameTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>

TimeFrameDataView::TimeFrameDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new TimeFrameTableModel(this)) {
    _setupUi();
}

TimeFrameDataView::~TimeFrameDataView() {
    removeCallbacks();
}

void TimeFrameDataView::setActiveKey(std::string const & key) {
    if (_active_key == key) {
        return;
    }

    removeCallbacks();
    _active_key = key;
    updateView();
}

void TimeFrameDataView::removeCallbacks() {
    // TimeFrame objects don't currently support per-object callbacks
}

void TimeFrameDataView::updateView() {
    if (_active_key.empty() || !_table_model) {
        _table_model->setTimeValues({});
        _info_label->setText(QStringLiteral("No TimeFrame selected"));
        _info_label->setVisible(true);
        _table_view->setVisible(false);
        return;
    }

    auto timeframe = dataManager()->getTime(TimeKey(_active_key));
    if (!timeframe) {
        _table_model->setTimeValues({});
        _info_label->setText(
            QStringLiteral("TimeFrame not found: %1")
                .arg(QString::fromStdString(_active_key)));
        _info_label->setVisible(true);
        _table_view->setVisible(false);
        return;
    }

    // Build the time values vector by iterating through all indices
    auto const total = timeframe->getTotalFrameCount();
    std::vector<int> times;
    times.reserve(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i) {
        times.push_back(timeframe->getTimeAtIndex(TimeFrameIndex(i)));
    }

    _table_model->setTimeValues(times);

    _info_label->setText(
        QStringLiteral("TimeFrame: %1  |  %2 entries")
            .arg(QString::fromStdString(_active_key))
            .arg(total));
    _info_label->setVisible(true);
    _table_view->setVisible(true);
}

void TimeFrameDataView::_setupUi() {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(2);

    _info_label = new QLabel(QStringLiteral("No TimeFrame selected"), this);
    _info_label->setWordWrap(true);
    _info_label->setContentsMargins(4, 4, 4, 4);
    _layout->addWidget(_info_label);

    _table_view = new QTableView(this);
    _table_view->setModel(_table_model);
    _table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table_view->setAlternatingRowColors(true);
    _table_view->setSortingEnabled(true);
    _table_view->horizontalHeader()->setStretchLastSection(true);
    _table_view->setVisible(false);

    _layout->addWidget(_table_view);
}
