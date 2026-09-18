/**
 * @file MediaToolOptionsBar_Widget.cpp
 * @brief Horizontal options strip above the Media Viewer rulers
 */

#include "MediaToolOptionsBar_Widget.hpp"

#include "Core/MediaWidgetState.hpp"
#include "EraserToolOptions_Widget.hpp"
#include "PenToolOptions_Widget.hpp"
#include "SelectToolOptions_Widget.hpp"
#include "SmoothToolOptions_Widget.hpp"

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QStackedWidget>

MediaToolOptionsBar_Widget::MediaToolOptionsBar_Widget(QWidget * parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("MediaToolOptionsBar_Widget"));
    setFixedHeight(kBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _stack = new QStackedWidget(this);
    _empty_page = new QWidget(_stack);
    _select_options = new SelectToolOptions_Widget(_stack);
    _pen_options = new PenToolOptions_Widget(_stack);
    _eraser_options = new EraserToolOptions_Widget(_stack);
    _smooth_options = new SmoothToolOptions_Widget(_stack);

    _stack->addWidget(_empty_page);
    _stack->addWidget(_select_options);
    _stack->addWidget(_pen_options);
    _stack->addWidget(_eraser_options);
    _stack->addWidget(_smooth_options);

    auto * layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_stack);

    _applyStyle();
    setActiveTool(MediaToolId::Select);
}

void MediaToolOptionsBar_Widget::setState(MediaWidgetState * state) {
    if (_select_options) {
        _select_options->setState(state);
    }
    if (_pen_options) {
        _pen_options->setState(state);
    }
    if (_eraser_options) {
        _eraser_options->setState(state);
    }
    if (_smooth_options) {
        _smooth_options->setState(state);
    }
}

void MediaToolOptionsBar_Widget::setActiveTool(MediaToolId tool) {
    if (!_stack) {
        return;
    }

    switch (tool) {
        case MediaToolId::Select:
            if (_select_options) {
                _stack->setCurrentWidget(_select_options);
            }
            break;
        case MediaToolId::Pen:
            if (_pen_options) {
                _stack->setCurrentWidget(_pen_options);
            }
            break;
        case MediaToolId::Eraser:
            if (_eraser_options) {
                _stack->setCurrentWidget(_eraser_options);
            }
            break;
        case MediaToolId::Smooth:
            if (_smooth_options) {
                _stack->setCurrentWidget(_smooth_options);
            }
            break;
        case MediaToolId::None:
        default:
            if (_empty_page) {
                _stack->setCurrentWidget(_empty_page);
            }
            break;
    }
}

QSize MediaToolOptionsBar_Widget::sizeHint() const {
    return {QWidget::sizeHint().width(), kBarHeight};
}

void MediaToolOptionsBar_Widget::_applyStyle() {
    setStyleSheet(QStringLiteral(R"(
        #MediaToolOptionsBar_Widget {
            background-color: rgb(36, 36, 36);
            border-bottom: 1px solid rgb(90, 90, 90);
        }
    )"));
}
