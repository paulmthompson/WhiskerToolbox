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
 *    n_components and scale parameters.
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

#include <memory>
#include <string>

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

    /// Whether to scale features (PCA-specific)
    [[nodiscard]] bool scaleFeatures() const;

    /// Whether to z-score normalize features
    [[nodiscard]] bool zscoreNormalize() const;

    /// Get the output key for the reduced TensorData
    [[nodiscard]] std::string outputKey() const;

    /// Whether the panel has valid configuration for running
    [[nodiscard]] bool hasValidConfiguration() const;

    /// Create parameters for the selected algorithm
    [[nodiscard]] std::unique_ptr<MLCore::MLModelParametersBase> currentParameters() const;

    /// Show results after a successful pipeline run
    void showResults(std::size_t num_observations,
                     std::size_t num_input_features,
                     std::size_t num_output_components,
                     std::size_t rows_dropped,
                     std::vector<double> const & explained_variance);

    /// Clear any displayed results
    void clearResults();

public slots:
    /// Refresh TensorData key list from DataManager
    void refreshTensorList();

signals:
    /// Emitted when the user clicks "Run Reduction"
    void runRequested();

    /// Emitted when the selected tensor changes
    void tensorSelectionChanged(QString const & key);

private slots:
    void _onTensorComboChanged(int index);
    void _onAlgorithmChanged(int index);
    void _onRunClicked();

private:
    void _setupConnections();
    void _populateAlgorithms();
    void _updateTensorInfo();
    void _registerDataManagerObserver();
    void _restoreFromState();
    void _syncToState();
    void _updateOutputKeyFromInput();

    Ui::DimReductionPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<MLCore::MLModelRegistry> _registry;
    int _dm_observer_id = -1;
    bool _updating = false;
};

#endif// DIM_REDUCTION_PANEL_HPP
