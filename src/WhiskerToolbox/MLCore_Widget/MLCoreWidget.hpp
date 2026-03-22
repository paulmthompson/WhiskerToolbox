#ifndef MLCORE_WIDGET_HPP
#define MLCORE_WIDGET_HPP

/**
 * @file MLCoreWidget.hpp
 * @brief Main widget for the MLCore ML workflow UI
 *
 * MLCoreWidget is the top-level view widget for interactive machine learning
 * workflows in WhiskerToolbox. It provides a tab-based interface for:
 *
 * - **Classification**: Supervised classification using MLCore pipelines
 * - **Clustering**: Unsupervised clustering using MLCore pipelines
 * - **Dim Reduction**: Dimensionality reduction (PCA) using MLCore pipelines
 *
 * The Classification tab contains sub-panels for each workflow step:
 * FeatureSelectionPanel, RegionSelectionPanel,
 * LabelConfigPanel, ModelConfigPanel,
 * PredictionPanel, and ResultsPanel.
 *
 * The widget wires up the ClassificationPipeline: the "Train"
 * button in ModelConfigPanel triggers a full train+predict pipeline run in a
 * background thread. Progress is reported via a status label and progress bar.
 * On completion, results are displayed in ResultsPanel.
 *
 * ## SelectionContext Integration
 *
 * MLCoreWidget implements the DataFocusAware mixin for passive awareness:
 *
 * - **Incoming focus**: When another widget focuses a TensorData key via
 *   SelectionContext, the FeatureSelectionPanel auto-selects it. This lets
 *   users click a tensor in DataInspector and have it appear here.
 *
 * - **Outgoing focus**: When the user clicks an output data key in
 *   ResultsPanel (e.g., a predicted interval series or probability series),
 *   MLCoreWidget calls SelectionContext::setDataFocus so other widgets
 *   (DataViewer, DataInspector) can navigate to that output.
 *
 * ## Architecture
 *
 * MLCoreWidget follows the self-contained tool widget pattern:
 * - Single widget (no view/properties split)
 * - Placed in Zone::Right as a tabbed panel
 * - Consumes MLCoreWidgetState for serializable configuration
 * - Accesses DataManager for data keys and SelectionContext for focus awareness
 * - Inherits DataFocusAware for passive data focus handling
 *
 * @see MLCoreWidgetState for the state this widget observes
 * @see MLCoreWidgetRegistration for how this widget is created
 * @see ClassificationPipeline for the underlying supervised pipeline
 * @see ClusteringPipeline for the underlying unsupervised pipeline
 */

#include "EditorState/DataFocusAware.hpp"

#include <QWidget>

#include <memory>
#include <string>

class ClusteringPanel;
class ClusterOutputPanel;
class DataManager;
class DimReductionPanel;
class FeatureSelectionPanel;
class GroupManager;
class LabelConfigPanel;
class MLCoreWidgetState;
class ModelConfigPanel;
class PredictionPanel;
class QLabel;
class QProgressBar;
class RegionSelectionPanel;
class ResultsPanel;
class SelectionContext;

namespace MLCore {
class MLModelRegistry;
struct ClassificationPipelineConfig;
struct ClassificationPipelineResult;
struct ClusteringPipelineConfig;
struct ClusteringPipelineResult;
struct DimReductionPipelineConfig;
struct DimReductionPipelineResult;
}// namespace MLCore

class MLCoreWidget : public QWidget, public DataFocusAware {
    Q_OBJECT

public:
    /**
     * @brief Construct the ML workflow widget
     *
     * @param state Shared state object for serialization and signal coordination
     * @param data_manager Shared DataManager for accessing data keys
     * @param selection_context SelectionContext for passive data focus awareness
     * @param group_manager GroupManager for group colors and entity tracking (nullable)
     * @param parent Optional parent widget
     */
    explicit MLCoreWidget(std::shared_ptr<MLCoreWidgetState> state,
                          std::shared_ptr<DataManager> data_manager,
                          SelectionContext * selection_context,
                          GroupManager * group_manager = nullptr,
                          QWidget * parent = nullptr);

    ~MLCoreWidget() override;

    // === DataFocusAware interface ===

    /**
     * @brief Respond to data focus changes from other widgets
     *
     * When a TensorData key is focused elsewhere, auto-selects it in the
     * FeatureSelectionPanel. This provides passive awareness so users can
     * click a tensor in DataInspector and have it appear in the ML workflow.
     *
     * @param data_key The newly focused data key
     * @param data_type The type of the focused data (e.g., "TensorData")
     */
    void onDataFocusChanged(EditorLib::SelectedDataKey const & data_key,
                            QString const & data_type) override;

signals:
    /**
     * @brief Emitted from the worker thread to report classification pipeline progress
     *
     * Connected to _onPipelineProgress via Qt::QueuedConnection to safely
     * update UI from the main thread.
     */
    void _pipelineProgressReported(int stage_index, QString message);

    /**
     * @brief Emitted when the classification pipeline finishes (success or failure)
     */
    void _pipelineFinished();

    /**
     * @brief Emitted from the worker thread to report clustering pipeline progress
     */
    void _clusteringProgressReported(int stage_index, QString message);

    /**
     * @brief Emitted when the clustering pipeline finishes (success or failure)
     */
    void _clusteringPipelineFinished();

    /**
     * @brief Emitted from the worker thread to report dim reduction pipeline progress
     */
    void _dimReductionProgressReported(int stage_index, QString message);

    /**
     * @brief Emitted when the dim reduction pipeline finishes
     */
    void _dimReductionPipelineFinished();

private slots:
    void _onTrainRequested();
    void _onPredictRequested();
    void _onPipelineProgress(int stage_index, QString const & message);
    void _onPipelineComplete();

    void _onClusteringFitRequested();
    void _onClusteringProgress(int stage_index, QString const & message);
    void _onClusteringPipelineComplete();

    void _onDimReductionRunRequested();
    void _onDimReductionProgress(int stage_index, QString const & message);
    void _onDimReductionPipelineComplete();

private:
    void _setupUi();
    void _connectSignals();

    [[nodiscard]] bool _validatePanels() const;
    [[nodiscard]] MLCore::ClassificationPipelineConfig _buildPipelineConfig() const;
    void _runPipelineAsync(MLCore::ClassificationPipelineConfig config);
    void _setPipelineRunning(bool running);

    [[nodiscard]] bool _validateClusteringPanels() const;
    [[nodiscard]] MLCore::ClusteringPipelineConfig _buildClusteringPipelineConfig() const;
    void _runClusteringPipelineAsync(MLCore::ClusteringPipelineConfig config);
    void _setClusteringPipelineRunning(bool running);

    [[nodiscard]] bool _validateDimReductionPanels() const;
    [[nodiscard]] MLCore::DimReductionPipelineConfig _buildDimReductionPipelineConfig() const;
    void _runDimReductionPipelineAsync(MLCore::DimReductionPipelineConfig config);
    void _setDimReductionPipelineRunning(bool running);

    /**
     * @brief Select all entities in a group via SelectionContext
     *
     * When the user clicks a putative group entry in ClusterOutputPanel or
     * ResultsPanel, this method retrieves the group's member entities from
     * EntityGroupManager and calls SelectionContext::setSelectedEntities().
     *
     * @param group_id The GroupId whose entities should be selected
     */
    void _selectEntitiesInGroup(int group_id);

    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    SelectionContext * _selection_context;
    GroupManager * _group_manager = nullptr;

    // Sub-panels (Classification)
    FeatureSelectionPanel * _feature_panel = nullptr;
    RegionSelectionPanel * _training_region_panel = nullptr;
    LabelConfigPanel * _label_panel = nullptr;
    ModelConfigPanel * _model_config_panel = nullptr;
    PredictionPanel * _prediction_panel = nullptr;
    ResultsPanel * _results_panel = nullptr;

    // Sub-panels (Clustering)
    ClusteringPanel * _clustering_panel = nullptr;
    ClusterOutputPanel * _cluster_output_panel = nullptr;

    // Sub-panels (Dim Reduction)
    DimReductionPanel * _dim_reduction_panel = nullptr;

    // Progress UI (Classification)
    QLabel * _status_label = nullptr;
    QProgressBar * _progress_bar = nullptr;

    // Progress UI (Clustering)
    QLabel * _clustering_status_label = nullptr;
    QProgressBar * _clustering_progress_bar = nullptr;

    // Progress UI (Dim Reduction)
    QLabel * _dim_reduction_status_label = nullptr;
    QProgressBar * _dim_reduction_progress_bar = nullptr;

    // Pipeline infrastructure
    std::unique_ptr<MLCore::MLModelRegistry> _registry;
    std::unique_ptr<MLCore::ClassificationPipelineResult> _last_result;
    std::unique_ptr<MLCore::ClusteringPipelineResult> _last_clustering_result;
    std::unique_ptr<MLCore::DimReductionPipelineResult> _last_dim_reduction_result;
    bool _pipeline_running = false;
    bool _clustering_pipeline_running = false;
    bool _dim_reduction_pipeline_running = false;
};

#endif// MLCORE_WIDGET_HPP
