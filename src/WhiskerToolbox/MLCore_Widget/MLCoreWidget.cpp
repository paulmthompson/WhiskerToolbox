#include "MLCoreWidget.hpp"

#include "Core/MLCoreWidgetState.hpp"
#include "UI/ClusterOutputPanel/ClusterOutputPanel.hpp"
#include "UI/ClusteringPanel/ClusteringPanel.hpp"
#include "UI/DimReductionPanel/DimReductionPanel.hpp"
#include "UI/FeatureSelectionPanel/FeatureSelectionPanel.hpp"
#include "UI/LabelConfigPanel/LabelConfigPanel.hpp"
#include "UI/ModelConfigPanel/ModelConfigPanel.hpp"
#include "UI/PredictionPanel/PredictionPanel.hpp"
#include "UI/RegionSelectionPanel/RegionSelectionPanel.hpp"
#include "UI/ResultsPanel/ResultsPanel.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/SelectionContext.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLModelRegistry.hpp"
#include "MLCore/models/supervised/HiddenMarkovModelOperation.hpp"
#include "MLCore/pipelines/ClassificationPipeline.hpp"
#include "MLCore/pipelines/ClusteringPipeline.hpp"
#include "MLCore/pipelines/DimReductionPipeline.hpp"
#include "MLCore/pipelines/SupervisedDimReductionPipeline.hpp"
#include "MLCore/preprocessing/ClassBalancing.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

// =============================================================================
// Pipeline worker thread
// =============================================================================

namespace {

/**
 * @brief Worker that runs the ClassificationPipeline on a background thread
 *
 * Owns the pipeline config and result. Emits progress via a signal connected
 * to the main thread with Qt::QueuedConnection (handled by MLCoreWidget).
 */
class PipelineWorker : public QThread {
    Q_OBJECT

public:
    PipelineWorker(DataManager * dm,
                   MLCore::MLModelRegistry const * registry,
                   MLCore::ClassificationPipelineConfig config,
                   QObject * parent = nullptr)
        : QThread(parent),
          _dm(dm),
          _registry(registry),
          _config(std::move(config)) {}

    [[nodiscard]] MLCore::ClassificationPipelineResult takeResult() {
        return std::move(_result);
    }

signals:
    void progressUpdate(int _t1, QString _t2);

protected:
    void run() override {
        auto progress_cb = [this](MLCore::ClassificationStage stage,
                                  std::string const & msg) {
            emit progressUpdate(static_cast<int>(stage), QString::fromStdString(msg));
        };

        _result = MLCore::runClassificationPipeline(*_dm, *_registry, _config, progress_cb);
    }

private:
    DataManager * _dm;
    MLCore::MLModelRegistry const * _registry;
    MLCore::ClassificationPipelineConfig _config;
    MLCore::ClassificationPipelineResult _result;
};

/**
 * @brief Worker that runs the ClusteringPipeline on a background thread
 *
 * Owns the pipeline config and result. Emits progress via a signal connected
 * to the main thread with Qt::QueuedConnection (handled by MLCoreWidget).
 */
class ClusteringPipelineWorker : public QThread {
    Q_OBJECT

public:
    ClusteringPipelineWorker(DataManager * dm,
                             MLCore::MLModelRegistry const * registry,
                             MLCore::ClusteringPipelineConfig config,
                             QObject * parent = nullptr)
        : QThread(parent),
          _dm(dm),
          _registry(registry),
          _config(std::move(config)) {}

    [[nodiscard]] MLCore::ClusteringPipelineResult takeResult() {
        return std::move(_result);
    }

signals:
    void progressUpdate(int _t1, QString _t2);

protected:
    void run() override {
        auto progress_cb = [this](MLCore::ClusteringStage stage,
                                  std::string const & msg) {
            emit progressUpdate(static_cast<int>(stage), QString::fromStdString(msg));
        };

        _result = MLCore::runClusteringPipeline(*_dm, *_registry, _config, progress_cb);
    }

private:
    DataManager * _dm;
    MLCore::MLModelRegistry const * _registry;
    MLCore::ClusteringPipelineConfig _config;
    MLCore::ClusteringPipelineResult _result;
};

/**
 * @brief Worker that runs the DimReductionPipeline on a background thread
 *
 * Owns the pipeline config and result. Emits progress via a signal connected
 * to the main thread with Qt::QueuedConnection (handled by MLCoreWidget).
 */
class DimReductionPipelineWorker : public QThread {
    Q_OBJECT

public:
    DimReductionPipelineWorker(DataManager * dm,
                               MLCore::MLModelRegistry const * registry,
                               MLCore::DimReductionPipelineConfig config,
                               QObject * parent = nullptr)
        : QThread(parent),
          _dm(dm),
          _registry(registry),
          _config(std::move(config)) {}

    [[nodiscard]] MLCore::DimReductionPipelineResult takeResult() {
        return std::move(_result);
    }

signals:
    void progressUpdate(int _t1, QString _t2);

protected:
    void run() override {
        auto progress_cb = [this](MLCore::DimReductionStage stage,
                                  std::string const & msg) {
            emit progressUpdate(static_cast<int>(stage), QString::fromStdString(msg));
        };

        _result = MLCore::runDimReductionPipeline(*_dm, *_registry, _config, progress_cb);
    }

private:
    DataManager * _dm;
    MLCore::MLModelRegistry const * _registry;
    MLCore::DimReductionPipelineConfig _config;
    MLCore::DimReductionPipelineResult _result;
};

/**
 * @brief Worker that runs the SupervisedDimReductionPipeline on a background thread
 *
 * Owns the pipeline config and result. Emits progress via a signal connected
 * to the main thread with Qt::QueuedConnection (handled by MLCoreWidget).
 */
class SupervisedDimReductionPipelineWorker : public QThread {
    Q_OBJECT

public:
    SupervisedDimReductionPipelineWorker(
            DataManager * dm,
            MLCore::MLModelRegistry const * registry,
            MLCore::SupervisedDimReductionPipelineConfig config,
            QObject * parent = nullptr)
        : QThread(parent),
          _dm(dm),
          _registry(registry),
          _config(std::move(config)) {}

    [[nodiscard]] MLCore::SupervisedDimReductionPipelineResult takeResult() {
        return std::move(_result);
    }

signals:
    void progressUpdate(int _t1, QString _t2);

protected:
    void run() override {
        auto progress_cb = [this](MLCore::SupervisedDimReductionStage stage,
                                  std::string const & msg) {
            emit progressUpdate(static_cast<int>(stage), QString::fromStdString(msg));
        };

        _result = MLCore::runSupervisedDimReductionPipeline(
                *_dm, *_registry, _config, progress_cb);
    }

private:
    DataManager * _dm;
    MLCore::MLModelRegistry const * _registry;
    MLCore::SupervisedDimReductionPipelineConfig _config;
    MLCore::SupervisedDimReductionPipelineResult _result;
};

}// namespace

// Need to include the moc for the anonymous-namespace QObject subclass
#include "MLCoreWidget.moc"

// =============================================================================
// Construction / destruction
// =============================================================================

MLCoreWidget::MLCoreWidget(std::shared_ptr<MLCoreWidgetState> state,
                           std::shared_ptr<DataManager> data_manager,
                           SelectionContext * selection_context,
                           GroupManager * group_manager,
                           QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _selection_context(selection_context),
      _group_manager(group_manager),
      _registry(std::make_unique<MLCore::MLModelRegistry>()) {
    _setupUi();
    _connectSignals();

    // Set initial visibility of constrained decoding based on restored model
    if (_state) {
        auto temp_model = _registry->create(_state->selectedModelName());
        _prediction_panel->setSequenceModelActive(
                temp_model && temp_model->isSequenceModel());
    }

    // Connect to SelectionContext for passive data focus awareness (task 4.9)
    if (_selection_context) {
        connectToSelectionContext(_selection_context, this);
    }
}

MLCoreWidget::~MLCoreWidget() = default;

// =============================================================================
// DataFocusAware — passive awareness (task 4.9)
// =============================================================================

void MLCoreWidget::onDataFocusChanged(EditorLib::SelectedDataKey const & data_key,
                                      QString const & data_type) {
    if (!_state || !_data_manager) {
        return;
    }

    QString const & key_str = data_key.toString();
    if (key_str.isEmpty()) {
        return;
    }

    // When a TensorData key is focused elsewhere, auto-select it in
    // the feature panel so the user can quickly switch feature matrices.
    if (data_type == QStringLiteral("TensorData")) {
        _state->setFeatureTensorKey(key_str.toStdString());
    }
}

// =============================================================================
// UI setup
// =============================================================================

void MLCoreWidget::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto * tabs = new QTabWidget(this);

    // Classification tab
    auto * classification_tab = new QWidget();
    auto * classification_layout = new QVBoxLayout(classification_tab);

    // Feature selection panel
    _feature_panel = new FeatureSelectionPanel(_state, _data_manager, classification_tab);
    classification_layout->addWidget(_feature_panel);

    // Training region panel
    _training_region_panel = new RegionSelectionPanel(
            _state, _data_manager, RegionMode::Training, classification_tab);
    classification_layout->addWidget(_training_region_panel);

    // Label configuration panel
    _label_panel = new LabelConfigPanel(_state, _data_manager, classification_tab);
    classification_layout->addWidget(_label_panel);

    // Model configuration panel
    _model_config_panel = new ModelConfigPanel(_state, _data_manager, classification_tab);
    classification_layout->addWidget(_model_config_panel);

    // Prediction panel
    _prediction_panel = new PredictionPanel(_state, _data_manager, classification_tab);
    classification_layout->addWidget(_prediction_panel);

    // Progress / status UI (between prediction and results)
    auto * progress_widget = new QWidget(classification_tab);
    auto * progress_layout = new QHBoxLayout(progress_widget);
    progress_layout->setContentsMargins(0, 2, 0, 2);

    _status_label = new QLabel(classification_tab);
    _status_label->setWordWrap(true);
    progress_layout->addWidget(_status_label, 1);

    _progress_bar = new QProgressBar(classification_tab);
    _progress_bar->setRange(0, 0);// indeterminate
    _progress_bar->setMaximumWidth(120);
    _progress_bar->setVisible(false);
    progress_layout->addWidget(_progress_bar);

    classification_layout->addWidget(progress_widget);

    // Results panel
    _results_panel = new ResultsPanel(_group_manager, classification_tab);
    classification_layout->addWidget(_results_panel);

    classification_layout->addStretch();

    // Clustering tab
    auto * clustering_tab = new QWidget();
    auto * clustering_layout = new QVBoxLayout(clustering_tab);

    _clustering_panel = new ClusteringPanel(_state, _data_manager, clustering_tab);
    clustering_layout->addWidget(_clustering_panel);

    // Progress / status UI for clustering (between panel and output)
    auto * clustering_progress_widget = new QWidget(clustering_tab);
    auto * clustering_progress_layout = new QHBoxLayout(clustering_progress_widget);
    clustering_progress_layout->setContentsMargins(0, 2, 0, 2);

    _clustering_status_label = new QLabel(clustering_tab);
    _clustering_status_label->setWordWrap(true);
    clustering_progress_layout->addWidget(_clustering_status_label, 1);

    _clustering_progress_bar = new QProgressBar(clustering_tab);
    _clustering_progress_bar->setRange(0, 0);// indeterminate
    _clustering_progress_bar->setMaximumWidth(120);
    _clustering_progress_bar->setVisible(false);
    clustering_progress_layout->addWidget(_clustering_progress_bar);

    clustering_layout->addWidget(clustering_progress_widget);

    _cluster_output_panel = new ClusterOutputPanel(_group_manager, clustering_tab);
    clustering_layout->addWidget(_cluster_output_panel);
    clustering_layout->addStretch();

    // Dim Reduction tab
    auto * dim_reduction_tab = new QWidget();
    auto * dim_reduction_layout = new QVBoxLayout(dim_reduction_tab);

    _dim_reduction_panel = new DimReductionPanel(_state, _data_manager, dim_reduction_tab);
    dim_reduction_layout->addWidget(_dim_reduction_panel);

    // Progress / status UI for dim reduction
    auto * dim_reduction_progress_widget = new QWidget(dim_reduction_tab);
    auto * dim_reduction_progress_layout = new QHBoxLayout(dim_reduction_progress_widget);
    dim_reduction_progress_layout->setContentsMargins(0, 2, 0, 2);

    _dim_reduction_status_label = new QLabel(dim_reduction_tab);
    _dim_reduction_status_label->setWordWrap(true);
    dim_reduction_progress_layout->addWidget(_dim_reduction_status_label, 1);

    _dim_reduction_progress_bar = new QProgressBar(dim_reduction_tab);
    _dim_reduction_progress_bar->setRange(0, 0);// indeterminate
    _dim_reduction_progress_bar->setMaximumWidth(120);
    _dim_reduction_progress_bar->setVisible(false);
    dim_reduction_progress_layout->addWidget(_dim_reduction_progress_bar);

    dim_reduction_layout->addWidget(dim_reduction_progress_widget);
    dim_reduction_layout->addStretch();

    tabs->addTab(classification_tab, QStringLiteral("Classification"));
    tabs->addTab(clustering_tab, QStringLiteral("Clustering"));
    tabs->addTab(dim_reduction_tab, QStringLiteral("Dim Reduction"));

    // Restore active tab from state
    if (_state) {
        tabs->setCurrentIndex(_state->activeTab());
    }

    // Track tab changes in state
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (_state) {
            _state->setActiveTab(index);
        }
    });

    layout->addWidget(tabs);
}

// =============================================================================
// Signal wiring
// =============================================================================

void MLCoreWidget::_connectSignals() {
    // "Train" button in ModelConfigPanel → run full train+predict pipeline
    connect(_model_config_panel, &ModelConfigPanel::trainRequested,
            this, &MLCoreWidget::_onTrainRequested);

    // "Predict" button in PredictionPanel → run prediction using last trained model
    connect(_prediction_panel, &PredictionPanel::predictRequested,
            this, &MLCoreWidget::_onPredictRequested);

    // Model selection changed → show/hide constrained decoding checkbox
    connect(_model_config_panel, &ModelConfigPanel::modelChanged,
            this, [this](QString const & name) {
                auto temp_model = _registry->create(name.toStdString());
                bool const is_seq = temp_model && temp_model->isSequenceModel();
                _prediction_panel->setSequenceModelActive(is_seq);
            });

    // "Fit & Assign" button in ClusteringPanel → run clustering pipeline
    connect(_clustering_panel, &ClusteringPanel::fitRequested,
            this, &MLCoreWidget::_onClusteringFitRequested);

    // Pipeline progress (cross-thread signal → main-thread slot)
    connect(this, &MLCoreWidget::_pipelineProgressReported,
            this, &MLCoreWidget::_onPipelineProgress,
            Qt::QueuedConnection);

    // Clustering pipeline progress (cross-thread signal → main-thread slot)
    connect(this, &MLCoreWidget::_clusteringProgressReported,
            this, &MLCoreWidget::_onClusteringProgress,
            Qt::QueuedConnection);

    // "Run Reduction" button in DimReductionPanel → run dim reduction pipeline
    connect(_dim_reduction_panel, &DimReductionPanel::runRequested,
            this, &MLCoreWidget::_onDimReductionRunRequested);

    // Dim Reduction pipeline progress (cross-thread signal → main-thread slot)
    connect(this, &MLCoreWidget::_dimReductionProgressReported,
            this, &MLCoreWidget::_onDimReductionProgress,
            Qt::QueuedConnection);

    // "Run Reduction" button (supervised mode) → supervised dim reduction pipeline
    connect(_dim_reduction_panel, &DimReductionPanel::supervisedRunRequested,
            this, &MLCoreWidget::_onSupervisedDimReductionRunRequested);

    // Supervised Dim Reduction pipeline progress (cross-thread signal → main-thread slot)
    connect(this, &MLCoreWidget::_supervisedDimReductionProgressReported,
            this, &MLCoreWidget::_onSupervisedDimReductionProgress,
            Qt::QueuedConnection);

    // Output key clicked in ResultsPanel → emit data focus via SelectionContext (task 4.9)
    connect(_results_panel, &ResultsPanel::outputKeyClicked,
            this, [this](QString const & key) {
                if (!_selection_context || !_data_manager) {
                    return;
                }

                std::string const key_std = key.toStdString();

                // Look up the data type from DataManager and convert to string
                DM_DataType const dm_type = _data_manager->getType(key_std);
                QString const type_str = QString::fromStdString(
                        convert_data_type_to_string(dm_type));

                SelectionSource const source{
                        _state ? EditorLib::EditorInstanceId(_state->getInstanceId())
                               : EditorLib::EditorInstanceId{},
                        QStringLiteral("ResultsPanel")};

                _selection_context->setDataFocus(
                        EditorLib::SelectedDataKey(key), type_str, source);
            });

    // Output key clicked in ClusterOutputPanel → emit data focus via SelectionContext
    connect(_cluster_output_panel, &ClusterOutputPanel::outputKeyClicked,
            this, [this](QString const & key) {
                if (!_selection_context || !_data_manager) {
                    return;
                }

                std::string const key_std = key.toStdString();

                DM_DataType const dm_type = _data_manager->getType(key_std);
                QString const type_str = QString::fromStdString(
                        convert_data_type_to_string(dm_type));

                SelectionSource const source{
                        _state ? EditorLib::EditorInstanceId(_state->getInstanceId())
                               : EditorLib::EditorInstanceId{},
                        QStringLiteral("ClusterOutputPanel")};

                _selection_context->setDataFocus(
                        EditorLib::SelectedDataKey(key), type_str, source);
            });

    // Group clicked in ResultsPanel → select entities in group via SelectionContext
    connect(_results_panel, &ResultsPanel::groupClicked,
            this, &MLCoreWidget::_selectEntitiesInGroup);

    // Group clicked in ClusterOutputPanel → select entities in group via SelectionContext
    connect(_cluster_output_panel, &ClusterOutputPanel::groupClicked,
            this, &MLCoreWidget::_selectEntitiesInGroup);

    // Incoming entity selection changes — highlight matching groups (task 5.4)
    if (_selection_context) {
        connect(_selection_context, &SelectionContext::entitySelectionChanged,
                this, [this](SelectionSource const & source) {
                    // Ignore our own entity selection broadcasts
                    if (_state && source.editor_instance_id == EditorLib::EditorInstanceId(_state->getInstanceId())) {
                        return;
                    }

                    if (!_data_manager || !_selection_context) {
                        return;
                    }

                    auto const selected_entities = _selection_context->selectedEntities();
                    if (selected_entities.empty()) {
                        return;
                    }

                    // Check if any selected entity belongs to one of the ML-created groups.
                    // If so, highlight that group in the output panel by selecting the
                    // corresponding list item.
                    auto * egm = _data_manager->getEntityGroupManager();
                    if (!egm) {
                        return;
                    }

                    // Check clustering groups
                    if (_last_clustering_result && _last_clustering_result->success) {
                        for (auto const gid: _last_clustering_result->putative_group_ids) {
                            auto members = egm->getEntitiesInGroup(gid);
                            for (auto const & eid: selected_entities) {
                                EntityId const entity_id(static_cast<uint64_t>(eid));
                                auto it = std::find(members.begin(), members.end(), entity_id);
                                if (it != members.end()) {
                                    // Found a match — this entity is in this cluster group
                                    // Could highlight in ClusterOutputPanel in the future
                                    break;
                                }
                            }
                        }
                    }
                });
    }
}

// =============================================================================
// Train / Predict slots
// =============================================================================

void MLCoreWidget::_onTrainRequested() {
    if (_pipeline_running) {
        return;
    }

    if (!_validatePanels()) {
        return;
    }

    auto config = _buildPipelineConfig();
    _runPipelineAsync(std::move(config));
}

void MLCoreWidget::_onPredictRequested() {
    if (_pipeline_running) {
        return;
    }

    if (!_validatePanels()) {
        return;
    }

    // "Predict" runs the full pipeline (train then predict) since we
    // compose training + prediction in a single pipeline call. The user
    // can adjust prediction region / threshold between runs.
    auto config = _buildPipelineConfig();
    _runPipelineAsync(std::move(config));
}

// =============================================================================
// Pipeline progress and completion
// =============================================================================

void MLCoreWidget::_onPipelineProgress(int stage_index, QString const & message) {
    // Map stage index to progress description
    auto const stage = static_cast<MLCore::ClassificationStage>(stage_index);
    QString const stage_name = QString::fromStdString(MLCore::toString(stage));

    _status_label->setText(
            QStringLiteral("<b>%1:</b> %2").arg(stage_name, message));
}

void MLCoreWidget::_onPipelineComplete() {
    _setPipelineRunning(false);

    if (!_last_result) {
        _status_label->setText(
                QStringLiteral("<span style='color: red;'>Pipeline returned no result</span>"));
        return;
    }

    if (!_last_result->success) {
        auto const stage_name = QString::fromStdString(
                MLCore::toString(_last_result->failed_stage));
        _status_label->setText(
                QStringLiteral("<span style='color: red;'>Failed at %1: %2</span>")
                        .arg(stage_name,
                             QString::fromStdString(_last_result->error_message)));
        return;
    }

    // Success — write deferred output on the main thread (thread-safe)
    if (_last_result->deferred_output.has_value() && _data_manager) {
        try {
            auto writer_result = MLCore::writePredictions(
                    *_data_manager,
                    *_last_result->deferred_output,
                    _last_result->class_names,
                    _last_result->deferred_output_config.value_or(MLCore::PredictionWriterConfig{}));
            _last_result->writer_result = std::move(writer_result);
        } catch (std::exception const & e) {
            _status_label->setText(
                    QStringLiteral("<span style='color: red;'>Output writing failed: %1</span>")
                            .arg(QString::fromStdString(e.what())));
            return;
        }
        _last_result->deferred_output.reset();
        _last_result->deferred_output_config.reset();
    }

    // Success — show results
    _status_label->setText(
            QStringLiteral("<span style='color: green;'>Pipeline completed successfully</span>"));

    std::string const model_name = _model_config_panel->selectedModelName();
    _results_panel->showClassificationResult(*_last_result, model_name);

    // Show transition matrix for HMM models
    if (_last_result->trained_model) {
        auto const * hmm = dynamic_cast<MLCore::HiddenMarkovModelOperation const *>(
                _last_result->trained_model.get());
        if (hmm != nullptr) {
            arma::mat const trans = hmm->getTransitionMatrix();
            _results_panel->showTransitionMatrix(trans, _last_result->class_names);
        }
    }
}

// =============================================================================
// Validation
// =============================================================================

bool MLCoreWidget::_validatePanels() const {
    // Build a list of validation failures
    QStringList issues;

    if (!_feature_panel->hasValidSelection()) {
        issues << QStringLiteral("No feature tensor selected");
    }

    if (!_label_panel->hasValidSelection()) {
        issues << QStringLiteral("Label configuration is incomplete");
    }

    if (!_model_config_panel->hasValidConfiguration()) {
        issues << QStringLiteral("No model selected");
    }

    if (!issues.isEmpty()) {
        QMessageBox::warning(
                const_cast<MLCoreWidget *>(this),
                QStringLiteral("Cannot Run Pipeline"),
                QStringLiteral("Please fix the following issues before running:\n\n• ") +
                        issues.join(QStringLiteral("\n• ")));
        return false;
    }

    return true;
}

// =============================================================================
// Pipeline config assembly
// =============================================================================

MLCore::ClassificationPipelineConfig MLCoreWidget::_buildPipelineConfig() const {
    MLCore::ClassificationPipelineConfig config;

    // -- Model --
    config.model_name = _model_config_panel->selectedModelName();
    // model_params is owned by the pipeline config — use getDefaultParameters as
    // the pipeline will re-create the model, but we need to pass params through.
    // Since the pipeline takes a raw pointer, we set it to nullptr and rely on
    // the pipeline's model creation to use defaults — BUT we should pass the
    // user-configured parameters. The pipeline takes a non-owning pointer, so
    // we'll store the parameters on the stack in _runPipelineAsync.
    config.model_params = nullptr;// set in _runPipelineAsync

    // -- Features --
    config.feature_tensor_key = _feature_panel->selectedTensorKey();

    // -- Time key (from the tensor's registered time frame in DataManager) --
    if (_data_manager) {
        TimeKey const tk = _data_manager->getTimeKey(config.feature_tensor_key);
        if (!tk.empty()) {
            config.time_key_str = tk.str();
        }
    }

    // -- Labels --
    std::string const source_type = _label_panel->labelSourceType();

    if (source_type == "intervals") {
        MLCore::LabelFromIntervals label_cfg;
        if (_state) {
            label_cfg.positive_class_name = _state->labelPositiveClassName();
            label_cfg.negative_class_name = _state->labelNegativeClassName();
        }
        config.label_config = label_cfg;
        if (_state) {
            config.label_interval_key = _state->labelIntervalKey();
        }
    } else if (source_type == "groups") {
        MLCore::LabelFromTimeEntityGroups label_cfg;
        auto const group_ids = _label_panel->selectedGroupIds();
        label_cfg.class_groups.assign(group_ids.begin(), group_ids.end());
        label_cfg.time_key = config.time_key_str;
        config.label_config = label_cfg;
    } else if (source_type == "entity_groups") {
        MLCore::LabelFromDataEntityGroups label_cfg;
        auto const group_ids = _label_panel->selectedGroupIds();
        label_cfg.class_groups.assign(group_ids.begin(), group_ids.end());
        if (_state) {
            label_cfg.data_key = _state->labelDataKey();
        }
        config.label_config = label_cfg;
    } else if (source_type == "events") {
        MLCore::LabelFromEvents const label_cfg;
        config.label_config = label_cfg;
        config.label_event_key = _label_panel->selectedEventKey();
    }

    // -- Feature conversion (use sensible defaults) --
    config.conversion_config.drop_nan = true;
    config.conversion_config.zscore_normalize = false;

    // -- Training region --
    if (_training_region_panel && !_training_region_panel->isAllFramesChecked()) {
        config.training_interval_key = _training_region_panel->selectedRegionKey();
    }

    // -- Class balancing --
    config.balance_classes = _model_config_panel->isBalancingEnabled();
    if (config.balance_classes) {
        config.balancing_config.strategy = _model_config_panel->balancingStrategy();
        config.balancing_config.max_ratio = _model_config_panel->balancingMaxRatio();
    }

    // -- Prediction region --
    if (_prediction_panel->isAllFramesChecked()) {
        config.prediction_region.predict_all_rows = true;
        config.prediction_region.prediction_tensor_key.clear();
    } else {
        std::string const pred_key = _prediction_panel->selectedRegionKey();
        if (pred_key.empty()) {
            // No prediction region selected — predict on training data
            config.prediction_region.predict_all_rows = true;
        } else {
            // Predict on all rows (for temporal context), but filter output
            // to only the frames within the selected interval.
            config.prediction_region.predict_all_rows = true;
            config.prediction_region.prediction_interval_key = pred_key;
        }
    }
    if (_state) {
        config.prediction_region.constrained_decoding = _state->constrainedDecoding();
    }

    // -- Output --
    config.output_config.output_prefix = _prediction_panel->outputPrefix();
    config.output_config.write_intervals = _prediction_panel->outputPredictions();
    config.output_config.write_probabilities = _prediction_panel->outputProbabilities();
    config.output_config.write_to_putative_groups = true;
    config.output_config.time_key_str = config.time_key_str;

    // -- Thread safety --
    // Defer DataManager writes to the main thread to avoid triggering
    // observer callbacks from the background PipelineWorker thread.
    config.defer_dm_writes = true;

    return config;
}

// =============================================================================
// Async pipeline execution
// =============================================================================

void MLCoreWidget::_runPipelineAsync(MLCore::ClassificationPipelineConfig config) {
    _setPipelineRunning(true);
    _results_panel->clearResults();

    // Retrieve user-configured model parameters and bind to config
    auto params = _model_config_panel->currentParameters();
    config.model_params = params.get();

    auto * worker = new PipelineWorker(
            _data_manager.get(), _registry.get(), std::move(config), this);

    // Forward worker progress signal → our cross-thread signal
    connect(worker, &PipelineWorker::progressUpdate,
            this, &MLCoreWidget::_pipelineProgressReported);

    // On completion, harvest result and clean up
    connect(worker, &QThread::finished, this, [this, worker, p = std::move(params)]() mutable {
        _last_result = std::make_unique<MLCore::ClassificationPipelineResult>(
                worker->takeResult());
        worker->deleteLater();
        p.reset();// release parameter storage now that pipeline is done
        _onPipelineComplete();
    });

    worker->start();
}

// =============================================================================
// UI state management
// =============================================================================

void MLCoreWidget::_setPipelineRunning(bool running) {
    _pipeline_running = running;
    _progress_bar->setVisible(running);

    if (running) {
        _status_label->setText(QStringLiteral("Starting pipeline..."));
    }

    // Disable interaction with panels that feed into the pipeline
    _feature_panel->setEnabled(!running);
    _training_region_panel->setEnabled(!running);
    _label_panel->setEnabled(!running);
    _model_config_panel->setEnabled(!running);
    _prediction_panel->setEnabled(!running);
}

// =============================================================================
// Clustering — Fit slot
// =============================================================================

void MLCoreWidget::_onClusteringFitRequested() {
    if (_clustering_pipeline_running) {
        return;
    }

    if (!_validateClusteringPanels()) {
        return;
    }

    auto config = _buildClusteringPipelineConfig();
    _runClusteringPipelineAsync(std::move(config));
}

// =============================================================================
// Clustering — Validation
// =============================================================================

bool MLCoreWidget::_validateClusteringPanels() const {
    QStringList issues;

    if (!_clustering_panel->hasValidConfiguration()) {
        issues << QStringLiteral("Clustering panel configuration is incomplete "
                                 "(select a tensor and algorithm)");
    }

    if (!issues.isEmpty()) {
        QMessageBox::warning(
                const_cast<MLCoreWidget *>(this),
                QStringLiteral("Cannot Run Clustering Pipeline"),
                QStringLiteral("Please fix the following issues before running:\n\n• ") +
                        issues.join(QStringLiteral("\n• ")));
        return false;
    }

    return true;
}

// =============================================================================
// Clustering — Pipeline config assembly
// =============================================================================

MLCore::ClusteringPipelineConfig MLCoreWidget::_buildClusteringPipelineConfig() const {
    MLCore::ClusteringPipelineConfig config;

    // -- Model --
    config.model_name = _clustering_panel->selectedAlgorithmName();
    config.model_params = nullptr;// set in _runClusteringPipelineAsync

    // -- Features --
    config.feature_tensor_key = _clustering_panel->selectedTensorKey();

    // -- Time key (from the tensor's registered time frame in DataManager) --
    if (_data_manager) {
        TimeKey const tk = _data_manager->getTimeKey(config.feature_tensor_key);
        if (!tk.empty()) {
            config.time_key_str = tk.str();
        }
    }

    // -- Feature conversion --
    config.conversion_config.drop_nan = true;
    config.conversion_config.zscore_normalize = _clustering_panel->zscoreNormalize();

    // -- Assignment region (assign all rows of fitting tensor) --
    config.assignment_region.assign_all_rows = true;

    // -- Output --
    config.output_config.output_prefix = _clustering_panel->outputPrefix();
    config.output_config.write_intervals = _clustering_panel->writeIntervals();
    config.output_config.write_probabilities = _clustering_panel->writeProbabilities();
    config.output_config.write_to_putative_groups = true;
    config.output_config.time_key_str = config.time_key_str;

    // -- Thread safety --
    config.defer_dm_writes = true;

    return config;
}

// =============================================================================
// Clustering — Async execution
// =============================================================================

void MLCoreWidget::_runClusteringPipelineAsync(MLCore::ClusteringPipelineConfig config) {
    _setClusteringPipelineRunning(true);
    _cluster_output_panel->clearResults();

    // Retrieve user-configured model parameters and bind to config
    auto params = _clustering_panel->currentParameters();
    config.model_params = params.get();

    auto * worker = new ClusteringPipelineWorker(
            _data_manager.get(), _registry.get(), std::move(config), this);

    // Forward worker progress signal → our cross-thread signal
    connect(worker, &ClusteringPipelineWorker::progressUpdate,
            this, &MLCoreWidget::_clusteringProgressReported);

    // On completion, harvest result and clean up
    connect(worker, &QThread::finished, this, [this, worker, p = std::move(params)]() mutable {
        _last_clustering_result = std::make_unique<MLCore::ClusteringPipelineResult>(
                worker->takeResult());
        worker->deleteLater();
        p.reset();// release parameter storage now that pipeline is done
        _onClusteringPipelineComplete();
    });

    worker->start();
}

// =============================================================================
// Clustering — Progress and completion
// =============================================================================

void MLCoreWidget::_onClusteringProgress(int stage_index, QString const & message) {
    auto const stage = static_cast<MLCore::ClusteringStage>(stage_index);
    QString const stage_name = QString::fromStdString(MLCore::toString(stage));

    _clustering_status_label->setText(
            QStringLiteral("<b>%1:</b> %2").arg(stage_name, message));
}

void MLCoreWidget::_onClusteringPipelineComplete() {
    _setClusteringPipelineRunning(false);

    if (!_last_clustering_result) {
        _clustering_status_label->setText(
                QStringLiteral("<span style='color: red;'>Pipeline returned no result</span>"));
        return;
    }

    if (!_last_clustering_result->success) {
        auto const stage_name = QString::fromStdString(
                MLCore::toString(_last_clustering_result->failed_stage));
        _clustering_status_label->setText(
                QStringLiteral("<span style='color: red;'>Failed at %1: %2</span>")
                        .arg(stage_name,
                             QString::fromStdString(_last_clustering_result->error_message)));
        return;
    }

    // Write deferred output on the main thread (thread-safe)
    if (_last_clustering_result->deferred_output.has_value() && _data_manager) {
        try {
            auto writer_result = MLCore::writePredictions(
                    *_data_manager,
                    *_last_clustering_result->deferred_output,
                    _last_clustering_result->deferred_cluster_names,
                    _last_clustering_result->deferred_output_config.value_or(
                            MLCore::PredictionWriterConfig{}));
            _last_clustering_result->interval_keys = std::move(writer_result.interval_keys);
            _last_clustering_result->probability_keys = std::move(writer_result.probability_keys);
            _last_clustering_result->putative_group_ids = std::move(writer_result.putative_group_ids);
        } catch (std::exception const & e) {
            _clustering_status_label->setText(
                    QStringLiteral("<span style='color: red;'>Output writing failed: %1</span>")
                            .arg(QString::fromStdString(e.what())));
            return;
        }
        _last_clustering_result->deferred_output.reset();
        _last_clustering_result->deferred_output_config.reset();
    }

    // Success — show results
    _clustering_status_label->setText(
            QStringLiteral("<span style='color: green;'>Clustering completed successfully</span>"));

    std::string const algorithm_name = _clustering_panel->selectedAlgorithmName();
    _cluster_output_panel->showClusteringResult(*_last_clustering_result, algorithm_name);
}

// =============================================================================
// Clustering — UI state management
// =============================================================================

void MLCoreWidget::_setClusteringPipelineRunning(bool running) {
    _clustering_pipeline_running = running;
    _clustering_progress_bar->setVisible(running);

    if (running) {
        _clustering_status_label->setText(QStringLiteral("Starting clustering pipeline..."));
    }

    // Disable the clustering panel during pipeline execution
    _clustering_panel->setEnabled(!running);
}

// =============================================================================
// Dim Reduction — Run slot
// =============================================================================

void MLCoreWidget::_onDimReductionRunRequested() {
    if (_dim_reduction_pipeline_running) {
        return;
    }

    if (!_validateDimReductionPanels()) {
        return;
    }

    auto config = _buildDimReductionPipelineConfig();
    _runDimReductionPipelineAsync(std::move(config));
}

// =============================================================================
// Dim Reduction — Validation
// =============================================================================

bool MLCoreWidget::_validateDimReductionPanels() const {
    QStringList issues;

    if (!_dim_reduction_panel->hasValidConfiguration()) {
        issues << QStringLiteral("Dim reduction panel configuration is incomplete "
                                 "(select a tensor and algorithm)");
    }

    if (!issues.isEmpty()) {
        QMessageBox::warning(
                const_cast<MLCoreWidget *>(this),
                QStringLiteral("Cannot Run Dim Reduction Pipeline"),
                QStringLiteral("Please fix the following issues before running:\n\n• ") +
                        issues.join(QStringLiteral("\n• ")));
        return false;
    }

    return true;
}

// =============================================================================
// Dim Reduction — Pipeline config assembly
// =============================================================================

MLCore::DimReductionPipelineConfig MLCoreWidget::_buildDimReductionPipelineConfig() const {
    MLCore::DimReductionPipelineConfig config;

    // -- Model --
    config.model_name = _dim_reduction_panel->selectedAlgorithmName();
    config.model_params = nullptr;// set in _runDimReductionPipelineAsync

    // -- Features --
    config.feature_tensor_key = _dim_reduction_panel->selectedTensorKey();

    // -- Time key (from the tensor's registered time frame in DataManager) --
    if (_data_manager) {
        TimeKey const tk = _data_manager->getTimeKey(config.feature_tensor_key);
        if (!tk.empty()) {
            config.time_key_str = tk.str();
        }
    }

    // -- Feature conversion --
    config.conversion_config.drop_nan = true;
    config.conversion_config.zscore_normalize = _dim_reduction_panel->zscoreNormalize();

    // -- Output --
    config.output_config.output_key = _dim_reduction_panel->outputKey();
    config.output_config.time_key_str = config.time_key_str;

    // -- Thread safety --
    config.defer_dm_writes = true;

    return config;
}

// =============================================================================
// Dim Reduction — Async execution
// =============================================================================

void MLCoreWidget::_runDimReductionPipelineAsync(MLCore::DimReductionPipelineConfig config) {
    _setDimReductionPipelineRunning(true);
    _dim_reduction_panel->clearResults();

    // Retrieve user-configured model parameters and bind to config
    auto params = _dim_reduction_panel->currentParameters();
    config.model_params = params.get();

    auto * worker = new DimReductionPipelineWorker(
            _data_manager.get(), _registry.get(), std::move(config), this);

    // Forward worker progress signal → our cross-thread signal
    connect(worker, &DimReductionPipelineWorker::progressUpdate,
            this, &MLCoreWidget::_dimReductionProgressReported);

    // On completion, harvest result and clean up
    connect(worker, &QThread::finished, this, [this, worker, p = std::move(params)]() mutable {
        _last_dim_reduction_result = std::make_unique<MLCore::DimReductionPipelineResult>(
                worker->takeResult());
        worker->deleteLater();
        p.reset();// release parameter storage now that pipeline is done
        _onDimReductionPipelineComplete();
    });

    worker->start();
}

// =============================================================================
// Dim Reduction — Progress and completion
// =============================================================================

void MLCoreWidget::_onDimReductionProgress(int stage_index, QString const & message) {
    auto const stage = static_cast<MLCore::DimReductionStage>(stage_index);
    QString const stage_name = QString::fromStdString(MLCore::toString(stage));

    _dim_reduction_status_label->setText(
            QStringLiteral("<b>%1:</b> %2").arg(stage_name, message));
}

void MLCoreWidget::_onDimReductionPipelineComplete() {
    _setDimReductionPipelineRunning(false);

    if (!_last_dim_reduction_result) {
        _dim_reduction_status_label->setText(
                QStringLiteral("<span style='color: red;'>Pipeline returned no result</span>"));
        return;
    }

    if (!_last_dim_reduction_result->success) {
        auto const stage_name = QString::fromStdString(
                MLCore::toString(_last_dim_reduction_result->failed_stage));
        _dim_reduction_status_label->setText(
                QStringLiteral("<span style='color: red;'>Failed at %1: %2</span>")
                        .arg(stage_name,
                             QString::fromStdString(_last_dim_reduction_result->error_message)));
        return;
    }

    // Write deferred output on the main thread (thread-safe)
    if (_last_dim_reduction_result->deferred_output && _data_manager) {
        std::string const & out_key = _last_dim_reduction_result->deferred_output_key;
        std::string const & time_key = _last_dim_reduction_result->deferred_time_key;
        _data_manager->setData(out_key, _last_dim_reduction_result->deferred_output,
                               TimeKey(time_key));
        _last_dim_reduction_result->output_key = out_key;
        _last_dim_reduction_result->deferred_output.reset();
    }

    // Success — show results
    _dim_reduction_status_label->setText(
            QStringLiteral("<span style='color: green;'>Dim reduction completed successfully</span>"));

    _dim_reduction_panel->showResults(
            _last_dim_reduction_result->num_observations,
            _last_dim_reduction_result->num_input_features,
            _last_dim_reduction_result->num_output_components,
            _last_dim_reduction_result->rows_dropped_nan,
            _last_dim_reduction_result->explained_variance_ratio);

    // Auto-focus the output tensor so other widgets (ScatterPlot, DataViewer) can pick it up
    if (_selection_context && !_last_dim_reduction_result->output_key.empty()) {
        SelectionSource const source{
                _state ? EditorLib::EditorInstanceId(_state->getInstanceId())
                       : EditorLib::EditorInstanceId{},
                QStringLiteral("DimReductionPanel")};

        _selection_context->setDataFocus(
                EditorLib::SelectedDataKey(
                        QString::fromStdString(_last_dim_reduction_result->output_key)),
                QStringLiteral("TensorData"),
                source);
    }

    // Refresh tensor list in the panel so the new output appears
    _dim_reduction_panel->refreshTensorList();
}

// =============================================================================
// Dim Reduction — UI state management
// =============================================================================

void MLCoreWidget::_setDimReductionPipelineRunning(bool running) {
    _dim_reduction_pipeline_running = running;
    _dim_reduction_progress_bar->setVisible(running);

    if (running) {
        _dim_reduction_status_label->setText(QStringLiteral("Starting dim reduction pipeline..."));
    }

    // Disable the dim reduction panel during pipeline execution
    _dim_reduction_panel->setEnabled(!running);
}

// =============================================================================
// Supervised Dim Reduction — Slot
// =============================================================================

void MLCoreWidget::_onSupervisedDimReductionRunRequested() {
    if (_supervised_dim_reduction_pipeline_running) {
        return;
    }

    if (!_validateSupervisedDimReductionPanels()) {
        return;
    }

    auto config = _buildSupervisedDimReductionPipelineConfig();
    _runSupervisedDimReductionPipelineAsync(std::move(config));
}

// =============================================================================
// Supervised Dim Reduction — Validation
// =============================================================================

bool MLCoreWidget::_validateSupervisedDimReductionPanels() const {
    QStringList issues;

    if (!_dim_reduction_panel->hasValidConfiguration()) {
        issues << QStringLiteral("Supervised dim reduction panel configuration is incomplete "
                                 "(select a tensor, algorithm, and label source)");
    }

    if (!issues.isEmpty()) {
        QMessageBox::warning(
                const_cast<MLCoreWidget *>(this),
                QStringLiteral("Cannot Run Supervised Dim Reduction Pipeline"),
                QStringLiteral("Please fix the following issues before running:\n\n• ") +
                        issues.join(QStringLiteral("\n• ")));
        return false;
    }

    return true;
}

// =============================================================================
// Supervised Dim Reduction — Pipeline config assembly
// =============================================================================

MLCore::SupervisedDimReductionPipelineConfig
MLCoreWidget::_buildSupervisedDimReductionPipelineConfig() const {
    MLCore::SupervisedDimReductionPipelineConfig config;

    // -- Model --
    config.model_name = _dim_reduction_panel->selectedAlgorithmName();
    config.model_params = nullptr;// set in _runSupervisedDimReductionPipelineAsync

    // -- Features --
    config.feature_tensor_key = _dim_reduction_panel->selectedTensorKey();

    // -- Time key --
    if (_data_manager) {
        TimeKey const tk = _data_manager->getTimeKey(config.feature_tensor_key);
        if (!tk.empty()) {
            config.time_key_str = tk.str();
        }
    }

    // -- Feature conversion --
    config.conversion_config.drop_nan = true;
    config.conversion_config.zscore_normalize = _dim_reduction_panel->zscoreNormalize();

    // -- Labels (same pattern as Classification pipeline) --
    std::string const source_type = _dim_reduction_panel->labelSourceType();

    if (source_type == "intervals") {
        MLCore::LabelFromIntervals label_cfg;
        label_cfg.positive_class_name = _dim_reduction_panel->labelPositiveClassName();
        label_cfg.negative_class_name = _dim_reduction_panel->labelNegativeClassName();
        config.label_config = label_cfg;
        config.label_interval_key = _dim_reduction_panel->labelIntervalKey();
    } else if (source_type == "groups") {
        MLCore::LabelFromTimeEntityGroups label_cfg;
        auto const group_ids = _dim_reduction_panel->selectedGroupIds();
        label_cfg.class_groups.assign(group_ids.begin(), group_ids.end());
        label_cfg.time_key = config.time_key_str;
        config.label_config = label_cfg;
    } else if (source_type == "entity_groups") {
        MLCore::LabelFromDataEntityGroups label_cfg;
        auto const group_ids = _dim_reduction_panel->selectedGroupIds();
        label_cfg.class_groups.assign(group_ids.begin(), group_ids.end());
        label_cfg.data_key = _dim_reduction_panel->labelDataKey();
        config.label_config = label_cfg;
    } else if (source_type == "events") {
        MLCore::LabelFromEvents const label_cfg;
        config.label_config = label_cfg;
        config.label_event_key = _dim_reduction_panel->labelEventKey();
    }

    // -- Output --
    config.output_key = _dim_reduction_panel->outputKey();

    // -- Thread safety --
    config.defer_dm_writes = true;

    return config;
}

// =============================================================================
// Supervised Dim Reduction — Async execution
// =============================================================================

void MLCoreWidget::_runSupervisedDimReductionPipelineAsync(
        MLCore::SupervisedDimReductionPipelineConfig config) {
    _setSupervisedDimReductionPipelineRunning(true);
    _dim_reduction_panel->clearResults();

    // Retrieve user-configured model parameters and bind to config
    auto params = _dim_reduction_panel->currentParameters();
    config.model_params = params.get();

    auto * worker = new SupervisedDimReductionPipelineWorker(
            _data_manager.get(), _registry.get(), std::move(config), this);

    // Forward worker progress signal → our cross-thread signal
    connect(worker, &SupervisedDimReductionPipelineWorker::progressUpdate,
            this, &MLCoreWidget::_supervisedDimReductionProgressReported);

    // On completion, harvest result and clean up
    connect(worker, &QThread::finished, this, [this, worker, p = std::move(params)]() mutable {
        _last_supervised_dim_reduction_result =
                std::make_unique<MLCore::SupervisedDimReductionPipelineResult>(
                        worker->takeResult());
        worker->deleteLater();
        p.reset();
        _onSupervisedDimReductionPipelineComplete();
    });

    worker->start();
}

// =============================================================================
// Supervised Dim Reduction — Progress and completion
// =============================================================================

void MLCoreWidget::_onSupervisedDimReductionProgress(
        int stage_index, QString const & message) {
    auto const stage = static_cast<MLCore::SupervisedDimReductionStage>(stage_index);
    QString const stage_name = QString::fromStdString(MLCore::toString(stage));

    _dim_reduction_status_label->setText(
            QStringLiteral("<b>%1:</b> %2").arg(stage_name, message));
}

void MLCoreWidget::_onSupervisedDimReductionPipelineComplete() {
    _setSupervisedDimReductionPipelineRunning(false);

    if (!_last_supervised_dim_reduction_result) {
        _dim_reduction_status_label->setText(
                QStringLiteral("<span style='color: red;'>Pipeline returned no result</span>"));
        return;
    }

    if (!_last_supervised_dim_reduction_result->success) {
        auto const stage_name = QString::fromStdString(
                MLCore::toString(_last_supervised_dim_reduction_result->failed_stage));
        _dim_reduction_status_label->setText(
                QStringLiteral("<span style='color: red;'>Failed at %1: %2</span>")
                        .arg(stage_name,
                             QString::fromStdString(
                                     _last_supervised_dim_reduction_result->error_message)));
        return;
    }

    // Write deferred output on the main thread (thread-safe)
    if (_last_supervised_dim_reduction_result->deferred_output && _data_manager) {
        std::string const & out_key =
                _last_supervised_dim_reduction_result->deferred_output_key;
        std::string const & time_key =
                _last_supervised_dim_reduction_result->deferred_time_key;
        _data_manager->setData(out_key,
                               _last_supervised_dim_reduction_result->deferred_output,
                               TimeKey(time_key));
        _last_supervised_dim_reduction_result->output_key = out_key;
        _last_supervised_dim_reduction_result->deferred_output.reset();
    }

    // Success — show supervised results
    _dim_reduction_status_label->setText(
            QStringLiteral("<span style='color: green;'>Supervised dim reduction completed "
                           "successfully</span>"));

    _dim_reduction_panel->showSupervisedResults(
            _last_supervised_dim_reduction_result->num_observations,
            _last_supervised_dim_reduction_result->num_training_observations,
            _last_supervised_dim_reduction_result->num_input_features,
            _last_supervised_dim_reduction_result->num_output_dimensions,
            _last_supervised_dim_reduction_result->rows_dropped_nan,
            _last_supervised_dim_reduction_result->unlabeled_count,
            _last_supervised_dim_reduction_result->num_classes,
            _last_supervised_dim_reduction_result->class_names);

    // Auto-focus the output tensor
    if (_selection_context && !_last_supervised_dim_reduction_result->output_key.empty()) {
        SelectionSource const source{
                _state ? EditorLib::EditorInstanceId(_state->getInstanceId())
                       : EditorLib::EditorInstanceId{},
                QStringLiteral("DimReductionPanel")};

        _selection_context->setDataFocus(
                EditorLib::SelectedDataKey(
                        QString::fromStdString(
                                _last_supervised_dim_reduction_result->output_key)),
                QStringLiteral("TensorData"),
                source);
    }

    // Refresh tensor list in the panel so the new output appears
    _dim_reduction_panel->refreshTensorList();
}

// =============================================================================
// Supervised Dim Reduction — UI state management
// =============================================================================

void MLCoreWidget::_setSupervisedDimReductionPipelineRunning(bool running) {
    _supervised_dim_reduction_pipeline_running = running;
    _dim_reduction_progress_bar->setVisible(running);

    if (running) {
        _dim_reduction_status_label->setText(
                QStringLiteral("Starting supervised dim reduction pipeline..."));
    }

    _dim_reduction_panel->setEnabled(!running);
}

// =============================================================================
// Entity selection via SelectionContext (task 5.4)
// =============================================================================

void MLCoreWidget::_selectEntitiesInGroup(int group_id) {
    if (!_selection_context || !_data_manager) {
        return;
    }

    auto * egm = _data_manager->getEntityGroupManager();
    if (!egm) {
        return;
    }

    auto const entity_ids = egm->getEntitiesInGroup(static_cast<GroupId>(group_id));
    if (entity_ids.empty()) {
        return;
    }

    // Convert EntityId → int64_t for SelectionContext
    std::vector<int64_t> selection;
    selection.reserve(entity_ids.size());
    for (auto const & eid: entity_ids) {
        selection.push_back(static_cast<int64_t>(eid.id));
    }

    SelectionSource const source{
            _state ? EditorLib::EditorInstanceId(_state->getInstanceId())
                   : EditorLib::EditorInstanceId{},
            QStringLiteral("MLCoreWidget")};

    _selection_context->setSelectedEntities(selection, source);
}
