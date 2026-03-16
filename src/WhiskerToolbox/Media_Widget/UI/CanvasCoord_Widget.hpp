/**
 * @file CanvasCoord_Widget.hpp
 * @brief Widget for viewing and overriding the canvas coordinate system
 */
#ifndef CANVAS_COORD_WIDGET_HPP
#define CANVAS_COORD_WIDGET_HPP

#include "CorePlotting/Layout/CanvasCoordinateSystem.hpp"

#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>
#include <QWidget>

class MediaWidgetState;

/**
 * @brief Read-only display and manual override controls for the canvas coordinate system
 *
 * Shows the resolved coordinate system dimensions and source, with an optional
 * manual override toggle and width/height spinboxes.
 */
class CanvasCoord_Widget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasCoord_Widget(QWidget * parent = nullptr);

    void setState(MediaWidgetState * state);

private slots:
    void _onCoordSystemChanged(CanvasCoordinateSystem const & coord_system);
    void _onOverrideToggled(bool checked);
    void _onOverrideDimsChanged();

private:
    void _buildUi();
    void _updateDisplay(CanvasCoordinateSystem const & cs);
    static QString _sourceToString(CanvasCoordinateSource source, std::string const & key);

    MediaWidgetState * _state{nullptr};

    QLabel * _resolved_label{nullptr};
    QLabel * _source_label{nullptr};
    QCheckBox * _override_checkbox{nullptr};
    QSpinBox * _width_spin{nullptr};
    QSpinBox * _height_spin{nullptr};
};

#endif// CANVAS_COORD_WIDGET_HPP
