#include "TimeFrameInspector.hpp"

#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QLabel>
#include <QVBoxLayout>

TimeFrameInspector::TimeFrameInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
}

TimeFrameInspector::~TimeFrameInspector() {
    removeCallbacks();
}

void TimeFrameInspector::setActiveKey(std::string const & key) {
    if (_active_key == key) {
        return;
    }

    removeCallbacks();
    _active_key = key;
    updateView();
}

void TimeFrameInspector::removeCallbacks() {
    // TimeFrame objects don't currently support per-object callbacks
    // so nothing to clean up
}

void TimeFrameInspector::updateView() {
    if (_active_key.empty() || !_info_label) {
        if (_info_label) {
            _info_label->setText(QStringLiteral("No TimeFrame selected"));
        }
        return;
    }

    auto timeframe = dataManager()->getTime(TimeKey(_active_key));
    if (timeframe) {
        int const sample_count = timeframe->getTotalFrameCount();
        _info_label->setText(
            QStringLiteral("TimeFrame: %1\nSamples: %2")
                .arg(QString::fromStdString(_active_key))
                .arg(sample_count));
    } else {
        _info_label->setText(
            QStringLiteral("TimeFrame not found: %1")
                .arg(QString::fromStdString(_active_key)));
    }
}

void TimeFrameInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    _info_label = new QLabel(QStringLiteral("No TimeFrame selected"), this);
    _info_label->setWordWrap(true);
    _info_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(_info_label);

    layout->addStretch();
}
