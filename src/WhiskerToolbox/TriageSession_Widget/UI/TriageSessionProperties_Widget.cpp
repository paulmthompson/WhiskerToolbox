/**
 * @file TriageSessionProperties_Widget.cpp
 * @brief Implementation of the Triage Session properties widget
 */

#include "TriageSessionProperties_Widget.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/Validation.hpp"
#include "Core/TriageSessionState.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "GuidedPipelineEditor.hpp"
#include "KeymapSystem/KeyActionAdapter.hpp"
#include "KeymapSystem/KeymapCommandBridge.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "StateManagement/AppFileDialog.hpp"
#include "TimeController/TimeController.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TriageSession/TriageSession.hpp"

#include <rfl/json.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

#include <iostream>

namespace {

QString formatMessages(QString const & heading, std::vector<std::string> const & messages) {
    QString text = heading;
    for (auto const & msg: messages) {
        text += QStringLiteral("\n- ");
        text += QString::fromStdString(msg);
    }
    return text;
}

commands::CommandContext makeValidationContext(
        std::shared_ptr<DataManager> data_manager,
        EditorRegistry * registry) {
    commands::CommandContext ctx;
    ctx.data_manager = std::move(data_manager);
    ctx.time_controller = registry != nullptr ? registry->timeController() : nullptr;

    if (ctx.time_controller != nullptr) {
        ctx.runtime_variables["current_frame"] =
                std::to_string(ctx.time_controller->currentTimeIndex().getValue());
        ctx.runtime_variables["current_time_key"] =
                ctx.time_controller->activeTimeKey().str();
    }

    return ctx;
}

}// namespace

TriageSessionProperties_Widget::TriageSessionProperties_Widget(
        std::shared_ptr<TriageSessionState> state,
        EditorRegistry * editor_registry,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _editor_registry(editor_registry),
      _session(std::make_unique<triage::TriageSession>()) {

    _buildUI();
    _connectSignals();
    _syncSlotEditorFromState();
    _updateStateDisplay();
    _updateButtonStates();
}

TriageSessionProperties_Widget::~TriageSessionProperties_Widget() = default;

void TriageSessionProperties_Widget::setKeymapManager(KeymapSystem::KeymapManager * manager) {
    if (!manager || _key_adapter) {
        return;
    }

    _keymap_manager = manager;

    _key_adapter = new KeymapSystem::KeyActionAdapter(this);
    _key_adapter->setTypeId(EditorLib::EditorTypeId(QStringLiteral("TriageSessionWidget")));

    _key_adapter->setHandler([this](QString const & action_id) -> bool {
        if (action_id == QStringLiteral("triage.mark")) {
            _onMarkClicked();
            return true;
        }
        if (action_id == QStringLiteral("triage.commit")) {
            _onCommitClicked();
            return true;
        }
        if (action_id == QStringLiteral("triage.recall")) {
            _onRecallClicked();
            return true;
        }
        if (action_id.startsWith(QStringLiteral("triage.sequence_slot_"))) {
            return _handleSequenceSlotAction(action_id);
        }
        return false;
    });

    manager->registerAdapter(_key_adapter);
    connect(manager, &KeymapSystem::KeymapManager::bindingsChanged,
            this, [this]() { _updateSlotBindingDisplay(); });
    _updateSlotBindingDisplay();
}

void TriageSessionProperties_Widget::_buildUI() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(6, 6, 6, 6);
    main_layout->setSpacing(8);

    // === Status Section ===
    auto * status_group = new QGroupBox(tr("Status"), this);
    auto * status_layout = new QVBoxLayout(status_group);
    _status_label = new QLabel(tr("State: Idle"), this);
    _status_label->setWordWrap(true);
    status_layout->addWidget(_status_label);
    main_layout->addWidget(status_group);

    // === Controls Section ===
    auto * controls_group = new QGroupBox(tr("Controls"), this);
    auto * controls_layout = new QHBoxLayout(controls_group);

    _mark_button = new QPushButton(tr("Mark"), this);
    _mark_button->setToolTip(tr("Start marking from the current frame (M)"));
    controls_layout->addWidget(_mark_button);

    _commit_button = new QPushButton(tr("Commit"), this);
    _commit_button->setToolTip(tr("Execute pipeline for [mark_frame, current_frame] (C)"));
    controls_layout->addWidget(_commit_button);

    _recall_button = new QPushButton(tr("Recall"), this);
    _recall_button->setToolTip(tr("Abort marking and return to idle (R)"));
    controls_layout->addWidget(_recall_button);

    main_layout->addWidget(controls_group);

    // === Pipeline Section ===
    auto * pipeline_group = new QGroupBox(tr("Pipeline"), this);
    auto * pipeline_layout = new QVBoxLayout(pipeline_group);

    // Slot controls
    auto * slot_row_layout = new QHBoxLayout();
    slot_row_layout->addWidget(new QLabel(tr("Slot"), this));
    _slot_selector = new QComboBox(this);
    for (int slot_index = 1; slot_index <= TriageSessionState::sequenceSlotCount(); ++slot_index) {
        _slot_selector->addItem(tr("Slot %1").arg(slot_index), slot_index);
    }
    slot_row_layout->addWidget(_slot_selector);

    _slot_enabled_checkbox = new QCheckBox(tr("Enabled"), this);
    _slot_enabled_checkbox->setChecked(true);
    slot_row_layout->addWidget(_slot_enabled_checkbox);
    slot_row_layout->addStretch();
    pipeline_layout->addLayout(slot_row_layout);

    auto * slot_name_layout = new QHBoxLayout();
    slot_name_layout->addWidget(new QLabel(tr("Display Name"), this));
    _slot_name_edit = new QLineEdit(this);
    _slot_name_edit->setPlaceholderText(tr("Optional slot label"));
    slot_name_layout->addWidget(_slot_name_edit);
    pipeline_layout->addLayout(slot_name_layout);

    _slot_action_id_label = new QLabel(this);
    _slot_binding_label = new QLabel(this);
    _slot_action_id_label->setTextFormat(Qt::PlainText);
    _slot_binding_label->setTextFormat(Qt::PlainText);
    pipeline_layout->addWidget(_slot_action_id_label);
    pipeline_layout->addWidget(_slot_binding_label);

    // Pipeline name
    _pipeline_name_label = new QLabel(tr("No pipeline loaded"), this);
    _pipeline_name_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    pipeline_layout->addWidget(_pipeline_name_label);

    _pipeline_validation_label = new QLabel(this);
    _pipeline_validation_label->setWordWrap(true);
    _pipeline_validation_label->setStyleSheet(QStringLiteral("color: #b46900;"));
    _pipeline_validation_label->hide();
    pipeline_layout->addWidget(_pipeline_validation_label);

    // Load button
    auto * pipeline_button_layout = new QHBoxLayout();
    _load_pipeline_button = new QPushButton(tr("Load JSON..."), this);
    _load_pipeline_button->setToolTip(tr("Load JSON into the selected slot"));
    pipeline_button_layout->addWidget(_load_pipeline_button);

    _copy_pipeline_button = new QPushButton(tr("Copy JSON"), this);
    _copy_pipeline_button->setToolTip(tr("Copy selected slot JSON to clipboard"));
    pipeline_button_layout->addWidget(_copy_pipeline_button);

    _export_pipeline_button = new QPushButton(tr("Export JSON..."), this);
    _export_pipeline_button->setToolTip(tr("Export selected slot JSON to file"));
    pipeline_button_layout->addWidget(_export_pipeline_button);

    pipeline_button_layout->addStretch();
    pipeline_layout->addLayout(pipeline_button_layout);

    // Guided command editor
    _guided_editor = new GuidedPipelineEditor(this);
    pipeline_layout->addWidget(_guided_editor);

    // Collapsible raw JSON editor
    _json_group = new QGroupBox(tr("Raw JSON"), this);
    _json_group->setCheckable(true);
    _json_group->setChecked(false);
    auto * json_layout = new QVBoxLayout(_json_group);
    _pipeline_text_edit = new QTextEdit(this);
    _pipeline_text_edit->setPlaceholderText(tr("Paste or load a command sequence JSON..."));
    _pipeline_text_edit->setMaximumHeight(200);
    _pipeline_text_edit->setAcceptRichText(false);
    json_layout->addWidget(_pipeline_text_edit);
    pipeline_layout->addWidget(_json_group);

    main_layout->addWidget(pipeline_group);

    // === Tracked Regions Section ===
    auto * tracked_group = new QGroupBox(tr("Tracked Regions"), this);
    auto * tracked_layout = new QVBoxLayout(tracked_group);
    _tracked_summary_label = new QLabel(tr("No tracked regions data"), this);
    _tracked_summary_label->setWordWrap(true);
    tracked_layout->addWidget(_tracked_summary_label);
    main_layout->addWidget(tracked_group);

    main_layout->addStretch();
}

void TriageSessionProperties_Widget::_connectSignals() {
    connect(_mark_button, &QPushButton::clicked,
            this, &TriageSessionProperties_Widget::_onMarkClicked);
    connect(_commit_button, &QPushButton::clicked,
            this, &TriageSessionProperties_Widget::_onCommitClicked);
    connect(_recall_button, &QPushButton::clicked,
            this, &TriageSessionProperties_Widget::_onRecallClicked);
    connect(_load_pipeline_button, &QPushButton::clicked,
            this, &TriageSessionProperties_Widget::_onLoadPipelineClicked);
    connect(_copy_pipeline_button, &QPushButton::clicked,
            this, &TriageSessionProperties_Widget::_onCopyPipelineClicked);
    connect(_export_pipeline_button, &QPushButton::clicked,
            this, &TriageSessionProperties_Widget::_onExportPipelineClicked);
    connect(_pipeline_text_edit, &QTextEdit::textChanged,
            this, &TriageSessionProperties_Widget::_onPipelineTextChanged);
    connect(_guided_editor, &GuidedPipelineEditor::pipelineChanged,
            this, &TriageSessionProperties_Widget::_onGuidedEditorChanged);
    connect(_slot_selector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TriageSessionProperties_Widget::_onSlotSelectionChanged);
    connect(_slot_name_edit, &QLineEdit::textChanged,
            this, &TriageSessionProperties_Widget::_onSlotDisplayNameChanged);
    connect(_slot_enabled_checkbox, &QCheckBox::toggled,
            this, &TriageSessionProperties_Widget::_onSlotEnabledChanged);

    if (_editor_registry) {
        connect(_editor_registry, &EditorRegistry::timeChanged,
                this, &TriageSessionProperties_Widget::_onTimeChanged);
    }

    if (_state) {
        connect(_state.get(), &TriageSessionState::sequenceSlotsChanged,
                this, &TriageSessionProperties_Widget::_syncPipelineFromState);
        connect(_state.get(), &TriageSessionState::pipelineChanged,
                this, &TriageSessionProperties_Widget::_syncPipelineFromState);
    }
}

void TriageSessionProperties_Widget::_onMarkClicked() {
    _session->mark(_current_frame);
    _updateStateDisplay();
    _updateButtonStates();
}

void TriageSessionProperties_Widget::_onCommitClicked() {
    if (!_state || !_state->dataManager()) {
        QMessageBox::warning(this, tr("Commit Failed"),
                             tr("No DataManager available."));
        return;
    }

    TimeController * const time_controller =
            _editor_registry != nullptr ? _editor_registry->timeController() : nullptr;
    auto result = _session->commit(_current_frame, _state->dataManager(), time_controller, _command_recorder);

    if (!result.success) {
        QMessageBox::warning(this, tr("Commit Failed"),
                             QString::fromStdString(result.error_message));
    }

    _updateStateDisplay();
    _updateButtonStates();
    _updateTrackedRegionsSummary();
}

void TriageSessionProperties_Widget::_onRecallClicked() {
    _session->recall();
    _updateStateDisplay();
    _updateButtonStates();
}

void TriageSessionProperties_Widget::_onLoadPipelineClicked() {
    auto filepath = AppFileDialog::getOpenFileName(
            this, QStringLiteral("triage_pipeline"),
            tr("Load Pipeline JSON"),
            tr("JSON Files (*.json);;All Files (*)"));

    if (filepath.isEmpty()) {
        return;
    }

    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Load Failed"),
                             tr("Could not open file: %1").arg(filepath));
        return;
    }

    auto json_str = QString(file.readAll());
    file.close();

    // Block signals to avoid re-entrant parsing while setting text
    _pipeline_text_edit->blockSignals(true);
    _pipeline_text_edit->setPlainText(json_str);
    _pipeline_text_edit->blockSignals(false);

    // Parse and apply
    _onPipelineTextChanged();
}

void TriageSessionProperties_Widget::_onPipelineTextChanged() {
    if (_syncing_from_editor) {
        return;
    }

    auto json_str = _pipeline_text_edit->toPlainText().toStdString();

    if (json_str.empty()) {
        _pipeline_name_label->setText(tr("No pipeline loaded"));
        _clearValidationBanner();
        _guided_editor->blockSignals(true);
        _guided_editor->clear();
        _guided_editor->blockSignals(false);
        _session->setPipeline(commands::CommandSequenceDescriptor{});
        if (_state) {
            _state->setSequenceSlotJson(_selected_slot_index, std::nullopt);
        }
        _updateButtonStates();
        return;
    }

    auto result = rfl::json::read<commands::CommandSequenceDescriptor>(json_str);
    if (!result) {
        _pipeline_name_label->setText(tr("Invalid JSON"));
        _pipeline_validation_label->setText(tr("JSON parse failed. Fix syntax before execution."));
        _pipeline_validation_label->show();
        return;
    }

    auto const & seq = *result;
    _session->setPipeline(seq);

    if (_state) {
        _syncing_from_editor = true;
        _state->setSequenceSlotJson(_selected_slot_index, json_str);
        _syncing_from_editor = false;
    }

    // Sync to guided editor (without re-triggering)
    _guided_editor->blockSignals(true);
    _guided_editor->fromSequence(seq);
    _guided_editor->blockSignals(false);

    _updateValidationBanner(seq);
    _updateCommandSummary();
    _updateButtonStates();
}

void TriageSessionProperties_Widget::_onGuidedEditorChanged() {
    auto seq = _guided_editor->toSequence();

    // Preserve existing name/version/variables from the current pipeline
    auto const & current = _session->pipeline();
    seq.name = current.name;
    seq.version = current.version;
    seq.variables = current.variables;

    _session->setPipeline(seq);

    auto const json_str = rfl::json::write(seq);

    if (_state) {
        // Guard: prevent _syncPipelineFromState from rebuilding the guided
        // editor in response to the state change we are about to trigger.
        _syncing_from_editor = true;
        _state->setSequenceSlotJson(_selected_slot_index, json_str);
        _syncing_from_editor = false;
    }

    // Sync to JSON editor (without re-triggering)
    _pipeline_text_edit->blockSignals(true);
    _pipeline_text_edit->setPlainText(QString::fromStdString(json_str));
    _pipeline_text_edit->blockSignals(false);

    _updateValidationBanner(seq);
    _updateCommandSummary();
    _updateButtonStates();
}

void TriageSessionProperties_Widget::_onTimeChanged(TimePosition const & position) {
    _current_frame = position.index;
    _updateStateDisplay();
    _updateTrackedRegionsSummary();
}

void TriageSessionProperties_Widget::_updateStateDisplay() {
    auto const state = _session->state();

    if (state == triage::TriageSession::State::Idle) {
        _status_label->setText(
                tr("State: <b>Idle</b> | Frame: %1")
                        .arg(_current_frame.getValue()));
    } else {
        _status_label->setText(
                tr("State: <b>Marking</b> (from %1) | Frame: %2")
                        .arg(_session->markFrame().getValue())
                        .arg(_current_frame.getValue()));
    }
}

void TriageSessionProperties_Widget::_updateButtonStates() {
    auto const state = _session->state();
    bool const has_pipeline = !_session->pipeline().commands.empty();

    _mark_button->setEnabled(state == triage::TriageSession::State::Idle);
    _commit_button->setEnabled(state == triage::TriageSession::State::Marking && has_pipeline);
    _recall_button->setEnabled(state == triage::TriageSession::State::Marking);
}

void TriageSessionProperties_Widget::_updateCommandSummary() {
    auto const & pipeline = _session->pipeline();

    // Pipeline name
    if (pipeline.name.has_value()) {
        _pipeline_name_label->setText(QString::fromStdString(*pipeline.name));
    } else if (pipeline.commands.empty()) {
        _pipeline_name_label->setText(tr("No pipeline loaded"));
    } else {
        _pipeline_name_label->setText(
                tr("Pipeline (%1 commands)").arg(pipeline.commands.size()));
    }
}

void TriageSessionProperties_Widget::_updateTrackedRegionsSummary() {
    if (!_state || !_state->dataManager()) {
        _tracked_summary_label->setText(tr("No DataManager available"));
        return;
    }

    auto const & key = _state->trackedRegionsKey();
    auto dm = _state->dataManager();
    auto intervals = dm->getData<DigitalIntervalSeries>(key);

    if (!intervals) {
        _tracked_summary_label->setText(tr("No tracked regions data (\"%1\")").arg(QString::fromStdString(key)));
        return;
    }

    auto const interval_count = intervals->size();
    int64_t total_frames = 0;
    for (auto const & iwid: intervals->view()) {
        total_frames += iwid.interval.end - iwid.interval.start;
    }

    _tracked_summary_label->setText(
            tr("%1 intervals, %2 frames tracked")
                    .arg(interval_count)
                    .arg(total_frames));
}

void TriageSessionProperties_Widget::_syncPipelineFromState() {
    _syncSlotEditorFromState();
}

QString TriageSessionProperties_Widget::_selectedSlotActionId() const {
    return QStringLiteral("triage.sequence_slot_%1").arg(_selected_slot_index);
}

void TriageSessionProperties_Widget::_refreshSlotSelectorLabels() {
    if (!_state || _syncing_from_editor) {
        return;
    }

    _syncing_slot_controls = true;
    for (int i = 0; i < _slot_selector->count(); ++i) {
        int const slot_index = _slot_selector->itemData(i).toInt();
        auto const slot = _state->sequenceSlot(slot_index);
        QString label = tr("Slot %1").arg(slot_index);
        if (slot.has_value() && !slot->display_name.empty()) {
            label += tr(" - %1").arg(QString::fromStdString(slot->display_name));
        }
        _slot_selector->setItemText(i, label);
    }
    _syncing_slot_controls = false;
}

void TriageSessionProperties_Widget::_updateSlotBindingDisplay() {
    auto const action_id = _selectedSlotActionId();
    _slot_action_id_label->setText(tr("Action ID: %1").arg(action_id));

    if (!_keymap_manager) {
        _slot_binding_label->setText(tr("Binding: unavailable"));
        return;
    }

    auto const binding = _keymap_manager->bindingFor(action_id);
    if (binding.isEmpty()) {
        _slot_binding_label->setText(tr("Binding: <unbound>"));
        return;
    }

    _slot_binding_label->setText(
            tr("Binding: %1").arg(binding.toString(QKeySequence::NativeText)));
}

void TriageSessionProperties_Widget::_syncSlotEditorFromState() {
    if (!_state || _syncing_from_editor) {
        return;
    }

    auto const maybe_slot = _state->sequenceSlot(_selected_slot_index);
    if (!maybe_slot.has_value()) {
        return;
    }

    _refreshSlotSelectorLabels();
    _syncing_slot_controls = true;
    _slot_name_edit->setText(QString::fromStdString(maybe_slot->display_name));
    _slot_enabled_checkbox->setChecked(maybe_slot->enabled);
    _syncing_slot_controls = false;
    _updateSlotBindingDisplay();

    _syncing_from_editor = true;
    if (!maybe_slot->sequence_json.has_value() || maybe_slot->sequence_json->empty()) {
        _session->setPipeline(commands::CommandSequenceDescriptor{});

        _pipeline_text_edit->blockSignals(true);
        _pipeline_text_edit->clear();
        _pipeline_text_edit->blockSignals(false);

        _guided_editor->blockSignals(true);
        _guided_editor->clear();
        _guided_editor->blockSignals(false);

        _pipeline_name_label->setText(tr("No pipeline loaded"));
        _clearValidationBanner();
        _syncing_from_editor = false;
        _updateButtonStates();
        return;
    }

    auto const & pipeline_json = *maybe_slot->sequence_json;
    auto result = rfl::json::read<commands::CommandSequenceDescriptor>(pipeline_json);
    if (!result) {
        _pipeline_text_edit->blockSignals(true);
        _pipeline_text_edit->setPlainText(QString::fromStdString(pipeline_json));
        _pipeline_text_edit->blockSignals(false);

        _guided_editor->blockSignals(true);
        _guided_editor->clear();
        _guided_editor->blockSignals(false);

        _pipeline_name_label->setText(tr("Invalid JSON"));
        _pipeline_validation_label->setText(tr("JSON parse failed. Fix syntax before execution."));
        _pipeline_validation_label->show();
        _syncing_from_editor = false;
        _updateButtonStates();
        return;
    }

    _session->setPipeline(*result);

    _pipeline_text_edit->blockSignals(true);
    _pipeline_text_edit->setPlainText(QString::fromStdString(pipeline_json));
    _pipeline_text_edit->blockSignals(false);

    _guided_editor->blockSignals(true);
    _guided_editor->fromSequence(*result);
    _guided_editor->blockSignals(false);

    _syncing_from_editor = false;
    _updateValidationBanner(*result);
    _updateCommandSummary();
    _updateButtonStates();
}

void TriageSessionProperties_Widget::_onSlotSelectionChanged(int const index) {
    if (_syncing_slot_controls || index < 0) {
        return;
    }

    int const slot_index = _slot_selector->itemData(index).toInt();
    if (slot_index < 1 || slot_index > TriageSessionState::sequenceSlotCount()) {
        return;
    }

    _selected_slot_index = slot_index;
    _syncSlotEditorFromState();
}

void TriageSessionProperties_Widget::_onSlotDisplayNameChanged(QString const & name) {
    if (_syncing_slot_controls || !_state) {
        return;
    }

    auto slot = _state->sequenceSlot(_selected_slot_index);
    if (!slot.has_value()) {
        return;
    }

    slot->display_name = name.toStdString();
    _state->setSequenceSlot(_selected_slot_index, *slot);
    _refreshSlotSelectorLabels();
}

void TriageSessionProperties_Widget::_onSlotEnabledChanged(bool const enabled) {
    if (_syncing_slot_controls || !_state) {
        return;
    }

    auto slot = _state->sequenceSlot(_selected_slot_index);
    if (!slot.has_value()) {
        return;
    }

    slot->enabled = enabled;
    _state->setSequenceSlot(_selected_slot_index, *slot);
}

void TriageSessionProperties_Widget::_onCopyPipelineClicked() {
    if (!_state) {
        return;
    }

    auto const slot = _state->sequenceSlot(_selected_slot_index);
    if (!slot.has_value() || !slot->sequence_json.has_value() || slot->sequence_json->empty()) {
        QMessageBox::information(this,
                                 tr("Nothing to Copy"),
                                 tr("Selected slot does not contain JSON."));
        return;
    }

    if (auto * clipboard = QApplication::clipboard(); clipboard != nullptr) {
        clipboard->setText(QString::fromStdString(*slot->sequence_json));
    }
}

void TriageSessionProperties_Widget::_onExportPipelineClicked() {
    if (!_state) {
        return;
    }

    auto const slot = _state->sequenceSlot(_selected_slot_index);
    if (!slot.has_value() || !slot->sequence_json.has_value() || slot->sequence_json->empty()) {
        QMessageBox::information(this,
                                 tr("Nothing to Export"),
                                 tr("Selected slot does not contain JSON."));
        return;
    }

    auto const suggested_name = QStringLiteral("triage_sequence_slot_%1.json").arg(_selected_slot_index);
    auto file_path = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("triage_slot_pipeline_export"),
            tr("Export Slot JSON"),
            tr("JSON Files (*.json);;All Files (*)"),
            QString{},
            suggested_name);
    if (file_path.isEmpty()) {
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this,
                             tr("Export Failed"),
                             tr("Could not write file: %1").arg(file_path));
        return;
    }

    file.write(QByteArray::fromStdString(*slot->sequence_json));
    file.close();
}

void TriageSessionProperties_Widget::_clearValidationBanner() {
    _pipeline_validation_label->clear();
    _pipeline_validation_label->hide();
}

void TriageSessionProperties_Widget::_updateValidationBanner(
        commands::CommandSequenceDescriptor const & seq) {
    auto ctx = makeValidationContext(
            _state != nullptr ? _state->dataManager() : nullptr,
            _editor_registry);
    auto validation = commands::validateSequence(seq, ctx);

    QStringList banner_parts;
    if (!validation.errors.empty()) {
        banner_parts.push_back(
                formatMessages(tr("Validation errors:"), validation.errors));
    }
    if (!validation.warnings.empty()) {
        banner_parts.push_back(
                formatMessages(tr("Validation warnings:"), validation.warnings));
    }

    if (banner_parts.empty()) {
        _clearValidationBanner();
        return;
    }

    _pipeline_validation_label->setText(banner_parts.join(QStringLiteral("\n")));
    _pipeline_validation_label->show();
}

std::optional<int> TriageSessionProperties_Widget::_parseSequenceSlotIndex(QString const & action_id) {
    QString const prefix = QStringLiteral("triage.sequence_slot_");
    if (!action_id.startsWith(prefix)) {
        return std::nullopt;
    }

    bool ok = false;
    int const slot_index = action_id.mid(prefix.size()).toInt(&ok);
    if (!ok || slot_index < 1 || slot_index > TriageSessionState::sequenceSlotCount()) {
        return std::nullopt;
    }

    return slot_index;
}

bool TriageSessionProperties_Widget::_handleSequenceSlotAction(QString const & action_id) {
    auto const maybe_slot_index = _parseSequenceSlotIndex(action_id);
    if (!maybe_slot_index.has_value()) {
        return false;
    }

    int const slot_index = *maybe_slot_index;

    if (!_state) {
        QMessageBox::warning(this,
                             tr("Sequence Slot Failed"),
                             tr("Triage state is unavailable for slot %1.").arg(slot_index));
        return true;
    }

    auto const slot = _state->sequenceSlot(slot_index);
    if (!slot.has_value()) {
        QMessageBox::warning(this,
                             tr("Sequence Slot Failed"),
                             tr("Sequence slot %1 could not be resolved.").arg(slot_index));
        return true;
    }

    if (!slot->enabled) {
        QMessageBox::information(this,
                                 tr("Sequence Slot Disabled"),
                                 tr("Sequence slot %1 is disabled.").arg(slot_index));
        return true;
    }

    if (!slot->sequence_json.has_value() || slot->sequence_json->empty()) {
        QMessageBox::information(this,
                                 tr("Sequence Slot Empty"),
                                 tr("Sequence slot %1 does not have a command sequence configured.")
                                         .arg(slot_index));
        return true;
    }

    auto sequence_parse_result =
            rfl::json::read<commands::CommandSequenceDescriptor>(*slot->sequence_json);
    if (!sequence_parse_result) {
        QMessageBox::warning(this,
                             tr("Invalid Sequence JSON"),
                             tr("Sequence slot %1 contains invalid command sequence JSON.")
                                     .arg(slot_index));
        return true;
    }

    auto data_manager = _state->dataManager();
    if (!data_manager) {
        QMessageBox::warning(this,
                             tr("Sequence Slot Failed"),
                             tr("No DataManager available for slot %1 execution.").arg(slot_index));
        return true;
    }

    auto validation_ctx = makeValidationContext(data_manager, _editor_registry);
    auto validation = commands::validateSequence(*sequence_parse_result, validation_ctx);
    if (!validation.errors.empty()) {
        QMessageBox::warning(this,
                             tr("Sequence Validation Failed"),
                             formatMessages(tr("Resolve these errors before execution:"),
                                            validation.errors));
        return true;
    }
    if (!validation.warnings.empty()) {
        auto response = QMessageBox::warning(
                this,
                tr("Sequence Validation Warning"),
                formatMessages(tr("Sequence has warnings. Execute anyway?"),
                               validation.warnings),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
        if (response != QMessageBox::Yes) {
            return true;
        }
    }

    auto seq_result = KeymapSystem::executeCommandSequenceFromRegistry(
            std::move(data_manager), _editor_registry, *sequence_parse_result);

    if (!seq_result.result.success) {
        QMessageBox::warning(this,
                             tr("Sequence Execution Failed"),
                             tr("Sequence slot %1 failed: %2")
                                     .arg(slot_index)
                                     .arg(QString::fromStdString(seq_result.result.error_message)));
        return true;
    }

    _updateTrackedRegionsSummary();
    return true;
}
