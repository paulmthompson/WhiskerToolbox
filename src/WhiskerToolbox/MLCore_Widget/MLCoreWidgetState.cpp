#include "MLCoreWidgetState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

MLCoreWidgetState::MLCoreWidgetState(QObject * parent)
    : EditorState(parent) {
    _data.instance_id = getInstanceId().toStdString();
}

// === EditorState interface ===

QString MLCoreWidgetState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void MLCoreWidgetState::setDisplayName(QString const & name) {
    auto const name_std = name.toStdString();
    if (_data.display_name != name_std) {
        _data.display_name = name_std;
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string MLCoreWidgetState::toJson() const {
    auto data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool MLCoreWidgetState::fromJson(std::string const & json) {
    auto result = rfl::json::read<MLCoreWidgetStateData>(json);
    if (result) {
        _data = *result;
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        emit stateChanged();
        emit featureTensorKeyChanged(QString::fromStdString(_data.feature_tensor_key));
        emit trainingRegionKeyChanged(QString::fromStdString(_data.training_region_key));
        emit predictionRegionKeyChanged(QString::fromStdString(_data.prediction_region_key));
        emit labelSourceTypeChanged(QString::fromStdString(_data.label_source_type));
        emit labelIntervalKeyChanged(QString::fromStdString(_data.label_interval_key));
        emit labelGroupIdsChanged();
        emit selectedModelNameChanged(QString::fromStdString(_data.selected_model_name));
        emit modelParametersJsonChanged();
        emit outputPrefixChanged(QString::fromStdString(_data.output_prefix));
        emit probabilityThresholdChanged(_data.probability_threshold);
        emit outputProbabilitiesChanged(_data.output_probabilities);
        emit outputPredictionsChanged(_data.output_predictions);
        emit activeTabChanged(_data.active_tab);
        return true;
    }
    return false;
}

// === Feature configuration ===

void MLCoreWidgetState::setFeatureTensorKey(std::string const & key) {
    if (_data.feature_tensor_key != key) {
        _data.feature_tensor_key = key;
        markDirty();
        emit featureTensorKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::featureTensorKey() const {
    return _data.feature_tensor_key;
}

// === Region configuration ===

void MLCoreWidgetState::setTrainingRegionKey(std::string const & key) {
    if (_data.training_region_key != key) {
        _data.training_region_key = key;
        markDirty();
        emit trainingRegionKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::trainingRegionKey() const {
    return _data.training_region_key;
}

void MLCoreWidgetState::setPredictionRegionKey(std::string const & key) {
    if (_data.prediction_region_key != key) {
        _data.prediction_region_key = key;
        markDirty();
        emit predictionRegionKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::predictionRegionKey() const {
    return _data.prediction_region_key;
}

// === Label configuration ===

void MLCoreWidgetState::setLabelSourceType(std::string const & type) {
    if (_data.label_source_type != type) {
        _data.label_source_type = type;
        markDirty();
        emit labelSourceTypeChanged(QString::fromStdString(type));
    }
}

std::string const & MLCoreWidgetState::labelSourceType() const {
    return _data.label_source_type;
}

void MLCoreWidgetState::setLabelIntervalKey(std::string const & key) {
    if (_data.label_interval_key != key) {
        _data.label_interval_key = key;
        markDirty();
        emit labelIntervalKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::labelIntervalKey() const {
    return _data.label_interval_key;
}

void MLCoreWidgetState::setLabelGroupIds(std::vector<uint64_t> const & ids) {
    if (_data.label_group_ids != ids) {
        _data.label_group_ids = ids;
        markDirty();
        emit labelGroupIdsChanged();
    }
}

std::vector<uint64_t> const & MLCoreWidgetState::labelGroupIds() const {
    return _data.label_group_ids;
}

// === Model configuration ===

void MLCoreWidgetState::setSelectedModelName(std::string const & name) {
    if (_data.selected_model_name != name) {
        _data.selected_model_name = name;
        markDirty();
        emit selectedModelNameChanged(QString::fromStdString(name));
    }
}

std::string const & MLCoreWidgetState::selectedModelName() const {
    return _data.selected_model_name;
}

void MLCoreWidgetState::setModelParametersJson(std::string const & json) {
    if (_data.model_parameters_json != json) {
        _data.model_parameters_json = json;
        markDirty();
        emit modelParametersJsonChanged();
    }
}

std::string const & MLCoreWidgetState::modelParametersJson() const {
    return _data.model_parameters_json;
}

// === Output configuration ===

void MLCoreWidgetState::setOutputPrefix(std::string const & prefix) {
    if (_data.output_prefix != prefix) {
        _data.output_prefix = prefix;
        markDirty();
        emit outputPrefixChanged(QString::fromStdString(prefix));
    }
}

std::string const & MLCoreWidgetState::outputPrefix() const {
    return _data.output_prefix;
}

void MLCoreWidgetState::setProbabilityThreshold(double threshold) {
    if (_data.probability_threshold != threshold) {
        _data.probability_threshold = threshold;
        markDirty();
        emit probabilityThresholdChanged(threshold);
    }
}

double MLCoreWidgetState::probabilityThreshold() const {
    return _data.probability_threshold;
}

void MLCoreWidgetState::setOutputProbabilities(bool enabled) {
    if (_data.output_probabilities != enabled) {
        _data.output_probabilities = enabled;
        markDirty();
        emit outputProbabilitiesChanged(enabled);
    }
}

bool MLCoreWidgetState::outputProbabilities() const {
    return _data.output_probabilities;
}

void MLCoreWidgetState::setOutputPredictions(bool enabled) {
    if (_data.output_predictions != enabled) {
        _data.output_predictions = enabled;
        markDirty();
        emit outputPredictionsChanged(enabled);
    }
}

bool MLCoreWidgetState::outputPredictions() const {
    return _data.output_predictions;
}

// === UI state ===

void MLCoreWidgetState::setActiveTab(int tab) {
    if (_data.active_tab != tab) {
        _data.active_tab = tab;
        markDirty();
        emit activeTabChanged(tab);
    }
}

int MLCoreWidgetState::activeTab() const {
    return _data.active_tab;
}
