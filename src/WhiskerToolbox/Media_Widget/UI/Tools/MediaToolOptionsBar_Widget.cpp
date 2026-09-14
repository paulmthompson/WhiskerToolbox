/**
 * @file MediaToolOptionsBar_Widget.cpp
 * @brief Horizontal options strip above the Media Viewer rulers
 */

#include "MediaToolOptionsBar_Widget.hpp"

#include "Core/MediaWidgetState.hpp"
#include "SelectToolOptions_Widget.hpp"

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QStackedWidget>

MediaToolOptionsBar_Widget::MediaToolOptionsBar_Widget(QWidget * parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("MediaToolOptionsBar_Widget"));
    setFixedHeight(kBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _stack = new QStackedWidget(this);
    _select_options = new SelectToolOptions_Widget(_stack);

    _stack->addWidget(_select_options);

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
}

void MediaToolOptionsBar_Widget::setActiveTool(MediaToolId tool) {
    switch (tool) {
        case MediaToolId::Select:
            if (_stack && _select_options) {
                _stack->setCurrentWidget(_select_options);
            }
            setVisible(true);
            break;
        default:
            setVisible(false);
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
