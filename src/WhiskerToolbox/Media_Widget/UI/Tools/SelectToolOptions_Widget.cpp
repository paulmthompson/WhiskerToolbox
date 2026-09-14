/**
 * @file SelectToolOptions_Widget.cpp
 * @brief Options controls for the Media Viewer Select tool
 */

#include "SelectToolOptions_Widget.hpp"

#include "Core/MediaWidgetState.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

#include <algorithm>
#include <vector>

namespace {

struct EnabledFeatureItem {
    QString key;
    QString data_type;
};

/**
 * @brief Collect all enabled feature keys across selectable data types
 * @param state Media widget state
 * @return Sorted list of enabled features
 */
[[nodiscard]] std::vector<EnabledFeatureItem> collectEnabledFeatures(MediaWidgetState const & state) {
    static QStringList const k_data_types{
            QStringLiteral("line"),
            QStringLiteral("mask"),
            QStringLiteral("point"),
    };

    std::vector<EnabledFeatureItem> items;
    for (QString const & data_type: k_data_types) {
        for (QString const & key: state.enabledFeatures(data_type)) {
            items.push_back({key, data_type});
        }
    }

    std::ranges::sort(items, [](EnabledFeatureItem const & a, EnabledFeatureItem const & b) {
        if (a.data_type != b.data_type) {
            return a.data_type < b.data_type;
        }
        return a.key < b.key;
    });

    return items;
}

QString formatFeatureLabel(QString const & data_type, QString const & key) {
    return QStringLiteral("%1 · %2").arg(data_type, key);
}

}// namespace

SelectToolOptions_Widget::SelectToolOptions_Widget(QWidget * parent)
    : QWidget(parent) {
    _buildUi();
}

void SelectToolOptions_Widget::setState(MediaWidgetState * state) {
    if (_state) {
        disconnect(_state, nullptr, this, nullptr);
    }

    _state = state;

    if (!_state) {
        return;
    }

    connect(_state, &MediaWidgetState::featureEnabledChanged,
            this, [this](QString const &, QString const &, bool) {
                _onEnabledFeaturesChanged();
            });
    connect(_state, &MediaWidgetState::displayOptionsChanged,
            this, [this](QString const &, QString const &) {
                _onEnabledFeaturesChanged();
            });
    connect(_state, &MediaWidgetState::displayOptionsRemoved,
            this, [this](QString const &, QString const &) {
                _onEnabledFeaturesChanged();
            });
    connect(_state, &MediaWidgetState::selectPrefsChanged,
            this, &SelectToolOptions_Widget::_syncFromState);

    _rebuildFilterCombo();
    _syncFromState();
}

void SelectToolOptions_Widget::_buildUi() {
    _layout = new QHBoxLayout(this);
    _layout->setContentsMargins(6, 2, 6, 2);
    _layout->setSpacing(6);

    _filter_label = new QLabel(tr("Selection filter:"), this);
    _filter_combo = new QComboBox(this);
    _filter_combo->setMinimumWidth(180);
    _filter_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _layout->addWidget(_filter_label);
    _layout->addWidget(_filter_combo, 1);

    connect(_filter_combo, &QComboBox::currentIndexChanged,
            this, &SelectToolOptions_Widget::_onFilterComboChanged);
}

void SelectToolOptions_Widget::_rebuildFilterCombo() {
    if (!_filter_combo) {
        return;
    }

    _filter_combo->blockSignals(true);
    _filter_combo->clear();
    _filter_combo->addItem(tr("All enabled features"));

    if (_state) {
        for (EnabledFeatureItem const & item: collectEnabledFeatures(*_state)) {
            QString const label = formatFeatureLabel(item.data_type, item.key);
            _filter_combo->addItem(label, QVariantList{item.key, item.data_type});
        }
    }

    int const target_index = _indexForCurrentFilter();
    _filter_combo->setCurrentIndex(target_index);
    _filter_combo->blockSignals(false);

    if (_state && _state->selectPrefs().filter_to_key && target_index == 0) {
        SelectToolPrefs prefs;
        prefs.filter_to_key = false;
        _state->setSelectPrefs(prefs);
    }
}

int SelectToolOptions_Widget::_indexForCurrentFilter() const {
    if (!_state || !_state->selectPrefs().filter_to_key) {
        return 0;
    }

    SelectToolPrefs const & prefs = _state->selectPrefs();
    QString const filter_key = QString::fromStdString(prefs.filter_key);
    QString const filter_type = QString::fromStdString(prefs.filter_data_type);

    for (int i = 1; i < _filter_combo->count(); ++i) {
        QVariantList const data = _filter_combo->itemData(i).toList();
        if (data.size() == 2 && data[0].toString() == filter_key && data[1].toString() == filter_type) {
            return i;
        }
    }

    return 0;
}

void SelectToolOptions_Widget::_onFilterComboChanged(int index) {
    if (_updating_from_state || !_state || !_filter_combo) {
        return;
    }

    SelectToolPrefs prefs = _state->selectPrefs();

    if (index <= 0) {
        prefs.filter_to_key = false;
        prefs.filter_key.clear();
        prefs.filter_data_type.clear();
    } else {
        QVariantList const data = _filter_combo->itemData(index).toList();
        if (data.size() != 2) {
            return;
        }

        prefs.filter_to_key = true;
        prefs.filter_key = data[0].toString().toStdString();
        prefs.filter_data_type = data[1].toString().toStdString();
    }

    _state->setSelectPrefs(prefs);
}

void SelectToolOptions_Widget::_onEnabledFeaturesChanged() {
    _rebuildFilterCombo();
}

void SelectToolOptions_Widget::_syncFromState() {
    if (!_state || !_filter_combo) {
        return;
    }

    _updating_from_state = true;
    int const target_index = _indexForCurrentFilter();
    if (_filter_combo->currentIndex() != target_index) {
        _filter_combo->setCurrentIndex(target_index);
    }
    _updating_from_state = false;
}
