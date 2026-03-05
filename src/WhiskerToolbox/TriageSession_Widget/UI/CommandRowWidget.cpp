/**
 * @file CommandRowWidget.cpp
 * @brief Implementation of the expandable command row widget
 */

#include "CommandRowWidget.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"

#include <rfl/json.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

CommandRowWidget::CommandRowWidget(commands::CommandInfo info,
                                   QWidget * parent)
    : QWidget(parent),
      _info(std::move(info)) {
    _buildUI();
}

std::string CommandRowWidget::commandName() const {
    return _info.name;
}

commands::CommandDescriptor CommandRowWidget::toDescriptor() const {
    commands::CommandDescriptor desc;
    desc.command_name = _info.name;

    auto const json = _param_widget->toJson();
    auto parsed = rfl::json::read<rfl::Generic>(json);
    if (parsed) {
        desc.parameters = std::move(*parsed);
    }

    desc.description = _info.description;
    return desc;
}

void CommandRowWidget::fromDescriptor(commands::CommandDescriptor const & desc) {
    if (!desc.parameters.has_value()) {
        return;
    }
    auto const json = rfl::json::write(*desc.parameters);
    _param_widget->fromJson(json);
}

void CommandRowWidget::setMoveUpEnabled(bool enabled) {
    _move_up_button->setEnabled(enabled);
}

void CommandRowWidget::setMoveDownEnabled(bool enabled) {
    _move_down_button->setEnabled(enabled);
}

void CommandRowWidget::_buildUI() {
    auto * main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(2);

    // === Header bar ===
    auto * header = new QWidget(this);
    header->setStyleSheet(
            QStringLiteral("background-color: palette(alternate-base); "
                           "border-radius: 3px; padding: 2px;"));
    auto * header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(4, 2, 4, 2);
    header_layout->setSpacing(4);

    _expand_button = new QToolButton(this);
    _expand_button->setArrowType(Qt::RightArrow);
    _expand_button->setAutoRaise(true);
    _expand_button->setToolTip(tr("Expand/collapse parameters"));
    header_layout->addWidget(_expand_button);

    _name_label = new QLabel(QString::fromStdString(_info.name), this);
    _name_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    header_layout->addWidget(_name_label, 1);

    _move_up_button = new QToolButton(this);
    _move_up_button->setArrowType(Qt::UpArrow);
    _move_up_button->setAutoRaise(true);
    _move_up_button->setToolTip(tr("Move up"));
    header_layout->addWidget(_move_up_button);

    _move_down_button = new QToolButton(this);
    _move_down_button->setArrowType(Qt::DownArrow);
    _move_down_button->setAutoRaise(true);
    _move_down_button->setToolTip(tr("Move down"));
    header_layout->addWidget(_move_down_button);

    _remove_button = new QPushButton(tr("Remove"), this);
    _remove_button->setFlat(true);
    _remove_button->setToolTip(tr("Remove this command"));
    header_layout->addWidget(_remove_button);

    main_layout->addWidget(header);

    // === Content (collapsed by default) ===
    _content_widget = new QWidget(this);
    auto * content_layout = new QVBoxLayout(_content_widget);
    content_layout->setContentsMargins(20, 4, 4, 4);

    _param_widget = new AutoParamWidget(_content_widget);
    _param_widget->setSchema(_info.parameter_schema);
    content_layout->addWidget(_param_widget);

    _content_widget->setVisible(false);
    main_layout->addWidget(_content_widget);

    // === Connections ===
    connect(_expand_button, &QToolButton::clicked,
            this, &CommandRowWidget::_toggleExpanded);
    connect(_remove_button, &QPushButton::clicked,
            this, &CommandRowWidget::removeRequested);
    connect(_move_up_button, &QToolButton::clicked,
            this, &CommandRowWidget::moveUpRequested);
    connect(_move_down_button, &QToolButton::clicked,
            this, &CommandRowWidget::moveDownRequested);
    connect(_param_widget, &AutoParamWidget::parametersChanged,
            this, &CommandRowWidget::parametersChanged);
}

void CommandRowWidget::_toggleExpanded() {
    _expanded = !_expanded;
    _content_widget->setVisible(_expanded);
    _expand_button->setArrowType(_expanded ? Qt::DownArrow : Qt::RightArrow);
}
