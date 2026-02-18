#include "TransformsV2Properties_Widget.hpp"
#include "ui_TransformsV2Properties_Widget.h"

#include "PipelineStepListWidget.hpp"
#include "PreReductionPanel.hpp"
#include "StepConfigPanel.hpp"

#include "Core/TransformsV2State.hpp"
#include "EditorState/SelectionContext.hpp"

#include "TransformsV2/detail/ContainerTraits.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Construction / Destruction
// ============================================================================

TransformsV2Properties_Widget::TransformsV2Properties_Widget(
        std::shared_ptr<TransformsV2State> state,
        SelectionContext * selection_context,
        QWidget * parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::TransformsV2Properties_Widget>())
    , _state(std::move(state))
    , _selection_context(selection_context) {

    ui->setupUi(this);
    setupUI();

    // Connect to SelectionContext for DataFocusAware
    if (_selection_context) {
        connectToSelectionContext(_selection_context, this);

        // Initialize from current focus if available
        auto const & current_key = _selection_context->dataFocus();
        auto const & current_type = _selection_context->dataFocusType();
        if (!current_key.isEmpty()) {
            onDataFocusChanged(current_key, current_type);
        }
    }
}

TransformsV2Properties_Widget::~TransformsV2Properties_Widget() = default;

// ============================================================================
// DataFocusAware
// ============================================================================

void TransformsV2Properties_Widget::onDataFocusChanged(
        EditorLib::SelectedDataKey const & data_key,
        QString const & data_type) {

    _input_data_key = data_key.toStdString();
    _input_data_type_name = data_type.toStdString();

    resolveInputTypes();
    updateInputDisplay();

    // Update sub-widgets with new input type
    _step_list->setInputType(_input_element_type, _input_container_type);
    _pre_reduction_panel->setInputType(_input_element_type);

    // Store in state
    _state->setInputDataKey(_input_data_key);
}

// ============================================================================
// Slots
// ============================================================================

void TransformsV2Properties_Widget::onStepSelected(int step_index) {
    if (step_index < 0 || step_index >= static_cast<int>(_step_list->steps().size())) {
        _step_config->clearConfig();
        return;
    }

    auto const & step = _step_list->steps()[static_cast<size_t>(step_index)];
    _step_config->showStepConfig(step.transform_name, step.parameters_json);
}

void TransformsV2Properties_Widget::onPipelineChanged() {
    // Pipeline changed — state tracking will be extended in Phase 5
    emit _state->stateChanged();
}

void TransformsV2Properties_Widget::onStepParametersChanged(std::string const & params_json) {
    int selected = _step_list->selectedStepIndex();
    if (selected >= 0) {
        _step_list->updateStepParams(selected, params_json);
    }
}

void TransformsV2Properties_Widget::onValidationChanged(bool all_valid) {
    if (all_valid) {
        _validation_label->setText(tr("Pipeline valid ✓"));
        _validation_label->setStyleSheet("color: green; font-weight: bold;");
    } else {
        _validation_label->setText(tr("Pipeline has type errors ✗"));
        _validation_label->setStyleSheet("color: red; font-weight: bold;");
    }
    _validation_label->setVisible(!_step_list->steps().empty());
}

// ============================================================================
// Private: UI Setup
// ============================================================================

void TransformsV2Properties_Widget::setupUI() {
    auto * main_layout = ui->verticalLayout;

    // --- Input Section ---
    _input_group = new QGroupBox(tr("Input"), this);
    auto * input_layout = new QVBoxLayout(_input_group);
    input_layout->setSpacing(2);

    _input_key_label = new QLabel(tr("No data selected"), _input_group);
    _input_key_label->setStyleSheet("font-weight: bold;");
    input_layout->addWidget(_input_key_label);

    _input_type_label = new QLabel(_input_group);
    _input_type_label->setStyleSheet("color: gray; font-size: 9pt;");
    _input_type_label->setVisible(false);
    input_layout->addWidget(_input_type_label);

    main_layout->addWidget(_input_group);

    // --- Pre-Reduction Panel ---
    _pre_reduction_panel = new PreReductionPanel(this);
    main_layout->addWidget(_pre_reduction_panel);

    // --- Pipeline Steps (with splitter between list and config) ---
    auto * pipeline_group = new QGroupBox(tr("Pipeline Steps"), this);
    auto * pipeline_layout = new QVBoxLayout(pipeline_group);
    pipeline_layout->setSpacing(4);

    auto * splitter = new QSplitter(Qt::Vertical, pipeline_group);

    _step_list = new PipelineStepListWidget(splitter);
    splitter->addWidget(_step_list);

    _step_config = new StepConfigPanel(splitter);
    splitter->addWidget(_step_config);

    splitter->setStretchFactor(0, 1); // Step list gets 1 part
    splitter->setStretchFactor(1, 2); // Config panel gets 2 parts

    pipeline_layout->addWidget(splitter);

    // Validation label
    _validation_label = new QLabel(this);
    _validation_label->setVisible(false);
    pipeline_layout->addWidget(_validation_label);

    main_layout->addWidget(pipeline_group, 1);

    // --- Connections ---
    connect(_step_list, &PipelineStepListWidget::stepSelected,
            this, &TransformsV2Properties_Widget::onStepSelected);
    connect(_step_list, &PipelineStepListWidget::pipelineChanged,
            this, &TransformsV2Properties_Widget::onPipelineChanged);
    connect(_step_list, &PipelineStepListWidget::validationChanged,
            this, &TransformsV2Properties_Widget::onValidationChanged);
    connect(_step_config, &StepConfigPanel::parametersChanged,
            this, &TransformsV2Properties_Widget::onStepParametersChanged);
    connect(_pre_reduction_panel, &PreReductionPanel::preReductionsChanged,
            this, &TransformsV2Properties_Widget::onPipelineChanged);
}

// ============================================================================
// Private: Input Display
// ============================================================================

void TransformsV2Properties_Widget::updateInputDisplay() {
    if (_input_data_key.empty()) {
        _input_key_label->setText(tr("No data selected"));
        _input_type_label->setVisible(false);
        return;
    }

    _input_key_label->setText(QString::fromStdString(_input_data_key));
    _input_type_label->setText(QString::fromStdString(_input_data_type_name));
    _input_type_label->setVisible(true);
}

void TransformsV2Properties_Widget::resolveInputTypes() {
    if (_input_data_type_name.empty()) {
        _input_element_type = typeid(void);
        _input_container_type = typeid(void);
        return;
    }

    try {
        _input_container_type = TypeIndexMapper::stringToContainer(_input_data_type_name);
        _input_element_type = TypeIndexMapper::containerToElement(_input_container_type);
    } catch (std::exception const & e) {
        std::cerr << "TransformsV2Properties_Widget: Could not resolve types for '"
                  << _input_data_type_name << "': " << e.what() << std::endl;
        _input_element_type = typeid(void);
        _input_container_type = typeid(void);
    }
}
