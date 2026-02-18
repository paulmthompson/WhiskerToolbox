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

class TransformsV2State;
class SelectionContext;
class PipelineStepListWidget;
class StepConfigPanel;
class PreReductionPanel;
class QLabel;
class QGroupBox;
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

private slots:
    void onStepSelected(int step_index);
    void onPipelineChanged();
    void onStepParametersChanged(std::string const & params_json);
    void onValidationChanged(bool all_valid);

private:
    void setupUI();
    void updateInputDisplay();
    void resolveInputTypes();

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
};

#endif // TRANSFORMS_V2_PROPERTIES_WIDGET_HPP
