#ifndef CLUSTERING_PANEL_HPP
#define CLUSTERING_PANEL_HPP

/**
 * @file ClusteringPanel.hpp
 * @brief Panel for configuring and running unsupervised clustering workflows
 *
 * ClusteringPanel is the main panel for the Clustering tab in MLCoreWidget.
 * It provides a complete UI for selecting data, choosing a clustering algorithm,
 * configuring per-algorithm parameters, and launching a clustering pipeline run.
 *
 * ## Sections
 *
 * 1. **Data Source** — Combo box listing all TensorData keys in DataManager.
 *    Shows shape, column names, and row type for the selected tensor.
 *    Optional z-score normalization checkbox.
 *
 * 2. **Algorithm Configuration** — Combo box listing clustering algorithms
 *    from the MLModelRegistry (K-Means, DBSCAN, GMM). A QStackedWidget
 *    shows per-algorithm parameter widgets:
 *    - K-Means: K, max iterations, random seed
 *    - DBSCAN: epsilon, min points
 *    - GMM: K, max EM iterations, random seed
 *
 * 3. **Output Configuration** — Output prefix for group/data key names,
 *    checkboxes for writing intervals and probabilities.
 *
 * 4. **Fit & Assign button** — Triggers the clustering pipeline. Emits
 *    `fitRequested()` for the parent MLCoreWidget to wire up.
 *
 * ## State Integration
 *
 * The panel reads/writes these MLCoreWidgetState fields:
 * - `clustering_tensor_key` — the selected TensorData key
 * - `clustering_model_name` — the selected algorithm name
 * - `clustering_output_prefix` — output prefix string
 * - `clustering_write_intervals` — whether to write interval series
 * - `clustering_write_probabilities` — whether to write probability series
 * - `clustering_zscore_normalize` — whether to z-score normalize features
 *
 * ## Architecture
 *
 * Uses the MLModelRegistry to enumerate clustering models. Parameter widgets
 * directly create MLCore parameter structs (KMeansParameters, DBSCANParameters,
 * GMMParameters) without intermediate serialization.
 *
 * @see MLCoreWidget for the parent widget
 * @see MLCoreWidgetState for the persisted clustering configuration
 * @see MLModelRegistry for the model factory
 * @see MLModelParameters.hpp for parameter struct definitions
 * @see ClusteringPipeline for the underlying pipeline
 */

#include <QWidget>

#include <memory>
#include <string>

// Forward declarations
class DataManager;
class MLCoreWidgetState;

namespace MLCore {
class MLModelRegistry;
struct MLModelParametersBase;
} // namespace MLCore

namespace Ui {
class ClusteringPanel;
}

class ClusteringPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the clustering panel
     *
     * @param state Shared MLCoreWidgetState for reading/writing clustering settings
     * @param data_manager Shared DataManager for listing TensorData keys
     * @param parent Optional parent widget
     */
    explicit ClusteringPanel(std::shared_ptr<MLCoreWidgetState> state,
                             std::shared_ptr<DataManager> data_manager,
                             QWidget * parent = nullptr);

    ~ClusteringPanel() override;

    /**
     * @brief Get the currently selected tensor key for clustering
     *
     * @return The DataManager key of the selected TensorData, or empty string
     */
    [[nodiscard]] std::string selectedTensorKey() const;

    /**
     * @brief Get the currently selected clustering algorithm name
     *
     * @return The MLModelRegistry name (e.g. "K-Means"), or empty if none
     */
    [[nodiscard]] std::string selectedAlgorithmName() const;

    /**
     * @brief Create a parameter object matching the current UI settings
     *
     * Returns a concrete MLModelParametersBase subclass populated from the
     * current parameter form values.
     *
     * @return Owning pointer to the parameter object, or nullptr if no model selected
     */
    [[nodiscard]] std::unique_ptr<MLCore::MLModelParametersBase> currentParameters() const;

    /**
     * @brief Get the output prefix for clustering results
     *
     * @return The prefix string (e.g. "Cluster:")
     */
    [[nodiscard]] std::string outputPrefix() const;

    /**
     * @brief Whether to write cluster assignments as DigitalIntervalSeries
     */
    [[nodiscard]] bool writeIntervals() const;

    /**
     * @brief Whether to write cluster probabilities as AnalogTimeSeries
     */
    [[nodiscard]] bool writeProbabilities() const;

    /**
     * @brief Whether to z-score normalize features before clustering
     */
    [[nodiscard]] bool zscoreNormalize() const;

    /**
     * @brief Whether the panel has a valid configuration for running the pipeline
     *
     * Returns true if a tensor and algorithm are both selected.
     */
    [[nodiscard]] bool hasValidConfiguration() const;

public slots:
    /**
     * @brief Refresh the list of available TensorData keys from DataManager
     *
     * Called automatically on DataManager changes and via the refresh button.
     * Preserves the current selection if still available.
     */
    void refreshTensorList();

signals:
    /**
     * @brief Emitted when the user clicks "Fit & Assign"
     *
     * The parent MLCoreWidget (or future wiring) should connect this signal
     * to trigger the ClusteringPipeline.
     */
    void fitRequested();

    /**
     * @brief Emitted when the selected algorithm changes
     *
     * @param name The new algorithm name (may be empty)
     */
    void algorithmChanged(QString const & name);

    /**
     * @brief Emitted when the selected tensor changes
     *
     * @param key The new tensor key (may be empty)
     */
    void tensorSelectionChanged(QString const & key);

    /**
     * @brief Emitted when any parameter value changes
     */
    void parametersChanged();

private slots:
    void _onTensorComboChanged(int index);
    void _onAlgorithmChanged(int index);
    void _onFitClicked();
    void _onParameterValueChanged();

private:
    void _setupConnections();
    void _populateAlgorithms();
    void _updateTensorInfo();
    void _switchParameterPage(std::string const & model_name);
    [[nodiscard]] int _pageIndexForModel(std::string const & name) const;
    void _registerDataManagerObserver();
    void _restoreFromState();
    void _syncToState();

    Ui::ClusteringPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<MLCore::MLModelRegistry> _registry;
    int _dm_observer_id = -1;
    bool _updating = false;
};

#endif // CLUSTERING_PANEL_HPP
