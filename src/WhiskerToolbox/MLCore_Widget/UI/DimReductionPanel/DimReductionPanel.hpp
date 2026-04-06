/**
 * @file DimReductionPanel.hpp
 * @brief Panel for configuring and running dimensionality reduction workflows
 *
 * DimReductionPanel is the main panel for the Dim Reduction tab in MLCoreWidget.
 * It provides UI for selecting data, choosing a reduction algorithm (PCA),
 * configuring parameters, and launching a reduction pipeline run.
 *
 * ## Sections
 *
 * 1. **Data Source** — Combo box listing all TensorData keys.
 *    Shows shape info. Optional z-score normalization.
 *
 * 2. **Algorithm Configuration** — Algorithm combo (currently PCA) with
 *    n_components parameter.
 *
 * 3. **Output Configuration** — Output key for the reduced TensorData.
 *
 * 4. **Run button** — Triggers the pipeline. Emits `runRequested()`.
 *
 * 5. **Results** — Shows explained variance ratio after completion.
 *
 * @see MLCoreWidget for the parent widget
 * @see MLCoreWidgetState for persisted dim reduction configuration
 * @see DimReductionPipeline for the underlying pipeline
 * @see PCAOperation for the PCA algorithm
 */

#ifndef DIM_REDUCTION_PANEL_HPP
#define DIM_REDUCTION_PANEL_HPP

#include <QWidget>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class MLCoreWidgetState;

namespace MLCore {
class MLModelRegistry;
struct MLModelParametersBase;
}// namespace MLCore

namespace Ui {
class DimReductionPanel;
}

class DimReductionPanel : public QWidget {
    Q_OBJECT

public:
    explicit DimReductionPanel(std::shared_ptr<MLCoreWidgetState> state,
                               std::shared_ptr<DataManager> data_manager,
                               QWidget * parent = nullptr);

    ~DimReductionPanel() override;

    /// Get the selected TensorData key
    [[nodiscard]] std::string selectedTensorKey() const;

    /// Get the selected algorithm name
    [[nodiscard]] std::string selectedAlgorithmName() const;

    /// Get the number of output components
    [[nodiscard]] int nComponents() const;

    /// Whether to z-score normalize features
    [[nodiscard]] bool zscoreNormalize() const;

    /// Get the output key for the reduced TensorData
    [[nodiscard]] std::string outputKey() const;

    /// Whether the panel is in supervised mode
    [[nodiscard]] bool isSupervisedMode() const;

    /// Whether the panel has valid configuration for running
    [[nodiscard]] bool hasValidConfiguration() const;

    /// Create parameters for the selected algorithm
    [[nodiscard]] std::unique_ptr<MLCore::MLModelParametersBase> currentParameters() const;

    // === Supervised mode accessors ===

    /// Get the label source type ("intervals", "groups", "entity_groups", "events")
    [[nodiscard]] std::string labelSourceType() const;

    /// Get the selected interval key (for interval label mode)
    [[nodiscard]] std::string labelIntervalKey() const;

    /// Get the positive class name (for interval/event modes)
    [[nodiscard]] std::string labelPositiveClassName() const;

    /// Get the negative class name (for interval/event modes)
    [[nodiscard]] std::string labelNegativeClassName() const;

    /// Get the selected event key (for event label mode)
    [[nodiscard]] std::string labelEventKey() const;

    /// Get the selected group IDs (for group label modes)
    [[nodiscard]] std::vector<uint64_t> selectedGroupIds() const;

    /// Get the selected data key (for data entity group mode)
    [[nodiscard]] std::string labelDataKey() const;

    /// Show results after a successful unsupervised pipeline run
    void showResults(std::size_t num_observations,
                     std::size_t num_input_features,
                     std::size_t num_output_components,
                     std::size_t rows_dropped,
                     std::vector<double> const & explained_variance);

    /// Show results after a successful supervised pipeline run
    void showSupervisedResults(std::size_t num_observations,
                               std::size_t num_training_observations,
                               std::size_t num_input_features,
                               std::size_t num_output_dimensions,
                               std::size_t rows_dropped,
                               std::size_t unlabeled_count,
                               std::size_t num_classes,
                               std::vector<std::string> const & class_names);

    /// Clear any displayed results
    void clearResults();

public slots:
    /// Refresh TensorData key list from DataManager
    void refreshTensorList();

    /// Refresh label source combos from DataManager
    void refreshLabelSources();

signals:
    /// Emitted when the user clicks "Run Reduction" in unsupervised mode
    void runRequested();

    /// Emitted when the user clicks "Run Reduction" in supervised mode
    void supervisedRunRequested();

    /// Emitted when the selected tensor changes
    void tensorSelectionChanged(QString const & key);

private slots:
    void _onTensorComboChanged(int index);
    void _onAlgorithmChanged(int index);
    void _onRunClicked();
    void _onModeToggled(bool supervised);
    void _onLabelSourceChanged(int index);
    void _onAddGroupClicked();
    void _onRemoveGroupClicked();
    void _onAddDataGroupClicked();
    void _onRemoveDataGroupClicked();

private:
    void _setupConnections();
    void _populateAlgorithms();
    void _updateTensorInfo();
    void _registerDataManagerObserver();
    void _restoreFromState();
    void _syncToState();
    void _updateOutputKeyFromInput();
    void _updateSupervisedVisibility(bool supervised);
    void _populateLabelSourceCombo();
    void _refreshIntervalCombo();
    void _refreshGroupCombo();
    void _refreshDataKeyCombo();
    void _refreshDataGroupCombo();
    void _refreshEventCombo();
    void _rebuildGroupClassList();
    void _rebuildDataGroupClassList();
    void _syncGroupIdsToState();

    Ui::DimReductionPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<MLCore::MLModelRegistry> _registry;
    int _dm_observer_id = -1;
    int _group_observer_id = -1;
    bool _updating = false;

    /// Currently selected group IDs for entity groups mode
    std::vector<uint64_t> _selected_group_ids;

    /// Currently selected group IDs for data entity groups mode
    std::vector<uint64_t> _selected_data_group_ids;
};

#endif// DIM_REDUCTION_PANEL_HPP
