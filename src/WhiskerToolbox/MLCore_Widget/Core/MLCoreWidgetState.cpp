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
        emit classificationZscoreNormalizeChanged(_data.classification_zscore_normalize);
        emit trainingRegionKeyChanged(QString::fromStdString(_data.training_region_key));
        emit predictionRegionKeyChanged(QString::fromStdString(_data.prediction_region_key));
        emit validationRegionKeyChanged(QString::fromStdString(_data.validation_region_key));
        emit labelSourceTypeChanged(QString::fromStdString(_data.label_source_type));
        emit labelIntervalKeyChanged(QString::fromStdString(_data.label_interval_key));
        emit labelPositiveClassNameChanged(QString::fromStdString(_data.label_positive_class_name));
        emit labelNegativeClassNameChanged(QString::fromStdString(_data.label_negative_class_name));
        emit labelGroupIdsChanged();
        emit labelDataKeyChanged(QString::fromStdString(_data.label_data_key));
        emit labelEventKeyChanged(QString::fromStdString(_data.label_event_key));
        emit selectedModelNameChanged(QString::fromStdString(_data.selected_model_name));
        emit modelParametersJsonChanged();
        emit balancingEnabledChanged(_data.balancing_enabled);
        emit balancingStrategyChanged(QString::fromStdString(_data.balancing_strategy));
        emit balancingMaxRatioChanged(_data.balancing_max_ratio);
        emit cvEnabledChanged(_data.cv_enabled);
        emit maxCvFoldsChanged(_data.max_cv_folds);
        emit outputPrefixChanged(QString::fromStdString(_data.output_prefix));
        emit probabilityThresholdChanged(_data.probability_threshold);
        emit outputProbabilitiesChanged(_data.output_probabilities);
        emit outputPredictionsChanged(_data.output_predictions);
        emit constrainedDecodingChanged(_data.constrained_decoding);
        emit hmmDiagonalCovarianceChanged(_data.hmm_diagonal_covariance);
        emit hmmGMMEmissionsChanged(_data.hmm_gmm_emissions);
        emit hmmNumGaussiansChanged(_data.hmm_num_gaussians);
        emit activeTabChanged(_data.active_tab);
        emit clusteringTensorKeyChanged(QString::fromStdString(_data.clustering_tensor_key));
        emit clusteringModelNameChanged(QString::fromStdString(_data.clustering_model_name));
        emit clusteringOutputPrefixChanged(QString::fromStdString(_data.clustering_output_prefix));
        emit clusteringWriteIntervalsChanged(_data.clustering_write_intervals);
        emit clusteringWriteProbabilitiesChanged(_data.clustering_write_probabilities);
        emit clusteringZscoreNormalizeChanged(_data.clustering_zscore_normalize);
        emit dimReductionTensorKeyChanged(QString::fromStdString(_data.dim_reduction_tensor_key));
        emit dimReductionModelNameChanged(QString::fromStdString(_data.dim_reduction_model_name));
        emit dimReductionOutputKeyChanged(QString::fromStdString(_data.dim_reduction_output_key));
        emit dimReductionNComponentsChanged(_data.dim_reduction_n_components);
        emit dimReductionZscoreNormalizeChanged(_data.dim_reduction_zscore_normalize);
        emit dimReductionSupervisedChanged(_data.dim_reduction_supervised);
        emit dimReductionLabelSourceTypeChanged(QString::fromStdString(_data.dim_reduction_label_source_type));
        emit dimReductionLabelIntervalKeyChanged(QString::fromStdString(_data.dim_reduction_label_interval_key));
        emit dimReductionLabelPositiveClassChanged(QString::fromStdString(_data.dim_reduction_label_positive_class));
        emit dimReductionLabelNegativeClassChanged(QString::fromStdString(_data.dim_reduction_label_negative_class));
        emit dimReductionLabelEventKeyChanged(QString::fromStdString(_data.dim_reduction_label_event_key));
        emit dimReductionLabelGroupIdsChanged();
        emit dimReductionLabelDataKeyChanged(QString::fromStdString(_data.dim_reduction_label_data_key));
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

void MLCoreWidgetState::setValidationRegionKey(std::string const & key) {
    if (_data.validation_region_key != key) {
        _data.validation_region_key = key;
        markDirty();
        emit validationRegionKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::validationRegionKey() const {
    return _data.validation_region_key;
}

void MLCoreWidgetState::setConstrainedDecoding(bool enabled) {
    if (_data.constrained_decoding != enabled) {
        _data.constrained_decoding = enabled;
        markDirty();
        emit constrainedDecodingChanged(enabled);
    }
}

bool MLCoreWidgetState::constrainedDecoding() const {
    return _data.constrained_decoding;
}

void MLCoreWidgetState::setHmmDiagonalCovariance(bool enabled) {
    if (_data.hmm_diagonal_covariance != enabled) {
        _data.hmm_diagonal_covariance = enabled;
        markDirty();
        emit hmmDiagonalCovarianceChanged(enabled);
    }
}

bool MLCoreWidgetState::hmmDiagonalCovariance() const {
    return _data.hmm_diagonal_covariance;
}

void MLCoreWidgetState::setHmmGMMEmissions(bool enabled) {
    if (_data.hmm_gmm_emissions != enabled) {
        _data.hmm_gmm_emissions = enabled;
        markDirty();
        emit hmmGMMEmissionsChanged(enabled);
    }
}

bool MLCoreWidgetState::hmmGMMEmissions() const {
    return _data.hmm_gmm_emissions;
}

void MLCoreWidgetState::setHmmNumGaussians(int n) {
    if (_data.hmm_num_gaussians != n) {
        _data.hmm_num_gaussians = n;
        markDirty();
        emit hmmNumGaussiansChanged(n);
    }
}

int MLCoreWidgetState::hmmNumGaussians() const {
    return _data.hmm_num_gaussians;
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

void MLCoreWidgetState::setLabelPositiveClassName(std::string const & name) {
    if (_data.label_positive_class_name != name) {
        _data.label_positive_class_name = name;
        markDirty();
        emit labelPositiveClassNameChanged(QString::fromStdString(name));
    }
}

std::string const & MLCoreWidgetState::labelPositiveClassName() const {
    return _data.label_positive_class_name;
}

void MLCoreWidgetState::setLabelNegativeClassName(std::string const & name) {
    if (_data.label_negative_class_name != name) {
        _data.label_negative_class_name = name;
        markDirty();
        emit labelNegativeClassNameChanged(QString::fromStdString(name));
    }
}

std::string const & MLCoreWidgetState::labelNegativeClassName() const {
    return _data.label_negative_class_name;
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

void MLCoreWidgetState::setLabelDataKey(std::string const & key) {
    if (_data.label_data_key != key) {
        _data.label_data_key = key;
        markDirty();
        emit labelDataKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::labelDataKey() const {
    return _data.label_data_key;
}

void MLCoreWidgetState::setLabelEventKey(std::string const & key) {
    if (_data.label_event_key != key) {
        _data.label_event_key = key;
        markDirty();
        emit labelEventKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::labelEventKey() const {
    return _data.label_event_key;
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

// === Balancing configuration ===

void MLCoreWidgetState::setBalancingEnabled(bool enabled) {
    if (_data.balancing_enabled != enabled) {
        _data.balancing_enabled = enabled;
        markDirty();
        emit balancingEnabledChanged(enabled);
    }
}

bool MLCoreWidgetState::balancingEnabled() const {
    return _data.balancing_enabled;
}

void MLCoreWidgetState::setBalancingStrategy(std::string const & strategy) {
    if (_data.balancing_strategy != strategy) {
        _data.balancing_strategy = strategy;
        markDirty();
        emit balancingStrategyChanged(QString::fromStdString(strategy));
    }
}

std::string const & MLCoreWidgetState::balancingStrategy() const {
    return _data.balancing_strategy;
}

void MLCoreWidgetState::setBalancingMaxRatio(double ratio) {
    if (_data.balancing_max_ratio != ratio) {
        _data.balancing_max_ratio = ratio;
        markDirty();
        emit balancingMaxRatioChanged(ratio);
    }
}

double MLCoreWidgetState::balancingMaxRatio() const {
    return _data.balancing_max_ratio;
}

// === Cross-validation configuration ===

void MLCoreWidgetState::setCvEnabled(bool enabled) {
    if (_data.cv_enabled != enabled) {
        _data.cv_enabled = enabled;
        markDirty();
        emit cvEnabledChanged(enabled);
    }
}

bool MLCoreWidgetState::cvEnabled() const {
    return _data.cv_enabled;
}

void MLCoreWidgetState::setMaxCvFolds(int folds) {
    if (_data.max_cv_folds != folds) {
        _data.max_cv_folds = folds;
        markDirty();
        emit maxCvFoldsChanged(folds);
    }
}

int MLCoreWidgetState::maxCvFolds() const {
    return _data.max_cv_folds;
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

// === Clustering configuration ===

void MLCoreWidgetState::setClusteringTensorKey(std::string const & key) {
    if (_data.clustering_tensor_key != key) {
        _data.clustering_tensor_key = key;
        markDirty();
        emit clusteringTensorKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::clusteringTensorKey() const {
    return _data.clustering_tensor_key;
}

void MLCoreWidgetState::setClusteringModelName(std::string const & name) {
    if (_data.clustering_model_name != name) {
        _data.clustering_model_name = name;
        markDirty();
        emit clusteringModelNameChanged(QString::fromStdString(name));
    }
}

std::string const & MLCoreWidgetState::clusteringModelName() const {
    return _data.clustering_model_name;
}

void MLCoreWidgetState::setClusteringOutputPrefix(std::string const & prefix) {
    if (_data.clustering_output_prefix != prefix) {
        _data.clustering_output_prefix = prefix;
        markDirty();
        emit clusteringOutputPrefixChanged(QString::fromStdString(prefix));
    }
}

std::string const & MLCoreWidgetState::clusteringOutputPrefix() const {
    return _data.clustering_output_prefix;
}

void MLCoreWidgetState::setClusteringWriteIntervals(bool enabled) {
    if (_data.clustering_write_intervals != enabled) {
        _data.clustering_write_intervals = enabled;
        markDirty();
        emit clusteringWriteIntervalsChanged(enabled);
    }
}

bool MLCoreWidgetState::clusteringWriteIntervals() const {
    return _data.clustering_write_intervals;
}

void MLCoreWidgetState::setClusteringWriteProbabilities(bool enabled) {
    if (_data.clustering_write_probabilities != enabled) {
        _data.clustering_write_probabilities = enabled;
        markDirty();
        emit clusteringWriteProbabilitiesChanged(enabled);
    }
}

bool MLCoreWidgetState::clusteringWriteProbabilities() const {
    return _data.clustering_write_probabilities;
}

void MLCoreWidgetState::setClassificationZscoreNormalize(bool enabled) {
    if (_data.classification_zscore_normalize != enabled) {
        _data.classification_zscore_normalize = enabled;
        markDirty();
        emit classificationZscoreNormalizeChanged(enabled);
    }
}

bool MLCoreWidgetState::classificationZscoreNormalize() const {
    return _data.classification_zscore_normalize;
}

void MLCoreWidgetState::setClusteringZscoreNormalize(bool enabled) {
    if (_data.clustering_zscore_normalize != enabled) {
        _data.clustering_zscore_normalize = enabled;
        markDirty();
        emit clusteringZscoreNormalizeChanged(enabled);
    }
}

bool MLCoreWidgetState::clusteringZscoreNormalize() const {
    return _data.clustering_zscore_normalize;
}

// === Dim Reduction configuration ===

void MLCoreWidgetState::setDimReductionTensorKey(std::string const & key) {
    if (_data.dim_reduction_tensor_key != key) {
        _data.dim_reduction_tensor_key = key;
        markDirty();
        emit dimReductionTensorKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::dimReductionTensorKey() const {
    return _data.dim_reduction_tensor_key;
}

void MLCoreWidgetState::setDimReductionModelName(std::string const & name) {
    if (_data.dim_reduction_model_name != name) {
        _data.dim_reduction_model_name = name;
        markDirty();
        emit dimReductionModelNameChanged(QString::fromStdString(name));
    }
}

std::string const & MLCoreWidgetState::dimReductionModelName() const {
    return _data.dim_reduction_model_name;
}

void MLCoreWidgetState::setDimReductionOutputKey(std::string const & key) {
    if (_data.dim_reduction_output_key != key) {
        _data.dim_reduction_output_key = key;
        markDirty();
        emit dimReductionOutputKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::dimReductionOutputKey() const {
    return _data.dim_reduction_output_key;
}

void MLCoreWidgetState::setDimReductionNComponents(int n) {
    if (_data.dim_reduction_n_components != n) {
        _data.dim_reduction_n_components = n;
        markDirty();
        emit dimReductionNComponentsChanged(n);
    }
}

int MLCoreWidgetState::dimReductionNComponents() const {
    return _data.dim_reduction_n_components;
}

void MLCoreWidgetState::setDimReductionZscoreNormalize(bool enabled) {
    if (_data.dim_reduction_zscore_normalize != enabled) {
        _data.dim_reduction_zscore_normalize = enabled;
        markDirty();
        emit dimReductionZscoreNormalizeChanged(enabled);
    }
}

bool MLCoreWidgetState::dimReductionZscoreNormalize() const {
    return _data.dim_reduction_zscore_normalize;
}

// === Dim Reduction supervised configuration ===

void MLCoreWidgetState::setDimReductionSupervised(bool enabled) {
    if (_data.dim_reduction_supervised != enabled) {
        _data.dim_reduction_supervised = enabled;
        markDirty();
        emit dimReductionSupervisedChanged(enabled);
    }
}

bool MLCoreWidgetState::dimReductionSupervised() const {
    return _data.dim_reduction_supervised;
}

void MLCoreWidgetState::setDimReductionLabelSourceType(std::string const & type) {
    if (_data.dim_reduction_label_source_type != type) {
        _data.dim_reduction_label_source_type = type;
        markDirty();
        emit dimReductionLabelSourceTypeChanged(QString::fromStdString(type));
    }
}

std::string const & MLCoreWidgetState::dimReductionLabelSourceType() const {
    return _data.dim_reduction_label_source_type;
}

void MLCoreWidgetState::setDimReductionLabelIntervalKey(std::string const & key) {
    if (_data.dim_reduction_label_interval_key != key) {
        _data.dim_reduction_label_interval_key = key;
        markDirty();
        emit dimReductionLabelIntervalKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::dimReductionLabelIntervalKey() const {
    return _data.dim_reduction_label_interval_key;
}

void MLCoreWidgetState::setDimReductionLabelPositiveClass(std::string const & name) {
    if (_data.dim_reduction_label_positive_class != name) {
        _data.dim_reduction_label_positive_class = name;
        markDirty();
        emit dimReductionLabelPositiveClassChanged(QString::fromStdString(name));
    }
}

std::string const & MLCoreWidgetState::dimReductionLabelPositiveClass() const {
    return _data.dim_reduction_label_positive_class;
}

void MLCoreWidgetState::setDimReductionLabelNegativeClass(std::string const & name) {
    if (_data.dim_reduction_label_negative_class != name) {
        _data.dim_reduction_label_negative_class = name;
        markDirty();
        emit dimReductionLabelNegativeClassChanged(QString::fromStdString(name));
    }
}

std::string const & MLCoreWidgetState::dimReductionLabelNegativeClass() const {
    return _data.dim_reduction_label_negative_class;
}

void MLCoreWidgetState::setDimReductionLabelEventKey(std::string const & key) {
    if (_data.dim_reduction_label_event_key != key) {
        _data.dim_reduction_label_event_key = key;
        markDirty();
        emit dimReductionLabelEventKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::dimReductionLabelEventKey() const {
    return _data.dim_reduction_label_event_key;
}

void MLCoreWidgetState::setDimReductionLabelGroupIds(std::vector<uint64_t> const & ids) {
    if (_data.dim_reduction_label_group_ids != ids) {
        _data.dim_reduction_label_group_ids = ids;
        markDirty();
        emit dimReductionLabelGroupIdsChanged();
    }
}

std::vector<uint64_t> const & MLCoreWidgetState::dimReductionLabelGroupIds() const {
    return _data.dim_reduction_label_group_ids;
}

void MLCoreWidgetState::setDimReductionLabelDataKey(std::string const & key) {
    if (_data.dim_reduction_label_data_key != key) {
        _data.dim_reduction_label_data_key = key;
        markDirty();
        emit dimReductionLabelDataKeyChanged(QString::fromStdString(key));
    }
}

std::string const & MLCoreWidgetState::dimReductionLabelDataKey() const {
    return _data.dim_reduction_label_data_key;
}
