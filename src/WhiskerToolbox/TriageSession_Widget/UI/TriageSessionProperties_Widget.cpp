/**
 * @file TriageSessionProperties_Widget.cpp
 * @brief Implementation of the Triage Session properties widget
 */

#include "TriageSessionProperties_Widget.hpp"

#include "Commands/Core/CommandDescriptor.hpp"
#include "Core/TriageSessionState.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "GuidedPipelineEditor.hpp"
#include "KeymapSystem/KeyActionAdapter.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "StateManagement/AppFileDialog.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TriageSession/TriageSession.hpp"

#include <rfl/json.hpp>

#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include <iostream>

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
    _syncPipelineFromState();
    _updateStateDisplay();
    _updateButtonStates();
}

TriageSessionProperties_Widget::~TriageSessionProperties_Widget() = default;

void TriageSessionProperties_Widget::setKeymapManager(KeymapSystem::KeymapManager * manager) {
    if (!manager || _key_adapter) {
        return;
    }

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
        return false;
    });

    manager->registerAdapter(_key_adapter);
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

    // Pipeline name
    _pipeline_name_label = new QLabel(tr("No pipeline loaded"), this);
    _pipeline_name_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    pipeline_layout->addWidget(_pipeline_name_label);

    // Load button
    auto * pipeline_button_layout = new QHBoxLayout();
    _load_pipeline_button = new QPushButton(tr("Load JSON..."), this);
    _load_pipeline_button->setToolTip(tr("Load a command sequence JSON file"));
    pipeline_button_layout->addWidget(_load_pipeline_button);
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
    connect(_pipeline_text_edit, &QTextEdit::textChanged,
            this, &TriageSessionProperties_Widget::_onPipelineTextChanged);
    connect(_guided_editor, &GuidedPipelineEditor::pipelineChanged,
            this, &TriageSessionProperties_Widget::_onGuidedEditorChanged);

    if (_editor_registry) {
        connect(_editor_registry, &EditorRegistry::timeChanged,
                this, &TriageSessionProperties_Widget::_onTimeChanged);
    }

    if (_state) {
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
    auto json_str = _pipeline_text_edit->toPlainText().toStdString();

    if (json_str.empty()) {
        _pipeline_name_label->setText(tr("No pipeline loaded"));
        return;
    }

    auto result = rfl::json::read<commands::CommandSequenceDescriptor>(json_str);
    if (!result) {
        _pipeline_name_label->setText(tr("Invalid JSON"));
        return;
    }

    auto const & seq = *result;
    _session->setPipeline(seq);

    if (_state) {
        _state->setPipelineJson(json_str);
    }

    // Sync to guided editor (without re-triggering)
    _guided_editor->blockSignals(true);
    _guided_editor->fromSequence(seq);
    _guided_editor->blockSignals(false);

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
        _state->setPipelineJson(json_str);
        _syncing_from_editor = false;
    }

    // Sync to JSON editor (without re-triggering)
    _pipeline_text_edit->blockSignals(true);
    _pipeline_text_edit->setPlainText(QString::fromStdString(json_str));
    _pipeline_text_edit->blockSignals(false);

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
    if (!_state || _syncing_from_editor) {
        return;
    }

    auto pipeline_json = _state->pipelineJson();
    if (!pipeline_json.has_value()) {
        return;
    }

    auto result = rfl::json::read<commands::CommandSequenceDescriptor>(*pipeline_json);
    if (result) {
        _session->setPipeline(*result);

        _pipeline_text_edit->blockSignals(true);
        _pipeline_text_edit->setPlainText(QString::fromStdString(*pipeline_json));
        _pipeline_text_edit->blockSignals(false);

        _guided_editor->blockSignals(true);
        _guided_editor->fromSequence(*result);
        _guided_editor->blockSignals(false);

        _updateCommandSummary();
        _updateButtonStates();
    }
}
