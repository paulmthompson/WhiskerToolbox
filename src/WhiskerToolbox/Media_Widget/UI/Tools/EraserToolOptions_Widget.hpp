#ifndef ERASER_TOOL_OPTIONS_WIDGET_HPP
#define ERASER_TOOL_OPTIONS_WIDGET_HPP

/**
 * @file EraserToolOptions_Widget.hpp
 * @brief Options bar page for the Media Viewer Eraser tool
 */

#include <QWidget>

namespace Ui {
class EraserToolOptions_Widget;
}

class MediaWidgetState;

/**
 * @brief Displays Eraser tool hover-circle size controls
 */
class EraserToolOptions_Widget : public QWidget {
    Q_OBJECT

public:
    explicit EraserToolOptions_Widget(QWidget * parent = nullptr);
    ~EraserToolOptions_Widget() override;

    /**
     * @brief Bind to shared media widget state
     * @param state State object (non-owning)
     */
    void setState(MediaWidgetState * state);

private slots:
    void _onEraserSizeChanged(int radius_px);
    void _syncFromState();

private:
    Ui::EraserToolOptions_Widget * ui;
    MediaWidgetState * _state{nullptr};
    bool _updating_from_state{false};
};

#endif// ERASER_TOOL_OPTIONS_WIDGET_HPP
