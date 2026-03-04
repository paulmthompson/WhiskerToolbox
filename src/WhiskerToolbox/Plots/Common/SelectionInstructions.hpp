#ifndef SELECTION_INSTRUCTIONS_HPP
#define SELECTION_INSTRUCTIONS_HPP

/**
 * @file SelectionInstructions.hpp
 * @brief Shared selection mode instruction text for plot widgets
 *
 * Provides human-readable instruction strings for various selection modes
 * (single point, polygon, line). Used by ScatterPlotPropertiesWidget,
 * TemporalProjectionViewPropertiesWidget, and any future widgets that
 * support interactive selection.
 */

#include <QString>

namespace WhiskerToolbox::Plots::SelectionInstructions {

/**
 * @brief Instructions for single-point selection mode
 *
 * Ctrl+Click toggles selection; Shift+Click removes.
 */
inline QString singlePoint()
{
    return QStringLiteral(
        "Ctrl+Click: toggle point selection\n"
        "Shift+Click: remove from selection\n"
        "Right-click: group context menu");
}

/**
 * @brief Instructions for polygon (lasso) selection mode
 */
inline QString polygon()
{
    return QStringLiteral(
        "Ctrl+Click: add polygon vertex\n"
        "Enter: close polygon & select enclosed points\n"
        "Backspace: undo last vertex\n"
        "Escape: cancel polygon\n"
        "Right-click: group context menu");
}

/**
 * @brief Instructions for line-crossing selection mode
 */
inline QString lineCrossing()
{
    return QStringLiteral(
        "Ctrl+Drag: select crossed lines\n"
        "Ctrl+Shift+Drag: remove crossed lines\n"
        "Right-click: group context menu");
}

/**
 * @brief Instructions when no selection mode is active
 */
inline QString none()
{
    return QStringLiteral("No selection mode active");
}

}  // namespace WhiskerToolbox::Plots::SelectionInstructions

#endif  // SELECTION_INSTRUCTIONS_HPP
