#ifndef SMOOTH_TOOL_OPTIONS_WIDGET_HPP
#define SMOOTH_TOOL_OPTIONS_WIDGET_HPP

/**
 * @file SmoothToolOptions_Widget.hpp
 * @brief Options bar page for the Media Viewer Smooth tool
 */

#include <QWidget>

namespace Ui {
class SmoothToolOptions_Widget;
}

class MediaWidgetState;

/**
 * @brief Displays Smooth tool hover-circle size controls
 */
class SmoothToolOptions_Widget : public QWidget {
    Q_OBJECT

public:
    explicit SmoothToolOptions_Widget(QWidget * parent = nullptr);
    ~SmoothToolOptions_Widget() override;

    /**
     * @brief Bind to shared media widget state
     * @param state State object (non-owning)
     */
    void setState(MediaWidgetState * state);

private slots:
    void _onSmoothSizeChanged(int radius_px);
    void _syncFromState();

private:
    Ui::SmoothToolOptions_Widget * ui;
    MediaWidgetState * _state{nullptr};
    bool _updating_from_state{false};
};

#endif// SMOOTH_TOOL_OPTIONS_WIDGET_HPP
