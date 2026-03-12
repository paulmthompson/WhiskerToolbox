/**
 * @file CommandLogWidget.cpp
 * @brief Implementation of the Command Log dock widget
 */

#include "CommandLogWidget.hpp"

#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/CommandRecorder.hpp"

#include <rfl/json.hpp>

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

CommandLogWidget::CommandLogWidget(commands::CommandRecorder * recorder,
                                   QWidget * parent)
    : QWidget(parent),
      _recorder(recorder) {
    auto * layout = new QVBoxLayout(this);

    // Title
    auto * title = new QLabel(QStringLiteral("Command Log"), this);
    title->setAlignment(Qt::AlignCenter);
    auto title_font = title->font();
    title_font.setBold(true);
    title_font.setPointSize(title_font.pointSize() + 1);
    title->setFont(title_font);
    layout->addWidget(title);

    // Command count label
    _count_label = new QLabel(QStringLiteral("0 commands recorded"), this);
    _count_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(_count_label);

    // Command list
    _command_list = new QListWidget(this);
    _command_list->setAlternatingRowColors(true);
    layout->addWidget(_command_list);

    // Buttons
    auto * button_layout = new QHBoxLayout();

    _copy_btn = new QPushButton(QStringLiteral("Copy as JSON"), this);
    _copy_btn->setToolTip(QStringLiteral("Copy recorded commands as a CommandSequenceDescriptor JSON to clipboard"));
    button_layout->addWidget(_copy_btn);

    _clear_btn = new QPushButton(QStringLiteral("Clear"), this);
    _clear_btn->setToolTip(QStringLiteral("Clear all recorded commands"));
    button_layout->addWidget(_clear_btn);

    layout->addLayout(button_layout);

    // Connections
    connect(_copy_btn, &QPushButton::clicked,
            this, &CommandLogWidget::_onCopyAsJsonClicked);
    connect(_clear_btn, &QPushButton::clicked,
            this, &CommandLogWidget::_onClearClicked);

    // Poll for new commands every 500ms
    auto * poll_timer = new QTimer(this);
    connect(poll_timer, &QTimer::timeout, this, &CommandLogWidget::refresh);
    poll_timer->start(500);

    refresh();
}

void CommandLogWidget::refresh() {
    if (!_recorder) {
        return;
    }

    auto const current_size = _recorder->size();
    if (current_size == _last_known_size) {
        return;
    }

    auto const & trace = _recorder->trace();

    // Append only new entries for efficiency
    for (size_t i = _last_known_size; i < trace.size(); ++i) {
        auto const & desc = trace[i];
        QString display_text = QString::fromStdString(desc.command_name);
        if (desc.description) {
            display_text += QStringLiteral(" — ") + QString::fromStdString(*desc.description);
        }
        if (desc.parameters) {
            // Show a compact summary of parameters
            auto params_json = rfl::json::write(*desc.parameters);
            if (params_json.size() > 80) {
                params_json = params_json.substr(0, 77) + "...";
            }
            display_text += QStringLiteral("\n  ") + QString::fromStdString(params_json);
        }
        _command_list->addItem(display_text);
    }

    _last_known_size = current_size;

    // Update count label
    _count_label->setText(QString::number(current_size) + QStringLiteral(" command(s) recorded"));

    // Scroll to bottom to show latest
    if (_command_list->count() > 0) {
        _command_list->scrollToBottom();
    }
}

void CommandLogWidget::_onCopyAsJsonClicked() {
    if (!_recorder) {
        return;
    }

    if (_recorder->empty()) {
        QMessageBox::information(this,
                                 QStringLiteral("Command Log"),
                                 QStringLiteral("No commands recorded yet."));
        return;
    }

    auto sequence = _recorder->toSequence("recorded_session");
    auto json = rfl::json::write(sequence);

    QApplication::clipboard()->setText(QString::fromStdString(json));

    QMessageBox::information(this,
                             QStringLiteral("Command Log"),
                             QStringLiteral("Copied %1 command(s) as JSON to clipboard.")
                                     .arg(_recorder->size()));
}

void CommandLogWidget::_onClearClicked() {
    if (!_recorder) {
        return;
    }

    _recorder->clear();
    _command_list->clear();
    _last_known_size = 0;
    _count_label->setText(QStringLiteral("0 commands recorded"));
}
