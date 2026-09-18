/**
 * @file MediaToolStrip_Widget.cpp
 * @brief Vertical toolbar for Media Viewer canvas tools
 */

#include "MediaToolStrip_Widget.hpp"

#include "Media_Widget/UI/Tools/MediaToolIcons.hpp"

#include <QButtonGroup>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include <cassert>

MediaToolStrip_Widget::MediaToolStrip_Widget(QWidget * parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("MediaToolStrip_Widget"));
    setFixedWidth(kStripWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(2, 4, 2, 4);
    _layout->setSpacing(2);

    _button_group = new QButtonGroup(this);
    _button_group->setExclusive(true);

    _addToolButton(MediaToolId::Select,
                   MediaToolIcons::createSelectToolIcon(),
                   tr("Select — click again to deactivate"));

    _addToolButton(MediaToolId::Pen,
                   MediaToolIcons::createPenToolIcon(),
                   tr("Pen — edit selected line"));

    _addToolButton(MediaToolId::Eraser,
                   MediaToolIcons::createEraserToolIcon(),
                   tr("Eraser — remove vertices from selected line"));

    _addToolButton(MediaToolId::Smooth,
                   MediaToolIcons::createSmoothToolIcon(),
                   tr("Smooth — local smoothing on selected line"));

    _layout->addStretch();

    connect(_button_group, &QButtonGroup::idClicked, this, &MediaToolStrip_Widget::_onToolIdClicked);

    _applyStyle();
    setActiveTool(MediaToolId::Select);
}

void MediaToolStrip_Widget::_uncheckAllButtons() {
    _button_group->setExclusive(false);
    for (auto * button: _button_group->buttons()) {
        button->setChecked(false);
    }
    _button_group->setExclusive(true);
}

void MediaToolStrip_Widget::setActiveTool(MediaToolId tool) {
    if (tool == MediaToolId::None) {
        if (_active_tool == MediaToolId::None) {
            return;
        }

        _uncheckAllButtons();
        _active_tool = MediaToolId::None;
        emit activeToolChanged(MediaToolId::None);
        return;
    }

    auto * button = _button_group->button(static_cast<int>(tool));
    assert(button != nullptr && "setActiveTool: unknown tool id");

    if (_active_tool == tool && button->isChecked()) {
        return;
    }

    button->setChecked(true);
    _active_tool = tool;
    emit activeToolChanged(tool);
}

void MediaToolStrip_Widget::_addToolButton(MediaToolId tool, QIcon const & icon, QString const & tooltip) {
    auto * button = new QToolButton(this);
    button->setCheckable(true);
    button->setAutoRaise(false);
    button->setIcon(icon);
    button->setIconSize(QSize(kButtonSize - 8, kButtonSize - 8));
    button->setToolTip(tooltip);
    button->setFixedSize(kButtonSize, kButtonSize);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    _button_group->addButton(button, static_cast<int>(tool));
    _layout->addWidget(button, 0, Qt::AlignHCenter);
}

void MediaToolStrip_Widget::_applyStyle() {
    setStyleSheet(QStringLiteral(R"(
        #MediaToolStrip_Widget {
            background-color: rgb(30, 30, 30);
            border-right: 1px solid rgb(150, 150, 150);
        }
        #MediaToolStrip_Widget QToolButton {
            background-color: rgb(42, 42, 42);
            border: 1px solid rgb(60, 60, 60);
            border-radius: 2px;
        }
        #MediaToolStrip_Widget QToolButton:hover:!checked {
            background-color: rgb(55, 55, 55);
        }
        #MediaToolStrip_Widget QToolButton:checked {
            background-color: rgb(58, 82, 118);
            border: 1px solid rgb(110, 150, 210);
        }
    )"));
}

void MediaToolStrip_Widget::_onToolIdClicked(int id) {
    auto const tool = static_cast<MediaToolId>(id);
    if (_active_tool == tool) {
        _uncheckAllButtons();
        _active_tool = MediaToolId::None;
        emit activeToolChanged(MediaToolId::None);
        return;
    }

    _active_tool = tool;
    emit activeToolChanged(tool);
}
