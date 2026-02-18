#ifndef WHISKERTOOLBOX_PIPELINE_STEP_LIST_WIDGET_HPP
#define WHISKERTOOLBOX_PIPELINE_STEP_LIST_WIDGET_HPP

/**
 * @file PipelineStepListWidget.hpp
 * @brief Widget for displaying and managing pipeline steps
 *
 * Shows the current pipeline steps in a list with add/remove/reorder controls.
 * Type-directed step suggestions filter available transforms based on the
 * current output type of the last step in the chain.
 */

#include <QWidget>

#include <optional>
#include <string>
#include <typeindex>
#include <vector>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QVBoxLayout;

namespace WhiskerToolbox::Transforms::V2 {
struct TransformMetadata;
} // namespace WhiskerToolbox::Transforms::V2

namespace WhiskerToolbox::Transforms::V2::Examples {
struct PipelineStepDescriptor;
} // namespace WhiskerToolbox::Transforms::V2::Examples

/**
 * @brief Represents a single step in the pipeline UI
 */
struct PipelineStepEntry {
    std::string step_id;                     ///< Unique step identifier
    std::string transform_name;              ///< Name of the transform
    std::string parameters_json;             ///< Current parameters as JSON
    std::type_index input_type;              ///< Expected input element type
    std::type_index output_type;             ///< Produced output element type
    bool is_valid = true;                    ///< Whether this step is type-compatible
    bool is_container_transform = false;     ///< Whether this is a container-level transform

    PipelineStepEntry()
        : input_type(typeid(void))
        , output_type(typeid(void)) {}
};

/**
 * @brief Widget for managing the ordered list of pipeline steps
 *
 * Provides:
 * - Visual list of steps with numbering
 * - [+] Add Step button that suggests type-compatible transforms
 * - [✕] Remove button per step
 * - [↑][↓] Reorder buttons
 * - Inline validation indicators (red border on incompatible steps)
 * - Selection → emits stepSelected() for the config panel
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
     * @return The output container type after the last step, or the input container type if empty
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
     * Used by JSON → UI loading (Phase 2). Clears existing steps, then adds
     * each step using the descriptor's transform_name and serialized parameters.
     *
     * @param descriptors The step descriptors from a parsed PipelineDescriptor
     * @return true if all steps were loaded successfully
     */
    bool loadFromDescriptors(
            std::vector<WhiskerToolbox::Transforms::V2::Examples::PipelineStepDescriptor> const & descriptors);

signals:
    /**
     * @brief Emitted when a step is selected in the list
     * @param step_index The index of the selected step (-1 if deselected)
     */
    void stepSelected(int step_index);

    /**
     * @brief Emitted when the pipeline changes (add/remove/reorder/params)
     */
    void pipelineChanged();

    /**
     * @brief Emitted when the type chain validation state changes
     * @param all_valid True if the entire chain is valid
     */
    void validationChanged(bool all_valid);

private slots:
    void onAddStepClicked();
    void onRemoveStepClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onSelectionChanged();

private:
    void rebuildListDisplay();
    void validateTypeChain();
    std::vector<std::string> getCompatibleTransforms(std::type_index element_type,
                                                      std::type_index container_type) const;
    void updateButtonStates();

    QListWidget * _list_widget = nullptr;
    QPushButton * _add_button = nullptr;
    QPushButton * _remove_button = nullptr;
    QPushButton * _move_up_button = nullptr;
    QPushButton * _move_down_button = nullptr;

    std::vector<PipelineStepEntry> _steps;
    std::type_index _input_element_type{typeid(void)};
    std::type_index _input_container_type{typeid(void)};
};

#endif // WHISKERTOOLBOX_PIPELINE_STEP_LIST_WIDGET_HPP
