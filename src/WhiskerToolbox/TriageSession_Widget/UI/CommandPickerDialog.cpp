/**
 * @file CommandPickerDialog.cpp
 * @brief Implementation of CommandPickerDialog
 */

#include "CommandPickerDialog.hpp"

#include "DataManager/Commands/CommandFactory.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

CommandPickerDialog::CommandPickerDialog(QWidget * parent)
    : QDialog(parent) {
    setWindowTitle(tr("Add Command"));
    setMinimumWidth(350);
    _buildUI();
    _populateCommands();
}

std::optional<std::string> CommandPickerDialog::selectedCommandName() const {
    if (_commands.empty() || _command_combo->currentIndex() < 0) {
        return std::nullopt;
    }
    auto const idx = static_cast<size_t>(_command_combo->currentIndex());
    if (idx >= _commands.size()) {
        return std::nullopt;
    }
    return _commands[idx].name;
}

void CommandPickerDialog::_onSelectionChanged(int index) {
    if (index < 0 || static_cast<size_t>(index) >= _commands.size()) {
        _description_label->clear();
        _category_label->clear();
        return;
    }
    auto const & info = _commands[static_cast<size_t>(index)];
    _description_label->setText(QString::fromStdString(info.description));
    _category_label->setText(tr("Category: %1").arg(QString::fromStdString(info.category)));
}

void CommandPickerDialog::_buildUI() {
    auto * layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Select a command:"), this));

    _command_combo = new QComboBox(this);
    layout->addWidget(_command_combo);

    _description_label = new QLabel(this);
    _description_label->setWordWrap(true);
    _description_label->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(_description_label);

    _category_label = new QLabel(this);
    _category_label->setStyleSheet(QStringLiteral("font-style: italic;"));
    layout->addWidget(_category_label);

    auto * buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(_command_combo, &QComboBox::currentIndexChanged,
            this, &CommandPickerDialog::_onSelectionChanged);
}

void CommandPickerDialog::_populateCommands() {
    _commands = commands::getAvailableCommands();
    for (auto const & info: _commands) {
        _command_combo->addItem(QString::fromStdString(info.name));
    }
    if (!_commands.empty()) {
        _onSelectionChanged(0);
    }
}
