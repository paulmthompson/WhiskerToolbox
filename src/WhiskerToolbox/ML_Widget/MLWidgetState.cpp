#include "MLWidgetState.hpp"

#include <algorithm>
#include <iostream>

MLWidgetState::MLWidgetState(QObject * parent)
    : EditorState(parent)
{
    _data.instance_id = getInstanceId().toStdString();
}

QString MLWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void MLWidgetState::setDisplayName(QString const & name) {
    if (_data.display_name != name.toStdString()) {
        _data.display_name = name.toStdString();
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string MLWidgetState::toJson() const {
    return rfl::json::write(_data);
}

bool MLWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<MLWidgetStateData>(json);
    if (result) {
        _data = *result;
        // Restore instance ID
        setInstanceId(QString::fromStdString(_data.instance_id));
        emit stateChanged();
        return true;
    }
    std::cerr << "MLWidgetState::fromJson: Failed to parse JSON" << std::endl;
    return false;
}

// === Table Selection ===

QString MLWidgetState::selectedTableId() const {
    return QString::fromStdString(_data.selected_table_id);
}

void MLWidgetState::setSelectedTableId(QString const & table_id) {
    if (_data.selected_table_id != table_id.toStdString()) {
        _data.selected_table_id = table_id.toStdString();
        markDirty();
        emit selectedTableIdChanged(table_id);
    }
}

void MLWidgetState::setSelectedFeatureColumns(std::vector<std::string> const & columns) {
    if (_data.selected_feature_columns != columns) {
        _data.selected_feature_columns = columns;
        markDirty();
        emit selectedFeatureColumnsChanged(columns);
    }
}

void MLWidgetState::setSelectedMaskColumns(std::vector<std::string> const & columns) {
    if (_data.selected_mask_columns != columns) {
        _data.selected_mask_columns = columns;
        markDirty();
        emit selectedMaskColumnsChanged(columns);
    }
}

QString MLWidgetState::selectedLabelColumn() const {
    return QString::fromStdString(_data.selected_label_column);
}

void MLWidgetState::setSelectedLabelColumn(QString const & column) {
    if (_data.selected_label_column != column.toStdString()) {
        _data.selected_label_column = column.toStdString();
        markDirty();
        emit selectedLabelColumnChanged(column);
    }
}

// === Training Configuration ===

QString MLWidgetState::trainingIntervalKey() const {
    return QString::fromStdString(_data.training_interval_key);
}

void MLWidgetState::setTrainingIntervalKey(QString const & key) {
    if (_data.training_interval_key != key.toStdString()) {
        _data.training_interval_key = key.toStdString();
        markDirty();
        emit trainingIntervalKeyChanged(key);
    }
}

QString MLWidgetState::selectedModelType() const {
    return QString::fromStdString(_data.selected_model_type);
}

void MLWidgetState::setSelectedModelType(QString const & model_type) {
    if (_data.selected_model_type != model_type.toStdString()) {
        _data.selected_model_type = model_type.toStdString();
        markDirty();
        emit selectedModelTypeChanged(model_type);
    }
}

// === Outcomes ===

void MLWidgetState::setSelectedOutcomes(std::vector<std::string> const & outcomes) {
    if (_data.selected_outcomes != outcomes) {
        _data.selected_outcomes = outcomes;
        markDirty();
        emit selectedOutcomesChanged(outcomes);
    }
}

void MLWidgetState::addSelectedOutcome(std::string const & outcome) {
    auto it = std::find(_data.selected_outcomes.begin(), _data.selected_outcomes.end(), outcome);
    if (it == _data.selected_outcomes.end()) {
        _data.selected_outcomes.push_back(outcome);
        markDirty();
        emit selectedOutcomesChanged(_data.selected_outcomes);
    }
}

void MLWidgetState::removeSelectedOutcome(std::string const & outcome) {
    auto it = std::find(_data.selected_outcomes.begin(), _data.selected_outcomes.end(), outcome);
    if (it != _data.selected_outcomes.end()) {
        _data.selected_outcomes.erase(it);
        markDirty();
        emit selectedOutcomesChanged(_data.selected_outcomes);
    }
}
