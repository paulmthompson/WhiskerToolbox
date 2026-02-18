#include "TransformsV2Properties_Widget.hpp"
#include "ui_TransformsV2Properties_Widget.h"

#include "PipelineStepListWidget.hpp"
#include "PreReductionPanel.hpp"
#include "StepConfigPanel.hpp"

#include "Core/TransformsV2State.hpp"
#include "EditorState/SelectionContext.hpp"

#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/detail/ContainerTraits.hpp"

#include <QClipboard>
#include <QFileDialog>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

#include <iostream>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

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
    syncJsonFromUI();
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

    // --- JSON Panel (Phase 2) ---
    _json_group = new QGroupBox(tr("Pipeline JSON"), this);
    _json_group->setCheckable(true);
    _json_group->setChecked(_state->jsonPanelExpanded());
    auto * json_layout = new QVBoxLayout(_json_group);
    json_layout->setSpacing(4);

    _json_panel = new QTextEdit(_json_group);
    _json_panel->setFont(QFont("monospace", 9));
    _json_panel->setAcceptRichText(false);
    _json_panel->setPlaceholderText(tr("Pipeline JSON will appear here..."));
    _json_panel->setMinimumHeight(120);
    json_layout->addWidget(_json_panel);

    // Button row: Copy | Apply | Load | Save
    auto * json_button_layout = new QHBoxLayout();
    json_button_layout->setSpacing(4);

    _copy_json_button = new QPushButton(tr("Copy"), _json_group);
    _copy_json_button->setToolTip(tr("Copy pipeline JSON to clipboard"));
    json_button_layout->addWidget(_copy_json_button);

    _apply_json_button = new QPushButton(tr("Apply"), _json_group);
    _apply_json_button->setToolTip(tr("Apply edited JSON to rebuild the pipeline UI"));
    json_button_layout->addWidget(_apply_json_button);

    _load_json_button = new QPushButton(tr("Load..."), _json_group);
    _load_json_button->setToolTip(tr("Load pipeline JSON from file"));
    json_button_layout->addWidget(_load_json_button);

    _save_json_button = new QPushButton(tr("Save..."), _json_group);
    _save_json_button->setToolTip(tr("Save pipeline JSON to file"));
    json_button_layout->addWidget(_save_json_button);

    json_button_layout->addStretch();
    json_layout->addLayout(json_button_layout);

    main_layout->addWidget(_json_group);

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

    // JSON panel connections
    connect(_json_panel, &QTextEdit::textChanged,
            this, &TransformsV2Properties_Widget::onJsonPanelEdited);
    connect(_copy_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onCopyJsonClicked);
    connect(_apply_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onApplyJsonClicked);
    connect(_load_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onLoadJsonClicked);
    connect(_save_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onSaveJsonClicked);
    connect(_json_group, &QGroupBox::toggled,
            this, [this](bool checked) {
                _state->setJsonPanelExpanded(checked);
            });
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

// ============================================================================
// Phase 2: Bidirectional JSON Synchronization
// ============================================================================

std::string TransformsV2Properties_Widget::buildJsonFromUI() const {
    PipelineDescriptor descriptor;

    // Build steps from the step list widget
    auto const & steps = _step_list->steps();
    for (auto const & step : steps) {
        PipelineStepDescriptor step_desc;
        step_desc.step_id = step.step_id;
        step_desc.transform_name = step.transform_name;

        // Parse the stored JSON params string into rfl::Generic
        if (step.parameters_json != "{}" && !step.parameters_json.empty()) {
            auto params_result = rfl::json::read<rfl::Generic>(step.parameters_json);
            if (params_result) {
                step_desc.parameters = params_result.value();
            }
        }

        descriptor.steps.push_back(std::move(step_desc));
    }

    // Build pre-reductions from the pre-reduction panel
    auto const & pre_entries = _pre_reduction_panel->entries();
    if (!pre_entries.empty()) {
        std::vector<PreReductionStepDescriptor> pre_descs;
        for (auto const & entry : pre_entries) {
            PreReductionStepDescriptor pre_desc;
            pre_desc.reduction_name = entry.reduction_name;
            pre_desc.output_key = entry.output_key;

            if (entry.parameters_json != "{}" && !entry.parameters_json.empty()) {
                auto params_result = rfl::json::read<rfl::Generic>(entry.parameters_json);
                if (params_result) {
                    pre_desc.parameters = params_result.value();
                }
            }

            pre_descs.push_back(std::move(pre_desc));
        }
        descriptor.pre_reductions = std::move(pre_descs);
    }

    return savePipelineToJson(descriptor);
}

void TransformsV2Properties_Widget::syncJsonFromUI() {
    if (_syncing_json) {
        return;
    }
    _syncing_json = true;

    auto json = buildJsonFromUI();
    _json_panel->setPlainText(QString::fromStdString(json));

    // Store in state and emit signal
    _state->setPipelineJson(json);
    emit pipelineDescriptorChanged(json);

    _syncing_json = false;
}

bool TransformsV2Properties_Widget::loadUIFromJson(std::string const & json_str) {
    // Parse the JSON into a PipelineDescriptor
    auto result = rfl::json::read<PipelineDescriptor>(json_str);
    if (!result) {
        return false;
    }

    auto const & descriptor = result.value();

    _syncing_json = true;

    // Load pre-reductions
    if (descriptor.pre_reductions.has_value()) {
        _pre_reduction_panel->loadFromDescriptors(descriptor.pre_reductions.value());
    } else {
        _pre_reduction_panel->clearEntries();
    }

    // Load steps
    _step_list->loadFromDescriptors(descriptor.steps);

    // Update the JSON panel text to show the canonical (re-serialized) JSON
    auto canonical_json = buildJsonFromUI();
    _json_panel->setPlainText(QString::fromStdString(canonical_json));

    // Store in state and emit signal
    _state->setPipelineJson(canonical_json);
    emit pipelineDescriptorChanged(canonical_json);

    _syncing_json = false;
    return true;
}

// ============================================================================
// Phase 2: JSON Panel Slots
// ============================================================================

void TransformsV2Properties_Widget::onJsonPanelEdited() {
    // Ignore edits caused by syncJsonFromUI() or loadUIFromJson()
    // The user manually editing the JSON panel does NOT auto-apply.
    // They must click "Apply" to rebuild the UI from JSON.
}

void TransformsV2Properties_Widget::onCopyJsonClicked() {
    auto * clipboard = QGuiApplication::clipboard();
    clipboard->setText(_json_panel->toPlainText());
}

void TransformsV2Properties_Widget::onApplyJsonClicked() {
    auto json_str = _json_panel->toPlainText().toStdString();
    if (json_str.empty()) {
        QMessageBox::warning(this, tr("Empty JSON"),
                             tr("The JSON panel is empty. Nothing to apply."));
        return;
    }

    if (!loadUIFromJson(json_str)) {
        QMessageBox::warning(this, tr("Invalid JSON"),
                             tr("The JSON could not be parsed as a valid PipelineDescriptor. "
                                "Please check the format and try again."));
    }
}

void TransformsV2Properties_Widget::onLoadJsonClicked() {
    auto filepath = QFileDialog::getOpenFileName(
            this, tr("Load Pipeline JSON"),
            QString(), tr("JSON Files (*.json);;All Files (*)"));

    if (filepath.isEmpty()) {
        return;
    }

    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Load Failed"),
                             tr("Could not open file: %1").arg(filepath));
        return;
    }

    auto json_str = QString(file.readAll()).toStdString();
    file.close();

    if (!loadUIFromJson(json_str)) {
        QMessageBox::warning(this, tr("Invalid JSON"),
                             tr("The file does not contain a valid PipelineDescriptor JSON."));
    }
}

void TransformsV2Properties_Widget::onSaveJsonClicked() {
    auto filepath = QFileDialog::getSaveFileName(
            this, tr("Save Pipeline JSON"),
            QString(), tr("JSON Files (*.json);;All Files (*)"));

    if (filepath.isEmpty()) {
        return;
    }

    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not open file for writing: %1").arg(filepath));
        return;
    }

    file.write(_json_panel->toPlainText().toUtf8());
    file.close();
}
