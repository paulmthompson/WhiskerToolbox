#ifndef TOOL_OPTIONS_LAYOUT_HELPERS_HPP
#define TOOL_OPTIONS_LAYOUT_HELPERS_HPP

/**
 * @file ToolOptionsLayoutHelpers.hpp
 * @brief Shared layout helpers for Media Viewer tool option bar pages
 */

#include <QLabel>
#include <QSizePolicy>
#include <QWidget>

/**
 * @brief Allow a tool options page to shrink within the shared options bar
 * @param widget Tool options page root widget
 */
inline void configureShrinkableToolOptionsPage(QWidget * widget) {
    if (!widget) {
        return;
    }

    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    widget->setMinimumWidth(0);
}

/**
 * @brief Keep long instruction text in a tooltip without reserving bar width
 * @param label Instruction label from a tool options page
 */
inline void configureCompactInstructionLabel(QLabel * label) {
    if (!label) {
        return;
    }

    if (!label->text().isEmpty()) {
        label->setToolTip(label->text());
    }

    label->hide();
}

#endif// TOOL_OPTIONS_LAYOUT_HELPERS_HPP
