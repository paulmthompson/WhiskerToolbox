#ifndef TRANSFORMS_V2_STATE_HPP
#define TRANSFORMS_V2_STATE_HPP

/**
 * @file TransformsV2State.hpp
 * @brief State class for TransformsV2_Widget
 *
 * TransformsV2State manages the serializable state for the TransformsV2 properties widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 *
 * @see EditorState for base class documentation
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>

class DataManager;

/**
 * @brief Serializable data structure for TransformsV2State
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct TransformsV2StateData {
    std::string instance_id;                   ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Transforms V2";///< User-visible name

    // Phase 1: Input state
    std::optional<std::string> input_data_key;///< Currently focused input data key

    // Phase 2: Pipeline JSON — single source of truth for pipeline state
    std::optional<std::string> pipeline_json;///< Current PipelineDescriptor as JSON

    // Phase 3: Output configuration
    std::optional<std::string> output_data_key; ///< Output data key for execution results
    std::string execution_mode = "data_manager";///< "data_manager" or "json_only"

    // Phase 2: UI preferences
    bool json_panel_expanded = false;///< Whether the JSON panel is expanded
};

/**
 * @brief State class for TransformsV2_Widget
 *
 * Manages state for the TransformsV2 properties panel which provides
 * a UI for the TransformsV2 transform pipeline architecture.
 */
class TransformsV2State : public EditorState {
    Q_OBJECT

public:
    explicit TransformsV2State(std::shared_ptr<DataManager> data_manager = nullptr,
                               QObject * parent = nullptr);
    ~TransformsV2State() override = default;

    // === Type Identification ===
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TransformsV2Widget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Accessors ===
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    // Phase 1: Input data key
    void setInputDataKey(std::string const & key);
    [[nodiscard]] std::optional<std::string> inputDataKey() const { return _data.input_data_key; }

    // Phase 2: Pipeline JSON
    void setPipelineJson(std::string const & json);
    [[nodiscard]] std::optional<std::string> pipelineJson() const { return _data.pipeline_json; }

    // Phase 3: Output data key
    void setOutputDataKey(std::string const & key);
    [[nodiscard]] std::optional<std::string> outputDataKey() const { return _data.output_data_key; }

    // Phase 3: Execution mode
    void setExecutionMode(std::string const & mode);
    [[nodiscard]] std::string executionMode() const { return _data.execution_mode; }

    // Phase 2: JSON panel expanded state
    void setJsonPanelExpanded(bool expanded);
    [[nodiscard]] bool jsonPanelExpanded() const { return _data.json_panel_expanded; }

signals:
    void inputDataKeyChanged(std::string const & key);
    void pipelineJsonChanged(std::string const & json);
    void outputDataKeyChanged(std::string const & key);

private:
    std::shared_ptr<DataManager> _data_manager;
    TransformsV2StateData _data;
};

#endif// TRANSFORMS_V2_STATE_HPP
