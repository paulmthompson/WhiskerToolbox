/**
 * @file GuidedPipelineEditor.cpp
 * @brief Implementation of the guided pipeline editor
 */

#include "GuidedPipelineEditor.hpp"

#include "CommandPickerDialog.hpp"
#include "CommandRowWidget.hpp"

#include "Commands/CommandFactory.hpp"

#include <rfl/json.hpp>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

GuidedPipelineEditor::GuidedPipelineEditor(QWidget * parent)
    : QWidget(parent) {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(4);

    // Commands list area
    _rows_layout = new QVBoxLayout();
    _rows_layout->setContentsMargins(0, 0, 0, 0);
    _rows_layout->setSpacing(2);
    main_layout->addLayout(_rows_layout);

    // Add Command button
    _add_button = new QPushButton(tr("Add Command..."), this);
    _add_button->setToolTip(tr("Add a new command to the pipeline"));
    main_layout->addWidget(_add_button);

    connect(_add_button, &QPushButton::clicked,
            this, &GuidedPipelineEditor::_onAddCommandClicked);
}

commands::CommandSequenceDescriptor GuidedPipelineEditor::toSequence() const {
    commands::CommandSequenceDescriptor seq;
    seq.commands.reserve(_rows.size());
    for (auto const * row: _rows) {
        seq.commands.push_back(row->toDescriptor());
    }
    return seq;
}

void GuidedPipelineEditor::fromSequence(
        commands::CommandSequenceDescriptor const & seq) {
    _updating = true;
    clear();

    for (auto const & cmd: seq.commands) {
        auto info_opt = commands::getCommandInfo(cmd.command_name);
        if (!info_opt.has_value()) {
            continue;// Skip unknown commands
        }

        auto * row = new CommandRowWidget(std::move(*info_opt), this);
        row->fromDescriptor(cmd);

        connect(row, &CommandRowWidget::removeRequested,
                this, [this, row]() { _removeRow(row); });
        connect(row, &CommandRowWidget::moveUpRequested,
                this, [this, row]() { _moveRowUp(row); });
        connect(row, &CommandRowWidget::moveDownRequested,
                this, [this, row]() { _moveRowDown(row); });
        connect(row, &CommandRowWidget::parametersChanged,
                this, &GuidedPipelineEditor::_onRowChanged);

        _rows_layout->addWidget(row);
        _rows.push_back(row);
    }

    _updateMoveButtons();
    _updating = false;
}

void GuidedPipelineEditor::clear() {
    for (auto * row: _rows) {
        _rows_layout->removeWidget(row);
        row->deleteLater();
    }
    _rows.clear();
}

void GuidedPipelineEditor::_onAddCommandClicked() {
    CommandPickerDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto const name = dialog.selectedCommandName();
    if (!name.has_value()) {
        return;
    }

    _addCommandRow(*name);
    emit pipelineChanged();
}

void GuidedPipelineEditor::_addCommandRow(std::string const & command_name) {
    auto info_opt = commands::getCommandInfo(command_name);
    if (!info_opt.has_value()) {
        return;
    }

    auto * row = new CommandRowWidget(std::move(*info_opt), this);

    connect(row, &CommandRowWidget::removeRequested,
            this, [this, row]() { _removeRow(row); });
    connect(row, &CommandRowWidget::moveUpRequested,
            this, [this, row]() { _moveRowUp(row); });
    connect(row, &CommandRowWidget::moveDownRequested,
            this, [this, row]() { _moveRowDown(row); });
    connect(row, &CommandRowWidget::parametersChanged,
            this, &GuidedPipelineEditor::_onRowChanged);

    _rows_layout->addWidget(row);
    _rows.push_back(row);
    _updateMoveButtons();
}

void GuidedPipelineEditor::_removeRow(CommandRowWidget * row) {
    auto it = std::find(_rows.begin(), _rows.end(), row);
    if (it == _rows.end()) {
        return;
    }

    _rows.erase(it);
    _rows_layout->removeWidget(row);
    row->deleteLater();

    _updateMoveButtons();
    emit pipelineChanged();
}

void GuidedPipelineEditor::_moveRowUp(CommandRowWidget * row) {
    auto it = std::find(_rows.begin(), _rows.end(), row);
    if (it == _rows.end() || it == _rows.begin()) {
        return;
    }

    auto const idx = static_cast<int>(std::distance(_rows.begin(), it));
    std::iter_swap(it - 1, it);

    // Re-layout: remove and re-insert
    _rows_layout->removeWidget(row);
    _rows_layout->insertWidget(idx - 1, row);

    _updateMoveButtons();
    emit pipelineChanged();
}

void GuidedPipelineEditor::_moveRowDown(CommandRowWidget * row) {
    auto it = std::find(_rows.begin(), _rows.end(), row);
    if (it == _rows.end() || std::next(it) == _rows.end()) {
        return;
    }

    auto const idx = static_cast<int>(std::distance(_rows.begin(), it));
    std::iter_swap(it, it + 1);

    // Re-layout: remove and re-insert
    _rows_layout->removeWidget(row);
    _rows_layout->insertWidget(idx + 1, row);

    _updateMoveButtons();
    emit pipelineChanged();
}

void GuidedPipelineEditor::_updateMoveButtons() {
    for (size_t i = 0; i < _rows.size(); ++i) {
        _rows[i]->setMoveUpEnabled(i > 0);
        _rows[i]->setMoveDownEnabled(i + 1 < _rows.size());
    }
}

void GuidedPipelineEditor::_onRowChanged() {
    if (!_updating) {
        emit pipelineChanged();
    }
}
