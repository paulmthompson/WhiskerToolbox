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
 *
 * The Classification tab contains sub-panels for each workflow step:
 * FeatureSelectionPanel (4.2), RegionSelectionPanel (4.3),
 * LabelConfigPanel (4.4), ModelConfigPanel (4.5),
 * PredictionPanel (4.6), and ResultsPanel (4.7).
 *
 * The widget wires up the ClassificationPipeline (task 4.8): the "Train"
 * button in ModelConfigPanel triggers a full train+predict pipeline run in a
 * background thread. Progress is reported via a status label and progress bar.
 * On completion, results are displayed in ResultsPanel.
 *
 * ## Architecture
 *
 * MLCoreWidget follows the self-contained tool widget pattern:
 * - Single widget (no view/properties split)
 * - Placed in Zone::Right as a tabbed panel
 * - Consumes MLCoreWidgetState for serializable configuration
 * - Accesses DataManager for data keys and SelectionContext for focus awareness
 *
 * @see MLCoreWidgetState for the state this widget observes
 * @see MLCoreWidgetRegistration for how this widget is created
 * @see ClassificationPipeline for the underlying supervised pipeline
 * @see ClusteringPipeline for the underlying unsupervised pipeline
 */

#include <QWidget>

#include <memory>
#include <string>

// Forward declarations
class DataManager;
class FeatureSelectionPanel;
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
} // namespace MLCore

class MLCoreWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the ML workflow widget
     *
     * @param state Shared state object for serialization and signal coordination
     * @param data_manager Shared DataManager for accessing data keys
     * @param selection_context SelectionContext for passive data focus awareness
     * @param parent Optional parent widget
     */
    explicit MLCoreWidget(std::shared_ptr<MLCoreWidgetState> state,
                          std::shared_ptr<DataManager> data_manager,
                          SelectionContext * selection_context,
                          QWidget * parent = nullptr);

    ~MLCoreWidget() override;

signals:
    /**
     * @brief Emitted from the worker thread to report pipeline progress
     *
     * Connected to _onPipelineProgress via Qt::QueuedConnection to safely
     * update UI from the main thread.
     */
    void _pipelineProgressReported(int stage_index, QString message);

    /**
     * @brief Emitted when the pipeline finishes (success or failure)
     */
    void _pipelineFinished();

private slots:
    void _onTrainRequested();
    void _onPredictRequested();
    void _onPipelineProgress(int stage_index, QString const & message);
    void _onPipelineComplete();

private:
    void _setupUi();
    void _connectSignals();

    [[nodiscard]] bool _validatePanels() const;
    [[nodiscard]] MLCore::ClassificationPipelineConfig _buildPipelineConfig() const;
    void _runPipelineAsync(MLCore::ClassificationPipelineConfig config);
    void _setPipelineRunning(bool running);

    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    SelectionContext * _selection_context;

    // Sub-panels
    FeatureSelectionPanel * _feature_panel = nullptr;
    RegionSelectionPanel * _training_region_panel = nullptr;
    LabelConfigPanel * _label_panel = nullptr;
    ModelConfigPanel * _model_config_panel = nullptr;
    PredictionPanel * _prediction_panel = nullptr;
    ResultsPanel * _results_panel = nullptr;

    // Progress UI
    QLabel * _status_label = nullptr;
    QProgressBar * _progress_bar = nullptr;

    // Pipeline infrastructure
    std::unique_ptr<MLCore::MLModelRegistry> _registry;
    std::unique_ptr<MLCore::ClassificationPipelineResult> _last_result;
    bool _pipeline_running = false;
};

#endif // MLCORE_WIDGET_HPP
