#ifndef RESULTS_PANEL_HPP
#define RESULTS_PANEL_HPP

/**
 * @file ResultsPanel.hpp
 * @brief Panel for displaying ML classification/clustering results
 *
 * ResultsPanel shows the outcome of a ClassificationPipeline (or
 * ClusteringPipeline) run. It contains:
 *
 * 1. **Model & training summary** — model name, observation count, feature
 *    count, class count, whether balancing was applied.
 *
 * 2. **Classification metrics** — accuracy, sensitivity/recall, specificity,
 *    F1/Dice score displayed as percentages. 
 *
 * 3. **Confusion matrix** — monospaced text display of the binary confusion
 *    matrix (TP, TN, FP, FN) or multi-class confusion matrix.
 *
 * 4. **Output data keys list** — a clickable QListWidget showing all data keys
 *    created by the PredictionWriter (interval series, probability series,
 *    putative groups). Clicking an item emits `outputKeyClicked()` so the
 *    parent widget can request focus in a DataViewer.
 *
 * ## State
 *
 * ResultsPanel does not persist any state in MLCoreWidgetState — results
 * are ephemeral and regenerated on each pipeline run.
 *
 * ## Integration
 *
 * - Embedded in the Classification tab of MLCoreWidget below PredictionPanel
 * - Populated by calling `showClassificationResult()` after a pipeline run
 * - Cleared via `clearResults()` or the Clear button
 * - The `outputKeyClicked()` signal can be wired to SelectionContext or
 *   DataViewer for inter-widget navigation
 *
 * @see MLCoreWidget for the parent widget
 * @see ClassificationPipeline for the pipeline whose results are displayed
 * @see PredictionWriter for the output keys shown in the list
 * @see MLCore::BinaryClassificationMetrics for the binary metrics type
 * @see MLCore::MultiClassMetrics for the multi-class metrics type
 */

#include <QWidget>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace arma {
template<typename eT>
class Mat;
using mat = Mat<double>;
}// namespace arma

namespace MLCore {
struct BinaryClassificationMetrics;
struct MultiClassMetrics;
struct ClassificationPipelineResult;
struct PredictionWriterResult;
}// namespace MLCore

class GroupManager;

namespace Ui {
class ResultsPanel;
}

class ResultsPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the results panel
     *
     * @param group_manager Optional GroupManager for color display and entity tracking
     * @param parent Optional parent widget
     */
    explicit ResultsPanel(GroupManager * group_manager = nullptr,
                          QWidget * parent = nullptr);

    ~ResultsPanel() override;

    // =========================================================================
    // Public API — display results
    // =========================================================================

    /**
     * @brief Display results from a completed classification pipeline run
     *
     * Populates the model summary, metrics display, confusion matrix, and
     * output keys list from the pipeline result.
     *
     * @param result The completed pipeline result (must have success == true)
     * @param model_name Human-readable name of the model used
     */
    void showClassificationResult(MLCore::ClassificationPipelineResult const & result,
                                  std::string const & model_name);

    /**
     * @brief Display binary classification metrics directly
     *
     * Use when you have metrics but not a full pipeline result.
     *
     * @param metrics The binary metrics to display
     * @param model_name Human-readable model name
     * @param training_observations Number of training samples
     * @param num_features Number of features
     */
    void showBinaryMetrics(MLCore::BinaryClassificationMetrics const & metrics,
                           std::string const & model_name,
                           std::size_t training_observations = 0,
                           std::size_t num_features = 0);

    /**
     * @brief Display multi-class classification metrics
     *
     * @param metrics The multi-class metrics to display
     * @param class_names Names for each class (optional, indices used if empty)
     * @param model_name Human-readable model name
     * @param training_observations Number of training samples
     * @param num_features Number of features
     */
    void showMultiClassMetrics(MLCore::MultiClassMetrics const & metrics,
                               std::vector<std::string> const & class_names,
                               std::string const & model_name,
                               std::size_t training_observations = 0,
                               std::size_t num_features = 0);

    /**
     * @brief Set the list of output data keys to display
     *
     * @param interval_keys DataManager keys for DigitalIntervalSeries outputs
     * @param probability_keys DataManager keys for AnalogTimeSeries outputs
     * @param class_names Class names corresponding to the keys
     * @param putative_group_ids GroupIds of created putative groups (optional)
     */
    void setOutputKeys(std::vector<std::string> const & interval_keys,
                       std::vector<std::string> const & probability_keys,
                       std::vector<std::string> const & class_names,
                       std::vector<uint64_t> const & putative_group_ids = {});

    /**
     * @brief Clear all displayed results and return to the placeholder state
     */
    void clearResults();

    /**
     * @brief Display a transition matrix (for HMM or other sequence models)
     *
     * @param transition_matrix Square matrix where entry (i,j) is P(state j | state i)
     * @param class_names Optional class names for row/column headers
     */
    void showTransitionMatrix(arma::mat const & transition_matrix,
                              std::vector<std::string> const & class_names = {});

    /**
     * @brief Whether valid results are currently displayed
     */
    [[nodiscard]] bool hasResults() const;

signals:
    /**
     * @brief Emitted when the user clicks an output data key in the list
     *
     * The parent widget should wire this to a DataViewer focus mechanism
     * (e.g., SelectionContext::setDataFocus).
     *
     * @param key The DataManager key of the clicked output
     */
    void outputKeyClicked(QString const & key);

    /**
     * @brief Emitted when the user clicks a putative group entry in the list
     *
     * The parent widget should wire this to
     * SelectionContext::setSelectedEntities() using the group's members.
     *
     * @param group_id The GroupId of the clicked group
     */
    void groupClicked(int group_id);

    /**
     * @brief Emitted when results are cleared (via button or programmatically)
     */
    void resultsCleared();

private slots:
    void _onOutputItemClicked(class QListWidgetItem * item);
    void _onClearClicked();

private:
    void _showNoResultsState();
    void _showResultsState();

    void _updateBinaryMetricsDisplay(MLCore::BinaryClassificationMetrics const & metrics);
    void _updateMultiClassMetricsDisplay(MLCore::MultiClassMetrics const & metrics,
                                         std::vector<std::string> const & class_names);
    void _updateModelSummary(std::string const & model_name,
                             std::size_t training_observations,
                             std::size_t num_features,
                             std::size_t num_classes,
                             bool was_balanced);

    void _showValidationMetrics(MLCore::BinaryClassificationMetrics const & metrics,
                                std::size_t validation_observations);
    void _showValidationMultiClassMetrics(MLCore::MultiClassMetrics const & metrics,
                                          std::vector<std::string> const & class_names,
                                          std::size_t validation_observations);
    void _clearValidationWidgets();

    [[nodiscard]] static QString _formatConfusionMatrix(
            MLCore::BinaryClassificationMetrics const & metrics);

    [[nodiscard]] static QString _formatMultiClassConfusionMatrix(
            MLCore::MultiClassMetrics const & metrics,
            std::vector<std::string> const & class_names);

    [[nodiscard]] static QString _formatTransitionMatrix(
            arma::mat const & transition_matrix,
            std::vector<std::string> const & class_names);

    Ui::ResultsPanel * ui;
    GroupManager * _group_manager = nullptr;
    std::vector<uint64_t> _last_putative_group_ids;
    std::vector<QWidget *> _validation_widgets;///< Dynamically created validation metric widgets
    bool _has_results = false;
};

#endif// RESULTS_PANEL_HPP
