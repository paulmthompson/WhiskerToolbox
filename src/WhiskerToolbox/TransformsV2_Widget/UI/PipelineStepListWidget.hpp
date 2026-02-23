#ifndef WHISKERTOOLBOX_PIPELINE_STEP_LIST_WIDGET_HPP
#define WHISKERTOOLBOX_PIPELINE_STEP_LIST_WIDGET_HPP

/**
 * @file PipelineStepListWidget.hpp
 * @brief Widget for displaying and managing pipeline steps
 *
 * Shows the current pipeline steps in a table with columns for step number,
 * input type, transform name, and output type.
 *
 * Below the steps table is an inline "Available Transforms" browser table
 * that shows compatible transforms based on the current chain output type.
 * Double-clicking a row in the browser adds the transform as a new step.
 *
 * All type-chain resolution (validation, ragged-tracking, container naming)
 * is delegated to @ref resolveTypeChain, which mirrors the logic in
 * TransformPipeline::execute() without requiring actual data.
 */

#include "TransformsV2/core/TypeChainResolver.hpp"

#include <QWidget>

#include <string>
#include <typeindex>
#include <vector>

class QTableWidget;
class QLabel;
class QPushButton;

namespace WhiskerToolbox::Transforms::V2::Examples {
struct PipelineStepDescriptor;
} // namespace WhiskerToolbox::Transforms::V2::Examples

/**
 * @brief Represents a single step in the pipeline UI
 */
struct PipelineStepEntry {
    std::string step_id;                ///< Unique step identifier
    std::string transform_name;         ///< Name of the transform
    std::string parameters_json;        ///< Current parameters as JSON
    std::type_index input_type{typeid(void)};  ///< Expected input element type
    std::type_index output_type{typeid(void)}; ///< Produced output element type
    bool is_valid = true;               ///< Whether this step is type-compatible
    bool is_container_transform = false;///< Whether this is a container-level transform

    PipelineStepEntry() = default;
};

/**
 * @brief Widget for managing the ordered list of pipeline steps
 *
 * Provides:
 * - Steps table with columns: #, Input Type, Transform, Output Type
 * - Inline "Available Transforms" browser (no popup dialog)
 * - [Remove] button for the selected step
 * - Inline validation indicators (red background on incompatible steps)
 * - Selection → emits stepSelected() for the config panel
 *
 * Double-clicking a row in the Available Transforms table adds it to the pipeline.
 */
class PipelineStepListWidget : public QWidget {
    Q_OBJECT

public:
    explicit PipelineStepListWidget(QWidget * parent = nullptr);
    ~PipelineStepListWidget() override;

    /**
     * @brief Set the input element type for the pipeline
     *
     * This determines which transforms are available for the first step.
     * @param element_type The element type (e.g., typeid(Mask2D))
     * @param container_type The container type (e.g., typeid(MaskData))
     */
    void setInputType(std::type_index element_type, std::type_index container_type);

    /**
     * @brief Get the current output element type of the pipeline
     * @return The output type after the last step, or the input type if empty
     */
    [[nodiscard]] std::type_index currentOutputElementType() const;

    /**
     * @brief Get the current output container type of the pipeline
     * @return The output container type after the last step
     */
    [[nodiscard]] std::type_index currentOutputContainerType() const;

    /**
     * @brief Get all pipeline steps
     */
    [[nodiscard]] std::vector<PipelineStepEntry> const & steps() const { return _steps; }

    /**
     * @brief Get the currently selected step index
     * @return The index or -1 if none selected
     */
    [[nodiscard]] int selectedStepIndex() const;

    /**
     * @brief Clear all steps
     */
    void clearSteps();

    /**
     * @brief Add a step with the given transform name and optional params JSON
     * @param transform_name The registered transform name
     * @param params_json Optional parameters as JSON string
     * @return true if the step was successfully added
     */
    bool addStep(std::string const & transform_name, std::string const & params_json = "{}");

    /**
     * @brief Update the parameters JSON for a specific step
     * @param step_index The index of the step to update
     * @param params_json The new parameters JSON
     */
    void updateStepParams(int step_index, std::string const & params_json);

    /**
     * @brief Rebuild all steps from a list of PipelineStepDescriptors
     *
     * Used by JSON → UI loading.  Clears existing steps, then adds
     * each step using the descriptor's transform_name and serialized parameters.
     *
     * @param descriptors The step descriptors from a parsed PipelineDescriptor
     * @return true if all steps were loaded successfully
     */
    bool loadFromDescriptors(
            std::vector<WhiskerToolbox::Transforms::V2::Examples::PipelineStepDescriptor> const & descriptors);

signals:
    void stepSelected(int step_index);
    void pipelineChanged();
    void validationChanged(bool all_valid);

private slots:
    void onRemoveStepClicked();
    void onStepSelectionChanged();
    void onAvailableTransformDoubleClicked(int row, int column);

private:
    /**
     * @brief Re-resolve the type chain and refresh all UI
     *
     * Calls resolveTypeChain(), updates per-step validity,
     * rebuilds the steps table, refreshes the browser, and
     * emits validationChanged().
     */
    void refreshChain();

    void rebuildStepsTable();
    void updateAvailableTransforms();
    std::vector<std::string> getCompatibleTransforms(
            std::type_index element_type,
            std::type_index container_type) const;
    void updateButtonStates();

    // --- Widgets ---
    QTableWidget * _steps_table = nullptr;
    QPushButton * _remove_button = nullptr;
    QLabel * _browser_label = nullptr;
    QTableWidget * _browser_table = nullptr;

    // --- Model ---
    std::vector<PipelineStepEntry> _steps;
    std::vector<std::string> _current_compatible; ///< Names in the browser table
    std::type_index _input_element_type{typeid(void)};
    std::type_index _input_container_type{typeid(void)};

    /// Cached result of the last resolveTypeChain() call
    WhiskerToolbox::Transforms::V2::TypeChainResult _chain_result;
};

#endif // WHISKERTOOLBOX_PIPELINE_STEP_LIST_WIDGET_HPP
