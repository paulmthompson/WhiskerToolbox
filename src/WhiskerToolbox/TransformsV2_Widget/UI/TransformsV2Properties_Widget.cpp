#include "TransformsV2Properties_Widget.hpp"
#include "ui_TransformsV2Properties_Widget.h"

#include "PipelineStepListWidget.hpp"
#include "StepConfigPanel.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/TransformsV2State.hpp"
#include "DataManager/DataManager.hpp"
#include "EditorState/OperationContext.hpp"
#include "EditorState/SelectionContext.hpp"
#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/detail/ContainerTraits.hpp"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

#include <chrono>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

// ============================================================================
// Construction / Destruction
// ============================================================================

TransformsV2Properties_Widget::TransformsV2Properties_Widget(
        std::shared_ptr<TransformsV2State> state,
        SelectionContext * selection_context,
        QWidget * parent)
    : QWidget(parent),
      ui(std::make_unique<Ui::TransformsV2Properties_Widget>()),
      _state(std::move(state)),
      _selection_context(selection_context) {

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
    updateExecuteButtonState();
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
    updateOutputKeyFromPipeline();
    updateExecuteButtonState();
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
    updateExecuteButtonState();
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

    // Execution mode combo
    auto * mode_layout = new QHBoxLayout();
    auto * mode_label = new QLabel(tr("Mode:"), _output_group);
    _execution_mode_combo = new QComboBox(_output_group);
    _execution_mode_combo->addItem(tr("Save to DataManager"), QStringLiteral("data_manager"));
    _execution_mode_combo->addItem(tr("JSON Pipeline Only"), QStringLiteral("json_only"));
    _execution_mode_combo->setToolTip(tr("Save to DataManager materializes results; JSON Pipeline Only just produces the descriptor"));

    // Restore saved execution mode
    auto const & saved_mode = _state->executionMode();
    int mode_index = _execution_mode_combo->findData(QString::fromStdString(saved_mode));
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

    _load_json_button = new QPushButton(tr("Load..."));
    _load_json_button->setToolTip(tr("Load pipeline JSON from file"));
    json_button_layout->addWidget(_load_json_button);

    _save_json_button = new QPushButton(tr("Save..."));
    _save_json_button->setToolTip(tr("Save pipeline JSON to file"));
    json_button_layout->addWidget(_save_json_button);

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
    // Output & Execution connections
    connect(_execute_button, &QPushButton::clicked,
            this, &TransformsV2Properties_Widget::onExecuteClicked);
    connect(_output_key_edit, &QLineEdit::textEdited,
            this, &TransformsV2Properties_Widget::onOutputKeyEdited);
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

std::string TransformsV2Properties_Widget::buildJsonFromUI() const {
    PipelineDescriptor descriptor;

    // Build steps from the step list widget
    auto const & steps = _step_list->steps();
    for (auto const & step: steps) {
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

    // Load steps (pre-reductions from JSON are preserved in the descriptor
    // but not exposed in the UI — they are an implementation detail)
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

    // For data_manager mode: need valid input, steps, and output key
    // For json_only mode: just need steps (the JSON is already produced)
    bool can_execute = false;
    if (is_data_manager_mode) {
        can_execute = has_input && has_steps && has_output_key;
    } else {
        // JSON-only mode: the JSON panel is always up-to-date; nothing to "execute"
        // But allow the button so users get feedback
        can_execute = has_steps;
    }

    _execute_button->setEnabled(can_execute);
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

    // Get the JSON from the panel
    auto json_str = _json_panel->toPlainText().toStdString();
    if (json_str.empty()) {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Error: Pipeline JSON is empty."));
        _error_label->setVisible(true);
        return;
    }

    // Build the TransformPipeline from the PipelineDescriptor JSON
    auto pipeline_result = loadPipelineFromJson(json_str);
    if (!pipeline_result) {
        _error_label->setStyleSheet("color: red; font-weight: bold; padding: 4px;");
        _error_label->setText(tr("Error: Failed to build pipeline from JSON."));
        _error_label->setVisible(true);
        return;
    }

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
    int total_steps = static_cast<int>(steps.size());
    _progress_bar->setRange(0, total_steps > 0 ? total_steps : 1);
    _progress_bar->setValue(0);
    _progress_bar->setVisible(true);
    _progress_label->setText(tr("Executing pipeline..."));
    _progress_label->setVisible(true);
    _execute_button->setEnabled(false);
    QApplication::processEvents();

    // Execute the pipeline
    auto start_time = std::chrono::steady_clock::now();

    try {
        // Update progress for each step (the executePipeline free function doesn't
        // provide per-step callbacks, so we show overall progress at start/end)
        for (int i = 0; i < total_steps; ++i) {
            _progress_bar->setValue(i);
            _progress_label->setText(
                    tr("Step %1/%2: %3")
                            .arg(i + 1)
                            .arg(total_steps)
                            .arg(QString::fromStdString(steps[static_cast<size_t>(i)].transform_name)));
            QApplication::processEvents();
        }

        auto result = executePipeline(input_variant.value(), pipeline_result.value());

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  end_time - start_time)
                                  .count();

        // Store the result in DataManager using the same time key as the input
        auto time_key = dm->getTimeKey(_input_data_key);
        dm->setData(output_key, result, time_key);

        // Store the output key in state
        _state->setOutputDataKey(output_key);

        // Show success
        _progress_bar->setValue(total_steps);
        _progress_label->setVisible(false);
        _error_label->setStyleSheet("color: green; font-weight: bold; padding: 4px;");
        _error_label->setText(
                tr("Success! Result stored as '%1' (%2 ms)")
                        .arg(QString::fromStdString(output_key))
                        .arg(elapsed_ms));
        _error_label->setVisible(true);

    } catch (std::exception const & e) {
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

    // Deliver the pipeline JSON as an OperationResult
    auto result = EditorLib::OperationResult::create(
            EditorLib::DataChannels::TransformPipeline,
            json_str);

    return _operation_context->deliverResult(tv2_type, std::move(result));
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
    _deliver_pipeline_btn->setVisible(pending.has_value());
}