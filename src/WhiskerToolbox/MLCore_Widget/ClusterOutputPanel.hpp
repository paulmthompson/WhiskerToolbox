#ifndef CLUSTER_OUTPUT_PANEL_HPP
#define CLUSTER_OUTPUT_PANEL_HPP

/**
 * @file ClusterOutputPanel.hpp
 * @brief Panel for displaying unsupervised clustering pipeline results
 *
 * ClusterOutputPanel shows the outcome of a ClusteringPipeline run.
 * It is the clustering counterpart of ResultsPanel (which handles
 * classification results). It contains:
 *
 * 1. **Summary** — algorithm name, fitting observation count, feature count,
 *    number of clusters discovered, rows dropped due to NaN.
 *
 * 2. **Cluster assignment table** — a QTableWidget showing each cluster name,
 *    the number of observations assigned to it, and the fraction of the total.
 *
 * 3. **Noise info** — if the algorithm is density-based (DBSCAN), shows the
 *    count of noise points not assigned to any cluster.
 *
 * 4. **Created outputs list** — a clickable QListWidget showing all data keys
 *    created by the PredictionWriter (interval series, probability series,
 *    putative groups). Clicking an item emits `outputKeyClicked()` so the
 *    parent widget can request focus in a DataViewer.
 *
 * ## State
 *
 * ClusterOutputPanel does not persist any state in MLCoreWidgetState — results
 * are ephemeral and regenerated on each pipeline run.
 *
 * ## Integration
 *
 * - Embedded in the Clustering tab of MLCoreWidget below ClusteringPanel
 * - Populated by calling `showClusteringResult()` after a pipeline run
 * - Cleared via `clearResults()` or the Clear button
 * - The `outputKeyClicked()` signal can be wired to SelectionContext or
 *   DataViewer for inter-widget navigation
 *
 * @see MLCoreWidget for the parent widget
 * @see ClusteringPipeline for the pipeline whose results are displayed
 * @see PredictionWriter for the output keys shown in the list
 * @see ResultsPanel for the classification counterpart
 */

#include <QWidget>

#include <cstddef>
#include <string>
#include <vector>

namespace MLCore {
struct ClusteringPipelineResult;
}// namespace MLCore

class GroupManager;

namespace Ui {
class ClusterOutputPanel;
}

class ClusterOutputPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the cluster output panel
     *
     * @param group_manager Optional GroupManager for color display and entity tracking
     * @param parent Optional parent widget
     */
    explicit ClusterOutputPanel(GroupManager * group_manager = nullptr,
                                QWidget * parent = nullptr);

    ~ClusterOutputPanel() override;

    // =========================================================================
    // Public API — display results
    // =========================================================================

    /**
     * @brief Display results from a completed clustering pipeline run
     *
     * Populates the summary, cluster table, noise info, and output keys list
     * from the pipeline result.
     *
     * @param result The completed pipeline result (must have success == true)
     * @param algorithm_name Human-readable name of the algorithm used
     */
    void showClusteringResult(MLCore::ClusteringPipelineResult const & result,
                              std::string const & algorithm_name);

    /**
     * @brief Set the list of output data keys to display
     *
     * @param interval_keys DataManager keys for DigitalIntervalSeries outputs
     * @param probability_keys DataManager keys for AnalogTimeSeries outputs
     * @param cluster_names Cluster names corresponding to the keys
     * @param group_ids GroupIds of created putative groups
     */
    void setOutputKeys(std::vector<std::string> const & interval_keys,
                       std::vector<std::string> const & probability_keys,
                       std::vector<std::string> const & cluster_names,
                       std::vector<uint64_t> const & group_ids);

    /**
     * @brief Clear all displayed results and return to the placeholder state
     */
    void clearResults();

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

    void _updateSummary(std::string const & algorithm_name,
                        std::size_t fitting_observations,
                        std::size_t num_features,
                        std::size_t num_clusters,
                        std::size_t rows_dropped_nan);

    void _updateClusterTable(std::vector<std::string> const & cluster_names,
                             std::vector<std::size_t> const & cluster_sizes,
                             std::size_t total_assigned);

    void _updateNoiseInfo(std::size_t noise_points);

    Ui::ClusterOutputPanel * ui;
    GroupManager * _group_manager = nullptr;
    std::vector<uint64_t> _last_putative_group_ids;
    bool _has_results = false;
};

#endif// CLUSTER_OUTPUT_PANEL_HPP
