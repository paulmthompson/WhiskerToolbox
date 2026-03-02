#include "TimeFrameInspector.hpp"

#include "TimeFrameDataView.hpp"

#include "DataInspector_Widget/Inspectors/GroupFilterHelper.hpp"

#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

TimeFrameInspector::TimeFrameInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
    _populateGroupFilterCombo();
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

void TimeFrameInspector::setDataView(TimeFrameDataView * view) {
    _data_view = view;
    if (_data_view && groupManager()) {
        _data_view->setGroupManager(groupManager());
    }
}

void TimeFrameInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    _info_label = new QLabel(QStringLiteral("No TimeFrame selected"), this);
    _info_label->setWordWrap(true);
    _info_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(_info_label);

    // Group filter section
    auto * filter_label = new QLabel(QStringLiteral("Filter by Group:"), this);
    layout->addWidget(filter_label);

    _group_filter_combo = new QComboBox(this);
    layout->addWidget(_group_filter_combo);

    layout->addStretch();
}

void TimeFrameInspector::_connectSignals() {
    connect(_group_filter_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TimeFrameInspector::_onGroupFilterChanged);

    connectGroupManagerSignals(groupManager(), this, &TimeFrameInspector::_onGroupChanged);
}

void TimeFrameInspector::_onGroupFilterChanged(int index) {
    if (!_data_view || !groupManager()) {
        return;
    }

    if (index == 0) {
        // "All Groups" selected
        _data_view->clearGroupFilter();
    } else {
        int group_id = _group_filter_combo->itemData(index).toInt();
        _data_view->setGroupFilter(group_id);
    }
}

void TimeFrameInspector::_onGroupChanged() {
    int current_index = _group_filter_combo->currentIndex();
    QString current_text;
    if (current_index >= 0 && current_index < _group_filter_combo->count()) {
        current_text = _group_filter_combo->currentText();
    }

    _populateGroupFilterCombo();
    restoreGroupFilterSelection(_group_filter_combo, current_index, current_text);
}

void TimeFrameInspector::_populateGroupFilterCombo() {
    populateGroupFilterCombo(_group_filter_combo, groupManager());
}
