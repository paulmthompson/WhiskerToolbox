#ifndef DATA_TRANSFORM_WIDGET_STATE_HPP
#define DATA_TRANSFORM_WIDGET_STATE_HPP

/**
 * @file DataTransformWidgetState.hpp
 * @brief State class for DataTransform_Widget
 * 
 * DataTransformWidgetState manages the serializable state for the DataTransform_Widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 * 
 * This is a key Phase 2.7 implementation that demonstrates a widget can operate
 * without an embedded Feature_Table_Widget by relying on SelectionContext for
 * input data selection.
 * 
 * State tracked:
 * - Selected input data key (from SelectionContext)
 * - Currently selected transform operation
 * - Last used output name
 * 
 * @see EditorState for base class documentation
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief Serializable data structure for DataTransformWidgetState
 * 
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataTransformWidgetStateData {
    std::string selected_input_data_key;  ///< Input data key for transform (from SelectionContext)
    std::string selected_operation;        ///< Currently selected transform operation name
    std::string last_output_name;          ///< Last used output name (for convenience)
    std::string instance_id;               ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Transform";  ///< User-visible name
};

/**
 * @brief State class for DataTransform_Widget
 * 
 * DataTransformWidgetState is the first EditorState subclass to fully rely on
 * SelectionContext for input data selection, eliminating the need for an
 * embedded Feature_Table_Widget.
 * 
 * ## Usage
 * 
 * ```cpp
 * // Create state (typically done in widget constructor)
 * auto state = std::make_shared<DataTransformWidgetState>();
 * workspace_manager->registerState(state);
 * 
 * // Connect to input data changes
 * connect(state.get(), &DataTransformWidgetState::selectedInputDataKeyChanged,
 *         this, &DataTransform_Widget::onInputDataChanged);
 * 
 * // Update input from SelectionContext
 * void onExternalSelectionChanged(SelectionSource const& source) {
 *     QString selected = selection_context->primarySelectedData();
 *     state->setSelectedInputDataKey(selected);
 * }
 * 
 * // Serialize for workspace save
 * std::string json = state->toJson();
 * ```
 * 
 * ## Integration with SelectionContext
 * 
 * Unlike widgets with embedded Feature_Table_Widget, DataTransform_Widget
 * receives its input data selection entirely from SelectionContext:
 * 
 * ```cpp
 * connect(_selection_context, &SelectionContext::selectionChanged,
 *         this, [this](SelectionSource const& source) {
 *     // Always respond to selection changes (except circular updates)
 *     if (source.editor_instance_id == _state->getInstanceId()) {
 *         return;
 *     }
 *     QString selected = _selection_context->primarySelectedData();
 *     _state->setSelectedInputDataKey(selected);
 * });
 * ```
 */
class DataTransformWidgetState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DataTransformWidgetState
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager)
     */
    explicit DataTransformWidgetState(QObject * parent = nullptr);

    ~DataTransformWidgetState() override = default;

    // === Type Identification ===

    /**
     * @brief Get the type name for this state
     * @return "DataTransformWidget"
     */
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("DataTransformWidget"); }

    /**
     * @brief Get the display name for UI
     * @return User-visible name (default: "Data Transform")
     */
    [[nodiscard]] QString getDisplayName() const override;

    /**
     * @brief Set the display name
     * @param name New display name
     */
    void setDisplayName(QString const & name) override;

    // === Serialization ===

    /**
     * @brief Serialize state to JSON
     * @return JSON string representation
     */
    [[nodiscard]] std::string toJson() const override;

    /**
     * @brief Restore state from JSON
     * @param json JSON string to parse
     * @return true if parsing succeeded
     */
    bool fromJson(std::string const & json) override;

    // === State Properties ===

    /**
     * @brief Set the selected input data key
     * 
     * This represents the data object that will be transformed.
     * Typically set from SelectionContext when user selects data
     * in DataManager_Widget or other widgets.
     * 
     * @param key Data key to use as input (empty string to clear)
     */
    void setSelectedInputDataKey(QString const & key);

    /**
     * @brief Get the selected input data key
     * @return Currently selected input data key, or empty string if none
     */
    [[nodiscard]] QString selectedInputDataKey() const;

    /**
     * @brief Set the selected transform operation
     * 
     * This is the name of the transform operation to apply.
     * 
     * @param operation Operation name (e.g., "Calculate Area", "Filter")
     */
    void setSelectedOperation(QString const & operation);

    /**
     * @brief Get the selected transform operation
     * @return Currently selected operation name, or empty string if none
     */
    [[nodiscard]] QString selectedOperation() const;

    /**
     * @brief Set the last used output name
     * 
     * Convenience for restoring the output name field.
     * 
     * @param name Output name
     */
    void setLastOutputName(QString const & name);

    /**
     * @brief Get the last used output name
     * @return Last output name, or empty string if none
     */
    [[nodiscard]] QString lastOutputName() const;

signals:
    /**
     * @brief Emitted when the selected input data key changes
     * @param key The new input data key
     */
    void selectedInputDataKeyChanged(QString const & key);

    /**
     * @brief Emitted when the selected operation changes
     * @param operation The new operation name
     */
    void selectedOperationChanged(QString const & operation);

    /**
     * @brief Emitted when the last output name changes
     * @param name The new output name
     */
    void lastOutputNameChanged(QString const & name);

private:
    DataTransformWidgetStateData _data;
};

#endif // DATA_TRANSFORM_WIDGET_STATE_HPP
