#ifndef PEN_TOOL_OPTIONS_WIDGET_HPP
#define PEN_TOOL_OPTIONS_WIDGET_HPP

/**
 * @file PenToolOptions_Widget.hpp
 * @brief Instruction-only options bar page for the Media Viewer Pen tool
 */

#include <QWidget>

namespace Ui {
class PenToolOptions_Widget;
}

/**
 * @brief Displays Pen tool usage instructions in the Media Viewer options bar
 */
class PenToolOptions_Widget : public QWidget {
    Q_OBJECT

public:
    explicit PenToolOptions_Widget(QWidget * parent = nullptr);
    ~PenToolOptions_Widget() override;

private:
    Ui::PenToolOptions_Widget * ui;
};

#endif// PEN_TOOL_OPTIONS_WIDGET_HPP
