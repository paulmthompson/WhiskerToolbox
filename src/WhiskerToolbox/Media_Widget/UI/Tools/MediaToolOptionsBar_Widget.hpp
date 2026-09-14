#ifndef MEDIA_TOOL_OPTIONS_BAR_WIDGET_HPP
#define MEDIA_TOOL_OPTIONS_BAR_WIDGET_HPP

/**
 * @file MediaToolOptionsBar_Widget.hpp
 * @brief Horizontal options strip above the Media Viewer rulers
 */

#include "Media_Widget/UI/Tools/MediaToolId.hpp"

#include <QWidget>

class MediaWidgetState;
class QStackedWidget;
class EraserToolOptions_Widget;
class PenToolOptions_Widget;
class SelectToolOptions_Widget;

/**
 * @brief Contextual tool options displayed above the horizontal ruler
 */
class MediaToolOptionsBar_Widget : public QWidget {
    Q_OBJECT

public:
    static constexpr int kBarHeight = 28;

    explicit MediaToolOptionsBar_Widget(QWidget * parent = nullptr);

    /**
     * @brief Bind to shared media widget state for tool option widgets
     * @param state State object (non-owning)
     */
    void setState(MediaWidgetState * state);

    /**
     * @brief Show options for the active canvas tool
     * @param tool Active tool from the left tool strip
     */
    void setActiveTool(MediaToolId tool);

    /**
     * @brief Access Select tool options widget
     * @return Select options widget (never null after construction)
     */
    [[nodiscard]] SelectToolOptions_Widget * selectOptionsWidget() const { return _select_options; }

    /**
     * @brief Access Pen tool options widget
     * @return Pen options widget (never null after construction)
     */
    [[nodiscard]] PenToolOptions_Widget * penOptionsWidget() const { return _pen_options; }

    /**
     * @brief Access Eraser tool options widget
     * @return Eraser options widget (never null after construction)
     */
    [[nodiscard]] EraserToolOptions_Widget * eraserOptionsWidget() const { return _eraser_options; }

    [[nodiscard]] QSize sizeHint() const override;

private:
    void _applyStyle();

    QStackedWidget * _stack{nullptr};
    QWidget * _empty_page{nullptr};
    SelectToolOptions_Widget * _select_options{nullptr};
    PenToolOptions_Widget * _pen_options{nullptr};
    EraserToolOptions_Widget * _eraser_options{nullptr};
};

#endif// MEDIA_TOOL_OPTIONS_BAR_WIDGET_HPP
