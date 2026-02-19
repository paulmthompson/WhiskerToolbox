#ifndef MODEL_CONFIG_PANEL_HPP
#define MODEL_CONFIG_PANEL_HPP

/**
 * @file ModelConfigPanel.hpp
 * @brief Panel for configuring ML model type, parameters, and class balancing
 *
 * ModelConfigPanel provides the model configuration UI for the Classification
 * tab in MLCoreWidget. It contains:
 *
 * 1. **Task type selector** — combo box filtering by MLTaskType (Binary
 *    Classification, Multi-Class Classification, Clustering). Changing the
 *    task type filters the algorithm combo box to show only compatible models.
 *
 * 2. **Algorithm selector** — combo box listing models from MLModelRegistry
 *    that match the selected task type. Changing the algorithm switches the
 *    stacked parameter widget to show the correct parameter form.
 *
 * 3. **Per-model parameter widgets** — a QStackedWidget with one page per
 *    model type, each exposing the algorithm-specific knobs defined in
 *    MLModelParameters.hpp (num_trees, epsilon, K, etc.).
 *
 * 4. **Class balancing** — embedded controls for enabling/disabling class
 *    balancing, selecting the balancing strategy (Subsample, Oversample),
 *    and configuring the max ratio. Maps to MLCore::BalancingConfig.
 *
 * 5. **Train button** — initiates training. Emits `trainRequested()` signal
 *    for the parent widget to wire up to the ClassificationPipeline.
 *
 * ## State Integration
 *
 * The panel reads/writes these MLCoreWidgetState fields:
 * - `selected_model_name` — the algorithm registry name
 * - `model_parameters_json` — serialized per-model parameters
 * - `balancing_enabled` — whether class balancing is active
 * - `balancing_strategy` — "subsample" or "oversample"
 * - `balancing_max_ratio` — max majority/minority ratio
 *
 * ## Architecture
 *
 * The panel uses the MLModelRegistry from MLCore to enumerate available models
 * and create fresh instances for parameter introspection. It does NOT use the
 * legacy MLParameterWidgetBase pattern (which depends on legacy MLModelParameters).
 * Instead, each page is a standard QWidget with spin boxes that directly
 * create MLCore::*Parameters structs.
 *
 * @see MLCoreWidget for the parent widget
 * @see MLCoreWidgetState for the persisted model configuration
 * @see MLModelRegistry for the model factory
 * @see MLModelParameters.hpp for the parameter struct definitions
 * @see ClassBalancing.hpp for BalancingConfig/BalancingStrategy
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
enum class BalancingStrategy;
} // namespace MLCore

namespace Ui {
class ModelConfigPanel;
}

class ModelConfigPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the model configuration panel
     *
     * @param state Shared MLCoreWidgetState for reading/writing model settings
     * @param data_manager Shared DataManager (reserved for future use)
     * @param parent Optional parent widget
     */
    explicit ModelConfigPanel(std::shared_ptr<MLCoreWidgetState> state,
                              std::shared_ptr<DataManager> data_manager,
                              QWidget * parent = nullptr);

    ~ModelConfigPanel() override;

    /**
     * @brief Get the currently selected model name
     *
     * @return The MLModelRegistry name (e.g. "Random Forest"), or empty if none
     */
    [[nodiscard]] std::string selectedModelName() const;

    /**
     * @brief Create a parameter object matching the current UI settings
     *
     * Returns a concrete MLModelParametersBase subclass populated from the
     * spin boxes on the currently visible parameter page.
     *
     * @return Owned parameter object, or nullptr if no model is selected
     */
    [[nodiscard]] std::unique_ptr<MLCore::MLModelParametersBase> currentParameters() const;

    /**
     * @brief Whether class balancing is enabled
     */
    [[nodiscard]] bool isBalancingEnabled() const;

    /**
     * @brief Get the selected balancing strategy
     */
    [[nodiscard]] MLCore::BalancingStrategy balancingStrategy() const;

    /**
     * @brief Get the balancing max ratio
     */
    [[nodiscard]] double balancingMaxRatio() const;

    /**
     * @brief Whether a valid model configuration is ready for training
     *
     * True when an algorithm is selected and the parameter page is populated.
     */
    [[nodiscard]] bool hasValidConfiguration() const;

signals:
    /**
     * @brief Emitted when the user clicks the Train button
     *
     * The parent widget should connect this to trigger ClassificationPipeline
     * or ClusteringPipeline execution.
     */
    void trainRequested();

    /**
     * @brief Emitted when the selected model changes
     *
     * @param name The new model name from the registry
     */
    void modelChanged(QString const & name);

    /**
     * @brief Emitted when any parameter value changes
     */
    void parametersChanged();

    /**
     * @brief Emitted when balancing settings change
     */
    void balancingChanged();

private slots:
    void _onTaskTypeChanged(int index);
    void _onAlgorithmChanged(int index);
    void _onTrainClicked();
    void _onBalancingToggled(bool enabled);
    void _onParameterValueChanged();

private:
    void _setupConnections();
    void _populateTaskTypes();
    void _populateAlgorithms();
    void _populateBalancingStrategies();
    void _switchParameterPage(std::string const & model_name);
    void _updateBalancingVisibility();
    void _syncToState();
    void _restoreFromState();

    /// Map model name → parameter stack page index
    [[nodiscard]] int _pageIndexForModel(std::string const & name) const;

    Ui::ModelConfigPanel * ui;
    std::shared_ptr<MLCoreWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;

    /// Cached registry instance for model enumeration
    std::unique_ptr<MLCore::MLModelRegistry> _registry;

    /// Suppress state writes during programmatic updates
    bool _updating = false;
};

#endif // MODEL_CONFIG_PANEL_HPP
