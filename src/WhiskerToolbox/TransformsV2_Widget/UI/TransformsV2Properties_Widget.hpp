#ifndef TRANSFORMS_V2_PROPERTIES_WIDGET_HPP
#define TRANSFORMS_V2_PROPERTIES_WIDGET_HPP

/**
 * @file TransformsV2Properties_Widget.hpp
 * @brief Properties panel widget for TransformsV2 transform pipelines
 *
 * This widget provides the UI for configuring and executing TransformsV2
 * transform pipelines. It is placed in the Right (properties) zone.
 *
 * Implements DataFocusAware to respond to data selection in DataManager_Widget.
 * When data is focused, the widget updates the input display and filters
 * available transforms to those compatible with the focused data type.
 */

#include "EditorState/DataFocusAware.hpp"
#include "EditorState/StrongTypes.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <typeindex>

namespace EditorLib {
class OperationContext;
} // namespace EditorLib

class TransformsV2State;
class SelectionContext;
class DataManager;
class PipelineStepListWidget;
class StepConfigPanel;
class PreReductionPanel;
class QLabel;
class QGroupBox;
class QComboBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTextEdit;
class QVBoxLayout;

namespace Ui {
class TransformsV2Properties_Widget;
}

/**
 * @brief Main properties widget for TransformsV2 pipeline builder
 *
 * Integrates:
 * - Input Selector (DataFocusAware) — responds to data selection
 * - Pipeline Step List — add/remove/reorder transform steps
 * - Step Configuration — auto-generated parameter forms
 * - Pre-Reduction Panel — pre-computation steps for pipeline
 * - Type Chain Validation — inline validation of the pipeline
 */
class TransformsV2Properties_Widget : public QWidget, public DataFocusAware {
    Q_OBJECT

public:
    explicit TransformsV2Properties_Widget(std::shared_ptr<TransformsV2State> state,
                                           SelectionContext * selection_context,
                                           QWidget * parent = nullptr);
    ~TransformsV2Properties_Widget() override;

    // === DataFocusAware ===
    void onDataFocusChanged(EditorLib::SelectedDataKey const & data_key,
                            QString const & data_type) override;

    /**
     * @brief Set the OperationContext for inter-widget pipeline delivery
     *
     * When set, the widget shows a "Send Pipeline" button whenever
     * there is a pending operation from a consumer (e.g., TensorDesigner's
     * ColumnConfigDialog). Clicking the button delivers the current
     * pipeline JSON to the requester.
     *
     * @param context OperationContext instance (can be nullptr)
     */
    void setOperationContext(EditorLib::OperationContext * context);

signals:
    /**
     * @brief Emitted whenever the pipeline descriptor changes
     *
     * This signal fires on any change to the pipeline (UI edits or JSON edits).
     * External consumers (Phase 4) can connect to this for real-time updates.
     * @param pipeline_json The current pipeline descriptor as a JSON string
     */
    void pipelineDescriptorChanged(std::string const & pipeline_json);

private slots:
    void onStepSelected(int step_index);
    void onPipelineChanged();
    void onStepParametersChanged(std::string const & params_json);
    void onValidationChanged(bool all_valid);
    void onJsonPanelEdited();
    void onCopyJsonClicked();
    void onLoadJsonClicked();
    void onSaveJsonClicked();
    void onApplyJsonClicked();

    // Phase 3: Execution slots
    void onExecuteClicked();
    void onOutputKeyEdited(QString const & text);

    // Phase 6.4: OperationContext delivery
    void onDeliverPipelineClicked();
    void onPendingOperationChanged(EditorLib::EditorTypeId const & producer_type);

private:
    void setupUI();
    void updateInputDisplay();
    void resolveInputTypes();

    /**
     * @brief Build a PipelineDescriptor from the current UI state
     * @return JSON string representing the PipelineDescriptor
     */
    [[nodiscard]] std::string buildJsonFromUI() const;

    /**
     * @brief Sync the JSON panel text from the current UI state
     *
     * Called after any UI change (add/remove/reorder steps, parameter changes, etc.).
     * Suppresses feedback loops with _syncing_json guard.
     */
    void syncJsonFromUI();

    /**
     * @brief Rebuild the UI from a JSON string
     * @param json_str The PipelineDescriptor JSON to load
     * @return true if the JSON was valid and loaded successfully
     */
    bool loadUIFromJson(std::string const & json_str);

    /**
     * @brief Generate an output key name from the input key and last transform
     * @return Auto-generated output key string
     */
    [[nodiscard]] std::string generateOutputName() const;

    /**
     * @brief Update the output key line edit with an auto-generated name
     */
    void updateOutputKeyFromPipeline();

    /**
     * @brief Update execute button enabled state based on validation
     */
    void updateExecuteButtonState();

    /**
     * @brief Attempt to deliver the current pipeline JSON to a pending consumer
     *
     * If there is a pending operation for "TransformsV2Widget", delivers
     * the pipeline JSON via OperationContext.
     *
     * @return true if delivery succeeded
     */
    bool tryDeliverPipeline();

    /**
     * @brief Update visibility/state of the deliver button based on pending operations
     */
    void updateDeliverButtonState();

    /// Guard against feedback loops during bidirectional sync
    bool _syncing_json = false;

    std::unique_ptr<Ui::TransformsV2Properties_Widget> ui;
    std::shared_ptr<TransformsV2State> _state;
    SelectionContext * _selection_context = nullptr;

    // Input state
    std::string _input_data_key;
    std::string _input_data_type_name;  // e.g. "MaskData", "AnalogTimeSeries"
    std::type_index _input_element_type{typeid(void)};
    std::type_index _input_container_type{typeid(void)};

    // Sub-widgets
    QLabel * _input_key_label = nullptr;
    QLabel * _input_type_label = nullptr;
    QGroupBox * _input_group = nullptr;
    PipelineStepListWidget * _step_list = nullptr;
    StepConfigPanel * _step_config = nullptr;
    PreReductionPanel * _pre_reduction_panel = nullptr;
    QLabel * _validation_label = nullptr;

    // JSON Panel (Phase 2)
    QGroupBox * _json_group = nullptr;
    QTextEdit * _json_panel = nullptr;
    QPushButton * _copy_json_button = nullptr;
    QPushButton * _load_json_button = nullptr;
    QPushButton * _save_json_button = nullptr;
    QPushButton * _apply_json_button = nullptr;

    // Output & Execution (Phase 3)
    QGroupBox * _output_group = nullptr;
    QLineEdit * _output_key_edit = nullptr;
    QComboBox * _execution_mode_combo = nullptr;
    QPushButton * _execute_button = nullptr;
    QProgressBar * _progress_bar = nullptr;
    QLabel * _progress_label = nullptr;
    QLabel * _error_label = nullptr;
    bool _output_key_user_edited = false;  ///< True if user manually edited the output key

    // OperationContext delivery (Phase 6.4)
    EditorLib::OperationContext * _operation_context = nullptr;
    QPushButton * _deliver_pipeline_btn = nullptr;
};

#endif // TRANSFORMS_V2_PROPERTIES_WIDGET_HPP
