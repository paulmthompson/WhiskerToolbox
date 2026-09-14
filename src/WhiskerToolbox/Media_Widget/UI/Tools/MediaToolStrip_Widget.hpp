#ifndef MEDIA_TOOL_STRIP_WIDGET_HPP
#define MEDIA_TOOL_STRIP_WIDGET_HPP

/**
 * @file MediaToolStrip_Widget.hpp
 * @brief Vertical toolbar for Media Viewer canvas tools
 */

#include "Media_Widget/UI/Tools/MediaToolId.hpp"

#include <QWidget>

class QButtonGroup;
class QToolButton;
class QVBoxLayout;

/**
 * @brief Inkscape-style vertical tool palette along the left edge of the Media Viewer
 *
 * Exactly one tool button may be selected at a time. Tool behavior is not wired yet;
 * this widget only tracks and exposes the active tool identity.
 */
class MediaToolStrip_Widget : public QWidget {
    Q_OBJECT

public:
    static constexpr int kStripWidth = 36;
    static constexpr int kButtonSize = 32;

    explicit MediaToolStrip_Widget(QWidget * parent = nullptr);

    /**
     * @brief Get the currently selected tool
     * @return Active tool identifier
     */
    [[nodiscard]] MediaToolId activeTool() const { return _active_tool; }

    /**
     * @brief Select a tool and update button checked state
     * @param tool Tool to activate
     * @pre tool must correspond to a registered toolbar button
     */
    void setActiveTool(MediaToolId tool);

signals:
    /**
     * @brief Emitted when the user selects a different tool
     * @param tool Newly active tool
     */
    void activeToolChanged(MediaToolId tool);

private:
    /**
     * @brief Register a checkable tool button in the exclusive group
     * @param tool Tool identifier stored as the button group id
     * @param icon Button icon
     * @param tooltip Hover tooltip text
     */
    void _addToolButton(MediaToolId tool, QIcon const & icon, QString const & tooltip);

    /**
     * @brief Apply shared toolbar styling
     */
    void _applyStyle();

    /**
     * @brief Handle exclusive tool selection from the button group
     * @param id Button group id (MediaToolId value)
     */
    void _onToolIdClicked(int id);

    QVBoxLayout * _layout{nullptr};
    QButtonGroup * _button_group{nullptr};
    MediaToolId _active_tool{MediaToolId::Select};
};

#endif// MEDIA_TOOL_STRIP_WIDGET_HPP
