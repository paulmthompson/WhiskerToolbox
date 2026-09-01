#include "TransformsV2Properties_Widget.hpp"
#include "ui_TransformsV2Properties_Widget.h"

#include "PipelineLibraryDialog.hpp"
#include "PipelineStepListWidget.hpp"
#include "StepConfigPanel.hpp"

#include "Common/Collapsible_Widget/Section.hpp"
#include "Core/MultiInputKeyResolver.hpp"
#include "Core/TransformsV2State.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/utils/ContainerTypeIndex.hpp"
#include "DataManager/utils/DataManagerKeys.hpp"
#include "DataManager/utils/DataTypeIndexBridge.hpp"
#include "EditorState/OperationContext.hpp"
#include "EditorState/SelectionContext.hpp"
#include "StateManagement/AppFileDialog.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/PipelineLibrary.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <filesystem>

using namespace Neuralyzer::Transforms::V2;
using namespace Neuralyzer::Transforms::V2::Examples;
using namespace Neuralyzer::TypeTraits;

// ============================================================================
// Construction / Destruction
// ============================================================================

TransformsV2Properties_Widget::TransformsV2Properties_Widget(
        std::shared_ptr<TransformsV2State> state,
        SelectionContext * selection_context,
        QString pipeline_config_dir,
        QWidget * parent)
    : QWidget(parent),
      ui(std::make_unique<Ui::TransformsV2Properties_Widget>()),
      _state(std::move(state)),
      _selection_context(selection_context),
      _config_dir(std::move(pipeline_config_dir)) {

    if (!_config_dir.isEmpty()) {
        auto const dir_result =
                ensureUserPipelineDirectory(_config_dir.toStdString());
        if (dir_result) {
            _pipeline_library_dir =
                    QString::fromStdString(dir_result.value().string());
        } else {
            spdlog::warn("[TransformsV2Widget] Failed to create pipeline library directory: {}",
                         dir_result.error()->what());
        }
    }

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

    // Once the pipeline has steps the input is pinned — ignore focus changes
    if (_input_pinned) {
        return;
    }

    _input_data_key = data_key.toStdString();

    // If the data_type is provided, use it directly.
    // Otherwise, look it up from DataManager (the SelectionContext may not
    // always provide the type — e.g. DataManager_Widget uses setSelectedData
    // which doesn't carry the type string).
    if (!data_type.isEmpty()) {
        _input_data_type_name = data_type.toStdString();
    } else {
        _input_data_type_name = resolveDataTypeFromManager(_input_data_key);
    }

    resolveInputTypes();
    updateInputDisplay();

    // Update sub-widgets with new input type
    _step_list->setInputType(_input_element_type, _input_container_type);

    // Store in state
    _state->setInputDataKey(_input_data_key);

    // Update output key and execute button state
    updateOutputKeyFromPipeline();
    refreshTimeKeyCombo();
    updateValidationLabel();
    updateExecuteButtonState();
    updateDeliverButtonState();
}

// ============================================================================
// Slots
// ============================================================================

void TransformsV2Properties_Widget::onStepSelected(int step_index) {
    if (step_index < 0 || step_index >= static_cast<int>(_step_list->steps().size())) {
        _step_config->clearConfig();
        return;
    }

    refreshSelectedStepConfig();
}

void TransformsV2Properties_Widget::refreshSelectedStepConfig() {
    int const step_index = _step_list->selectedStepIndex();
    if (step_index < 0 || step_index >= static_cast<int>(_step_list->steps().size())) {
        _step_config->clearConfig();
        return;
    }

    auto const & step = _step_list->steps()[static_cast<size_t>(step_index)];
    auto const multi_input = buildMultiInputStepContext(step);
    _step_config->showStepConfig(step.transform_name, step.parameters_json, multi_input);
}

MultiInputStepContext TransformsV2Properties_Widget::buildMultiInputStepContext(
        PipelineStepEntry const & step) const {
    MultiInputStepContext context;
    if (!step.is_multi_input || _input_data_key.empty()) {
        return context;
    }

    auto const multi_info = getMultiInputTransformInfo(step.transform_name);
    if (!multi_info.has_value()) {
        return context;
    }

    auto const secondary_container =
            getSecondaryContainerType(_input_container_type, multi_info->individual_input_types);
    if (!secondary_container.has_value()) {
        return context;
    }

    context.enabled = true;
    context.primary_input_key = _input_data_key;
    context.primary_input_type_name = _input_data_type_name;
    try {
        context.secondary_input_type_name =
                TypeIndexMapper::containerToString(*secondary_container);
    } catch (...) {
        context.secondary_input_type_name = "Unknown";
    }
    context.additional_input_key = step.additional_input_key;

    auto * dm = _state ? _state->dataManager().get() : nullptr;
    if (!dm) {
        return context;
    }

    auto const dm_type = containerTypeIndexToDmDataType(*secondary_container);
    if (!dm_type.has_value()) {
        return context;
    }

    context.available_secondary_keys = getKeysForTypes(*dm, {*dm_type});
    context.available_secondary_keys.erase(
            std::remove(context.available_secondary_keys.begin(),
                        context.available_secondary_keys.end(),
                        _input_data_key),
            context.available_secondary_keys.end());

    return context;
}

void TransformsV2Properties_Widget::onStepAdditionalInputChanged(
        std::string const & additional_input_key) {
    int const selected = _step_list->selectedStepIndex();
    if (selected < 0) {
        return;
    }

    _step_list->updateStepAdditionalInput(selected, additional_input_key);
    updateValidationLabel();
    updateExecuteButtonState();
}

void TransformsV2Properties_Widget::onPipelineChanged() {
    // Pin / unpin input based on whether the pipeline has steps
    bool const has_steps = !_step_list->steps().empty();
    if (has_steps && !_input_pinned) {
        _input_pinned = true;
        _input_pinned_label->setVisible(true);
    } else if (!has_steps && _input_pinned) {
        _input_pinned = false;
        _input_pinned_label->setVisible(false);

        // Re-sync from the current SelectionContext focus
        if (_selection_context) {
            auto const & key = _selection_context->dataFocus();
            auto const & type = _selection_context->dataFocusType();
            if (!key.isEmpty()) {
                // Call the base handler (pinning is now off)
                onDataFocusChanged(key, type);
            }
        }
    }

    syncJsonFromUI();
    updateOutputKeyFromPipeline();
    updateValidationLabel();
    updateExecuteButtonState();
    refreshSelectedStepConfig();
    emit _state->stateChanged();
}

void TransformsV2Properties_Widget::onStepParametersChanged(std::string const & params_json) {
    int const selected = _step_list->selectedStepIndex();
    if (selected >= 0) {
        _step_list->updateStepParams(selected, params_json);
    }
}

void TransformsV2Properties_Widget::onValidationChanged(bool /*all_valid*/) {
    updateValidationLabel();
    updateExecuteButtonState();
}

void TransformsV2Properties_Widget::updateValidationLabel() {
    if (_step_list->steps().empty()) {
        _validation_label->setVisible(false);
        return;
    }

    _validation_label->setVisible(true);

    if (_step_list->hasMultiInputWithExtraSteps()) {
        _validation_label->setText(
                tr("Multi-input transforms require a single-step pipeline. Remove extra steps or run "
                   "unary steps separately."));
        _validation_label->setStyleSheet("color: #b36b00; font-weight: bold;");
        return;
    }

    if (_step_list->hasMultiInputStep() && !_step_list->allMultiInputStepsConfigured()) {
        _validation_label->setText(tr("Second input required for multi-input step"));
        _validation_label->setStyleSheet("color: #b36b00; font-weight: bold;");
        return;
    }

    auto const chain_valid = [&]() {
        std::vector<std::string> names;
        for (auto const & step: _step_list->steps()) {
            names.push_back(step.transform_name);
        }
        return resolveTypeChain(_input_container_type, names).all_valid;
    }();

    if (chain_valid) {
        _validation_label->setText(tr("Pipeline valid ✓"));
        _validation_label->setStyleSheet("color: green; font-weight: bold;");
    } else {
        _validation_label->setText(tr("Pipeline has type errors ✗"));
        _validation_label->setStyleSheet("color: red; font-weight: bold;");
    }
}

// ============================================================================
// Private: UI Setup
// ============================================================================

void TransformsV2Properties_Widget::setupUI() {
    auto * outer_layout = ui->verticalLayout;

    // Wrap all content in a scroll area so the widget is scrollable
    auto * scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);

    auto * scroll_content = new QWidget();
    auto * main_layout = new QVBoxLayout(scroll_content);
    main_layout->setContentsMargins(0, 0, 0, 0);

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

    _input_pinned_label = new QLabel(tr("\xF0\x9F\x94\x92 Locked — clear pipeline to change input"), _input_group);
    _input_pinned_label->setStyleSheet("color: #888; font-size: 8pt; font-style: italic;");
    _input_pinned_label->setVisible(false);
    input_layout->addWidget(_input_pinned_label);

    main_layout->addWidget(_input_group);

    // --- Pipeline Steps (with splitter between list and config) ---
    auto * pipeline_group = new QGroupBox(tr("Pipeline Steps"), this);
    auto * pipeline_layout = new QVBoxLayout(pipeline_group);
    pipeline_layout->setSpacing(4);

    auto * splitter = new QSplitter(Qt::Vertical, pipeline_group);

    _step_list = new PipelineStepListWidget(splitter);
    splitter->addWidget(_step_list);

    _step_config = new StepConfigPanel(splitter);
    splitter->addWidget(_step_config);

    splitter->setStretchFactor(0, 1);// Step list gets 1 part
    splitter->setStretchFactor(1, 2);// Config panel gets 2 parts

    pipeline_layout->addWidget(splitter);

    // Validation label
    _validation_label = new QLabel(this);
    _validation_label->setVisible(false);
    pipeline_layout->addWidget(_validation_label);

    main_layout->addWidget(pipeline_group, 1);

    // --- Output & Execution Section ---
    _output_group = new QGroupBox(tr("Output && Execution"), this);
    auto * output_layout = new QVBoxLayout(_output_group);
    output_layout->setSpacing(4);

    // Output key name
    auto * output_key_layout = new QHBoxLayout();
    auto * output_key_label = new QLabel(tr("Output Key:"), _output_group);
    _output_key_edit = new QLineEdit(_output_group);
    _output_key_edit->setPlaceholderText(tr("Auto-generated from input + transform"));
    _output_key_edit->setToolTip(tr("Name of the data key to store the result in DataManager"));
    output_key_layout->addWidget(output_key_label);
    output_key_layout->addWidget(_output_key_edit, 1);
    output_layout->addLayout(output_key_layout);

    // Restore saved output key if available
    if (auto saved_key = _state->outputDataKey(); saved_key.has_value()) {
        _output_key_edit->setText(QString::fromStdString(*saved_key));
        _output_key_user_edited = true;
    }

    // Output TimeKey selector
    auto * time_key_layout = new QHBoxLayout();
    auto * time_key_label = new QLabel(tr("Output TimeKey:"), _output_group);
    _output_time_key_combo = new QComboBox(_output_group);
    _output_time_key_combo->setToolTip(
            tr("TimeKey for the output data. '(Same as input)' inherits the input's TimeKey.\n"
               "For transforms that change the number of samples (e.g. upsampling),\n"
               "select a TimeKey whose frame count matches the output size."));
    _output_time_key_combo->addItem(tr("(Same as input)"), QString());
    time_key_layout->addWidget(time_key_label);
    time_key_layout->addWidget(_output_time_key_combo, 1);
    output_layout->addLayout(time_key_layout);

    // Restore saved output time key if available
    if (auto saved_tk = _state->outputTimeKey(); saved_tk.has_value() && !saved_tk->empty()) {
        // Will be selected after refreshTimeKeyCombo() populates the combo
        // Store it so we can restore after population
        _output_time_key_combo->setProperty("pending_restore", QString::fromStdString(*saved_tk));
    }

    // Execution mode combo
    auto * mode_layout = new QHBoxLayout();
    auto * mode_label = new QLabel(tr("Mode:"), _output_group);
    _execution_mode_combo = new QComboBox(_output_group);
    _execution_mode_combo->addItem(tr("Save to DataManager"), QStringLiteral("data_manager"));
    _execution_mode_combo->addItem(tr("JSON Pipeline Only"), QStringLiteral("json_only"));
    _execution_mode_combo->setToolTip(tr("Save to DataManager materializes results; JSON Pipeline Only just produces the descriptor"));

    // Restore saved execution mode
    auto const & saved_mode = _state->executionMode();
    int const mode_index = _execution_mode_combo->findData(QString::fromStdString(saved_mode));
    if (mode_index >= 0) {
        _execution_mode_combo->setCurrentIndex(mode_index);
    }

    mode_layout->addWidget(mode_label);
    mode_layout->addWidget(_execution_mode_combo, 1);
    output_layout->addLayout(mode_layout);

    // Execute button
    _execute_button = new QPushButton(tr("Execute Pipeline"), _output_group);
    _execute_button->setToolTip(tr("Build and execute the pipeline on the selected input data"));
    _execute_button->setEnabled(false);// Enabled when pipeline is valid + input selected
    _execute_button->setStyleSheet(
            "QPushButton { font-weight: bold; padding: 6px; }"
            "QPushButton:enabled { background-color: #4CAF50; color: white; }"
            "QPushButton:disabled { background-color: #cccccc; }");
    output_layout->addWidget(_execute_button);

    // Progress bar
    _progress_bar = new QProgressBar(_output_group);
    _progress_bar->setRange(0, 100);
    _progress_bar->setValue(0);
    _progress_bar->setVisible(false);
    _progress_bar->setTextVisible(true);
    output_layout->addWidget(_progress_bar);

    // Progress label (step name)
    _progress_label = new QLabel(_output_group);
    _progress_label->setStyleSheet("color: gray; font-size: 9pt;");
    _progress_label->setVisible(false);
    output_layout->addWidget(_progress_label);

    // Error label
    _error_label = new QLabel(_output_group);
    _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
    _error_label->setWordWrap(true);
    _error_label->setVisible(false);
    output_layout->addWidget(_error_label);

    main_layout->addWidget(_output_group);

    // --- JSON Panel (collapsed by default) ---
    _json_section = new Section(scroll_content, tr("Pipeline JSON"));
    auto * json_content_layout = new QVBoxLayout();
    json_content_layout->setSpacing(4);

    _json_panel = new QTextEdit();
    _json_panel->setFont(QFont("monospace", 9));
    _json_panel->setAcceptRichText(false);
    _json_panel->setPlaceholderText(tr("Pipeline JSON will appear here..."));
    _json_panel->setMinimumHeight(120);
    json_content_layout->addWidget(_json_panel);

    // Button row: Copy | Apply | Load | Save
    auto * json_button_layout = new QHBoxLayout();
    json_button_layout->setSpacing(4);

    _copy_json_button = new QPushButton(tr("Copy"));
    _copy_json_button->setToolTip(tr("Copy pipeline JSON to clipboard"));
    json_button_layout->addWidget(_copy_json_button);

    _apply_json_button = new QPushButton(tr("Apply"));
    _apply_json_button->setToolTip(tr("Apply edited JSON to rebuild the pipeline UI"));
    json_button_layout->addWidget(_apply_json_button);

    _library_button = new QPushButton(tr("Library..."));
    _library_button->setToolTip(tr("Browse saved pipelines in the library folder"));
    json_button_layout->addWidget(_library_button);

    _load_json_button = new QPushButton(tr("Load..."));
    _load_json_button->setToolTip(tr("Load pipeline JSON from file"));
    json_button_layout->addWidget(_load_json_button);

    _save_json_button = new QPushButton(tr("Save..."));
    _save_json_button->setToolTip(tr("Save pipeline JSON to file"));
    json_button_layout->addWidget(_save_json_button);

    _save_to_library_button = new QPushButton(tr("Save to Library"));
    _save_to_library_button->setToolTip(
            tr("Save the current pipeline to the library with a name"));
    json_button_layout->addWidget(_save_to_library_button);

    json_button_layout->addStretch();
    json_content_layout->addLayout(json_button_layout);

    _json_section->setContentLayout(*json_content_layout);
    main_layout->addWidget(_json_section);

    // --- Pipeline Delivery Button ---
    // Shows when a consumer (e.g., TensorDesigner's ColumnConfigDialog) has
    // requested a pipeline via OperationContext.
    _deliver_pipeline_btn = new QPushButton(
            tr("Send Pipeline to Column Builder"), this);
    _deliver_pipeline_btn->setStyleSheet(
            QStringLiteral("QPushButton { background-color: #0066cc; color: white; "
                           "padding: 6px 12px; font-weight: bold; border-radius: 4px; }"
                           "QPushButton:hover { background-color: #0055aa; }"
                           "QPushButton:disabled { background-color: #999; }"));
    _deliver_pipeline_btn->setToolTip(
            tr("Deliver the current pipeline JSON to the widget that requested it"));
    _deliver_pipeline_btn->setVisible(false);// Hidden until a pending operation exists
    main_layout->addWidget(_deliver_pipeline_btn);

    // Finalize scroll area
    scroll_area->setWidget(scroll_content);
    outer_layout->addWidget(scroll_area);

    // --- Connections ---
    connect(_step_list, &PipelineStepListWidget::stepSelected,
            this, &TransformsV2Properties_Widget::onStepSelected);
    connect(_step_list, &PipelineStepListWidget::pipelineChanged,
            this, &TransformsV2Properties_Widget::onPipelineChanged);
    connect(_step_list, &PipelineStepListWidget::validationChanged,
            this, &TransformsV2Properties_Widget::onValidationChanged);
    connect(_step_config, &StepConfigPanel::parametersChanged,
            this, &TransformsV2Properties_Widget::onStepParametersChanged);
    connect(_step_config, &StepConfigPanel::additionalInputChanged,
            this, &TransformsV2Properties_Widget::onStepAdditionalInputChanged);

    // JSON panel connections
    connect(_json_panel, &QTextEdit::textChanged,
            this, &TransformsV2Properties_Widget::onJsonPanelEdited);
    connect(_copy_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onCopyJsonClicked);
    connect(_apply_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onApplyJsonClicked);
    connect(_library_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onLibraryClicked);
    connect(_load_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onLoadJsonClicked);
    connect(_save_json_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onSaveJsonClicked);
    connect(_save_to_library_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onSaveToLibraryClicked);
    // Output & Execution connections
    connect(_execute_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onExecuteClicked);
    connect(_output_key_edit, &QLineEdit::textEdited,
            this, &TransformsV2Properties_Widget::onOutputKeyEdited);
    connect(_output_time_key_combo, &QComboBox::currentIndexChanged,
            this, [this](int /*index*/) {
                auto tk = _output_time_key_combo->currentData().toString().toStdString();
                _state->setOutputTimeKey(tk);
            });
    connect(_execution_mode_combo, &QComboBox::currentIndexChanged,
            this, [this](int /*index*/) {
                auto mode = _execution_mode_combo->currentData().toString().toStdString();
                _state->setExecutionMode(mode);
                updateExecuteButtonState();
            });

    // OperationContext delivery connection
    connect(_deliver_pipeline_btn, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onDeliverPipelineClicked);
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

std::string TransformsV2Properties_Widget::resolveDataTypeFromManager(std::string const & key) const {
    auto dm = _state ? _state->dataManager() : nullptr;
    if (!dm || key.empty()) {
        return {};
    }

    auto const dm_type = dm->getType(key);
    switch (dm_type) {
        case DM_DataType::Mask:
            return "MaskData";
        case DM_DataType::Line:
            return "LineData";
        case DM_DataType::Points:
            return "PointData";
        case DM_DataType::Analog:
            return "AnalogTimeSeries";
        case DM_DataType::RaggedAnalog:
            return "RaggedAnalogTimeSeries";
        case DM_DataType::DigitalEvent:
            return "DigitalEventSeries";
        case DM_DataType::DigitalInterval:
            return "DigitalIntervalSeries";
        case DM_DataType::Tensor:
            return "TensorData";
        default:
            return {};
    }
}

void TransformsV2Properties_Widget::resolveInputTypes() {
    if (_input_data_type_name.empty()) {
        _input_element_type = typeid(void);
        _input_container_type = typeid(void);
        return;
    }

    try {
        _input_container_type = TypeIndexMapper::stringToContainer(_input_data_type_name);
    } catch (std::exception const & e) {
        std::cerr << "TransformsV2Properties_Widget: Could not resolve container type for '"
                  << _input_data_type_name << "': " << e.what() << std::endl;
        _input_element_type = typeid(void);
        _input_container_type = typeid(void);
        return;
    }

    // Some container types (e.g., DigitalEventSeries, DigitalIntervalSeries)
    // are container-only and don't have an element type mapping.
    // In that case we keep the container type valid but set element to void.
    try {
        _input_element_type = TypeIndexMapper::containerToElement(_input_container_type);
    } catch (std::exception const &) {
        _input_element_type = typeid(void);
    }
}

// ============================================================================
// Bidirectional JSON Synchronization
// ============================================================================

QString TransformsV2Properties_Widget::pipelineLibraryFallbackDir() const {
    if (!_pipeline_library_dir.isEmpty()) {
        return _pipeline_library_dir;
    }
    return QDir::homePath();
}

std::vector<PipelineStepDescriptor> TransformsV2Properties_Widget::buildStepDescriptorsFromUI() const {
    std::vector<PipelineStepDescriptor> step_descriptors;

    auto const & steps = _step_list->steps();
    step_descriptors.reserve(steps.size());

    for (auto const & step: steps) {
        PipelineStepDescriptor step_desc;
        step_desc.step_id = step.step_id;
        step_desc.transform_name = step.transform_name;

        if (step.parameters_json != "{}" && !step.parameters_json.empty()) {
            auto params_result = rfl::json::read<rfl::Generic>(step.parameters_json);
            if (params_result) {
                step_desc.parameters = params_result.value();
            }
        }

        step_descriptors.push_back(std::move(step_desc));
    }

    return step_descriptors;
}

std::optional<PipelineDescriptor> TransformsV2Properties_Widget::parseJsonPanelDescriptor() const {
    auto const json_str = _json_panel->toPlainText().toStdString();
    if (json_str.empty()) {
        return PipelineDescriptor{};
    }

    auto const result = rfl::json::read<PipelineDescriptor>(json_str);
    if (!result) {
        return std::nullopt;
    }

    return result.value();
}

PipelineDescriptor TransformsV2Properties_Widget::mergeStepsIntoDescriptor(
        PipelineDescriptor base) const {
    base.steps = buildStepDescriptorsFromUI();
    return base;
}

PipelineDescriptor TransformsV2Properties_Widget::currentPipelineDescriptor() const {
    auto base = parseJsonPanelDescriptor();
    if (!base.has_value()) {
        base = PipelineDescriptor{};
    }
    return mergeStepsIntoDescriptor(std::move(base.value()));
}

std::string TransformsV2Properties_Widget::buildJsonFromUI() const {
    return savePipelineToJson(currentPipelineDescriptor());
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

    // Load steps; metadata / pre_reductions / range_reduction stay in the descriptor
    _step_list->loadFromDescriptors(descriptor.steps);

    auto const canonical_json = savePipelineToJson(descriptor);
    _json_panel->setPlainText(QString::fromStdString(canonical_json));

    _state->setPipelineJson(canonical_json);
    emit pipelineDescriptorChanged(canonical_json);

    _syncing_json = false;
    return true;
}

// ============================================================================
// JSON Panel Slots
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
    auto const filepath = AppFileDialog::getOpenFileName(
            this,
            QStringLiteral("transformv2_pipeline_open"),
            tr("Load Pipeline JSON"),
            tr("JSON Files (*.json);;All Files (*)"),
            pipelineLibraryFallbackDir());

    if (filepath.isEmpty()) {
        return;
    }

    auto const load_result =
            loadPipelineDescriptorFromFile(filepath.toStdString());
    if (!load_result) {
        QMessageBox::warning(this, tr("Load Failed"),
                             tr("Could not load pipeline: %1")
                                     .arg(QString::fromStdString(load_result.error()->what())));
        return;
    }

    if (!loadUIFromJson(savePipelineToJson(load_result.value()))) {
        QMessageBox::warning(this, tr("Invalid JSON"),
                             tr("The file does not contain a valid PipelineDescriptor JSON."));
    }
}

void TransformsV2Properties_Widget::onLibraryClicked() {
    if (_pipeline_library_dir.isEmpty()) {
        QMessageBox::warning(this, tr("Library Unavailable"),
                             tr("The pipeline library directory is not configured."));
        return;
    }

    syncJsonFromUI();

    PipelineLibraryDialog dialog(_pipeline_library_dir, this);
    dialog.setDescriptorSupplier([this]() { return currentPipelineDescriptor(); });

    if (dialog.exec() != QDialog::Accepted || !dialog.hasLoadedPipeline()) {
        return;
    }

    if (!loadUIFromJson(dialog.loadedPipelineJson())) {
        QMessageBox::warning(this, tr("Invalid JSON"),
                             tr("The library entry could not be loaded into the pipeline UI."));
    }
}

void TransformsV2Properties_Widget::onSaveToLibraryClicked() {
    if (_pipeline_library_dir.isEmpty()) {
        QMessageBox::warning(this, tr("Library Unavailable"),
                             tr("The pipeline library directory is not configured."));
        return;
    }

    syncJsonFromUI();

    auto descriptor = currentPipelineDescriptor();

    auto const default_name =
            descriptor.metadata.has_value() && descriptor.metadata->name.has_value()
                    ? QString::fromStdString(*descriptor.metadata->name)
                    : tr("Untitled Pipeline");

    bool ok = false;
    auto const name = QInputDialog::getText(
            this, tr("Save to Library"), tr("Pipeline name:"), QLineEdit::Normal, default_name, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    if (!descriptor.metadata.has_value()) {
        descriptor.metadata = PipelineMetadata{};
    }
    descriptor.metadata->name = name.trimmed().toStdString();

    auto const filename =
            QString::fromStdString(sanitizePipelineFilename(descriptor.metadata->name.value())) +
            QStringLiteral(".json");
    auto const dest_path =
            std::filesystem::path(_pipeline_library_dir.toStdString()) / filename.toStdString();

    if (std::filesystem::exists(dest_path)) {
        auto const overwrite = QMessageBox::question(
                this,
                tr("Overwrite Pipeline"),
                tr("'%1' already exists in the library. Overwrite?").arg(filename),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
        if (overwrite != QMessageBox::Yes) {
            return;
        }
    }

    auto const save_result = savePipelineDescriptorToFile(dest_path, descriptor);
    if (!save_result) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not save to library:\n%1")
                                     .arg(QString::fromStdString(save_result.error()->what())));
    }
}

void TransformsV2Properties_Widget::onSaveJsonClicked() {
    syncJsonFromUI();

    auto const descriptor = currentPipelineDescriptor();
    auto suggested_name = QStringLiteral("pipeline.json");
    if (descriptor.metadata.has_value() && descriptor.metadata->name.has_value() &&
        !descriptor.metadata->name->empty()) {
        suggested_name = QString::fromStdString(
                                 sanitizePipelineFilename(*descriptor.metadata->name)) +
                         QStringLiteral(".json");
    }

    auto const filepath = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("transformv2_pipeline_save"),
            tr("Save Pipeline JSON"),
            tr("JSON Files (*.json);;All Files (*)"),
            pipelineLibraryFallbackDir(),
            suggested_name);

    if (filepath.isEmpty()) {
        return;
    }

    auto const save_result = savePipelineDescriptorToFile(filepath.toStdString(), descriptor);
    if (!save_result) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not save pipeline: %1")
                                     .arg(QString::fromStdString(save_result.error()->what())));
    }
}

// ============================================================================
// Output Key Generation
// ============================================================================

std::string TransformsV2Properties_Widget::generateOutputName() const {
    if (_input_data_key.empty()) {
        return {};
    }

    auto const & steps = _step_list->steps();
    if (steps.empty()) {
        return _input_data_key + "_transformed";
    }

    // Use the last transform name, cleaned up like V1
    auto transform_name = QString::fromStdString(steps.back().transform_name);
    transform_name = transform_name.toLower().replace(' ', '_');

    // Strip common prefixes
    for (auto const * prefix: {"calculate_", "extract_", "convert_", "threshold_"}) {
        if (transform_name.startsWith(QLatin1String(prefix))) {
            transform_name = transform_name.mid(
                    static_cast<int>(std::strlen(prefix)));
            break;
        }
    }

    return _input_data_key + "_" + transform_name.toStdString();
}

void TransformsV2Properties_Widget::updateOutputKeyFromPipeline() {
    // Only auto-update if the user hasn't manually edited the key
    if (_output_key_user_edited) {
        return;
    }

    auto name = generateOutputName();
    if (!name.empty()) {
        _output_key_edit->setText(QString::fromStdString(name));
    }
}

void TransformsV2Properties_Widget::updateExecuteButtonState() {
    bool const has_input = !_input_data_key.empty();
    bool const has_steps = !_step_list->steps().empty();
    bool const has_output_key = !_output_key_edit->text().trimmed().isEmpty();
    bool const is_data_manager_mode =
            _execution_mode_combo->currentData().toString() == QStringLiteral("data_manager");

    bool const multi_input_ready =
            !_step_list->hasMultiInputStep() ||
            (_step_list->allMultiInputStepsConfigured() && !_step_list->hasMultiInputWithExtraSteps());

    std::vector<std::string> step_names;
    for (auto const & step: _step_list->steps()) {
        step_names.push_back(step.transform_name);
    }
    bool const type_chain_valid = has_steps
                                          ? resolveTypeChain(_input_container_type, step_names).all_valid
                                          : true;

    // For data_manager mode: need valid input, steps, output key, and multi-input config
    // For json_only mode: just need steps (the JSON is already produced)
    bool can_execute = false;
    if (is_data_manager_mode) {
        can_execute = has_input && has_steps && has_output_key && multi_input_ready && type_chain_valid;
    } else {
        can_execute = has_steps;
    }

    _execute_button->setEnabled(can_execute);

    if (is_data_manager_mode && _step_list->hasMultiInputWithExtraSteps()) {
        _execute_button->setToolTip(
                tr("Multi-input transforms require a single-step pipeline. Remove extra steps or run "
                   "unary steps separately."));
    } else if (is_data_manager_mode && _step_list->hasMultiInputStep() &&
               !_step_list->allMultiInputStepsConfigured()) {
        _execute_button->setToolTip(tr("Select a second input key for the multi-input step."));
    } else if (isSingleStepBinaryPipeline() && hasJsonPreOrRangeReduction()) {
        _execute_button->setToolTip(
                tr("Pre-reductions and range reduction are not applied for multi-input execution in "
                   "this release."));
    } else {
        _execute_button->setToolTip(QString());
    }
}

bool TransformsV2Properties_Widget::isSingleStepBinaryPipeline() const {
    auto const & steps = _step_list->steps();
    return steps.size() == 1 && steps.front().is_multi_input;
}

bool TransformsV2Properties_Widget::hasJsonPreOrRangeReduction() const {
    auto const descriptor = parseJsonPanelDescriptor();
    if (!descriptor.has_value()) {
        return false;
    }
    return descriptor->pre_reductions.has_value() || descriptor->range_reduction.has_value();
}

void TransformsV2Properties_Widget::refreshTimeKeyCombo() {
    auto * dm = _state->dataManager().get();
    if (!dm || !_output_time_key_combo) {
        return;
    }

    // Remember current selection
    auto current_data = _output_time_key_combo->currentData().toString();

    // Check for a pending restore from state
    auto pending = _output_time_key_combo->property("pending_restore").toString();
    if (!pending.isEmpty()) {
        current_data = pending;
        _output_time_key_combo->setProperty("pending_restore", QVariant());
    }

    _output_time_key_combo->blockSignals(true);
    _output_time_key_combo->clear();
    _output_time_key_combo->addItem(tr("(Same as input)"), QString());

    auto const time_keys = dm->getTimeFrameKeys();
    for (auto const & tk: time_keys) {
        auto const tf = dm->getTime(tk);
        int const frame_count = tf ? tf->getTotalFrameCount() : 0;
        auto display = QString::fromStdString(tk.str()) + QString(" (%1 frames)").arg(frame_count);
        _output_time_key_combo->addItem(display, QString::fromStdString(tk.str()));
    }

    // Restore selection
    if (!current_data.isEmpty()) {
        int const idx = _output_time_key_combo->findData(current_data);
        if (idx >= 0) {
            _output_time_key_combo->setCurrentIndex(idx);
        }
    }

    _output_time_key_combo->blockSignals(false);
}

// ============================================================================
// Execution Slots
// ============================================================================

void TransformsV2Properties_Widget::onOutputKeyEdited(QString const & text) {
    _output_key_user_edited = !text.trimmed().isEmpty();
    _state->setOutputDataKey(text.toStdString());
    updateExecuteButtonState();
}

void TransformsV2Properties_Widget::onExecuteClicked() {
    _error_label->setVisible(false);

    auto mode = _execution_mode_combo->currentData().toString();

    if (mode == QStringLiteral("json_only")) {
        // JSON-only mode: the JSON is already in the panel, just confirm
        _error_label->setStyleSheet("color: green; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Pipeline JSON is ready in the panel above."));
        _error_label->setVisible(true);
        return;
    }

    // --- Data Manager execution mode ---

    auto * dm = _state->dataManager().get();
    if (!dm) {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Error: DataManager not available."));
        _error_label->setVisible(true);
        return;
    }

    if (_input_data_key.empty()) {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Error: No input data selected."));
        _error_label->setVisible(true);
        return;
    }

    auto output_key = _output_key_edit->text().trimmed().toStdString();
    if (output_key.empty()) {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Error: Output key name is empty."));
        _error_label->setVisible(true);
        return;
    }

    syncJsonFromUI();

    // -------------------------------------------------------------------------
    // Option A execution path: single-step binary transforms use
    // DataManagerPipelineExecutor with input_key + additional_input_keys.
    // Unary pipelines (single or multi-step) continue using executePipeline().
    // -------------------------------------------------------------------------
    if (isSingleStepBinaryPipeline()) {
        auto const & step = _step_list->steps().front();
        auto const multi_info = getMultiInputTransformInfo(step.transform_name);
        if (!multi_info.has_value() || !step.additional_input_key.has_value()) {
            _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
            _error_label->setText(tr("Error: Second input key is not configured."));
            _error_label->setVisible(true);
            return;
        }

        auto const ordered_keys = resolveOrderedBinaryInputKeys(
                _input_data_key,
                _input_container_type,
                *step.additional_input_key,
                multi_info->individual_input_types);
        if (!ordered_keys.has_value()) {
            _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
            _error_label->setText(
                    tr("Error: Primary and second input keys must be different and match the "
                       "transform's expected input types."));
            _error_label->setVisible(true);
            return;
        }

        if (hasJsonPreOrRangeReduction()) {
            _error_label->setStyleSheet("color: #b36b00; font-weight: bold; padding: 4px;");
            _error_label->setText(
                    tr("Note: Pre-reductions and range reduction are not applied for multi-input "
                       "execution in this release."));
            _error_label->setVisible(true);
            QApplication::processEvents();
        }

        nlohmann::json parameters = nlohmann::json::object();
        if (step.parameters_json != "{}" && !step.parameters_json.empty()) {
            try {
                parameters = nlohmann::json::parse(step.parameters_json);
            } catch (...) {
                _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
                _error_label->setText(tr("Error: Step parameters are not valid JSON."));
                _error_label->setVisible(true);
                return;
            }
        }

        nlohmann::json step_json;
        step_json["step_id"] = step.step_id;
        step_json["transform_name"] = step.transform_name;
        step_json["input_key"] = ordered_keys->first;
        step_json["additional_input_keys"] = nlohmann::json::array({ordered_keys->second});
        step_json["output_key"] = output_key;
        step_json["parameters"] = parameters;

        std::string const otk = _output_time_key_combo
                                        ? _output_time_key_combo->currentData().toString().toStdString()
                                        : std::string{};
        if (!otk.empty()) {
            step_json["output_time_key"] = otk;
        }

        nlohmann::json pipeline_json;
        pipeline_json["steps"] = nlohmann::json::array({step_json});

        _progress_bar->setRange(0, 1);
        _progress_bar->setValue(0);
        _progress_bar->setVisible(true);
        _progress_label->setText(tr("Executing multi-input pipeline..."));
        _progress_label->setVisible(true);
        _execute_button->setEnabled(false);
        QApplication::processEvents();

        auto start_time = std::chrono::steady_clock::now();

        try {
            DataManagerPipelineExecutor executor(dm);
            if (!executor.loadFromJson(pipeline_json)) {
                throw std::runtime_error("Failed to load multi-input pipeline configuration");
            }

            auto result = executor.execute();
            if (!result.success) {
                throw std::runtime_error(result.error_message.empty()
                                                 ? "Multi-input pipeline execution failed"
                                                 : result.error_message);
            }

            auto end_time = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      end_time - start_time)
                                      .count();

            _state->setOutputDataKey(output_key);
            _progress_bar->setValue(1);
            _progress_label->setVisible(false);
            _error_label->setStyleSheet("color: green; font-weight: bold; padding: 4px;");
            _error_label->setText(
                    tr("Success! Result stored as '%1' (%2 ms)")
                            .arg(QString::fromStdString(output_key))
                            .arg(elapsed_ms));
            _error_label->setVisible(true);
        } catch (std::exception const & e) {
            spdlog::error("[TransformsV2Widget] Multi-input execution exception: {}", e.what());
            _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
            _error_label->setText(
                    tr("Execution error: %1").arg(QString::fromUtf8(e.what())));
            _error_label->setVisible(true);
        }

        _progress_bar->setVisible(false);
        _progress_label->setVisible(false);
        updateExecuteButtonState();
        tryDeliverPipeline();
        return;
    }

    auto json_str = _json_panel->toPlainText().toStdString();
    if (json_str.empty()) {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Error: Pipeline JSON is empty."));
        _error_label->setVisible(true);
        return;
    }

    spdlog::debug("[TransformsV2Widget] onExecuteClicked: input_key='{}', output_key='{}'",
                  _input_data_key, output_key);
    spdlog::debug("[TransformsV2Widget] Pipeline JSON ({} chars): {}",
                  json_str.size(), json_str);

    // Build the TransformPipeline from the PipelineDescriptor JSON
    auto pipeline_result = loadPipelineFromJson(json_str);
    if (!pipeline_result) {
        auto err_msg = std::string(pipeline_result.error()->what());
        spdlog::error("[TransformsV2Widget] loadPipelineFromJson FAILED: {}", err_msg);
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(
                tr("Error: Failed to build pipeline from JSON: %1")
                        .arg(QString::fromStdString(err_msg)));
        _error_label->setVisible(true);
        return;
    }
    spdlog::debug("[TransformsV2Widget] Pipeline built successfully ({} steps)",
                  pipeline_result.value().size());

    // Get the input data variant
    auto input_variant = dm->getDataVariant(_input_data_key);
    if (!input_variant.has_value()) {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(
                tr("Error: Input data key '%1' not found in DataManager.")
                        .arg(QString::fromStdString(_input_data_key)));
        _error_label->setVisible(true);
        return;
    }

    // Show progress
    auto const & steps = _step_list->steps();
    int const total_steps = static_cast<int>(steps.size());
    _progress_bar->setRange(0, total_steps > 0 ? total_steps : 1);
    _progress_bar->setValue(0);
    _progress_bar->setVisible(true);
    _progress_label->setText(tr("Executing pipeline..."));
    _progress_label->setVisible(true);
    _execute_button->setEnabled(false);
    QApplication::processEvents();

    // Execute the pipeline (uses fusion for element-wise chains)
    auto start_time = std::chrono::steady_clock::now();

    try {
        for (int i = 0; i < total_steps; ++i) {
            _progress_bar->setValue(i);
            _progress_label->setText(
                    tr("Step %1/%2: %3")
                            .arg(i + 1)
                            .arg(total_steps)
                            .arg(QString::fromStdString(steps[static_cast<size_t>(i)].transform_name)));
            QApplication::processEvents();
        }

        spdlog::debug("[TransformsV2Widget] Calling executePipeline with input variant index={}",
                      input_variant.value().index());
        auto result = executePipeline(input_variant.value(), pipeline_result.value());
        spdlog::debug("[TransformsV2Widget] executePipeline returned variant index={}", result.index());

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  end_time - start_time)
                                  .count();

        // Determine TimeKey: use output_time_key combo if set, otherwise inherit from input
        std::string const otk = _output_time_key_combo
                                        ? _output_time_key_combo->currentData().toString().toStdString()
                                        : std::string{};
        TimeKey const time_key = otk.empty() ? dm->getTimeKey(_input_data_key) : TimeKey(otk);
        dm->setData(output_key, result, time_key);

        _state->setOutputDataKey(output_key);

        _progress_bar->setValue(total_steps);
        _progress_label->setVisible(false);
        _error_label->setStyleSheet("color: green; font-weight: bold; padding: 4px;");
        _error_label->setText(
                tr("Success! Result stored as '%1' (%2 ms)")
                        .arg(QString::fromStdString(output_key))
                        .arg(elapsed_ms));
        _error_label->setVisible(true);

    } catch (std::exception const & e) {
        spdlog::error("[TransformsV2Widget] Execution exception: {}", e.what());
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(
                tr("Execution error: %1").arg(QString::fromUtf8(e.what())));
        _error_label->setVisible(true);
    }

    // Restore UI
    _progress_bar->setVisible(false);
    _progress_label->setVisible(false);
    updateExecuteButtonState();

    // Also deliver to pending consumer if one exists
    tryDeliverPipeline();
}

// ============================================================================
// OperationContext Integration
// ============================================================================

void TransformsV2Properties_Widget::setOperationContext(EditorLib::OperationContext * context) {
    // Disconnect from old context
    if (_operation_context) {
        disconnect(_operation_context, nullptr, this, nullptr);
    }

    _operation_context = context;

    if (_operation_context) {
        connect(_operation_context,
                &EditorLib::OperationContext::pendingOperationChanged,
                this,
                &TransformsV2Properties_Widget::onPendingOperationChanged);
    }

    updateDeliverButtonState();
}

void TransformsV2Properties_Widget::onDeliverPipelineClicked() {
    if (tryDeliverPipeline()) {
        _error_label->setStyleSheet("color: green; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Pipeline delivered to requester."));
        _error_label->setVisible(true);
    } else {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Failed to deliver pipeline. No pending request found."));
        _error_label->setVisible(true);
    }
}

void TransformsV2Properties_Widget::onPendingOperationChanged(
        EditorLib::EditorTypeId const & producer_type) {

    // Only respond to changes for our producer type
    if (producer_type.toStdString() != "TransformsV2Widget") {
        return;
    }

    updateDeliverButtonState();

    // Check for seed data in the new pending operation
    if (_operation_context) {
        static auto const tv2_type = EditorLib::EditorTypeId(
                QStringLiteral("TransformsV2Widget"));
        auto pending = _operation_context->pendingOperationFor(tv2_type);
        if (pending.has_value() && pending->initialSeed()) {
            loadSeedFromOperation(*pending);
        }
    }
}

bool TransformsV2Properties_Widget::tryDeliverPipeline() {
    if (!_operation_context) {
        return false;
    }

    static auto const tv2_type = EditorLib::EditorTypeId(
            QStringLiteral("TransformsV2Widget"));

    auto pending = _operation_context->pendingOperationFor(tv2_type);
    if (!pending.has_value()) {
        return false;
    }

    // Get the current pipeline JSON from the panel
    auto json_str = _json_panel->toPlainText().trimmed().toStdString();
    if (json_str.empty()) {
        // Try building from the UI
        json_str = buildJsonFromUI();
    }

    if (json_str.empty()) {
        return false;
    }

    // Wrap pipeline JSON with input metadata so the requester knows
    // which DataManager key and data type the pipeline was configured for.
    nlohmann::json envelope;
    try {
        envelope["pipeline"] = nlohmann::json::parse(json_str);
    } catch (...) {
        // If the JSON panel text isn't valid JSON, store as raw string
        envelope["pipeline"] = json_str;
    }
    if (!_input_data_key.empty()) {
        envelope["input_key"] = _input_data_key;
    }
    if (!_input_data_type_name.empty()) {
        envelope["input_type"] = _input_data_type_name;
    }

    auto const envelope_str = envelope.dump();

    // Deliver the envelope as an OperationResult
    auto result = EditorLib::OperationResult::create(
            EditorLib::DataChannels::TransformPipeline,
            envelope_str);

    return _operation_context->deliverResult(tv2_type, result);
}

void TransformsV2Properties_Widget::updateDeliverButtonState() {
    if (!_deliver_pipeline_btn || !_operation_context) {
        if (_deliver_pipeline_btn) {
            _deliver_pipeline_btn->setVisible(false);
        }
        return;
    }

    static auto const tv2_type = EditorLib::EditorTypeId(
            QStringLiteral("TransformsV2Widget"));

    auto pending = _operation_context->pendingOperationFor(tv2_type);
    bool const has_pending = pending.has_value();
    bool const multi_input_blocks_delivery = _step_list && _step_list->hasMultiInputStep();

    _deliver_pipeline_btn->setVisible(has_pending);
    _deliver_pipeline_btn->setEnabled(has_pending && !multi_input_blocks_delivery);
    if (multi_input_blocks_delivery) {
        _deliver_pipeline_btn->setToolTip(
                tr("Pipeline delivery does not include second input bindings yet."));
    } else {
        _deliver_pipeline_btn->setToolTip(
                tr("Deliver the current pipeline JSON to the widget that requested it"));
    }
}

void TransformsV2Properties_Widget::loadSeedFromOperation(
        EditorLib::PendingOperation const & op) {

    auto const * seed = op.initialSeed();
    if (!seed) {
        return;
    }

    auto const * json_ptr = seed->peek<std::string>();
    if (!json_ptr) {
        return;
    }

    // Parse the envelope — same format as delivery: { pipeline, input_key, input_type }
    try {
        auto envelope = nlohmann::json::parse(*json_ptr);

        std::string pipeline_json_str;
        if (envelope.contains("pipeline")) {
            auto const & pipe = envelope["pipeline"];
            pipeline_json_str = pipe.is_string()
                                        ? pipe.get<std::string>()
                                        : pipe.dump();
        } else {
            // Fallback: treat entire string as pipeline JSON
            pipeline_json_str = *json_ptr;
        }

        // Set input key and type if provided, before loading pipeline
        // (loading pipeline pins the input)
        if (envelope.contains("input_key")) {
            auto input_key = envelope["input_key"].get<std::string>();
            _input_data_key = input_key;
            _state->setInputDataKey(input_key);

            if (envelope.contains("input_type")) {
                _input_data_type_name = envelope["input_type"].get<std::string>();
            } else {
                _input_data_type_name = resolveDataTypeFromManager(_input_data_key);
            }

            resolveInputTypes();
            updateInputDisplay();
            _step_list->setInputType(_input_element_type, _input_container_type);
        }

        // Load the pipeline into the UI
        if (!pipeline_json_str.empty()) {
            loadUIFromJson(pipeline_json_str);
        }
    } catch (...) {
        // If envelope parsing fails, try loading as raw pipeline JSON
        loadUIFromJson(*json_ptr);
    }
}