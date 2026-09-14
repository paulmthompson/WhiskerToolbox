#ifndef PEN_TOOL_OPTIONS_WIDGET_HPP
#define PEN_TOOL_OPTIONS_WIDGET_HPP

/**
 * @file PenToolOptions_Widget.hpp
 * @brief Options bar page for the Media Viewer Pen tool
 */

#include <QWidget>

namespace Ui {
class PenToolOptions_Widget;
}

class MediaWidgetState;

/**
 * @brief Displays Pen tool usage instructions and append-endpoint controls
 */
class PenToolOptions_Widget : public QWidget {
    Q_OBJECT

public:
    explicit PenToolOptions_Widget(QWidget * parent = nullptr);
    ~PenToolOptions_Widget() override;

    /**
     * @brief Bind to shared media widget state
     * @param state State object (non-owning)
     */
    void setState(MediaWidgetState * state);

private slots:
    void _onAppendEndpointChanged(int index);
    void _syncFromState();

private:
    void _populateAppendEndpointCombo();

    Ui::PenToolOptions_Widget * ui;
    MediaWidgetState * _state{nullptr};
    bool _updating_from_state{false};
};

#endif// PEN_TOOL_OPTIONS_WIDGET_HPP
