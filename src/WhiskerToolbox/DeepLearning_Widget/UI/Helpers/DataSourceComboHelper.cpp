/// @file DataSourceComboHelper.cpp
/// @brief Implementation of centralized DataManager-aware QComboBox helper.

#include "DataSourceComboHelper.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"

#include <QComboBox>

#include <cassert>

namespace dl::widget {

DataSourceComboHelper::DataSourceComboHelper(
        std::shared_ptr<DataManager> dm,
        QObject * parent)
    : QObject(parent),
      _dm(std::move(dm)) {
    assert(_dm && "DataSourceComboHelper: DataManager must not be null");
}

DataSourceComboHelper::~DataSourceComboHelper() = default;

// ── One-shot population ────────────────────────────────────────────────────

void DataSourceComboHelper::populateCombo(
        QComboBox * combo,
        std::vector<DM_DataType> const & types) const {

    assert(combo && "populateCombo: combo must not be null");

    QString const current = combo->currentText();
    combo->clear();
    combo->addItem(tr("(None)"));

    if (!_dm) return;

    auto const keys = keysForTypes(types);
    for (auto const & key: keys) {
        combo->addItem(QString::fromStdString(key));
    }

    int const idx = combo->findText(current);
    if (idx >= 0) {
        combo->setCurrentIndex(idx);
    }
}

void DataSourceComboHelper::populateCombo(
        QComboBox * combo, DM_DataType type) const {
    populateCombo(combo, std::vector<DM_DataType>{type});
}

// ── Tracking for auto-refresh ──────────────────────────────────────────────

void DataSourceComboHelper::track(
        QComboBox * combo,
        std::vector<DM_DataType> const & types) {

    assert(combo && "track: combo must not be null");

    // Replace existing entry if already tracked
    for (auto & entry: _tracked) {
        if (entry.combo == combo) {
            entry.types = types;
            return;
        }
    }

    _tracked.push_back({combo, types});

    // Auto-untrack when the combo is destroyed
    connect(combo, &QObject::destroyed, this, [this, combo]() {
        untrack(combo);
    });
}

void DataSourceComboHelper::track(QComboBox * combo, DM_DataType type) {
    track(combo, std::vector<DM_DataType>{type});
}

void DataSourceComboHelper::untrack(QComboBox * combo) {
    std::erase_if(_tracked, [combo](TrackedCombo const & tc) {
        return tc.combo == combo;
    });
}

void DataSourceComboHelper::refreshAll() {
    for (auto const & entry: _tracked) {
        populateCombo(entry.combo, entry.types);
    }
}

int DataSourceComboHelper::trackedCount() const {
    return static_cast<int>(_tracked.size());
}

// ── Utility ────────────────────────────────────────────────────────────────

std::vector<DM_DataType>
DataSourceComboHelper::typesFromHint(std::string const & type_hint) {

    if (type_hint == "MediaData") return {DM_DataType::Video, DM_DataType::Images};
    if (type_hint == "PointData") return {DM_DataType::Points};
    if (type_hint == "MaskData") return {DM_DataType::Mask};
    if (type_hint == "LineData") return {DM_DataType::Line};
    if (type_hint == "AnalogTimeSeries") return {DM_DataType::Analog};
    if (type_hint == "TensorData") return {DM_DataType::Tensor};

    return {};// empty = all keys
}

// ── Private helpers ────────────────────────────────────────────────────────

std::vector<std::string>
DataSourceComboHelper::keysForTypes(
        std::vector<DM_DataType> const & types) const {
    if (!_dm) return {};
    return getKeysForTypes(*_dm, types);
}

}// namespace dl::widget
