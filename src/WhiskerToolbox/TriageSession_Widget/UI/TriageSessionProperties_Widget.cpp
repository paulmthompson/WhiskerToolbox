/**
 * @file TriageSessionProperties_Widget.cpp
 * @brief Implementation of the Triage Session properties widget
 */

#include "TriageSessionProperties_Widget.hpp"

#include "Core/TriageSessionState.hpp"
#include "DataManager/Commands/CommandDescriptor.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/EditorRegistry.hpp"
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

    // JSON editor
    _pipeline_text_edit = new QTextEdit(this);
    _pipeline_text_edit->setPlaceholderText(tr("Paste or load a command sequence JSON..."));
    _pipeline_text_edit->setMaximumHeight(200);
    _pipeline_text_edit->setAcceptRichText(false);
    pipeline_layout->addWidget(_pipeline_text_edit);

    // Command summary
    _command_summary_label = new QLabel(this);
    _command_summary_label->setWordWrap(true);
    pipeline_layout->addWidget(_command_summary_label);

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

    auto result = _session->commit(_current_frame, _state->dataManager());

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
        _command_summary_label->clear();
        return;
    }

    auto result = rfl::json::read<commands::CommandSequenceDescriptor>(json_str);
    if (!result) {
        _pipeline_name_label->setText(tr("Invalid JSON"));
        _command_summary_label->setText(
                tr("<span style='color: red;'>Parse error — check JSON syntax</span>"));
        return;
    }

    auto const & seq = *result;
    _session->setPipeline(seq);

    if (_state) {
        _state->setPipelineJson(json_str);
    }

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
    } else {
        _pipeline_name_label->setText(tr("Unnamed Pipeline"));
    }

    // Command list
    auto const cmd_count = pipeline.commands.size();
    if (cmd_count == 0) {
        _command_summary_label->setText(tr("No commands"));
        return;
    }

    QString summary = tr("Commands (%1):").arg(cmd_count);
    int idx = 1;
    for (auto const & cmd: pipeline.commands) {
        summary += QStringLiteral("\n  %1. %2").arg(idx).arg(QString::fromStdString(cmd.command_name));
        if (cmd.description.has_value()) {
            summary += QStringLiteral(": %1").arg(
                    QString::fromStdString(*cmd.description));
        }
        ++idx;
    }
    _command_summary_label->setText(summary);
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
    if (!_state) {
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

        _updateCommandSummary();
        _updateButtonStates();
    }
}
